#pragma once
#include <cstddef>
#include <chrono>
#include <nlohmann/json.hpp>

namespace cloud {
namespace core {
namespace cache {

struct CacheMetrics {
    size_t currentSize = 0;         // Текущий размер кэша (байт)
    size_t maxSize = 0;             // Максимальный размер кэша (байт)
    size_t entryCount = 0;          // Количество записей
    double hitRate = 0.0;           // Частота попаданий
    double evictionRate = 0.0;      // Частота вытеснений
    size_t evictionCount = 0;       // Количество вытеснений
    size_t requestCount = 0;        // Количество запросов
    std::chrono::steady_clock::time_point lastUpdate; // Время последнего обновления

    nlohmann::json toJson() const {
        return {
            {"currentSize", currentSize},
            {"maxSize", maxSize},
            {"entryCount", entryCount},
            {"hitRate", hitRate},
            {"evictionRate", evictionRate},
            {"evictionCount", evictionCount},
            {"requestCount", requestCount},
            {"lastUpdate", std::chrono::duration_cast<std::chrono::milliseconds>(lastUpdate.time_since_epoch()).count()}
        };
    }
};

} // namespace cache
} // namespace core
} // namespace cloud 