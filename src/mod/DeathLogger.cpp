#include "DeathLogger.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

DeathLogger::DeathLogger(const std::string& dataDir) : baseDir_(dataDir) {
    std::filesystem::create_directories(baseDir_);
}

void DeathLogger::addRecord(const mce::UUID& uuid, const DeathRecord& record) {
    std::lock_guard lock(mutex_);
    std::string path = baseDir_ + "/" + uuid.asString() + "_deaths.log";
    std::ofstream file(path, std::ios::app);
    if (file) {
        nlohmann::json j;
        j["time"]   = record.timestamp;
        j["dim"]    = record.dimId;
        j["x"]      = record.x;
        j["y"]      = record.y;
        j["z"]      = record.z;
        j["cause"]  = record.causeName;
        j["attacker"] = record.attackerName;
        file << j.dump() << '\n';
    }
}

std::vector<DeathRecord> DeathLogger::getRecords(const mce::UUID& uuid, int64_t before, int limit) const {
    std::vector<DeathRecord> records;
    std::lock_guard lock(mutex_);
    std::string path = baseDir_ + "/" + uuid.asString() + "_deaths.log";
    if (!std::filesystem::exists(path)) return records;

    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        try {
            auto j = nlohmann::json::parse(line);
            DeathRecord r;
            r.timestamp = j["time"];
            r.dimId     = j["dim"];
            r.x         = j["x"];
            r.y         = j["y"];
            r.z         = j["z"];
            r.causeName = j["cause"];
            r.attackerName = j.value("attacker", "");
            if (before > 0 && r.timestamp >= before) continue;
            records.push_back(r);
        } catch (...) {}
    }

    std::sort(records.begin(), records.end(),
        [](const DeathRecord& a, const DeathRecord& b) { return a.timestamp > b.timestamp; });

    if (limit > 0 && records.size() > static_cast<size_t>(limit)) {
        records.resize(limit);
    }
    return records;
}