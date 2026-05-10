#include "WebPanelMod.h"
#include "Config.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/io/Logger.h"
#include <filesystem>

std::atomic<LogManager*> g_logManagerForHook(nullptr);

static WebPanelMod& getMod() {
    static WebPanelMod instance;
    return instance;
}

static ll::mod::NativeMod& getSelf() {
    return *ll::mod::NativeMod::current();
}

LL_REGISTER_MOD(WebPanelMod, getMod());

bool WebPanelMod::load() {
    auto& self = getSelf();
    Config::DATA_DIR = (self.getDataDir() / "player_logs").string();
    self.getLogger().info("WebPanel data dir: {}", Config::DATA_DIR);
    std::filesystem::create_directories(Config::DATA_DIR);
    return true;
}

bool WebPanelMod::enable() {
    auto& self = getSelf();
    auto& logger = self.getLogger();

    // 创建死亡日志目录
    std::string deathDir = (self.getDataDir() / "player_deaths").string();
    std::filesystem::create_directories(deathDir);

    logManager_  = std::make_unique<LogManager>(Config::DATA_DIR);
    deathLogger_ = std::make_unique<DeathLogger>(deathDir);   // 新增

    g_logManagerForHook.store(logManager_.get());

    auto& bus = ll::event::EventBus::getInstance();
    eventListeners_.registerAll(bus, *logManager_, *deathLogger_);

    httpServer_ = std::make_unique<HttpServer>(Config::HTTP_PORT, *logManager_, *deathLogger_);
    httpServer_->start();
    logger.info("WebPanel HTTP server started on port {}", Config::HTTP_PORT);
    return true;
}

bool WebPanelMod::disable() {
    auto& self = getSelf();
    auto& logger = self.getLogger();

    if (httpServer_) {
        httpServer_->stop();
        logger.info("HTTP server stopped");
    }

    auto& bus = ll::event::EventBus::getInstance();
    eventListeners_.unregisterAll(bus);

    g_logManagerForHook.store(nullptr);

    if (logManager_) {
        logManager_->shutdown();
    }
    return true;
}