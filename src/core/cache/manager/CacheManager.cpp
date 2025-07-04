#include "core/cache/manager/CacheManager.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#include <spdlog/spdlog.h>

namespace cloud {
namespace core {
namespace cache {

// Реализация PIMPL
struct CacheManager::Impl {
    CacheConfig config;             // Конфигурация кэша
    std::unordered_map<std::string, CacheEntry> entries; // Записи кэша
    std::mutex cacheMutex;          // Мьютекс для кэша
    std::chrono::steady_clock::time_point lastCleanup; // Время последней очистки
    std::chrono::steady_clock::time_point lastMetricsUpdate; // Время последнего обновления метрик
    size_t totalRequests = 0;
    size_t hitCount = 0;
    size_t evictionCount = 0;
    
    Impl(const CacheConfig& cfg)
        : config(cfg)
        , lastCleanup(std::chrono::steady_clock::now())
        , lastMetricsUpdate(std::chrono::steady_clock::now()) {
        // Инициализация логгера
        try {
            auto logger = spdlog::get("cachemanager");
            if (!logger) {
                logger = spdlog::rotating_logger_mt("cachemanager", "logs/cachemanager.log",
                                                   1024 * 1024 * 5, 3);
            }
            logger->set_level(spdlog::level::debug);
        } catch (const std::exception& e) {
            std::cerr << "Ошибка инициализации логгера: " << e.what() << std::endl;
        }
    }
};

// Конструктор
CacheManager::CacheManager(const CacheConfig& config)
    : pImpl(std::make_unique<Impl>(config))
    , initialized(false) {
}

// Деструктор
CacheManager::~CacheManager() = default;

// Инициализация менеджера кэша
bool CacheManager::initialize() {
    std::lock_guard<std::mutex> lock(pImpl->cacheMutex);
    if (initialized) return false;
    
    try {
        // Проверка конфигурации
        if (!pImpl->config.validate()) {
            throw std::runtime_error("Некорректная конфигурация кэша");
        }
        
        // Очистка кэша
        cleanupCache();
        
        initialized = true;
        spdlog::get("cachemanager")->info("CacheManager успешно инициализирован");
        return true;
    } catch (const std::exception& e) {
        spdlog::get("cachemanager")->error("Ошибка инициализации: {}", e.what());
        return false;
    }
}

// Получение данных из кэша
bool CacheManager::getData(const std::string& key, std::vector<uint8_t>& data) {
    if (!initialized) return false;
    
    std::lock_guard<std::mutex> lock(pImpl->cacheMutex);
    ++pImpl->totalRequests;
    try {
        // Поиск записи в кэше
        auto it = pImpl->entries.find(key);
        if (it == pImpl->entries.end()) {
            spdlog::get("cachemanager")->debug("Кэш-промах: {}", key);
            return false;
        }
        
        ++pImpl->hitCount;
        // Обновление времени доступа
        it->second.lastAccess = std::chrono::steady_clock::now();
        ++it->second.accessCount;
        
        // Копирование данных
        data = it->second.data;
        
        spdlog::get("cachemanager")->debug(
            "Кэш-попадание: key={}, size={}",
            key, data.size()
        );
        return true;
    } catch (const std::exception& e) {
        spdlog::get("cachemanager")->error("Ошибка получения данных: {}", e.what());
        return false;
    }
}

// Сохранение данных в кэш
bool CacheManager::putData(const std::string& key, const std::vector<uint8_t>& data) {
    if (!initialized) return false;
    
    std::lock_guard<std::mutex> lock(pImpl->cacheMutex);
    try {
        // Проверка размера данных
        if (data.size() > pImpl->config.maxSize) {
            throw std::runtime_error("Размер данных превышает максимально допустимый");
        }
        
        // Проверка количества записей
        if (pImpl->entries.size() >= pImpl->config.maxEntries) {
            cleanupCache();
        }
        
        // Создание записи
        CacheEntry entry{
            data,
            std::chrono::steady_clock::now(),
            0
        };
        
        // Сохранение записи
        pImpl->entries[key] = std::move(entry);
        
        spdlog::get("cachemanager")->debug(
            "Данные сохранены в кэш: key={}, size={}",
            key, data.size()
        );
        return true;
    } catch (const std::exception& e) {
        spdlog::get("cachemanager")->error("Ошибка сохранения данных: {}", e.what());
        return false;
    }
}

// Инвалидация данных в кэше
void CacheManager::invalidateData(const std::string& key) {
    std::lock_guard<std::mutex> lock(pImpl->cacheMutex);
    try {
        auto it = pImpl->entries.find(key);
        if (it != pImpl->entries.end()) {
            pImpl->entries.erase(it);
            spdlog::get("cachemanager")->debug("Данные инвалидированы: {}", key);
        }
    } catch (const std::exception& e) {
        spdlog::get("cachemanager")->error("Ошибка инвалидации данных: {}", e.what());
        throw;
    }
}

// Установка конфигурации
void CacheManager::setConfiguration(const CacheConfig& config) {
    std::lock_guard<std::mutex> lock(pImpl->cacheMutex);
    try {
        if (!config.validate()) {
            throw std::runtime_error("Некорректная конфигурация кэша");
        }
        
        pImpl->config = config;
        
        // Очистка кэша
        cleanupCache();
        
        spdlog::get("cachemanager")->info("Конфигурация кэша обновлена");
    } catch (const std::exception& e) {
        spdlog::get("cachemanager")->error("Ошибка обновления конфигурации: {}", e.what());
        throw;
    }
}

// Получение конфигурации
CacheConfig CacheManager::getConfiguration() const {
    std::lock_guard<std::mutex> lock(pImpl->cacheMutex);
    return pImpl->config;
}

// Получение размера кэша
size_t CacheManager::getCacheSize() const {
    std::lock_guard<std::mutex> lock(pImpl->cacheMutex);
    size_t totalSize = 0;
    for (const auto& entry : pImpl->entries) {
        totalSize += entry.second.data.size();
    }
    return totalSize;
}

// Получение количества записей
size_t CacheManager::getEntryCount() const {
    std::lock_guard<std::mutex> lock(pImpl->cacheMutex);
    return pImpl->entries.size();
}

// Получение метрик
CacheMetrics CacheManager::getMetrics() const {
    std::lock_guard<std::mutex> lock(pImpl->cacheMutex);
    CacheMetrics metrics;
    metrics.currentSize = getCacheSize();
    metrics.entryCount = pImpl->entries.size();
    metrics.hitRate = calculateHitRate();
    metrics.evictionRate = calculateEvictionRate();
    return metrics;
}

// Обновление метрик
void CacheManager::updateMetrics() {
    auto now = std::chrono::steady_clock::now();
    if (now - pImpl->lastMetricsUpdate > std::chrono::seconds(1)) {
        try {
            auto metrics = getMetrics();
            spdlog::get("cachemanager")->debug(
                "Метрики кэша: размер={}, записей={}, попаданий={}, вытеснений={}",
                metrics.currentSize, metrics.entryCount,
                metrics.hitRate, metrics.evictionRate
            );
            pImpl->lastMetricsUpdate = now;
        } catch (const std::exception& e) {
            spdlog::get("cachemanager")->error("Ошибка обновления метрик: {}", e.what());
        }
    }
}

// Очистка кэша
void CacheManager::cleanupCache() {
    try {
        auto now = std::chrono::steady_clock::now();
        size_t removedEntries = 0;
        
        // Удаление устаревших записей
        for (auto it = pImpl->entries.begin(); it != pImpl->entries.end();) {
            if (now - it->second.lastAccess > pImpl->config.entryLifetime) {
                it = pImpl->entries.erase(it);
                ++removedEntries;
                ++pImpl->evictionCount;
            } else {
                ++it;
            }
        }
        
        // Проверка размера кэша
        while (getCacheSize() > pImpl->config.maxSize) {
            auto oldestEntry = std::min_element(
                pImpl->entries.begin(),
                pImpl->entries.end(),
                [](const auto& a, const auto& b) {
                    return a.second.lastAccess < b.second.lastAccess;
                }
            );
            
            if (oldestEntry != pImpl->entries.end()) {
                pImpl->entries.erase(oldestEntry);
                ++removedEntries;
                ++pImpl->evictionCount;
            }
        }
        
        pImpl->lastCleanup = now;
        spdlog::get("cachemanager")->debug(
            "Очистка кэша: удалено {} записей",
            removedEntries
        );
    } catch (const std::exception& e) {
        spdlog::get("cachemanager")->error("Ошибка очистки кэша: {}", e.what());
    }
}

// Расчет частоты попаданий
double CacheManager::calculateHitRate() const {
    if (pImpl->totalRequests == 0) return 0.0;
    return static_cast<double>(pImpl->hitCount) / pImpl->totalRequests;
}

// Расчет частоты вытеснения
double CacheManager::calculateEvictionRate() const {
    if (pImpl->totalRequests == 0) return 0.0;
    return static_cast<double>(pImpl->evictionCount) / pImpl->totalRequests;
}

std::unordered_map<std::string, std::vector<uint8_t>> CacheManager::exportAll() const {
    std::lock_guard<std::mutex> lock(pImpl->cacheMutex);
    std::unordered_map<std::string, std::vector<uint8_t>> all;
    for (const auto& [key, entry] : pImpl->entries) {
        all[key] = entry.data;
    }
    return all;
}

} // namespace cache
} // namespace core
} // namespace cloud
