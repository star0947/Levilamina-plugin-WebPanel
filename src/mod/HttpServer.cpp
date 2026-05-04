#include "HttpServer.h"
#include "ApiHandlers.h"
#include "Config.h"
#include <filesystem>

HttpServer::HttpServer(int port, LogManager& logMgr) : logMgr_(logMgr) {
    // 设置静态文件目录，映射到根路径 "/"
    // Config::DATA_DIR 是 ".../data/player_logs"，取其父目录 "data"，再拼接 "web"
    std::string dataDir = Config::DATA_DIR.substr(0, Config::DATA_DIR.find_last_of("/\\"));
    std::string webDir = dataDir + "/web";

    // 如果 web 目录不存在则自动创建
    if (!std::filesystem::exists(webDir)) {
        std::filesystem::create_directory(webDir);
    }

    // 注册根路径 API 路由（必须放在 set_mount_point 之前，优先匹配）
    server_.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"running\"}", "application/json");
    });

    // 将 web 目录映射为网站的根目录
    server_.set_mount_point("/", webDir.c_str());

    // API 路由：玩家列表
    server_.Get("/api/players", [&](const httplib::Request& req, httplib::Response& res) {
        ApiHandlers::getPlayers(req, res, logMgr_);
    });

    // API 路由：玩家方块操作日志
    server_.Get(R"(/api/player/([^/]+)/actions)", [&](const httplib::Request& req, httplib::Response& res) {
        ApiHandlers::getPlayerActions(req, res, logMgr_);
    });
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::start() {
    running_ = true;
    thread_ = std::make_unique<std::thread>([this]() {
        server_.listen("0.0.0.0", 9047);
    });
}

void HttpServer::stop() {
    if (running_) {
        server_.stop();
        if (thread_ && thread_->joinable()) {
            thread_->join();
        }
        running_ = false;
    }
}