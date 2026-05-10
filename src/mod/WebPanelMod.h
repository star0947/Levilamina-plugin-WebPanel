#pragma once
#include <memory>
#include <atomic>
#include "EventListeners.h"
#include "LogManager.h"
#include "HttpServer.h"
#include "DeathLogger.h"

class WebPanelMod {
public:
    bool load();
    bool enable();
    bool disable();

private:
    std::unique_ptr<LogManager> logManager_;
    std::unique_ptr<DeathLogger> deathLogger_;       // 新增
    std::unique_ptr<HttpServer> httpServer_;
    EventListeners eventListeners_;
};