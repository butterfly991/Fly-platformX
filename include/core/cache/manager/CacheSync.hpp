#pragma once

#include "core/cache/CacheConfig.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>
#include <chrono>

namespace cloud {
namespace core {
namespace cache {

class CacheSync {
public:
    static CacheSync& getInstance();
    
    // Регистрация кэша для синхронизации
    void registerCache(const std::string& kernelId, std::shared_ptr<CacheManager> cache);
    void unregisterCache(const std::string& kernelId);
    
    // Синхронизация данных
    void syncData(const std::string& sourceKernelId, const std::string& targetKernelId);
    void syncAllCaches();
    
    // Миграция данных
    void migrateData(const std::string& sourceKernelId, const std::string& targetKernelId);
    
    // Получение статистики
    struct SyncStats {
        size_t syncCount;
        size_t migrationCount;
        std::chrono::steady_clock::time_point lastSync;
        double syncLatency;
    };
    SyncStats getStats() const;
    
private:
    CacheSync() = default;
    ~CacheSync() = default;
    CacheSync(const CacheSync&) = delete;
    CacheSync& operator=(const CacheSync&) = delete;
    
    std::unordered_map<std::string, std::shared_ptr<CacheManager>> caches_;
    mutable std::mutex mutex_;
    SyncStats stats_;
    
    // Вспомогательные методы
    bool validateSync(const std::string& sourceId, const std::string& targetId) const;
    void updateStats(size_t syncCount, size_t migrationCount, double latency);
};

} // namespace cache
} // namespace core
} // namespace cloud 