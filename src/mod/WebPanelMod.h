#pragma once
#include <memory>
#include "EventListeners.h"
#include "LogManager.h"
#include "HttpServer.h"

class WebPanelMod {
public:
    bool load();
    bool enable();
    bool disable();

private:
    std::unique_ptr<LogManager> logManager_;
    std::unique_ptr<HttpServer> httpServer_;
    EventListeners eventListeners_;   // 新增实例
};