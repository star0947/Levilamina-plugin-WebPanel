#pragma once
#include <string>
#include <vector>
#include <mutex>
#include "mc/platform/UUID.h"

struct DeathRecord {
    int64_t     timestamp;    // 毫秒时间戳
    int         dimId;        // 维度ID
    float       x, y, z;      // 死亡坐标
    std::string causeName;    // 伤害原因（如 "fall"）
    std::string attackerName; // 空字符串表示非实体伤害
};

class DeathLogger {
public:
    explicit DeathLogger(const std::string& dataDir);

    void addRecord(const mce::UUID& uuid, const DeathRecord& record);
    std::vector<DeathRecord> getRecords(const mce::UUID& uuid, int64_t before, int limit) const;

private:
    std::string baseDir_;
    mutable std::mutex mutex_;
};