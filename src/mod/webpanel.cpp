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
#include "ll/api/thread/ServerThreadExecutor.h"
#include "mc/entity/components/AttributesComponent.h"
#include "mc/world/attribute/AttributeInstance.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/deps/game_refs/WeakRef.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <ctime>
#include <filesystem>
#include <future>
#include <chrono>

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

            // --- 获取生命值：通过 ECS AttributesComponent，绕过 MCAPI ---
            float health = 0.0f;
            float maxHealth = 20.0f; // 默认值
            auto* attrComp = player.getEntityContext().tryGetComponent<AttributesComponent>();
            if (attrComp) {
                // 优先通过名称 "minecraft:health" 精确查找
                auto ref = attrComp->mAttributes.getMutableInstance(HashedString{"minecraft:health"});
                if (ref.mInstance) {
                    health = ref.mInstance->mCurrentValue;
                    maxHealth = ref.mInstance->mCurrentMaxValue;
                } else {
                    // 降级：遍历 mInstanceMap，取 mCurrentMaxValue 最大的属性（通常就是 HEALTH）
                    for (auto& [id, instance] : attrComp->mAttributes.mInstanceMap) {
                        if (instance.mCurrentMaxValue > maxHealth) {
                            health = instance.mCurrentValue;
                            maxHealth = instance.mCurrentMaxValue;
                        }
                    }
                }
            }

            // --- 获取维度 ID：直接访问 Dimension::mId，绕过 getDimensionId() ---
            int dimId = 0;
            // 尝试使用 getDimensionId()（可能失败），如果抛出异常则走内存路径
            try {
                dimId = static_cast<int>(player.getDimensionId());
            } catch (...) {
                // 降级：通过 mDimension 成员直接读取
                // mDimension 类型：TypedStorage<8,16,WeakRef<Dimension>>
                // WeakRef 有 lock() 方法返回 StackRefResult<Dimension>
                auto dimRef = player.mDimension->lock();
                if (dimRef) {
                    dimId = static_cast<int>(dimRef->mId);
                }
            }

            players.push_back({
                {"name", player.getRealName()},
                {"uuid", player.getUuid().asString()},
                {"health", health},
                {"maxHealth", maxHealth},
                {"pos", {{"x", pos.x}, {"y", pos.y}, {"z", pos.z}}},
                {"dimension", dimId}
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

    // 首页 - 文件读取，无需主线程调度
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

    // 玩家列表 API - 调度到主线程 + shared_ptr 防悬空
    mHttpServer->Get("/api/players", [this](const httplib::Request&, httplib::Response& res) {
        auto promise = std::make_shared<std::promise<std::string>>();
        auto future = promise->get_future();

        ll::thread::ServerThreadExecutor::getDefault().execute([this, promise]() {
            try {
                promise->set_value(getOnlinePlayersData().dump());
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });

        res.set_header("Content-Type", "application/json");
        auto status = future.wait_for(std::chrono::seconds(5));
        if (status == std::future_status::ready) {
            try {
                res.body = future.get();
                res.status = 200;
            } catch (const std::exception& e) {
                res.status = 500;
                res.body = R"({"error": ")" + std::string(e.what()) + R"("})";
            }
        } else {
            res.status = 503;
            res.body = R"({"error": "server shutting down or busy"})";
        }
    });

    // 世界信息 API - 调度到主线程 + shared_ptr 防悬空
    mHttpServer->Get("/api/world", [this](const httplib::Request&, httplib::Response& res) {
        auto promise = std::make_shared<std::promise<std::string>>();
        auto future = promise->get_future();

        ll::thread::ServerThreadExecutor::getDefault().execute([this, promise]() {
            try {
                promise->set_value(getWorldData().dump());
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });

        res.set_header("Content-Type", "application/json");
        auto status = future.wait_for(std::chrono::seconds(5));
        if (status == std::future_status::ready) {
            try {
                res.body = future.get();
                res.status = 200;
            } catch (const std::exception& e) {
                res.status = 500;
                res.body = R"({"error": ")" + std::string(e.what()) + R"("})";
            }
        } else {
            res.status = 503;
            res.body = R"({"error": "server shutting down or busy"})";
        }
    });

    // 日志 API - 线程安全，直接执行
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