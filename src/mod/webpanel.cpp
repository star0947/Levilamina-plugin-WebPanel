// ======================== WebPanel v1.0.0 (C++) ========================
// LeviLamina Native 插件 - 服务器状态网页面板
// 端口: 9047
// =================================================================

#include "ll/api/memory/MemoryOperators.h"
#include "ll/api/Logger.h"
#include "ll/api/service/ServiceManager.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/event/player/PlayerLeftEvent.h"
#include "ll/api/event/player/PlayerChatEvent.h"
#include "ll/api/event/player/PlayerDieEvent.h"
#include "ll/api/mc/Player.hpp"
#include "ll/api/mc/Level.hpp"
#include "ll/api/http/HttpServer.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <ctime>
#include <deque>
#include <mutex>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

const std::string PLUGIN_NAME = "WebPanel";
const std::string PLUGIN_VERSION = "1.0.0";
const int WEB_PORT = 9047;
const std::string LOG_FILE = "./plugins/WebPanel/logs.json";

std::deque<json> g_logs;
std::mutex g_logMutex;
const size_t MAX_LOGS = 500;

// --- 辅助函数 ---
void initDirectory() {
    fs::create_directories("./plugins/WebPanel");
}

std::string getLogFilePath() {
    return "./plugins/WebPanel/logs.json";
}

void loadLogsFromFile() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ifstream ifs(getLogFilePath());
    if (!ifs.is_open()) return;
    try {
        json j;
        ifs >> j;
        if (j.is_array()) {
            for (const auto& item : j) {
                g_logs.push_back(item);
                if (g_logs.size() > MAX_LOGS) g_logs.pop_front();
            }
        }
    } catch (...) {}
}

void saveLogsToFile() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ofstream ofs(getLogFilePath());
    if (!ofs.is_open()) return;
    json j = json::array();
    for (const auto& entry : g_logs) j.push_back(entry);
    ofs << j.dump(2);
}

void appendLog(const json& entry) {
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        g_logs.push_back(entry);
        if (g_logs.size() > MAX_LOGS) g_logs.pop_front();
    }
    saveLogsToFile();
}

std::vector<json> getLogsData(int limit) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::vector<json> result;
    int start = std::max(0, static_cast<int>(g_logs.size()) - limit);
    for (size_t i = start; i < g_logs.size(); ++i)
        result.push_back(g_logs[i]);
    return result;
}

// --- API 数据获取 ---
json getOnlinePlayersData() {
    json players = json::array();
    auto level = ll::service::getLevel();
    if (level) {
        for (auto player : level->getAllPlayers()) {
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
    }
    return players;
}

json getWorldData() {
    auto level = ll::service::getLevel();
    if (!level) return {};
    return {
        {"time", level->getTime()},
        {"seed", level->getSeed()},
        {"weather", static_cast<int>(level->getWeather())},
        {"playerCount", level->getAllPlayers().size()}
    };
}

// --- 事件监听 ---
void registerEvents() {
    using namespace ll::event;
    auto& bus = EventBus::getInstance();

    bus.emplaceListener<PlayerJoinEvent>([](const PlayerJoinEvent& ev) {
        auto& player = ev.self();
        ll::Logger::info("{} 加入了游戏", player.getRealName());
        appendLog({
            {"timestamp", [](){ auto t = std::time(nullptr); char buf[32]; std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t)); return std::string(buf); }()},
            {"player", player.getRealName()},
            {"action", "join"},
            {"ip", player.getIP()}
        });
    });

    bus.emplaceListener<PlayerLeftEvent>([](const PlayerLeftEvent& ev) {
        auto& player = ev.self();
        ll::Logger::info("{} 离开了游戏", player.getRealName());
        appendLog({
            {"timestamp", [](){ auto t = std::time(nullptr); char buf[32]; std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t)); return std::string(buf); }()},
            {"player", player.getRealName()},
            {"action", "leave"}
        });
    });

    bus.emplaceListener<PlayerChatEvent>([](const PlayerChatEvent& ev) {
        auto& player = ev.self();
        appendLog({
            {"timestamp", [](){ auto t = std::time(nullptr); char buf[32]; std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t)); return std::string(buf); }()},
            {"player", player.getRealName()},
            {"action", "chat"},
            {"message", ev.message()}
        });
    });

    bus.emplaceListener<PlayerDieEvent>([](const PlayerDieEvent& ev) {
        auto& player = ev.self();
        std::string cause = "unknown";
        if (auto src = ev.source(); src) cause = src->getName();
        appendLog({
            {"timestamp", [](){ auto t = std::time(nullptr); char buf[32]; std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t)); return std::string(buf); }()},
            {"player", player.getRealName()},
            {"action", "death"},
            {"cause", cause}
        });
    });
}

// --- HTTP 服务器 ---
void setupHttpServer() {
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
        res.body = getOnlinePlayersData().dump();
        res.status = 200;
    });

    http.onGet("/api/world", [](const ll::http::HttpRequest&, ll::http::HttpResponse& res) {
        res.setHeader("Content-Type", "application/json");
        res.body = getWorldData().dump();
        res.status = 200;
    });

    http.onGet("/api/logs", [](const ll::http::HttpRequest& req, ll::http::HttpResponse& res) {
        int limit = 100;
        if (req.hasParam("limit")) limit = std::stoi(req.getParam("limit"));
        auto logs = getLogsData(limit);
        json arr = json::array();
        for (const auto& l : logs) arr.push_back(l);
        res.setHeader("Content-Type", "application/json");
        res.body = arr.dump();
        res.status = 200;
    });

    http.listen("0.0.0.0", WEB_PORT, []() {
        ll::Logger::info("HTTP 服务器已启动，访问 http://0.0.0.0:{} 查看面板", WEB_PORT);
    });
}

// --- 插件入口（LL 新架构）---
LL_MEMORY_OPERATORS();

bool ll_mod_load() {
    initDirectory();
    loadLogsFromFile();
    return true;
}

bool ll_mod_enable() {
    registerEvents();
    setupHttpServer();
    ll::Logger::info("{} v{} 已启用！", PLUGIN_NAME, PLUGIN_VERSION);
    return true;
}

bool ll_mod_disable() {
    ll::http::HttpServer::getInstance().stop();
    ll::Logger::info("{} 已禁用！", PLUGIN_NAME);
    return true;
}