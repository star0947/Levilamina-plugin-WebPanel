#include "webpanel.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/player/PlayerChatEvent.h"
#include "ll/api/event/player/PlayerDieEvent.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/LevelData.h"
#include "ll/api/service/Bedrock.h"
#include "mc/world/actor/ActorDamageSource.h"
#include "mc/platform/UUID.h"
#include "magic_enum/magic_enum_all.hpp"
#include "ll/api/mod/RegisterHelper.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <ctime>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace webpanel {

const int WEB_PORT = 9047;
const size_t MAX_LOGS = 500;

// --- 工具函数 ---
std::string getCurrentTimestamp() {
    auto t = std::time(nullptr);
    struct tm tm_buf;
    gmtime_s(&tm_buf, &t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return std::string(buf);
}

// --- 类成员函数实现 ---
WebPanel& WebPanel::getInstance() {
    static WebPanel instance;
    return instance;
}

void WebPanel::initDirectory() const {
    auto dataDir = getSelf().getDataDir();
    fs::create_directories(dataDir);
}

std::string WebPanel::getLogFilePath() const {
    return (getSelf().getDataDir() / "logs.json").string();
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

void WebPanel::saveLogsToFile() {
    std::ofstream ofs(getLogFilePath());
    if (!ofs.is_open()) return;
    json j = json::array();
    for (const auto& entry : mLogs) j.push_back(entry);
    ofs << j.dump(2);
}

void WebPanel::appendLog(const json& entry) {
    std::lock_guard lock(mLogMutex);
    mLogs.push_back(entry);
    if (mLogs.size() > MAX_LOGS) mLogs.pop_front();
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
    ll::service::getLevel().transform([&](Level& level) {
        level.forEachPlayer([&](Player& player) {
            auto pos = player.getPosition();
            players.push_back({
                {"name", player.getRealName()},
                {"uuid", player.getUuid().asString()},
                {"health", player.getHealth()},
                {"maxHealth", player.getMaxHealth()},
                {"pos", {{"x", pos.x}, {"y", pos.y}, {"z", pos.z}}},
                {"dimension", static_cast<int>(player.getDimensionId())}
            });
            return true;
        });
        return true;
    });
    return players;
}

json WebPanel::getWorldData() const {
    json result;
    ll::service::getLevel().transform([&](Level& level) {
        auto& ld = level.getLevelData();
        bool isRaining = static_cast<float>(ld.mRainLevel) > 0.0f;
        bool isThunder = static_cast<float>(ld.mLightningLevel) > 0.0f;

        result = {
            {"time", level.getTime()},
            {"seed", level.getSeed()},
            {"isRaining", isRaining},
            {"isThunder", isThunder},
            {"playerCount", 0}
        };
        int count = 0;
        level.forEachPlayer([&](Player&) { count++; return true; });
        result["playerCount"] = count;
        return true;
    });
    return result;
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

    mPlayerJoinListener = bus.emplaceListener<PlayerJoinEvent>([this](PlayerJoinEvent& ev) {
        auto& player = ev.self();
        getSelf().getLogger().info("{} 加入了游戏", player.getRealName());
        appendLog({
            {"timestamp", getCurrentTimestamp()},
            {"player", player.getRealName()},
            {"action", "join"},
            {"ip", player.getIPAndPort()}
        });
    });

    mPlayerDisconnectListener = bus.emplaceListener<PlayerDisconnectEvent>([this](PlayerDisconnectEvent& ev) {
        auto& player = ev.self();
        getSelf().getLogger().info("{} 离开了游戏", player.getRealName());
        appendLog({
            {"timestamp", getCurrentTimestamp()},
            {"player", player.getRealName()},
            {"action", "leave"}
        });
    });

    mPlayerChatListener = bus.emplaceListener<PlayerChatEvent>([this](PlayerChatEvent& ev) {
        auto& player = ev.self();
        appendLog({
            {"timestamp", getCurrentTimestamp()},
            {"player", player.getRealName()},
            {"action", "chat"},
            {"message", ev.message()}
        });
    });

    mPlayerDieListener = bus.emplaceListener<PlayerDieEvent>([this](PlayerDieEvent& ev) {
        auto& player = ev.self();
        std::string cause = std::string(magic_enum::enum_name(ev.source().mCause));
        appendLog({
            {"timestamp", getCurrentTimestamp()},
            {"player", player.getRealName()},
            {"action", "death"},
            {"cause", cause}
        });
    });

    // HTTP 服务器
    mHttpServer = std::make_unique<httplib::Server>();

    mHttpServer->Get("/", [this](const httplib::Request&, httplib::Response& res) {
        res.set_header("Content-Type", "text/html; charset=utf-8");
        std::ifstream ifs((getSelf().getDataDir() / "index.html").string());
        if (ifs) {
            std::stringstream buf;
            buf << ifs.rdbuf();
            res.body = buf.str();
        } else {
            res.body = "<h1>Error: index.html not found</h1>";
        }
        res.status = 200;
    });

    mHttpServer->Get("/api/players", [this](const httplib::Request&, httplib::Response& res) {
        res.set_header("Content-Type", "application/json");
        res.body = getOnlinePlayersData().dump();
        res.status = 200;
    });

    mHttpServer->Get("/api/world", [this](const httplib::Request&, httplib::Response& res) {
        res.set_header("Content-Type", "application/json");
        res.body = getWorldData().dump();
        res.status = 200;
    });

    mHttpServer->Get("/api/logs", [this](const httplib::Request& req, httplib::Response& res) {
        int limit = 100;
        if (req.has_param("limit")) limit = std::stoi(req.get_param_value("limit"));
        auto logs = getLogs(limit);
        json arr = json::array();
        for (const auto& l : logs) arr.push_back(l);
        res.set_header("Content-Type", "application/json");
        res.body = arr.dump();
        res.status = 200;
    });

    mHttpThread = std::thread([this]() {
        if (!mHttpServer->listen("0.0.0.0", WEB_PORT)) {
            getSelf().getLogger().error("HTTP 服务器启动失败！");
        } else {
            getSelf().getLogger().info("HTTP 服务器已启动，访问 http://0.0.0.0:{} 查看面板", WEB_PORT);
        }
    });

    getSelf().getLogger().info("WebPanel v1.0.0 已启用！");
    return true;
}

bool WebPanel::disable() {
    auto& bus = ll::event::EventBus::getInstance();
    bus.removeListener(mPlayerJoinListener);
    bus.removeListener(mPlayerDisconnectListener);
    bus.removeListener(mPlayerChatListener);
    bus.removeListener(mPlayerDieListener);

    if (mHttpServer) {
        mHttpServer->stop();
    }
    if (mHttpThread.joinable()) {
        mHttpThread.join();
    }

    getSelf().getLogger().info("WebPanel 已禁用！");
    return true;
}

} // namespace webpanel

// 手动定义插件入口
extern "C" {
    LL_SHARED_EXPORT bool ll_mod_load(ll::mod::NativeMod& self) {
        auto& inst = webpanel::WebPanel::getInstance();
        inst.setSelf(&self);
        ::ll::mod::bindToMod(inst, self);
        return inst.load();
    }
}