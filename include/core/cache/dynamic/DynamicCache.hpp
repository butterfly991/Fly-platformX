#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <algorithm>
#include <list>
#include <spdlog/spdlog.h>
#include <initializer_list>

namespace cloud {
namespace core {
namespace cache {

/**
 * @brief Универсальный динамический кэш с поддержкой LRU, TTL, авторасширения и callback.
 * @details Используйте этот класс для всех задач кэширования в ядре и менеджерах.
 * @tparam Key Тип ключа (например, std::string)
 * @tparam Value Тип значения (например, std::vector<uint8_t>)
 */
template<typename Key, typename Value>
class DynamicCache {
public:
    using DataType = Value;
    using KeyType = Key;
    using EvictionCallback = std::function<void(const KeyType&, const DataType&)>;
    using Clock = std::chrono::steady_clock;
    struct Entry {
        DataType data;
        Clock::time_point lastAccess;
        size_t ttlSeconds; // 0 = бессрочно
    };

    DynamicCache(size_t initialSize, size_t defaultTTL = 0);
    ~DynamicCache();

    std::optional<Value> get(const Key& key);
    void put(const Key& key, const Value& value);
    void put(const Key& key, const Value& value, size_t ttlSeconds);
    void remove(const Key& key);
    void clear();
    size_t size() const;
    size_t allocatedSize() const;

    void resize(size_t newSize);
    void setEvictionCallback(EvictionCallback cb);
    void setAutoResize(bool enable, size_t minSize, size_t maxSize);
    void setCleanupInterval(size_t seconds);
    void batchPut(const std::unordered_map<KeyType, DataType>& data, size_t ttlSeconds = 0);

    /**
     * @brief Синхронизировать содержимое с другим DynamicCache (full copy).
     * @param other Кэш-источник
     */
    void syncWith(const DynamicCache& other);
    /**
     * @brief Мигрировать все данные в другой DynamicCache (full copy).
     * @param target Кэш-приёмник
     */
    void migrateTo(DynamicCache& target) const;
    // void setPreloadManager(std::shared_ptr<cloud::core::PreloadManager> pm); // временно убрано
    // void warmupFromPreload(); // временно убрано

private:
    void evictIfNeeded();
    void cleanupThreadFunc();
    void evictLRU();
    void autoResize();
    void removeExpired();

    size_t allocatedSize_;
    size_t defaultTTL_;
    std::unordered_map<KeyType, std::pair<typename std::list<KeyType>::iterator, Entry>> cache_;
    std::list<KeyType> lruList_;
    mutable std::shared_mutex mutex_;
    EvictionCallback evictionCallback_;
    std::thread cleanupThread_;
    std::atomic<bool> stopCleanup_;
    size_t cleanupIntervalSeconds_ = 10;
    bool autoResizeEnabled_ = false;
    size_t minSize_ = 16;
    size_t maxSize_ = 4096;
    // std::shared_ptr<cloud::core::PreloadManager> preloadManager_; // временно убрано
};

// Алиас для удобства использования динамического кэша по умолчанию
using DefaultDynamicCache = DynamicCache<std::string, std::vector<uint8_t>>;

} // namespace cache
} // namespace core
} // namespace cloud
