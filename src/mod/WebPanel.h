#pragma once

#include <memory>
#include <deque>
#include <mutex>
#include <thread>
#include <nlohmann/json.hpp>
#include "httplib.h"

namespace ll {
    class Logger;
    namespace event { using ListenerPtr = std::shared_ptr<class ListenerBase>; }
    namespace mod { class NativeMod; }
}

namespace webpanel {

class WebPanel {
public:
    static WebPanel& getInstance();

    WebPanel() = default;
    ~WebPanel() = default;

    bool load();
    bool enable();
    bool disable();

    void appendLog(const nlohmann::json& entry);
    std::vector<nlohmann::json> getLogs(int limit = 100) const;

    nlohmann::json getOnlinePlayersData() const;
    nlohmann::json getWorldData() const;

    void setSelf(ll::mod::NativeMod* self) { mSelf = self; }
    ll::mod::NativeMod& getSelf() const { return *mSelf; }

private:
    ll::mod::NativeMod* mSelf = nullptr;

    std::deque<nlohmann::json> mLogs;
    mutable std::mutex mLogMutex;
    const size_t MAX_LOGS = 500;

    ll::event::ListenerPtr mPlayerJoinListener;
    ll::event::ListenerPtr mPlayerDisconnectListener;
    ll::event::ListenerPtr mPlayerChatListener;
    ll::event::ListenerPtr mPlayerDieListener;

    std::unique_ptr<httplib::Server> mHttpServer;
    std::thread mHttpThread;

    void loadLogsFromFile();
    void saveLogsToFile();
    std::string getLogFilePath() const;
    void initDirectory() const;
};

} // namespace webpanel