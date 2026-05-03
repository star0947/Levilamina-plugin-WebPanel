#include "PlayerActionLog.h"
#include "Config.h"
#include "JsonHelper.h"
#include <chrono>
#include <fstream>
#include <algorithm>

PlayerActionLog::PlayerActionLog(const std::string& diskPath)
    : diskPath_(diskPath) {}

void PlayerActionLog::addAction(AggregatedBlockAction action) {
    if (currentAction_ &&
        currentAction_->blockType == action.blockType &&
        currentAction_->toolType == action.toolType &&
        currentAction_->action == action.action) {
        
        currentAction_->count++;
        currentAction_->lastTime = action.lastTime;
        currentAction_->lastPos = action.lastPos;
        
        if (currentAction_->count >= Config::MAX_COUNT_PER_LINE ||
            (action.lastTime - currentAction_->firstTime) > Config::MAX_MERGE_WINDOW_MS) {
            flushCurrent();
        }
    } else {
        flushCurrent();
        currentAction_ = action;
    }
}

void PlayerActionLog::flushCurrent() {
    if (!currentAction_) return;
    memoryHistory_.push_back(*currentAction_);
    currentAction_.reset();
    
    while (memoryHistory_.size() > Config::MAX_MEMORY_ENTRIES) {
        appendToDisk(memoryHistory_.front());
        memoryHistory_.pop_front();
    }
}

void PlayerActionLog::appendToDisk(const AggregatedBlockAction& entry) {
    std::lock_guard lock(diskMutex_);
    std::ofstream file(diskPath_, std::ios::app);
    if (file) {
        file << JsonHelper::serialize(entry) << '\n';
    }
}

std::vector<AggregatedBlockAction> PlayerActionLog::getHistory(int64_t before, int limit) const {
    std::vector<AggregatedBlockAction> result;
    {
        std::lock_guard lock(diskMutex_);
        std::ifstream file(diskPath_);
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            auto act = JsonHelper::deserialize(line);
            if (before > 0 && act.lastTime >= before) continue;
            result.push_back(act);
        }
    }
    
    for (const auto& act : memoryHistory_) {
        if (before > 0 && act.lastTime >= before) continue;
        result.push_back(act);
    }
    
    std::sort(result.begin(), result.end(),
        [](const AggregatedBlockAction& a, const AggregatedBlockAction& b) {
            return a.lastTime > b.lastTime;
        });
    
    if (result.size() > static_cast<size_t>(limit)) {
        result.resize(limit);
    }
    return result;
}

void PlayerActionLog::flushAllToDisk() {
    flushCurrent();
    while (!memoryHistory_.empty()) {
        appendToDisk(memoryHistory_.front());
        memoryHistory_.pop_front();
    }
}