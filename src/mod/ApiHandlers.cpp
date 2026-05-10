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
#include <future>
#include <string>

// 辅助函数：解析玩家名/UUID -> UUID
static bool resolveUuid(const std::string& id, mce::UUID& outUuid) {
    // 尝试在线玩家
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
    // 离线玩家使用 PlayerInfo
    auto& info = ll::service::PlayerInfo::getInstance();
    if (auto entry = info.fromName(id); entry) {
        outUuid = entry->uuid;
        return true;
    }
    // 直接当作 UUID 字符串解析
    outUuid = mce::UUID(id);
    return (bool)outUuid;
}

void ApiHandlers::getPlayers(const httplib::Request& req, httplib::Response& res, LogManager&) {
    std::promise<std::string> promise;
    auto future = promise.get_future();
    ll::thread::ServerThreadExecutor::getDefault().execute([&promise]() {
        nlohmann::json result;
        result["players"] = nlohmann::json::array();
        auto level = ll::service::getLevel();
        if (level) {
            level->forEachPlayer([&result](Player& player) {
                nlohmann::json pj;
                pj["name"] = player.getRealName();
                pj["uuid"] = player.getUuid().asString();
                result["players"].push_back(pj);
                return true;
            });
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
        status["error"] = "player not found"; // 默认错误

        auto level = ll::service::getLevel();
        if (!level) {
            promise.set_value(status.dump());
            return;
        }

        // 查找在线玩家
        Player* foundPlayer = nullptr;
        level->forEachPlayer([&](Player& p) {
            if (p.getRealName() == id || p.getUuid().asString() == id) {
                foundPlayer = &p;
                return false;
            }
            return true;
        });

        if (!foundPlayer) {
            promise.set_value(status.dump());
            return;
        }

        Player& player = *foundPlayer;
        status.clear();
        status["name"]        = player.getRealName();
        status["uuid"]        = player.getUuid().asString();
        status["health"]      = player.getHealth();
        status["max_health"]  = player.getMaxHealth();
        status["hunger"]      = player.getAttribute(Player::HUNGER()).mPtr->getCurrentValue();
        status["saturation"]  = player.getAttribute(Player::SATURATION()).mPtr->getCurrentValue();
        status["experience"]  = player.getAttribute(Player::EXPERIENCE()).mPtr->getCurrentValue();
        status["level"]       = static_cast<int>(player.getAttribute(Player::LEVEL()).mPtr->getCurrentValue());
        status["dimension"]   = static_cast<int>(player.getDimensionId());
        auto pos = player.getPosition();
        status["position"]    = {pos.x, pos.y, pos.z};
        status["gamemode"]    = static_cast<int>(player.getPlayerGameType());

        // 药水效果
        auto* comp = player.getEntityContext().tryGetComponent<MobEffectsComponent>();
        auto& effects = status["effects"] = nlohmann::json::array();
        if (comp) {
            for (auto const& eff : comp->mMobEffects) {
                effects.push_back({
                    {"id", eff.getId()},
                    {"amplifier", eff.getAmplifier()},
                    {"duration", eff.getDuration()}
                });
            }
        }

        promise.set_value(status.dump());
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
            {"time", r.timestamp},
            {"dimension", r.dimId},
            {"position", {r.x, r.y, r.z}},
            {"cause", r.causeName},
            {"attacker", r.attackerName}
        });
    }
    if (!records.empty()) {
        result["next_cursor"] = records.back().timestamp;
    }
    res.set_content(result.dump(), "application/json");
}