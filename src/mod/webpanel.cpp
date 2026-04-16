// ======================== WebPanel v1.0.0 (C++) ========================
// LeviLamina Native 插件 - 服务器状态网页面板
// 端口: 9047
// 功能: 在线玩家、世界属性、玩家行为日志
// =================================================================

#include "webpanel.h" // 你的头文件

// 核心模块头文件 (根据你实际使用的功能引入)
#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/event/player/PlayerLeftEvent.h"
#include "ll/api/event/player/PlayerChatEvent.h"
#include "ll/api/event/player/PlayerDieEvent.h"
#include "ll/api/mc/Player.hpp"
#include "ll/api/mc/Level.hpp"
#include "ll/api/service/ServiceManager.h"
#include "ll/api/http/HttpServer.h"
#include "ll/api/memory/MemoryOperators.h"

// 第三方库
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <ctime>
#include <filesystem>

// 启用内存操作符 (必须)
#define LL_MEMORY_OPERATORS
#include "ll/api/memory/MemoryOperators.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace webpanel {

const int WEB_PORT = 9047;
const std::string LOG_FILE = "./plugins/WebPanel/logs.json";
const size_t MAX_LOGS = 500;

// --- 工具函数 ---
std::string getCurrentTimestamp() {
    auto t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return std::string(buf);
}

// --- 类成员函数实现 ---
WebPanel& WebPanel::getInstance() {
    static WebPanel instance;
    return instance;
}

void WebPanel::initDirectory() const {
    fs::create_directories("./plugins/WebPanel");
}

std::string WebPanel::getLogFilePath() const {
    return LOG_FILE;
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

json WebPanel::getWorldData() const {
    auto level = ll::service::getLevel();
    if (!level) return {};
    return {
        {"time", level->getTime()},
        {"seed", level->getSeed()},
        {"weather", static_cast<int>(level->getWeather())},
        {"playerCount", level->getAllPlayers().size()}
    };
}

// --- 插件生命周期 ---
bool WebPanel::load() {
    initDirectory();
    loadLogsFromFile();
    getSelf().getLogger().info("WebPanel v1.0.0 已加载！");
    return true;
}

bool WebPanel::enable() {
    using namespace ll::event;
    auto& bus = EventBus::getInstance();

    // 玩家加入
    bus.emplaceListener<PlayerJoinEvent>([this](const PlayerJoinEvent& ev) {
        auto& player = ev.self();
        getSelf().getLogger().info("{} 加入了游戏", player.getRealName());
        appendLog({
            {"timestamp", getCurrentTimestamp()},
            {"player", player.getRealName()},
            {"action", "join"},
            {"ip", player.getIP()}
        });
    });

    // 玩家离开
    bus.emplaceListener<PlayerLeftEvent>([this](const PlayerLeftEvent& ev) {
        auto& player = ev.self();
        getSelf().getLogger().info("{} 离开了游戏", player.getRealName());
        appendLog({
            {"timestamp", getCurrentTimestamp()},
            {"player", player.getRealName()},
            {"action", "leave"}
        });
    });

    // 聊天
    bus.emplaceListener<PlayerChatEvent>([this](const PlayerChatEvent& ev) {
        auto& player = ev.self();
        appendLog({
            {"timestamp", getCurrentTimestamp()},
            {"player", player.getRealName()},
            {"action", "chat"},
            {"message", ev.message()}
        });
    });

    // 死亡
    bus.emplaceListener<PlayerDieEvent>([this](const PlayerDieEvent& ev) {
        auto& player = ev.self();
        std::string cause = "unknown";
        if (auto src = ev.source(); src) cause = src->getName();
        appendLog({
            {"timestamp", getCurrentTimestamp()},
            {"player", player.getRealName()},
            {"action", "death"},
            {"cause", cause}
        });
    });

    // HTTP 服务器
    auto& http = ll::http::HttpServer::getInstance();

    http.onGet("/", [this](const ll::http::HttpRequest&, ll::http::HttpResponse& res) {
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

    http.onGet("/api/players", [this](const ll::http::HttpRequest&, ll::http::HttpResponse& res) {
        res.setHeader("Content-Type", "application/json");
        res.body = getOnlinePlayersData().dump();
        res.status = 200;
    });

    http.onGet("/api/world", [this](const ll::http::HttpRequest&, ll::http::HttpResponse& res) {
        res.setHeader("Content-Type", "application/json");
        res.body = getWorldData().dump();
        res.status = 200;
    });

    http.onGet("/api/logs", [this](const ll::http::HttpRequest& req, ll::http::HttpResponse& res) {
        int limit = 100;
        if (req.hasParam("limit")) limit = std::stoi(req.getParam("limit"));
        auto logs = getLogs(limit);
        json arr = json::array();
        for (const auto& l : logs) arr.push_back(l);
        res.setHeader("Content-Type", "application/json");
        res.body = arr.dump();
        res.status = 200;
    });

    http.listen("0.0.0.0", WEB_PORT, [this]() {
        getSelf().getLogger().info("HTTP 服务器已启动，访问 http://0.0.0.0:{} 查看面板", WEB_PORT);
    });

    getSelf().getLogger().info("WebPanel v1.0.0 已启用！");
    return true;
}

bool WebPanel::disable() {
    ll::http::HttpServer::getInstance().stop();
    getSelf().getLogger().info("WebPanel 已禁用！");
    return true;
}

} // namespace webpanel

LL_REGISTER_MOD(webpanel::WebPanel, webpanel::WebPanel::getInstance());