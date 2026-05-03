#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include "PlayerActionLog.h"
#include "mc/deps/core/mce/UUID.h"

class LogManager {
public:
    explicit LogManager(const std::string& dataDir);

    void onPlayerJoin(const mce::UUID& uuid);
    void onPlayerLeave(const mce::UUID& uuid);
    void pushAction(const mce::UUID& uuid, const AggregatedBlockAction& action);
    std::vector<AggregatedBlockAction> getHistory(const mce::UUID& uuid, int64_t before, int limit) const;
    void shutdown();

private:
    std::string baseDir_;
    std::unordered_map<mce::UUID, std::unique_ptr<PlayerActionLog>> logs_;
};