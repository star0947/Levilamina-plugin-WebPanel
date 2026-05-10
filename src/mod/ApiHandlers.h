#pragma once
#include "httplib.h"
#include "LogManager.h"
#include "DeathLogger.h"

class ApiHandlers {
public:
    static void getPlayers(const httplib::Request& req, httplib::Response& res, LogManager& logMgr);
    static void getPlayerActions(const httplib::Request& req, httplib::Response& res, LogManager& logMgr);
    static void getPlayerStatus(const httplib::Request& req, httplib::Response& res);
    static void getPlayerDeaths(const httplib::Request& req, httplib::Response& res, DeathLogger& deathLogger);
};