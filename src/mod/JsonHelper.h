#pragma once
#include <nlohmann/json.hpp>
#include "PlayerActionLog.h"
#include "mc/world/level/BlockPos.h"

namespace JsonHelper {
    inline nlohmann::json toJson(const AggregatedBlockAction& act) {
        return {
            {"action", act.action},
            {"block", act.blockType},
            {"tool", act.toolType},
            {"count", act.count},
            {"first_time", act.firstTime},
            {"last_time", act.lastTime},
            {"last_pos", {act.lastPos.x, act.lastPos.y, act.lastPos.z}}
        };
    }

    inline std::string serialize(const AggregatedBlockAction& act) {
        return toJson(act).dump();
    }

    inline AggregatedBlockAction deserialize(const std::string& line) {
        auto j = nlohmann::json::parse(line);
        return {
            .blockType = j["block"],
            .toolType = j["tool"],
            .action = j["action"],
            .count = j["count"],
            .firstTime = j["first_time"],
            .lastTime = j["last_time"],
            .lastPos = BlockPos(
                j["last_pos"][0].get<int>(),
                j["last_pos"][1].get<int>(),
                j["last_pos"][2].get<int>()
            )
        };
    }
}