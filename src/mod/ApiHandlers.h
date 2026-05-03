#pragma once
#include "httplib.h"
#include "LogManager.h"

class ApiHandlers {
public:
    static void getPlayers(const httplib::Request& req, httplib::Response& res, LogManager& logMgr);
    static void getPlayerActions(const httplib::Request& req, httplib::Response& res, LogManager& logMgr);
};