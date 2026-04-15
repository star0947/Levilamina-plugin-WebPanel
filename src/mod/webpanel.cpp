#include "WebPanel.h"
#include "ll/api/LoggerAPI.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/event/player/PlayerLeftEvent.h"
#include "ll/api/event/player/PlayerChatEvent.h"
#include "ll/api/event/player/PlayerDieEvent.h"
#include "ll/api/mc/Player.hpp"
#include "ll/api/mc/Level.hpp"
#include "ll/api/mc/ServerPlayer.hpp"
#include "ll/api/http/HttpServer.h"
#include "ll/api/utils/StringUtils.h"
#include <fstream>
#include <sstream>
#include <ctime>
#include <filesystem>

#define PLUGIN_NAME "WebPanel"
#define PLUGIN_VERSION "1.0.0"
#define WEB_PORT 9047

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace webpanel {

WebPanel& WebPanel::getInstance() {
    static WebPanel instance;
    return instance;
}

void WebPanel::initDirectory() const {
    fs::create_directories("./plugins/WebPanel");
}

std::string WebPanel::getLogFilePath() const {
    return "./plugins/WebPanel/logs.json";
}

void WebPanel::loadLogsFromFile() {
    std::lock_guard lock(mLogMutex);
    std::ifstream ifs(getLogFilePath());
    if (!ifs.is_open()) return;
    try {
        json j;
        ifs >> j;
        if (j.is_array()) {
            for (const auto& item : j) {
                mLogs.push_back(item);
                if (mLogs.size() > MAX_LOGS) mLogs.pop_front();
            }
        }
    } catch (...) {}
}

void WebPanel::saveLogsToFile() const {
    std::lock_guard lock(mLogMutex);
    std::ofstream ofs(getLogFilePath());
    if (!ofs.is_open()) return;
    json j = json::array();
    for (const auto& entry : mLogs) j.push_back(entry);
    ofs << j.dump(2);
}

void WebPanel::appendLog(const json& entry) {
    {
        std::lock_guard lock(mLogMutex);
        mLogs.push_back(entry);
        if (mLogs.size() > MAX_LOGS) mLogs.pop_front();
    }
    saveLogsToFile();
}

std::vector<json> WebPanel::getLogs(int limit) const {
    std::lock_guard lock(mLogMutex);
    std::vector<json> result;
    int start = std::max(0, static_cast<int>(mLogs.size()) - limit);
    for (size_t i = start; i < mLogs.size(); ++i)
        result.push_back(mLogs[i]);
    return result;
}

json WebPanel::getOnlinePlayersData() const {
    json players = json::array();
    for (auto player : ll::service::getLevel()->getAllPlayers()) {
        auto pos = player->getPosition();
        players.push_back({
            {"name", player->getRealName()},
            {"uuid", player->getUuid()},
            {"gameMode", static_cast<int>(player->getGameMode())},
            {"health", player->getHealth()},
            {"maxHealth", player->getMaxHealth()},
            {"pos", {{"x", pos.x}, {"y", pos.y}, {"z", pos.z}}},
            {"dimension", static_cast<int>(pos.dim)}
        });
    }
    return players;
}

json WebPanel::getWorldData() const {
    auto level = ll::service::getLevel();
    return {
        {"time", level->getTime()},
        {"seed", level->getSeed()},
        {"weather", static_cast<int>(level->getWeather())},
        {"playerCount", level->getAllPlayers().size()}
    };
}

static std::string getCurrentTimestamp() {
    auto t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return buf;
}

bool WebPanel::load() {
    initDirectory();
    loadLogsFromFile();
    return true;
}

bool WebPanel::enable() {
    using namespace ll::event;

    // 玩家加入
    EventBus::getInstance().emplaceListener<PlayerJoinEvent>([](const PlayerJoinEvent& ev) {
        auto player = ev.self();
        getInstance().getSelf().getLogger().info("{} 加入了游戏", player.getRealName());
        json log = {
            {"timestamp", getCurrentTimestamp()},
            {"player", player.getRealName()},
            {"action", "join"},
            {"ip", player.getIP()}
        };
        getInstance().appendLog(log);
    });

    // 玩家离开
    EventBus::getInstance().emplaceListener<PlayerLeftEvent>([](const PlayerLeftEvent& ev) {
        auto player = ev.self();
        getInstance().getSelf().getLogger().info("{} 离开了游戏", player.getRealName());
        json log = {
            {"timestamp", getCurrentTimestamp()},
            {"player", player.getRealName()},
            {"action", "leave"}
        };
        getInstance().appendLog(log);
    });

    // 聊天
    EventBus::getInstance().emplaceListener<PlayerChatEvent>([](const PlayerChatEvent& ev) {
        auto player = ev.self();
        json log = {
            {"timestamp", getCurrentTimestamp()},
            {"player", player.getRealName()},
            {"action", "chat"},
            {"message", ev.message()}
        };
        getInstance().appendLog(log);
    });

    // 死亡
    EventBus::getInstance().emplaceListener<PlayerDieEvent>([](const PlayerDieEvent& ev) {
        auto player = ev.self();
        std::string cause = "unknown";
        if (auto src = ev.source(); src) cause = src->getName();
        json log = {
            {"timestamp", getCurrentTimestamp()},
            {"player", player.getRealName()},
            {"action", "death"},
            {"cause", cause}
        };
        getInstance().appendLog(log);
    });

    // HTTP 服务器
    auto& http = ll::http::HttpServer::getInstance();
    http.onGet("/", [](const ll::http::HttpRequest&, ll::http::HttpResponse& res) {
        res.setHeader("Content-Type", "text/html; charset=utf-8");
        std::ifstream ifs("./plugins/WebPanel/index.html");
        if (ifs) {
            std::stringstream buf;
            buf << ifs.rdbuf();
            res.body = buf.str();
        } else {
            res.body = "<h1>Error: index.html not found</h1>";
        }
        res.status = 200;
    });

    http.onGet("/api/players", [](const ll::http::HttpRequest&, ll::http::HttpResponse& res) {
        res.setHeader("Content-Type", "application/json");
        res.body = getInstance().getOnlinePlayersData().dump();
        res.status = 200;
    });

    http.onGet("/api/world", [](const ll::http::HttpRequest&, ll::http::HttpResponse& res) {
        res.setHeader("Content-Type", "application/json");
        res.body = getInstance().getWorldData().dump();
        res.status = 200;
    });

    http.onGet("/api/logs", [](const ll::http::HttpRequest& req, ll::http::HttpResponse& res) {
        int limit = 100;
        if (req.hasParam("limit")) limit = std::stoi(req.getParam("limit"));
        auto logs = getInstance().getLogs(limit);
        json arr = json::array();
        for (const auto& l : logs) arr.push_back(l);
        res.setHeader("Content-Type", "application/json");
        res.body = arr.dump();
        res.status = 200;
    });

    http.listen("0.0.0.0", WEB_PORT, []() {
        getInstance().getSelf().getLogger().info("HTTP Server started on 0.0.0.0:{}", WEB_PORT);
    });

    return true;
}

bool WebPanel::disable() {
    ll::http::HttpServer::getInstance().stop();
    return true;
}

} // namespace webpanel

LL_REGISTER_MOD(webpanel::WebPanel, webpanel::WebPanel::getInstance());