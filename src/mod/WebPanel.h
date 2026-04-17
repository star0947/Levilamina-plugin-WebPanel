#pragma once

#include "ll/api/mod/NativeMod.h"
#include <memory>
#include <deque>
#include <mutex>
#include <thread>
#include <nlohmann/json.hpp>
#include "httplib.h"

namespace ll::event { class ListenerBase; }

namespace webpanel {

class WebPanel : public ll::mod::NativeMod {
public:
    static WebPanel& getInstance();

    WebPanel() = default;
    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return ll::mod::NativeMod::current(); }

    bool load();
    bool enable();
    bool disable();

    void appendLog(const nlohmann::json& entry);
    std::vector<nlohmann::json> getLogs(int limit = 100) const;
    nlohmann::json getOnlinePlayersData() const;
    nlohmann::json getWorldData() const;

private:
    std::deque<nlohmann::json> mLogs;
    mutable std::mutex mLogMutex;
    const size_t MAX_LOGS = 500;

    std::shared_ptr<ll::event::ListenerBase> mPlayerJoinListener;
    std::shared_ptr<ll::event::ListenerBase> mPlayerDisconnectListener;
    std::shared_ptr<ll::event::ListenerBase> mPlayerChatListener;
    std::shared_ptr<ll::event::ListenerBase> mPlayerDieListener;

    std::unique_ptr<httplib::Server> mHttpServer;
    std::thread mHttpThread;

    void loadLogsFromFile();
    void saveLogsToFile();
    std::string getLogFilePath() const;
    void initDirectory() const;
};

} // namespace webpanel