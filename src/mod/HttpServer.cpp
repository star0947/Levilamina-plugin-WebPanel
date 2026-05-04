#include "HttpServer.h"
#include "ApiHandlers.h"
#include "Config.h"
#include <filesystem>

HttpServer::HttpServer(int port, LogManager& logMgr) : logMgr_(logMgr) {
    // 1. 先注册精确路由，确保根路径返回 JSON
    server_.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"running\"}", "application/json");
    });

    // 2. 设置静态文件目录
    std::string dataDir = Config::DATA_DIR.substr(0, Config::DATA_DIR.find_last_of("/\\"));
    std::string webDir = dataDir + "/web";
    if (!std::filesystem::exists(webDir)) {
        std::filesystem::create_directory(webDir);
    }
    server_.set_mount_point("/", webDir.c_str());

    // 3. 其他 API 路由
    server_.Get("/api/players", [&](const httplib::Request& req, httplib::Response& res) {
        ApiHandlers::getPlayers(req, res, logMgr_);
    });
    server_.Get(R"(/api/player/([^/]+)/actions)", [&](const httplib::Request& req, httplib::Response& res) {
        ApiHandlers::getPlayerActions(req, res, logMgr_);
    });
}

HttpServer::~HttpServer() { stop(); }

void HttpServer::start() {
    running_ = true;
    thread_ = std::make_unique<std::thread>([this]() {
        server_.listen("0.0.0.0", 9047);
    });
}

void HttpServer::stop() {
    if (running_) {
        server_.stop();
        if (thread_ && thread_->joinable()) thread_->join();
        running_ = false;
    }
}