#pragma once

#include <unordered_set>
#include <initializer_list>
#include <functional>
#include <memory>
#include <atomic>
#include <type_traits>
#include <utility>
#include <cstddef>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "core/thread/ThreadPool.hpp"

namespace cloud {
namespace core {
namespace cache {
namespace experimental {

/**
 * @brief Конфигурация предварительной загрузки
 * 
 * Содержит параметры для настройки поведения PreloadManager,
 * включая размеры очередей, окна предсказания и пороги.
 */
struct PreloadConfig {
    size_t maxQueueSize; ///< Максимальный размер очереди задач
    size_t maxBatchSize; ///< Максимальный размер пакета
    std::chrono::seconds predictionWindow; ///< Окно предсказания
    double predictionThreshold; ///< Порог предсказания
    
    /**
     * @brief Валидация конфигурации
     * 
     * @return true если конфигурация корректна
     */
    bool validate() const {
        return maxQueueSize > 0 && maxBatchSize > 0 && 
               predictionWindow.count() > 0 && predictionThreshold > 0.0;
    }
};

/**
 * @brief Задача предварительной загрузки
 * 
 * Представляет задачу для предварительной загрузки данных в кэш.
 */
struct PreloadTask {
    std::string key; ///< Ключ данных
    std::vector<uint8_t> data; ///< Данные для загрузки
    std::chrono::steady_clock::time_point timestamp; ///< Время создания
    double priority; ///< Приоритет задачи
};

/**
 * @brief Метрики предварительной загрузки
 * 
 * Содержит статистику работы PreloadManager для мониторинга
 * и оптимизации производительности.
 */
struct PreloadMetrics {
    size_t queueSize; ///< Размер очереди задач
    size_t activeTasks; ///< Количество активных задач
    double efficiency; ///< Эффективность предварительной загрузки
    double predictionAccuracy; ///< Точность предсказания
};

/**
 * @brief Менеджер предварительной загрузки
 * 
 * Экспериментальный компонент для предварительной загрузки данных в кэш
 * на основе предсказания будущих обращений. Использует машинное обучение
 * для оптимизации стратегий предзагрузки.
 * 
 * @note Потокобезопасен
 * @note Поддерживает асинхронную обработку задач
 * @note Интегрируется с DynamicCache для warm-up
 */
class PreloadManager {
public:
    /**
     * @brief Конструктор
     * 
     * @param config Конфигурация предварительной загрузки
     */
    explicit PreloadManager(const PreloadConfig& config);
    
    /**
     * @brief Деструктор
     * 
     * Освобождает ресурсы и останавливает обработку задач.
     */
    ~PreloadManager();
    
    /**
     * @brief Инициализация менеджера предварительной загрузки
     * 
     * Запускает внутренние компоненты, включая пул потоков
     * и обработчик задач предзагрузки.
     * 
     * @return true если инициализация успешна
     */
    bool initialize();
    
    /**
     * @brief Предварительная загрузка данных
     * 
     * Добавляет данные в очередь предзагрузки для последующего
     * размещения в кэше.
     * 
     * @param key Ключ данных
     * @param data Данные для предзагрузки
     * @return true если задача добавлена в очередь
     */
    bool preloadData(const std::string& key, const std::vector<uint8_t>& data);
    
    /**
     * @brief Добавить данные для предзагрузки
     * 
     * Альтернативный метод для добавления данных с автоматическим
     * определением приоритета.
     * 
     * @param key Ключ данных
     * @param data Данные для предзагрузки
     * @return true если данные добавлены
     */
    bool addData(const std::string& key, const std::vector<uint8_t>& data);
    
    /**
     * @brief Получение метрик
     * 
     * @return Текущие метрики предварительной загрузки
     */
    PreloadMetrics getMetrics() const;
    
    /**
     * @brief Обновление метрик
     * 
     * Пересчитывает метрики на основе текущего состояния
     * и истории работы.
     */
    void updateMetrics();
    
    /**
     * @brief Установка конфигурации
     * 
     * @param config Новая конфигурация
     */
    void setConfiguration(const PreloadConfig& config);
    
    /**
     * @brief Получение конфигурации
     * 
     * @return Текущая конфигурация
     */
    PreloadConfig getConfiguration() const;
    
    /**
     * @brief Остановка менеджера предварительной загрузки
     * 
     * Останавливает обработку задач и освобождает ресурсы.
     */
    void stop();

    /**
     * @brief Получить список всех ключей, доступных для предзагрузки
     * 
     * Возвращает все ключи, которые были добавлены в PreloadManager
     * и доступны для загрузки в кэш.
     * 
     * @return Вектор ключей
     */
    std::vector<std::string> getAllKeys() const;

    /**
     * @brief Получить данные по ключу (если доступны)
     * 
     * Извлекает данные из внутреннего хранилища PreloadManager.
     * 
     * @param key Ключ
     * @param data Буфер для результата
     * @return true если данные найдены
     */
    bool getDataForKey(const std::string& key, std::vector<uint8_t>& data) const;
    
    /**
     * @brief Получить данные по ключу (опциональный результат)
     * 
     * Альтернативный метод, возвращающий std::optional для более
     * удобной работы с результатом.
     * 
     * @param key Ключ
     * @return std::optional с данными или std::nullopt если не найдено
     */
    std::optional<std::vector<uint8_t>> getDataForKey(const std::string& key) const;

private:
    // Реализация PIMPL
    struct Impl;
    std::unique_ptr<Impl> pImpl;
    bool initialized;
    
    /**
     * @brief Запуск обработчика задач
     * 
     * Запускает фоновый поток для обработки задач предзагрузки.
     */
    void startTaskProcessor();
    
    /**
     * @brief Обработка задачи
     * 
     * @param task Задача для обработки
     */
    void processTask(const PreloadTask& task);
    
    /**
     * @brief Предсказание следующего доступа
     * 
     * Использует машинное обучение для предсказания вероятности
     * обращения к данным в ближайшем будущем.
     * 
     * @param key Ключ для предсказания
     * @return true если предсказание положительное
     */
    bool predictNextAccess(const std::string& key);
    
    /**
     * @brief Загрузка данных
     * 
     * @param key Ключ данных
     * @param data Буфер для результата
     * @return true если данные загружены
     */
    bool loadData(const std::string& key, std::vector<uint8_t>& data);
};

} // namespace experimental
} // namespace cache
} // namespace core
} // namespace cloud 

// Алиас для удобства использования
namespace cloud { namespace core {
using PreloadManager = cache::experimental::PreloadManager;
}} // namespace cloud::core 