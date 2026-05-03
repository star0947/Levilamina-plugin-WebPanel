#pragma once
#include "httplib.h"
#include <thread>
#include <memory>

class LogManager;

class HttpServer {
public:
    HttpServer(int port, LogManager& logMgr);
    ~HttpServer();
    void start();
    void stop();

private:
    httplib::Server server_;
    LogManager& logMgr_;
    std::unique_ptr<std::thread> thread_;
    bool running_ = false;
};