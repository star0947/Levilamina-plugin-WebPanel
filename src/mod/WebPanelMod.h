#pragma once
#include <memory>
#include <atomic>
#include "EventListeners.h"
#include "LogManager.h"
#include "HttpServer.h"

class WebPanelMod {
public:
    bool load();
    bool enable();
    bool disable();

    // 供钩子访问
    static LogManager* getLogManagerInstance();

private:
    std::unique_ptr<LogManager> logManager_;
    std::unique_ptr<HttpServer> httpServer_;
    EventListeners eventListeners_;

    static std::atomic<LogManager*> s_logManagerForHook;
};