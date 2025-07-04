#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include <optional>

namespace cloud {
namespace core {

/**
 * @brief Базовый шаблонный интерфейс кэша для всех реализаций.
 * @tparam Key Тип ключа (например, std::string)
 * @tparam Value Тип значения (например, std::vector<uint8_t>)
 */
template<typename Key, typename Value>
class BaseCache {
public:
    virtual ~BaseCache() = default;
    /// Получить значение по ключу. Возвращает std::optional<Value>.
    virtual std::optional<Value> get(const Key& key) = 0;
    /// Сохранить значение по ключу.
    virtual void put(const Key& key, const Value& value) = 0;
    /// Удалить значение по ключу.
    virtual void remove(const Key& key) = 0;
    /// Очистить кэш полностью.
    virtual void clear() = 0;
    /// Получить количество элементов в кэше.
    virtual size_t size() const = 0;
};

} // namespace core
} // namespace cloud 