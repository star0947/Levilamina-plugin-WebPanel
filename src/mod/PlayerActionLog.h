#pragma once
#include <deque>
#include <optional>
#include <string>
#include <vector>
#include <mutex>
#include "mc/world/level/BlockPos.h"

struct AggregatedBlockAction {
    std::string blockType;
    std::string toolType;
    std::string action;   // "break", "place", "interact"
    int count = 0;
    int64_t firstTime = 0;
    int64_t lastTime = 0;
    BlockPos lastPos;
};

class PlayerActionLog {
public:
    explicit PlayerActionLog(const std::string& diskPath);

    void addAction(AggregatedBlockAction action);
    void flushAllToDisk();
    std::vector<AggregatedBlockAction> getHistory(int64_t before, int limit) const;

private:
    void flushCurrent();
    void appendToDisk(const AggregatedBlockAction& entry);

    std::deque<AggregatedBlockAction> memoryHistory_;
    std::optional<AggregatedBlockAction> currentAction_;
    std::string diskPath_;
    mutable std::mutex diskMutex_;
};