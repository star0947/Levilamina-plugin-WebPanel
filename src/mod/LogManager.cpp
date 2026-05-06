// src/mod/LogManager.cpp
#include "LogManager.h"
#include "Config.h"
#include "JsonHelper.h"
#include <filesystem>
#include <fstream>

LogManager::LogManager(const std::string& dataDir) : baseDir_(dataDir) {
    std::filesystem::create_directories(baseDir_);
}

void LogManager::onPlayerJoin(const mce::UUID& uuid) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = baseDir_ + "/" + uuid.asString() + ".log";
    logs_[uuid] = std::make_unique<PlayerActionLog>(path);
}

void LogManager::onPlayerLeave(const mce::UUID& uuid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = logs_.find(uuid);
    if (it != logs_.end()) {
        it->second->flushAllToDisk();
        logs_.erase(it);
    }
}

void LogManager::pushAction(const mce::UUID& uuid, const AggregatedBlockAction& action) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = logs_.find(uuid);
    if (it != logs_.end()) {
        it->second->addAction(action);
    }
}

std::vector<AggregatedBlockAction> LogManager::getHistory(
    const mce::UUID& uuid, int64_t before, int limit) const {
    
    // 1. 如果玩家在线，使用内存中的 PlayerActionLog
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = logs_.find(uuid);
        if (it != logs_.end()) {
            // 获取内存中的历史（包含当前聚合中的数据）
            auto history = it->second->getHistory(before, limit);
            // 如果内存中有数据，不需要再从磁盘加载
            if (!history.empty() || limit <= 0) {
                return history;
            }
            // 否则，从磁盘加载更多历史
        }
    }

    // 2. 玩家离线或内存中无数据，从磁盘文件读取
    std::string path = baseDir_ + "/" + uuid.asString() + ".log";
    if (!std::filesystem::exists(path)) {
        return {};
    }

    std::vector<AggregatedBlockAction> all;
    {
        std::ifstream file(path);
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            try {
                auto act = JsonHelper::deserialize(line);
                if (before > 0 && act.lastTime >= before) continue;
                all.push_back(act);
            } catch (...) {
                // 忽略损坏的行
            }
        }
    }

    // 3. 按 lastTime 降序排序，最新的在前
    std::sort(all.begin(), all.end(),
        [](const AggregatedBlockAction& a, const AggregatedBlockAction& b) {
            return a.lastTime > b.lastTime;
        });
    
    if (limit > 0 && all.size() > static_cast<size_t>(limit)) {
        all.resize(limit);
    }
    return all;
}

void LogManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [uuid, log] : logs_) {
        log->flushAllToDisk();
    }
    logs_.clear();
}