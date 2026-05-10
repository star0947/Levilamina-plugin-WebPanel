#include "HttpServer.h"
#include "ApiHandlers.h"
#include "Config.h"
#include <filesystem>

HttpServer::HttpServer(int port, LogManager& logMgr, DeathLogger& deathLogger)
    : logMgr_(logMgr), deathLogger_(deathLogger) {

    // 静态文件
    std::string dataDir = Config::DATA_DIR.substr(0, Config::DATA_DIR.find_last_of("/\\"));
    std::string webDir = dataDir + "/web";
    if (!std::filesystem::exists(webDir)) {
        std::filesystem::create_directory(webDir);
    }

    // 预路由：为 JSON 请求返回状态
    server_.set_pre_routing_handler(
        [](const httplib::Request& req, httplib::Response& res) -> httplib::Server::HandlerResponse {
            if (req.path == "/") {
                std::string accept = req.get_header_value("Accept");
                if (accept.find("application/json") != std::string::npos) {
                    res.set_content("{\"status\":\"running\"}", "application/json");
                    return httplib::Server::HandlerResponse::Handled;
                }
            }
            return httplib::Server::HandlerResponse::Unhandled;
        }
    );

    server_.set_mount_point("/", webDir.c_str());

    // 原有 API
    server_.Get("/api/players", [&](const httplib::Request& req, httplib::Response& res) {
        ApiHandlers::getPlayers(req, res, logMgr_);
    });
    server_.Get(R"(/api/player/([^/]+)/actions)", [&](const httplib::Request& req, httplib::Response& res) {
        ApiHandlers::getPlayerActions(req, res, logMgr_);
    });

    // 新增 API：玩家状态
    server_.Get(R"(/api/player/([^/]+)/status)", [&](const httplib::Request& req, httplib::Response& res) {
        ApiHandlers::getPlayerStatus(req, res);
    });

    // 新增 API：死亡记录
    server_.Get(R"(/api/player/([^/]+)/deaths)", [&](const httplib::Request& req, httplib::Response& res) {
        ApiHandlers::getPlayerDeaths(req, res, deathLogger_);
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