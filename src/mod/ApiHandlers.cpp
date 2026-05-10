#include "ApiHandlers.h"
#include "JsonHelper.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/service/PlayerInfo.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include "mc/platform/UUID.h"
#include "mc/entity/components/MobEffectsComponent.h"
#include "mc/world/effect/MobEffectInstance.h"
#include "mc/world/attribute/AttributeInstanceConstRef.h"
#include "mc/world/attribute/AttributeInstance.h"
#include "ll/api/io/Logger.h"
#include "ll/api/mod/NativeMod.h"
#include <future>
#include <string>
#include <sstream>

static ll::io::Logger& getApiLogger() {
    return ll::mod::NativeMod::current()->getLogger();
}

static bool resolveUuid(const std::string& id, mce::UUID& outUuid) {
    auto level = ll::service::getLevel();
    if (level) {
        bool found = false;
        level->forEachPlayer([&](Player& p) {
            if (p.getRealName() == id || p.getUuid().asString() == id) {
                outUuid = p.getUuid();
                found = true;
                return false;
            }
            return true;
        });
        if (found) return true;
    }
    auto& info = ll::service::PlayerInfo::getInstance();
    if (auto entry = info.fromName(id); entry) {
        outUuid = entry->uuid;
        return true;
    }
    outUuid = mce::UUID(id);
    return (bool)outUuid;
}

void ApiHandlers::getPlayers(const httplib::Request& /*req*/, httplib::Response& res, LogManager&) {
    std::promise<std::string> promise;
    auto future = promise.get_future();
    ll::thread::ServerThreadExecutor::getDefault().execute([&promise]() {
        nlohmann::json result;
        result["players"] = nlohmann::json::array();
        auto level = ll::service::getLevel();
        if (level) {
            int count = 0;
            level->forEachPlayer([&](Player& player) {
                nlohmann::json pj;
                pj["name"] = player.getRealName();
                pj["uuid"] = player.getUuid().asString();
                result["players"].push_back(pj);
                ++count;
                return true;
            });
            getApiLogger().info("Players list: {} players online", count);
        } else {
            getApiLogger().error("Level not available for player list");
        }
        promise.set_value(result.dump());
    });
    res.set_content(future.get(), "application/json");
}

void ApiHandlers::getPlayerActions(const httplib::Request& req, httplib::Response& res, LogManager& logMgr) {
    if (req.matches.size() < 2) {
        res.status = 400;
        res.set_content("{\"error\":\"invalid path\"}", "application/json");
        return;
    }
    std::string id = req.matches[1];
    std::string type = req.get_param_value("type");
    int limit = 50;
    if (auto l = req.get_param_value("limit"); !l.empty()) limit = std::stoi(l);
    int64_t before = 0;
    if (auto b = req.get_param_value("before"); !b.empty()) before = std::stoll(b);
    if (type != "block") {
        res.status = 400;
        res.set_content("{\"error\":\"unsupported type\"}", "application/json");
        return;
    }
    mce::UUID uuid;
    if (!resolveUuid(id, uuid)) {
        res.status = 404;
        res.set_content("{\"error\":\"player not found\"}", "application/json");
        return;
    }
    auto history = logMgr.getHistory(uuid, before, limit);
    nlohmann::json result;
    result["player"] = id;
    result["actions"] = nlohmann::json::array();
    for (auto& act : history) {
        result["actions"].push_back(JsonHelper::toJson(act));
    }
    if (!history.empty()) {
        result["next_cursor"] = history.back().lastTime;
    }
    getApiLogger().info("Block actions for {}: {} records returned", id, history.size());
    res.set_content(result.dump(), "application/json");
}

void ApiHandlers::getPlayerStatus(const httplib::Request& req, httplib::Response& res) {
    if (req.matches.size() < 2) {
        res.status = 400;
        res.set_content("{\"error\":\"invalid path\"}", "application/json");
        return;
    }
    std::string id = req.matches[1];
    std::promise<std::string> promise;
    auto future = promise.get_future();

    ll::thread::ServerThreadExecutor::getDefault().execute([&]() {
        nlohmann::json status;
        status["error"] = "player not found";

        try {
            auto level = ll::service::getLevel();
            if (!level) {
                getApiLogger().error("Level not available for status query");
                promise.set_value(status.dump());
                return;
            }

            Player* foundPlayer = nullptr;
            level->forEachPlayer([&](Player& p) {
                if (p.getRealName() == id || p.getUuid().asString() == id) {
                    foundPlayer = &p;
                    return false;
                }
                return true;
            });

            if (!foundPlayer) {
                getApiLogger().warn("Player '{}' not found in status query", id);
                promise.set_value(status.dump());
                return;
            }

            Player& player = *foundPlayer;
            status.clear();
            status["name"]       = player.getRealName();
            status["uuid"]       = player.getUuid().asString();
            status["health"]     = player.getHealth();
            status["max_health"] = player.getMaxHealth();

            // 安全读取属性，增加空指针检查
            auto hungerAttr = player.getAttribute(Player::HUNGER());
            status["hunger"] = (hungerAttr.mPtr) ? hungerAttr.mPtr->mCurrentValue : 0.0f;
            
            auto satAttr = player.getAttribute(Player::SATURATION());
            status["saturation"] = (satAttr.mPtr) ? satAttr.mPtr->mCurrentValue : 0.0f;

            auto xpAttr = player.getAttribute(Player::EXPERIENCE());
            status["experience"] = (xpAttr.mPtr) ? xpAttr.mPtr->mCurrentValue : 0.0f;

            auto levelAttr = player.getAttribute(Player::LEVEL());
            status["level"] = (levelAttr.mPtr) ? static_cast<int>(levelAttr.mPtr->mCurrentValue) : 0;

            status["dimension"]  = static_cast<int>(player.getDimensionId());
            auto pos = player.getPosition();
            status["position"]   = {pos.x, pos.y, pos.z};
            status["gamemode"]   = static_cast<int>(player.getPlayerGameType());

            // 药水效果
            auto comp = player.getEntityContext().tryGetComponent<MobEffectsComponent>();
            auto& effects = status["effects"] = nlohmann::json::array();
            if (comp) {
                try {
                    for (auto const& effect : comp->mMobEffects.get()) {
                        effects.push_back({
                            {"id",        static_cast<int>(effect.mId)},
                            {"amplifier", effect.mAmplifier},
                            {"duration",  effect.mDuration.get().mValue}
                        });
                    }
                } catch(...) {
                    getApiLogger().error("Exception iterating mob effects for player {}", player.getRealName());
                }
            }

            getApiLogger().info("Status for {}: health={}/{} pos=({:.1f},{:.1f},{:.1f})", 
                player.getRealName(), status["health"], status["max_health"], pos.x, pos.y, pos.z);
            promise.set_value(status.dump());

        } catch(const std::exception& e) {
            getApiLogger().error("Exception in getPlayerStatus: {}", e.what());
            status.clear();
            status["error"] = "internal error";
            promise.set_value(status.dump());
        } catch(...) {
            getApiLogger().error("Unknown exception in getPlayerStatus for player {}", id);
            status.clear();
            status["error"] = "internal error";
            promise.set_value(status.dump());
        }
    });

    res.set_content(future.get(), "application/json");
}

void ApiHandlers::getPlayerDeaths(const httplib::Request& req, httplib::Response& res, DeathLogger& deathLogger) {
    if (req.matches.size() < 2) {
        res.status = 400;
        res.set_content("{\"error\":\"invalid path\"}", "application/json");
        return;
    }
    std::string id = req.matches[1];
    int limit = 50;
    if (auto l = req.get_param_value("limit"); !l.empty()) limit = std::stoi(l);
    int64_t before = 0;
    if (auto b = req.get_param_value("before"); !b.empty()) before = std::stoll(b);

    mce::UUID uuid;
    if (!resolveUuid(id, uuid)) {
        res.status = 404;
        res.set_content("{\"error\":\"player not found\"}", "application/json");
        return;
    }

    auto records = deathLogger.getRecords(uuid, before, limit);
    nlohmann::json result;
    result["player"] = id;
    auto& deaths = result["deaths"] = nlohmann::json::array();
    for (auto const& r : records) {
        deaths.push_back({
            {"time",      r.timestamp},
            {"dimension", r.dimId},
            {"position",  {r.x, r.y, r.z}},
            {"cause",     r.causeName},
            {"attacker",  r.attackerName}
        });
    }
    if (!records.empty()) {
        result["next_cursor"] = records.back().timestamp;
    }
    getApiLogger().info("Deaths for {}: {} records", id, records.size());
    res.set_content(result.dump(), "application/json");
}