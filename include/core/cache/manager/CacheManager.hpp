#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <queue>
#include <chrono>
#include "core/cache/metrics/CacheConfig.hpp"
#include "core/cache/base/BaseCache.hpp"
#include "core/cache/metrics/CacheMetrics.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"

namespace cloud {
namespace core {
namespace cache {

/**
 * @brief [DEPRECATED] Класс CacheManager устарел. Используйте DynamicCache вместо него.
 * @deprecated Используйте core::DynamicCache для всех новых реализаций кэша.
 */
class CacheManager {
public:
    // Конструктор
    explicit CacheManager(const CacheConfig& config);
    
    // Деструктор
    ~CacheManager();
    
    // Инициализация менеджера кэша
    bool initialize();
    
    // Получение данных из кэша
    bool getData(const std::string& key, std::vector<uint8_t>& data);
    
    // Сохранение данных в кэш
    bool putData(const std::string& key, const std::vector<uint8_t>& data);
    
    // Инвалидация данных в кэше
    void invalidateData(const std::string& key);
    
    // Установка конфигурации
    void setConfiguration(const CacheConfig& config);
    
    // Получение конфигурации
    CacheConfig getConfiguration() const;
    
    // Получение размера кэша
    size_t getCacheSize() const;
    
    // Получение количества записей
    size_t getEntryCount() const;
    
    // Получение метрик
    CacheMetrics getMetrics() const;
    
    // Обновление метрик
    void updateMetrics();
    
    // Очистка кэша
    void cleanupCache();

    // Экспорт всех данных кэша (для миграции)
    std::unordered_map<std::string, std::vector<uint8_t>> exportAll() const;

private:
    // Реализация PIMPL
    struct Impl;
    std::unique_ptr<Impl> pImpl;
    bool initialized;
    
    mutable std::shared_mutex cacheMutex;
};

} // namespace cache
} // namespace core
} // namespace cloud
