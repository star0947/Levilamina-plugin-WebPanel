#include "HttpServer.h"
#include "ApiHandlers.h"
#include "Config.h"
#include <filesystem>

HttpServer::HttpServer(int port, LogManager& logMgr) : logMgr_(logMgr) {
    // 设定静态文件根目录：data/web 路径
    std::string dataDir = Config::DATA_DIR.substr(0, Config::DATA_DIR.find_last_of("/\\"));
    std::string webDir = dataDir + "/web";
    if (!std::filesystem::exists(webDir)) {
        std::filesystem::create_directory(webDir);
    }

    // 使用 pre_routing_handler 拦截根路径的 JSON 状态请求
    server_.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        // 如果是根路径且 Accept 头包含 "application/json"，返回状态 JSON
        if (req.path == "/") {
            std::string accept = req.get_header_value("Accept");
            if (accept.find("application/json") != std::string::npos) {
                res.set_content("{\"status\":\"running\"}", "application/json");
                return httplib::Server::HandlerResponse::Handled;
            }
        }
        // 其他请求继续正常路由（包括静态文件服务）
        return httplib::Server::HandlerResponse::NextHandler;
    });

    // 静态文件挂载点
    server_.set_mount_point("/", webDir.c_str());

    // API 路由
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