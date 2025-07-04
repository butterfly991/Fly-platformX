#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace cloud {
namespace core {
namespace cache {

// Унифицированная конфигурация кэша
struct CacheConfig {
    // параметры кэша
    size_t initialSize = 256;
    size_t maxSize = 1024;
    size_t ttlSeconds = 3600;
    std::string evictionPolicy = "lru";
    // ... другие параметры ...
};

} // namespace cache
} // namespace core
} // namespace cloud 