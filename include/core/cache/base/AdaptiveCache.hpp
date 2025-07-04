#pragma once
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace cloud {
namespace core {

/**
 * @brief Легковесный адаптивный кэш. Для большинства задач используйте DynamicCache.
 * @note Рекомендуется только для очень простых и малых сценариев кэширования.
 */
class AdaptiveCache {
public:
    AdaptiveCache(size_t maxSize);
    ~AdaptiveCache();

    // Получение данных
    bool get(const std::string& key, std::vector<uint8_t>& data);
    // Сохранение данных
    void put(const std::string& key, const std::vector<uint8_t>& data);
    // Адаптация размера кэша
    void adapt(size_t newMaxSize);
    // Очистка кэша
    void clear();

    // Получение текущего размера
    size_t size() const;
    // Получение максимального размера
    size_t maxSize() const;

private:
    size_t maxSize_;
    std::unordered_map<std::string, std::vector<uint8_t>> cache_;
    mutable std::mutex mutex_;
};

} // namespace core
} // namespace cloud
