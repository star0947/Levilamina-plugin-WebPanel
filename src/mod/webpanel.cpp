#include <ll/api/LLAPI.h>
#include <ll/api/LoggerAPI.h>
#include <ll/api/mc/Player.hpp>
#include <ll/api/mc/Level.hpp>
#include <ll/api/mc/ServerPlayer.hpp>
#include <ll/api/EventAPI.h>
#include <ll/api/ScheduleAPI.h>
#include <ll/api/HttpServerAPI.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <ctime>
#include <vector>
#include <deque>
#include <mutex>

using json = nlohmann::json;

// 插件信息
const std::string PLUGIN_NAME = "WebPanel";
const std::string PLUGIN_VERSION = "1.0.0";
const int WEB_PORT = 9047;
const std::string LOG_FILE = "./plugins/WebPanel/logs.json";

// 日志缓存（最多保留 500 条）
std::deque<json> g_logs;
std::mutex g_logMutex;
const size_t MAX_LOGS = 500;

// 日志文件路径
std::string getLogFilePath() {
    return "./plugins/WebPanel/logs.json";
}

// 初始化目录
void initDirectory() {
    std::filesystem::create_directories("./plugins/WebPanel");
}

// 加载已有日志
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
                if (g_logs.size() > MAX_LOGS) {
                    g_logs.pop_front();
                }
            }
        }
    } catch (...) {
        // 忽略解析错误
    }
}

// 保存日志到文件
void saveLogsToFile() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ofstream ofs(getLogFilePath());
    if (!ofs.is_open()) return;
    json j = json::array();
    for (const auto& log : g_logs) {
        j.push_back(log);
    }
    ofs << j.dump(2);
}

// 添加一条日志
void appendLog(const json& entry) {
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        g_logs.push_back(entry);
        if (g_logs.size() > MAX_LOGS) {
            g_logs.pop_front();
        }
    }
    saveLogsToFile();
}

// 获取当前时间戳（ISO 8601 UTC）
std::string getCurrentTimestamp() {
    time_t now = time(nullptr);
    struct tm tm_gmt;
    gmtime_r(&now, &tm_gmt);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_gmt);
    return std::string(buf);
}

// 事件监听
void registerEvents() {
    using namespace Event;
    // 玩家加入
    PlayerJoinEvent::subscribe([](const PlayerJoinEvent& ev) {
        Player* player = ev.mPlayer;
        if (!player) return true;
        std::string name = player->getRealName();
        std::string ip = player->getIP();
        Logger(PLUGIN_NAME).info("{} 加入了游戏", name);
        json log = {
            {"timestamp", getCurrentTimestamp()},
            {"player", name},
            {"action", "join"},
            {"ip", ip}
        };
        appendLog(log);
        return true;
    });

    // 玩家离开
    PlayerLeftEvent::subscribe([](const PlayerLeftEvent& ev) {
        Player* player = ev.mPlayer;
        if (!player) return true;
        std::string name = player->getRealName();
        Logger(PLUGIN_NAME).info("{} 离开了游戏", name);
        json log = {
            {"timestamp", getCurrentTimestamp()},
            {"player", name},
            {"action", "leave"}
        };
        appendLog(log);
        return true;
    });

    // 聊天
    PlayerChatEvent::subscribe([](const PlayerChatEvent& ev) {
        Player* player = ev.mPlayer;
        if (!player) return true;
        std::string name = player->getRealName();
        std::string msg = ev.mMessage;
        json log = {
            {"timestamp", getCurrentTimestamp()},
            {"player", name},
            {"action", "chat"},
            {"message", msg}
        };
        appendLog(log);
        return true;
    });

    // 死亡
    PlayerDieEvent::subscribe([](const PlayerDieEvent& ev) {
        Player* player = ev.mPlayer;
        if (!player) return true;
        std::string name = player->getRealName();
        std::string cause = "unknown";
        auto src = ev.mSource;
        if (src) {
            cause = src->getName();
        }
        json log = {
            {"timestamp", getCurrentTimestamp()},
            {"player", name},
            {"action", "death"},
            {"cause", cause}
        };
        appendLog(log);
        return true;
    });
}

// HTTP 处理器
void setupHttpServer() {
    using namespace HttpServer;
    // 静态首页
    Server::onGet("/", [](const Request& req, Response& res) {
        res.setHeader("Content-Type", "text/html; charset=utf-8");
        std::ifstream ifs("./plugins/WebPanel/index.html");
        if (ifs.is_open()) {
            std::stringstream buffer;
            buffer << ifs.rdbuf();
            res.body = buffer.str();
        } else {
            res.body = "<h1>Error: index.html not found</h1>";
        }
        res.status = 200;
    });

    Server::onGet("/api/players", [](const Request& req, Response& res) {
        res.setHeader("Content-Type", "application/json");
        json players = json::array();
        for (auto player : Level::getAllPlayers()) {
            auto pos = player->getPosition();
            players.push_back({
                {"name", player->getRealName()},
                {"uuid", player->getUuid()},
                {"gameMode", (int)player->getGameMode()},
                {"health", player->getHealth()},
                {"maxHealth", player->getMaxHealth()},
                {"pos", {
                    {"x", pos.x},
                    {"y", pos.y},
                    {"z", pos.z}
                }},
                {"dimension", (int)pos.dim}
            });
        }
        res.body = players.dump();
        res.status = 200;
    });

    Server::onGet("/api/world", [](const Request& req, Response& res) {
        res.setHeader("Content-Type", "application/json");
        Level* level = Level::getCurrentLevel();
        json world = {
            {"time", level->getTime()},
            {"seed", level->getSeed()},
            {"weather", (int)level->getWeather()},
            {"playerCount", Level::getAllPlayers().size()}
        };
        res.body = world.dump();
        res.status = 200;
    });

    Server::onGet("/api/logs", [](const Request& req, Response& res) {
        res.setHeader("Content-Type", "application/json");
        int limit = 100;
        if (req.has_param("limit")) {
            limit = std::stoi(req.get_param_value("limit"));
        }
        json logs = json::array();
        {
            std::lock_guard<std::mutex> lock(g_logMutex);
            int start = std::max(0, (int)g_logs.size() - limit);
            for (size_t i = start; i < g_logs.size(); ++i) {
                logs.push_back(g_logs[i]);
            }
        }
        res.body = logs.dump();
        res.status = 200;
    });

    // 启动服务器
    if (!Server::start("0.0.0.0", WEB_PORT)) {
        Logger(PLUGIN_NAME).error("HTTP 服务器启动失败！端口: {}", WEB_PORT);
    } else {
        Logger(PLUGIN_NAME).info("HTTP 服务器已启动 -> http://0.0.0.0:{}", WEB_PORT);
    }
}

// 插件入口
void PluginInit() {
    initDirectory();
    loadLogsFromFile();
    registerEvents();
    setupHttpServer();
    Logger(PLUGIN_NAME).info("{} v{} 已加载", PLUGIN_NAME, PLUGIN_VERSION);
}

// 插件卸载
void PluginUnload() {
    HttpServer::Server::stop();
    Logger(PLUGIN_NAME).info("{} 已卸载", PLUGIN_NAME);
}

// LeviLamina 要求导出这两个函数
extern "C" {
    __attribute__((visibility("default"))) void onEnable() {
        PluginInit();
    }
    __attribute__((visibility("default"))) void onDisable() {
        PluginUnload();
    }
}