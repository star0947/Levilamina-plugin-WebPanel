#pragma once
#include <memory>

class LogManager;
class HttpServer;

class WebPanelMod {
public:
    bool load();
    bool enable();
    bool disable();

private:
    std::unique_ptr<LogManager> logManager_;
    std::unique_ptr<HttpServer> httpServer_;
};