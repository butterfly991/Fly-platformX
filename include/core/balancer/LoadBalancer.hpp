#pragma once
#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include "core/kernel/base/CoreKernel.hpp"
#include <chrono>
#include "core/balancer/TaskTypes.hpp"
#include <cstddef>

using cloud::core::kernel::IKernel;

namespace cloud {
namespace core {
namespace balancer {

/**
 * @brief Стратегии балансировки
 * 
 * Определяет различные алгоритмы балансировки нагрузки между ядрами.
 */
enum class BalancingStrategy {
    ResourceAware,      ///< Resource-Aware балансировка
    WorkloadSpecific,   ///< Workload-Specific балансировка
    HybridAdaptive,     ///< Гибридная адаптивная стратегия
    PriorityAdaptive,   ///< Приоритетная адаптивная (старая)
    LeastLoaded,        ///< Наименее загруженное ядро
    RoundRobin          ///< Round-robin
};

/**
 * @brief LoadBalancer — гибридная Resource-Aware + Workload-Specific балансировка
 * 
 * Продвинутый балансировщик нагрузки, который комбинирует Resource-Aware и Workload-Specific
 * подходы для оптимального распределения задач между ядрами. Поддерживает адаптивное
 * переключение стратегий на основе состояния системы.
 * 
 * @note Потокобезопасен
 * @note Поддерживает детальное логирование и метрики
 */
class LoadBalancer {
public:
    /**
     * @brief Конструктор по умолчанию
     * 
     * Инициализирует балансировщик с гибридной адаптивной стратегией.
     */
    LoadBalancer();
    
    /**
     * @brief Деструктор
     * 
     * Освобождает ресурсы балансировщика.
     */
    ~LoadBalancer();

    /**
     * @brief Гибридная Resource-Aware + Workload-Specific балансировка
     * 
     * Распределяет задачи между ядрами, используя комбинированный подход:
     * - Resource-Aware: учитывает доступность ресурсов (CPU, память, сеть, энергия)
     * - Workload-Specific: учитывает эффективность ядра для конкретного типа задачи
     * - Адаптивное переключение: автоматически переключается между стратегиями
     * 
     * @param kernels Вектор доступных ядер
     * @param tasks Вектор задач для распределения
     * @param metrics Метрики ядер (должен соответствовать размеру kernels)
     * 
     * @note Задачи обрабатываются по приоритету (высокий приоритет первым)
     * @note Метрики обновляются в реальном времени
     */
    void balance(const std::vector<std::shared_ptr<cloud::core::kernel::IKernel>>& kernels,
                 const std::vector<TaskDescriptor>& tasks,
                 const std::vector<KernelMetrics>& metrics);

    /**
     * @brief Установить веса для Resource-Aware метрик
     * 
     * Позволяет настроить важность различных ресурсов при принятии решений.
     * Сумма весов должна быть равна 1.0.
     * 
     * @param cpuWeight Вес CPU (0.0-1.0)
     * @param memoryWeight Вес памяти (0.0-1.0)
     * @param networkWeight Вес сети (0.0-1.0)
     * @param energyWeight Вес энергии (0.0-1.0)
     * 
     * @note Веса автоматически нормализуются
     */
    void setResourceWeights(double cpuWeight, double memoryWeight, 
                           double networkWeight, double energyWeight);
    
    /**
     * @brief Установить пороги для адаптивного переключения стратегий
     * 
     * Определяет условия для автоматического переключения между Resource-Aware
     * и Workload-Specific стратегиями.
     * 
     * @param resourceThreshold Порог ресурсов для переключения на Resource-Aware
     * @param workloadThreshold Порог эффективности для переключения на Workload-Specific
     */
    void setAdaptiveThresholds(double resourceThreshold, double workloadThreshold);

    // Старые методы для обратной совместимости
    /**
     * @brief Балансировка между ядрами (устаревший метод)
     * 
     * @deprecated Используйте balance(kernels, tasks, metrics)
     */
    void balance(const std::vector<std::shared_ptr<cloud::core::kernel::IKernel>>& kernels);
    
    /**
     * @brief Балансировка задач между очередями (устаревший метод)
     * 
     * @deprecated Используйте balance(kernels, tasks, metrics)
     */
    void balanceTasks(std::vector<std::vector<uint8_t>>& taskQueues);

    /**
     * @brief Установить стратегию балансировки по строке
     * 
     * @param strategy Название стратегии ("resource_aware", "workload_specific", "hybrid_adaptive", etc.)
     */
    void setStrategy(const std::string& strategy);
    
    /**
     * @brief Получить текущую стратегию балансировки
     * 
     * @return Название текущей стратегии
     */
    std::string getStrategy() const;
    
    /**
     * @brief Установить стратегию балансировки по enum
     * 
     * @param s Стратегия балансировки
     */
    void setStrategy(BalancingStrategy s);
    
    /**
     * @brief Получить текущую стратегию балансировки как enum
     * 
     * @return Текущая стратегия балансировки
     */
    BalancingStrategy getStrategyEnum() const;

private:
    // Resource-Aware методы
    /**
     * @brief Выбор ядра по Resource-Aware стратегии
     * 
     * Выбирает ядро на основе доступности ресурсов и предполагаемого использования.
     * 
     * @param metrics Метрики всех ядер
     * @param task Задача для распределения
     * @return Индекс выбранного ядра
     */
    size_t selectByResourceAware(const std::vector<KernelMetrics>& metrics, 
                                const TaskDescriptor& task);
    
    // Workload-Specific методы
    /**
     * @brief Выбор ядра по Workload-Specific стратегии
     * 
     * Выбирает ядро на основе эффективности для конкретного типа задачи.
     * 
     * @param metrics Метрики всех ядер
     * @param task Задача для распределения
     * @return Индекс выбранного ядра
     */
    size_t selectByWorkloadSpecific(const std::vector<KernelMetrics>& metrics,
                                   const TaskDescriptor& task);
    
    // Гибридные методы
    /**
     * @brief Выбор ядра по гибридной адаптивной стратегии
     * 
     * Комбинирует Resource-Aware и Workload-Specific подходы с адаптивным переключением.
     * 
     * @param metrics Метрики всех ядер
     * @param task Задача для распределения
     * @return Индекс выбранного ядра
     */
    size_t selectByHybridAdaptive(const std::vector<KernelMetrics>& metrics,
                                 const TaskDescriptor& task);
    
    // Вспомогательные методы
    /**
     * @brief Вычислить Resource-Aware score для ядра
     * 
     * @param metrics Метрики ядра
     * @param task Задача
     * @return Score (меньше = лучше)
     */
    double calculateResourceScore(const KernelMetrics& metrics, 
                                 const TaskDescriptor& task);
    
    /**
     * @brief Вычислить Workload-Specific score для ядра
     * 
     * @param metrics Метрики ядра
     * @param task Задача
     * @return Score (меньше = лучше)
     */
    double calculateWorkloadScore(const KernelMetrics& metrics,
                                 const TaskDescriptor& task);
    
    /**
     * @brief Определить необходимость переключения стратегии
     * 
     * @param metrics Метрики всех ядер
     * @return true если нужно переключить стратегию
     */
    bool shouldSwitchStrategy(const std::vector<KernelMetrics>& metrics);
    
    std::string strategy; ///< Текущая стратегия (строковое представление)
    BalancingStrategy strategyEnum = BalancingStrategy::HybridAdaptive; ///< Текущая стратегия (enum)
    mutable std::mutex mutex_; ///< Мьютекс для синхронизации
    size_t rrIdx = 0; ///< Индекс для round-robin
    
    // Resource-Aware веса
    double cpuWeight_ = 0.3; ///< Вес CPU
    double memoryWeight_ = 0.25; ///< Вес памяти
    double networkWeight_ = 0.25; ///< Вес сети
    double energyWeight_ = 0.2; ///< Вес энергии
    
    // Адаптивные пороги
    double resourceThreshold_ = 0.8; ///< Порог ресурсов
    double workloadThreshold_ = 0.7; ///< Порог эффективности
    
    // Статистика для адаптации
    size_t resourceAwareDecisions_ = 0; ///< Количество Resource-Aware решений
    size_t workloadSpecificDecisions_ = 0; ///< Количество Workload-Specific решений
    size_t totalDecisions_ = 0; ///< Общее количество решений
};

} // namespace balancer
} // namespace core
} // namespace cloud
