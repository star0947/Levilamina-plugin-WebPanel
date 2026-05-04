#include "ApiHandlers.h"
#include "JsonHelper.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/service/PlayerInfo.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include "mc/platform/UUID.h"
#include <future>
#include <string>

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
    // Route: /api/player/([^/]+)/actions
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
    
    // Resolve UUID from name or uuid string
    mce::UUID uuid;
    bool found = false;
    // Search online players
    auto level = ll::service::getLevel();
    if (level) {
        level->forEachPlayer([&](Player& p) {
            if (p.getRealName() == id || p.getUuid().asString() == id) {
                uuid = p.getUuid();
                found = true;
                return false; // stop iteration
            }
            return true;
        });
    }
    // If not online, try PlayerInfo service
    if (!found) {
        auto& info = ll::service::PlayerInfo::getInstance();
        if (auto entry = info.fromName(id); entry) {
            uuid = entry->uuid;
            found = true;
        }
    }
    // Also try as UUID string directly
    if (!found) {
        uuid = mce::UUID(id);
        if (uuid) found = true;
    }
    
    if (!found || !uuid) {
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
        // Use the oldest timestamp in returned set as next cursor
        result["next_cursor"] = history.back().lastTime;
    }
    
    res.set_content(result.dump(), "application/json");
}