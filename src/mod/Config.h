#pragma once
#include <string>

namespace Config {
    inline std::string DATA_DIR;
    constexpr int HTTP_PORT = 9047;
    constexpr int MAX_MEMORY_ENTRIES = 100;
    constexpr int MAX_COUNT_PER_LINE = 10000;
    constexpr int64_t MAX_MERGE_WINDOW_MS = 300000; // 5 minutes
}