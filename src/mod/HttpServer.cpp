#include "HttpServer.h"
#include "ApiHandlers.h"

HttpServer::HttpServer(int port, LogManager& logMgr) : logMgr_(logMgr) {
    server_.Get("/api/players", [&](const httplib::Request& req, httplib::Response& res) {
        ApiHandlers::getPlayers(req, res, logMgr_);
    });
    
    server_.Get(R"(/api/player/([^/]+)/actions)", [&](const httplib::Request& req, httplib::Response& res) {
        ApiHandlers::getPlayerActions(req, res, logMgr_);
    });
    
    server_.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"WebPanel running\"}", "application/json");
    });
    
    // Optional: set listen port
    // Actually listen is called in start()
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