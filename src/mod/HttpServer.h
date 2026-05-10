#pragma once
#include "httplib.h"
#include <thread>
#include <memory>

class LogManager;
class DeathLogger;

class HttpServer {
public:
    HttpServer(int port, LogManager& logMgr, DeathLogger& deathLogger);
    ~HttpServer();
    void start();
    void stop();

private:
    httplib::Server server_;
    LogManager& logMgr_;
    DeathLogger& deathLogger_;
    std::unique_ptr<std::thread> thread_;
    bool running_ = false;
};