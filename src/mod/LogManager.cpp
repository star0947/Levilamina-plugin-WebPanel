#include "LogManager.h"
#include <filesystem>
#include "Config.h"

LogManager::LogManager(const std::string& dataDir) : baseDir_(dataDir) {
    std::filesystem::create_directories(baseDir_);
}

void LogManager::onPlayerJoin(const mce::UUID& uuid) {
    std::string path = baseDir_ + "/" + uuid.asString() + ".log";
    logs_[uuid] = std::make_unique<PlayerActionLog>(path);
}

void LogManager::onPlayerLeave(const mce::UUID& uuid) {
    auto it = logs_.find(uuid);
    if (it != logs_.end()) {
        it->second->flushAllToDisk();
        logs_.erase(it);
    }
}

void LogManager::pushAction(const mce::UUID& uuid, const AggregatedBlockAction& action) {
    auto it = logs_.find(uuid);
    if (it != logs_.end()) {
        it->second->addAction(action);
    }
}

std::vector<AggregatedBlockAction> LogManager::getHistory(
    const mce::UUID& uuid, int64_t before, int limit) const {
    auto it = logs_.find(uuid);
    if (it != logs_.end()) {
        return it->second->getHistory(before, limit);
    }
    std::string path = baseDir_ + "/" + uuid.asString() + ".log";
    if (std::filesystem::exists(path)) {
        PlayerActionLog tempLog(path);
        return tempLog.getHistory(before, limit);
    }
    return {};
}

void LogManager::shutdown() {
    for (auto& [uuid, log] : logs_) {
        log->flushAllToDisk();
    }
    logs_.clear();
}