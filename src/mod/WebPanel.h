#pragma once

#include "ll/api/mod/NativeMod.h"
#include <memory>
#include <deque>
#include <mutex>
#include <nlohmann/json.hpp>

namespace webpanel {

class WebPanel {
public:
    static WebPanel& getInstance();

    WebPanel() : mSelf(*ll::mod::NativeMod::current()) {}
    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();

    // 日志存取
    void appendLog(const nlohmann::json& entry);
    std::vector<nlohmann::json> getLogs(int limit = 100) const;

    // 数据获取
    nlohmann::json getOnlinePlayersData() const;
    nlohmann::json getWorldData() const;

private:
    ll::mod::NativeMod& mSelf;

    std::deque<nlohmann::json> mLogs;
    mutable std::mutex mLogMutex;
    const size_t MAX_LOGS = 500;

    void loadLogsFromFile();
    void saveLogsToFile() const;
    std::string getLogFilePath() const;
    void initDirectory() const;
};

} // namespace webpanel