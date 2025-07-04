#pragma once

// Оптимизации компилятора
#if defined(__GNUC__) || defined(__clang__)
    #define CLOUD_ALWAYS_INLINE __attribute__((always_inline)) inline
    #define CLOUD_HOT __attribute__((hot))
    #define CLOUD_COLD __attribute__((cold))
    #define CLOUD_ALIGN(x) __attribute__((aligned(x)))
    #define CLOUD_PACKED __attribute__((packed))
#else
    #define CLOUD_ALWAYS_INLINE inline
    #define CLOUD_HOT
    #define CLOUD_COLD
    #define CLOUD_ALIGN(x)
    #define CLOUD_PACKED
#endif

// Макросы для условной компиляции
#define CLOUD_ENABLE_LOGGING 1
#define CLOUD_ENABLE_METRICS 1
#define CLOUD_ENABLE_PERFORMANCE_OPTIMIZATIONS 1

// Версия API
#define CLOUD_API_VERSION_MAJOR 1
#define CLOUD_API_VERSION_MINOR 0
#define CLOUD_API_VERSION_PATCH 0

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <type_traits>
#include <optional>
#include <variant>
#include <chrono>
#include <array>
#include <bitset>
#include <deque>
#include <shared_mutex>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

// Платформо-зависимые определения
#if defined(__APPLE__) && defined(__arm64__)
    #define PLATFORM_APPLE_ARM
    #include <mach/mach.h>
    #include <mach/processor_info.h>
    #include <mach/mach_host.h>
#elif defined(__linux__) && defined(__x86_64__)
    #define PLATFORM_LINUX_X64
    #include <sys/sysinfo.h>
    #include <sys/statvfs.h>
#endif

#include "core/kernel/base/CoreKernel.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/cache/dynamic/PlatformOptimizer.hpp"
#include "core/balancer/TaskTypes.hpp"
#include "core/balancer/LoadBalancer.hpp"

namespace cloud {
namespace core {
namespace kernel {

// Вложенные пространства имен для лучшей организации
namespace detail {
    class PerformanceMonitor;
    class ResourceManager;
    class AdaptiveController;
}

namespace config {
    struct AdaptiveConfig {
        double learningRate;
        double explorationRate;
        size_t historySize;
        std::chrono::milliseconds adaptationInterval;
        bool enableAutoTuning;
        
        // Валидация конфигурации
        bool validate() const {
            return learningRate > 0.0 && learningRate <= 1.0 &&
                   explorationRate >= 0.0 && explorationRate <= 1.0 &&
                   historySize > 0 &&
                   adaptationInterval.count() > 0;
        }
    };
    
    struct ResourceConfig {
        size_t minThreads;
        size_t maxThreads;
        size_t cacheSize;
        size_t memoryLimit;
        double cpuLimit;
        
        // Валидация конфигурации
        bool validate() const {
            return minThreads > 0 && maxThreads >= minThreads &&
                   cacheSize > 0 && memoryLimit > 0 &&
                   cpuLimit > 0.0 && cpuLimit <= 1.0;
        }
    };
}

namespace metrics {
    struct AdaptiveMetrics {
        double loadFactor;
        double efficiencyScore;
        double powerEfficiency;
        double thermalEfficiency;
        double resourceUtilization;
        std::chrono::steady_clock::time_point lastAdaptation;
        
        // Сериализация в JSON
        nlohmann::json toJson() const {
            return {
                {"loadFactor", loadFactor},
                {"efficiencyScore", efficiencyScore},
                {"powerEfficiency", powerEfficiency},
                {"thermalEfficiency", thermalEfficiency},
                {"resourceUtilization", resourceUtilization},
                {"lastAdaptation", std::chrono::duration_cast<std::chrono::milliseconds>(
                    lastAdaptation.time_since_epoch()).count()}
            };
        }
    };
}

// Конфигурация ядра
struct SmartKernelConfig {
    size_t maxThreads;           // Максимальное количество потоков
    size_t maxMemory;            // Максимальный объем памяти
    std::chrono::seconds metricsInterval; // Интервал обновления метрик
    double adaptationThreshold;  // Порог адаптации
    
    bool validate() const {
        return maxThreads > 0 && maxMemory > 0 && 
               metricsInterval.count() > 0 && adaptationThreshold > 0.0;
    }
};

// Метрики ядра
struct SmartKernelMetrics {
    double threadUtilization;    // Утилизация потоков
    double memoryUtilization;    // Утилизация памяти
    double cacheEfficiency;      // Эффективность кэша
    double preloadEfficiency;    // Эффективность предварительной загрузки
    double recoveryEfficiency;   // Эффективность восстановления
    double overallEfficiency;    // Общая эффективность
};

/**
 * @brief SmartKernel — интеллектуальное адаптивное ядро с поддержкой метрик, адаптации и управления ресурсами.
 * @details Реализует расширенный интерфейс IKernel, поддерживает адаптивные контроллеры, мониторинг, recovery и динамический кэш.
 */
class SmartKernel : public CoreKernel {
public:
    /** @brief Конструктор с конфигурацией. */
    explicit SmartKernel(const SmartKernelConfig& config = SmartKernelConfig{});
    /** @brief Деструктор. */
    ~SmartKernel() override;
    SmartKernel(const SmartKernel&) = delete;
    SmartKernel& operator=(const SmartKernel&) = delete;
    SmartKernel(SmartKernel&&) noexcept;
    SmartKernel& operator=(SmartKernel&&) noexcept;
    /** @brief Инициализация ядра. */
    bool initialize() override;
    /** @brief Завершение работы ядра. */
    void shutdown() override;
    /** @brief Получение метрик ядра. */
    PerformanceMetrics getMetrics() const override;
    /** @brief Обновление метрик ядра. */
    void updateMetrics() override;
    /** @brief Установка конфигурации ядра. */
    void setConfiguration(const SmartKernelConfig& config);
    /** @brief Получение конфигурации ядра. */
    SmartKernelConfig getConfiguration() const;
private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
    /** @brief Монитор производительности. */
    std::shared_ptr<detail::PerformanceMonitor> performanceMonitor;
    /** @brief Менеджер ресурсов. */
    std::shared_ptr<detail::ResourceManager> resourceManager;
    /** @brief Адаптивный контроллер. */
    std::shared_ptr<detail::AdaptiveController> adaptiveController;
    /** @brief Пул потоков. */
    std::shared_ptr<core::thread::ThreadPool> threadPool;
    /** @brief Менеджер восстановления. */
    std::unique_ptr<core::recovery::RecoveryManager> recoveryManager;
    /** @brief Динамический кэш. */
    std::unique_ptr<core::DefaultDynamicCache> dynamicCache;
    /** @brief Оптимизатор платформы. */
    std::unique_ptr<core::cache::PlatformOptimizer> platformOptimizer;
    /** @brief Мьютекс для потокобезопасности ядра. */
    mutable std::shared_mutex kernelMutex;
    /** @brief Флаг инициализации. */
    std::atomic<bool> initialized;
    /** @brief Флаг оптимизации. */
    std::atomic<bool> isOptimizing;
    /** @brief Инициализация логгера. */
    void initializeLogger();
    /** @brief Инициализация компонентов ядра. */
    void initializeComponents();
    /** @brief Завершение работы компонентов ядра. */
    void shutdownComponents();
    /** @brief Обработка ошибок. */
    void handleError(const std::string& error);
    void adaptThreadPool(const metrics::AdaptiveMetrics& metrics);
    void adaptCacheSize(const metrics::AdaptiveMetrics& metrics);
    void adaptRecovery();
};

} // namespace kernel
} // namespace core
} // namespace cloud
