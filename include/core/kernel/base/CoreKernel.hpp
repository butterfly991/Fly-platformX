#pragma once

// Platform detection
#if defined(__APPLE__) && defined(__arm64__)
    #define CLOUD_PLATFORM_APPLE_ARM
    #include <mach/mach.h>
    #include <mach/thread_act.h>
    #include <mach/thread_policy.h>
    #include <mach/processor_info.h>
    #include <mach/mach_time.h>
    #include <mach/vm_statistics.h>
    #include <sys/sysctl.h>
    #include <arm_neon.h>
#elif defined(__linux__) && defined(__x86_64__)
    #define CLOUD_PLATFORM_LINUX_X64
    #include <x86intrin.h>
    #include <cpuid.h>
    #include <sys/sysinfo.h>
    #include <sys/statvfs.h>
    #include <linux/perf_event.h>
    #include <linux/hw_breakpoint.h>
    #include <sys/ioctl.h>
    #include <unistd.h>
    #include <fcntl.h>
#else
    #error "Неподдерживаемая платформа. Поддерживаются только Apple Silicon (M1-M4) и Linux x86-64"
#endif

// Compiler optimizations
#if defined(__GNUC__) || defined(__clang__)
    #define CLOUD_ALWAYS_INLINE __attribute__((always_inline)) inline
    #define CLOUD_HOT __attribute__((hot))
    #define CLOUD_COLD __attribute__((cold))
    #define CLOUD_ALIGN(x) __attribute__((aligned(x)))
    #define CLOUD_PACKED __attribute__((packed))
    #define CLOUD_LIKELY(x) __builtin_expect(!!(x), 1)
    #define CLOUD_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define CLOUD_RESTRICT __restrict__
#else
    #define CLOUD_ALWAYS_INLINE inline
    #define CLOUD_HOT
    #define CLOUD_COLD
    #define CLOUD_ALIGN(x)
    #define CLOUD_PACKED
    #define CLOUD_LIKELY(x) (x)
    #define CLOUD_UNLIKELY(x) (x)
    #define CLOUD_RESTRICT
#endif

// Feature detection
#define CLOUD_ENABLE_LOGGING 1
#define CLOUD_ENABLE_METRICS 1
#define CLOUD_ENABLE_PERFORMANCE_OPTIMIZATIONS 1
#define CLOUD_ENABLE_HARDWARE_ACCELERATION 1
#define CLOUD_ENABLE_ADAPTIVE_SCALING 1
#define CLOUD_ENABLE_POWER_MANAGEMENT 1
#define CLOUD_ENABLE_THERMAL_MANAGEMENT 1

// Constants
#ifdef CLOUD_PLATFORM_APPLE_ARM
    constexpr size_t DEFAULT_CACHE_LINE_SIZE = 128;  // Apple Silicon использует 128-байтовые кэш-линии
    constexpr size_t MAX_KERNEL_THREADS = 16;        // Максимальное количество потоков для M1-M4
    constexpr size_t DEFAULT_THREAD_STACK_SIZE = 8 * 1024 * 1024; // 8MB стек для потоков
#elif defined(CLOUD_PLATFORM_LINUX_X64)
    constexpr size_t DEFAULT_CACHE_LINE_SIZE = 64;   // x86-64 использует 64-байтовые кэш-линии
    constexpr size_t MAX_KERNEL_THREADS = 32;        // Максимальное количество потоков для x86-64
    constexpr size_t DEFAULT_THREAD_STACK_SIZE = 2 * 1024 * 1024; // 2MB стек для потоков
#endif
#define MAX_TASK_PRIORITY 10
#define MIN_TASK_PRIORITY 0
#define DEFAULT_TASK_PRIORITY 5

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <queue>
#include <bitset>
#include <any>
#include <optional>
#include <variant>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include "core/cache/manager/CacheManager.hpp"
#include "core/cache/experimental/PreloadManager.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/cache/base/BaseCache.hpp"
#include "core/balancer/LoadBalancer.hpp"
#include "core/balancer/TaskTypes.hpp"
#include "core/cache/dynamic/PlatformOptimizer.hpp"
#include "core/balancer/TaskOrchestrator.hpp"

namespace cloud {
namespace core {
namespace kernel {

// Forward declarations
class CoreKernel;

namespace detail {
    class PerformanceMonitor;
    class ResourceManager;
    class AdaptiveController;
    class ThermalManager;
    class PowerManager;
    class HardwareAccelerator;
}

namespace platform {
    #ifdef CLOUD_PLATFORM_APPLE_ARM
        struct AppleARMConfig {
            bool enableNeon;              // Включить оптимизации NEON
            bool enableAMX;               // Включить Apple Matrix Extensions
            bool enablePowerManagement;   // Включить управление питанием
            bool enableThermalManagement; // Включить управление температурой
            size_t performanceCores;      // Количество производительных ядер
            size_t efficiencyCores;       // Количество энергоэффективных ядер
            double maxPowerLimit;         // Максимальный лимит мощности
            double maxTemperature;        // Максимальная температура
            
            bool validate() const {
                return performanceCores > 0 && efficiencyCores > 0 &&
                       maxPowerLimit > 0.0 && maxTemperature > 0.0;
            }
        };
    #elif defined(CLOUD_PLATFORM_LINUX_X64)
        struct LinuxX64Config {
            bool enableAVX2;              // Включить оптимизации AVX2
            bool enableAVX512;            // Включить оптимизации AVX-512
            bool enablePowerManagement;   // Включить управление питанием
            bool enableThermalManagement; // Включить управление температурой
            size_t physicalCores;         // Количество физических ядер
            size_t logicalCores;          // Количество логических ядер
            double maxPowerLimit;         // Максимальный лимит мощности
            double maxTemperature;        // Максимальная температура
            
            bool validate() const {
                return physicalCores > 0 && logicalCores > 0 &&
                       maxPowerLimit > 0.0 && maxTemperature > 0.0;
            }
        };
    #endif
}

namespace metrics {
    struct PerformanceMetrics {
        double cpu_usage;
        double memory_usage;
        double power_consumption;
        double temperature;
        uint64_t instructions_per_second;
        std::chrono::steady_clock::time_point timestamp;
        double efficiencyScore = 0.0;
        
        // Platform-specific metrics
        #ifdef CLOUD_PLATFORM_APPLE_ARM
            double performance_core_usage = 0.0;
            double efficiency_core_usage = 0.0;
            double gpu_usage = 0.0;
            double neural_engine_usage = 0.0;
        #elif defined(CLOUD_PLATFORM_LINUX_X64)
            double physical_core_usage = 0.0;
            double logical_core_usage = 0.0;
            double gpu_usage = 0.0;
            double avx_usage = 0.0;
        #endif
        
        nlohmann::json toJson() const {
            auto json = nlohmann::json{
                {"cpu_usage", cpu_usage},
                {"memory_usage", memory_usage},
                {"power_consumption", power_consumption},
                {"temperature", temperature},
                {"instructions_per_second", instructions_per_second},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    timestamp.time_since_epoch()).count()},
                {"efficiencyScore", efficiencyScore}
            };
            
            #ifdef CLOUD_PLATFORM_APPLE_ARM
                json["performance_core_usage"] = performance_core_usage;
                json["efficiency_core_usage"] = efficiency_core_usage;
                json["gpu_usage"] = gpu_usage;
                json["neural_engine_usage"] = neural_engine_usage;
            #elif defined(CLOUD_PLATFORM_LINUX_X64)
                json["physical_core_usage"] = physical_core_usage;
                json["logical_core_usage"] = logical_core_usage;
                json["gpu_usage"] = gpu_usage;
                json["avx_usage"] = avx_usage;
            #endif
            
            return json;
        }
    };
}

namespace config {
    struct ResourceLimits {
        size_t maxThreads;
        size_t maxMemory;
        double maxCpuUsage;
        double maxPowerConsumption;
        double maxTemperature;
        
        bool validate() const {
            return maxThreads > 0 && maxMemory > 0 &&
                   maxCpuUsage > 0.0 && maxCpuUsage <= 1.0 &&
                   maxPowerConsumption > 0.0 &&
                   maxTemperature > 0.0;
        }
    };
    
    struct OptimizationConfig {
        bool enableAutoTuning;
        bool enableAdaptiveScaling;
        bool enablePowerManagement;
        bool enableThermalManagement;
        bool enableHardwareAcceleration;
        double minPerformanceThreshold;
        double maxPerformanceThreshold;
        double learningRate = 0.1;
        double explorationRate = 0.1;
        size_t historySize = 10;
        
        bool validate() const {
            return minPerformanceThreshold >= 0.0 && minPerformanceThreshold <= 1.0 &&
                   maxPerformanceThreshold >= 0.0 && maxPerformanceThreshold <= 1.0 &&
                   minPerformanceThreshold <= maxPerformanceThreshold;
        }
    };
}

enum class KernelType : uint8_t {
    PARENT = 0,
    MICRO = 1,
    SMART = 2,
    COMPUTATIONAL = 3,
    ARCHITECTURAL = 4,
    ORCHESTRATION = 5,
    CRYPTO = 6
};

enum class TaskPriority : uint8_t {
    LOW = 0,
    NORMAL = 5,
    HIGH = 10
};

enum class SecurityLevel : uint8_t {
    LOW = 0,
    MEDIUM = 1,
    HIGH = 2
};

enum class OptimizationLevel : uint8_t {
    NONE = 0,
    BASIC = 1,
    AGGRESSIVE = 2
};

enum class CacheReplacementPolicy : uint8_t {
    LRU = 0,
    LFU = 1,
    FIFO = 2,
    RANDOM = 3
};

using EventCallback = std::function<void(const std::string&, const std::any&)>;

struct CLOUD_ALIGN(DEFAULT_CACHE_LINE_SIZE) PerformanceMetrics {
    double cpu_usage;
    double memory_usage;
    double power_consumption;
    double temperature;
    uint64_t instructions_per_second;
    std::chrono::steady_clock::time_point timestamp;
    
    // Platform-specific metrics
    #ifdef CLOUD_PLATFORM_APPLE_ARM
        double performance_core_usage = 0.0;
        double efficiency_core_usage = 0.0;
        double gpu_usage = 0.0;
        double neural_engine_usage = 0.0;
    #elif defined(CLOUD_PLATFORM_LINUX_X64)
        double physical_core_usage = 0.0;
        double logical_core_usage = 0.0;
        double gpu_usage = 0.0;
        double avx_usage = 0.0;
    #endif
};

/**
 * @brief Расширенные метрики ядра для интеграции с LoadBalancer
 */
struct ExtendedKernelMetrics {
    // Основные метрики
    double load; // относительная загрузка ядра (0..1)
    double latency; // средняя задержка
    double cacheEfficiency; // эффективность кэша
    double tunnelBandwidth; // пропускная способность туннеля
    int activeTasks; // число активных задач
    
    // Resource-Aware метрики
    double cpuUsage; // использование CPU (0..1)
    double memoryUsage; // использование памяти (0..1)
    double networkBandwidth; // доступная сетевая пропускная способность (MB/s)
    double diskIO; // дисковая активность (IOPS)
    double energyConsumption; // энергопотребление (Watts)
    
    // Workload-Specific метрики
    double cpuTaskEfficiency; // эффективность для CPU-задач
    double ioTaskEfficiency; // эффективность для I/O-задач
    double memoryTaskEfficiency; // эффективность для memory-задач
    double networkTaskEfficiency; // эффективность для network-задач
    
    // Конструктор по умолчанию
    ExtendedKernelMetrics() : load(0.0), latency(0.0), cacheEfficiency(0.0), 
                              tunnelBandwidth(0.0), activeTasks(0), cpuUsage(0.0),
                              memoryUsage(0.0), networkBandwidth(0.0), diskIO(0.0),
                              energyConsumption(0.0), cpuTaskEfficiency(0.0),
                              ioTaskEfficiency(0.0), memoryTaskEfficiency(0.0),
                              networkTaskEfficiency(0.0) {}
};

/**
 * @brief Task callback для выполнения задач
 */
using TaskCallback = std::function<void(const cloud::core::balancer::TaskDescriptor&)>;

class IKernel {
public:
    virtual ~IKernel() = default;
    
    // Core functionality
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool isRunning() const = 0;
    
    // Metrics and monitoring
    virtual metrics::PerformanceMetrics getMetrics() const = 0;
    virtual void updateMetrics() = 0;
    
    // Resource management
    virtual void setResourceLimit(const std::string& resource, double limit) = 0;
    virtual double getResourceUsage(const std::string& resource) const = 0;
    
    // Kernel information
    virtual KernelType getType() const = 0;
    virtual std::string getId() const = 0;
    
    // Control
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void reset() = 0;
    
    // Features
    virtual std::vector<std::string> getSupportedFeatures() const = 0;
    
    virtual void scheduleTask(std::function<void()> task, int priority) = 0;
};

template<typename T>
class ResourceManager {
public:
    explicit ResourceManager(size_t initialCapacity = 0)
        : capacity(initialCapacity) {}
    
    bool allocate(const T& resource) {
        std::shared_lock<std::shared_mutex> lock(mutex);
        if (resources.size() >= capacity) return false;
        resources.push_back(resource);
        return true;
    }
    
    void deallocate(const T& resource) {
        std::shared_lock<std::shared_mutex> lock(mutex);
        auto it = std::find(resources.begin(), resources.end(), resource);
        if (it != resources.end()) {
            resources.erase(it);
        }
    }
    
    size_t getUsage() const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return resources.size();
    }
    
private:
    std::vector<T> resources;
    size_t capacity;
    mutable std::shared_mutex mutex;
};

// Конфигурация ядра
struct CoreKernelConfig {
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
struct CoreMetrics {
    double threadUtilization;    // Утилизация потоков
    double memoryUtilization;    // Утилизация памяти
    double cacheEfficiency;      // Эффективность кэша
    double preloadEfficiency;    // Эффективность предварительной загрузки
    double recoveryEfficiency;   // Эффективность восстановления
    double overallEfficiency;    // Общая эффективность
};

/**
 * @brief Ядро системы — базовый класс для всех типов ядер.
 * @details Реализует интерфейс IKernel, поддерживает управление задачами, ресурсами, дочерними ядрами и событиями.
 */
class CoreKernel : public IKernel {
public:
    /** @brief Конструктор по умолчанию. */
    CoreKernel();
    /** @brief Конструктор с указанием идентификатора ядра. */
    CoreKernel(const std::string& id);
    /** @brief Конструктор с конфигурацией. */
    explicit CoreKernel(const CoreKernelConfig& config);
    /** @brief Деструктор. */
    ~CoreKernel();
    CoreKernel(CoreKernel&& other) noexcept;
    CoreKernel& operator=(CoreKernel&& other) noexcept;
    
    /** @brief Инициализация ядра. */
    bool initialize() override;
    /** @brief Обработка данных (ключ, данные). */
    bool processData(const std::string& key, const std::vector<uint8_t>& data);
    /** @brief Получение метрик ядра. */
    metrics::PerformanceMetrics getMetrics() const override;
    /** @brief Обновление метрик ядра. */
    void updateMetrics() override;
    /** @brief Установка конфигурации ядра. */
    void setConfiguration(const CoreKernelConfig& config);
    /** @brief Получение конфигурации ядра. */
    CoreKernelConfig getConfiguration() const;
    /** @brief Установка обработчика ошибок. */
    void setErrorCallback(std::function<void(const std::string&)> callback);
    /** @brief Завершение работы ядра. */
    void shutdown() override;
    /** @brief Проверка, работает ли ядро. */
    bool isRunning() const override;
    /** @brief Получение типа ядра. */
    KernelType getType() const override;
    /** @brief Получение идентификатора ядра. */
    std::string getId() const override;
    /** @brief Пауза ядра. */
    void pause() override;
    /** @brief Возобновление работы ядра. */
    void resume() override;
    /** @brief Сброс состояния ядра. */
    void reset() override;
    /** @brief Получение поддерживаемых функций ядра. */
    std::vector<std::string> getSupportedFeatures() const override;
    /** @brief Добавление дочернего ядра. */
    void addChildKernel(std::shared_ptr<IKernel> kernel);
    /** @brief Удаление дочернего ядра по id. */
    void removeChildKernel(const std::string& kernelId);
    /** @brief Получение списка дочерних ядер. */
    std::vector<std::shared_ptr<IKernel>> getChildKernels() const;
    /** @brief Планирование задачи с приоритетом. */
    void scheduleTask(std::function<void()> task, int priority) override;
    /** @brief Отмена задачи по идентификатору. */
    void cancelTask(const std::string& taskId);
    /** @brief Оптимизация под архитектуру. */
    void optimizeForArchitecture();
    /** @brief Включение аппаратного ускорения. */
    void enableHardwareAcceleration();
    /** @brief Конфигурирование кэша. */
    void configureCache();
    /** @brief Регистрация обработчика события. */
    void registerEventHandler(const std::string& event, EventCallback callback);
    /** @brief Отмена регистрации обработчика события. */
    void unregisterEventHandler(const std::string& event);
    /** @brief Установка режима производительности. */
    void setPerformanceMode(bool highPerformance);
    /** @brief Проверка режима производительности. */
    bool isHighPerformanceMode() const;
    
    /**
     * @brief Установить PreloadManager для warm-up
     */
    void setPreloadManager(std::shared_ptr<core::PreloadManager> preloadManager);
    
    /**
     * @brief Выполнить warm-up из PreloadManager
     */
    void warmupFromPreload();
    
    /**
     * @brief Получить расширенные метрики для LoadBalancer
     */
    ExtendedKernelMetrics getExtendedMetrics() const;
    
    /**
     * @brief Обновить расширенные метрики
     */
    void updateExtendedMetrics();
    
    /**
     * @brief Обработать задачу с новым TaskDescriptor
     */
    bool processTask(const cloud::core::balancer::TaskDescriptor& task);
    
    /**
     * @brief Запланировать задачу с приоритетом
     */
    void scheduleTask(const cloud::core::balancer::TaskDescriptor& task);
    
    /**
     * @brief Установить callback для обработки задач
     */
    void setTaskCallback(TaskCallback callback);
    
    /**
     * @brief Установить LoadBalancer для интеграции
     */
    void setLoadBalancer(std::shared_ptr<cloud::core::balancer::LoadBalancer> loadBalancer);
    
    /**
     * @brief Получить LoadBalancer
     */
    std::shared_ptr<cloud::core::balancer::LoadBalancer> getLoadBalancer() const;
    
    /**
     * @brief Установить event callback
     */
    void setEventCallback(const std::string& event, EventCallback callback);
    
    /**
     * @brief Удалить event callback
     */
    void removeEventCallback(const std::string& event);

private:
    struct Impl {
        std::string id;
        metrics::PerformanceMetrics currentMetrics;
        std::unordered_map<std::string, double> resourceLimits;
        std::unordered_map<std::string, double> resourceUsage;
        std::vector<std::thread> workerThreads;
        std::atomic<bool> paused;
        std::atomic<bool> highPerformanceMode;
        std::chrono::steady_clock::time_point lastOptimization;
        std::unordered_map<std::string, std::shared_ptr<IKernel>> childKernels;
        std::unordered_map<std::string, std::vector<EventCallback>> eventHandlers;
        std::priority_queue<std::pair<int, std::function<void()>>> taskQueue;
        std::condition_variable taskCondition;
        std::bitset<MAX_KERNEL_THREADS> threadStatus;
        std::atomic<bool> running = false;
        
        Impl(const std::string& kernelId = "");
        std::string generateUniqueId();
        void initializeMetrics();
        void updateMetrics();
        
        #ifdef CLOUD_PLATFORM_APPLE_ARM
            void updateAppleMetrics();
        #elif defined(CLOUD_PLATFORM_LINUX_X64)
            void updateLinuxMetrics();
        #endif
    };
    std::unique_ptr<Impl> pImpl;
    std::unique_ptr<core::cache::DefaultDynamicCache> dynamicCache;
    std::unique_ptr<core::recovery::RecoveryManager> recoveryManager;
    std::shared_ptr<cloud::core::thread::ThreadPool> threadPool;
    std::unique_ptr<core::cache::PlatformOptimizer> platformOptimizer;
    bool initialized = false;
    mutable std::shared_mutex kernelMutex;
    void initializeLogger();
    void shutdownComponents();
    
    // Инициализация компонентов
    bool initializeComponents();
    
    // Адаптивная обработка данных
    bool adaptiveProcess(const std::string& key, const std::vector<uint8_t>& data);
    
    // Оптимизация обработки
    void optimizeProcessing();
    
    // Оптимизация кэша
    void optimizeCache();
    
    // Оптимизация предварительной загрузки
    void optimizePreload();
    
    // Расчет метрик
    void calculateMetrics();
    
    // Расчет утилизации потоков
    double calculateThreadUtilization() const;
    
    // Расчет утилизации памяти
    double calculateMemoryUtilization() const;
    
    // Расчет общей эффективности
    double calculateOverallEfficiency() const;
    
    // Platform-specific components
    #ifdef CLOUD_PLATFORM_APPLE_ARM
        std::unique_ptr<platform::AppleARMConfig> appleConfig;
    #elif defined(CLOUD_PLATFORM_LINUX_X64)
        std::unique_ptr<platform::LinuxX64Config> linuxConfig;
    #endif
    
    // Core components
    std::shared_ptr<detail::PerformanceMonitor> performanceMonitor;
    std::shared_ptr<detail::ResourceManager> resourceManager;
    std::shared_ptr<detail::AdaptiveController> adaptiveController;
    std::shared_ptr<detail::ThermalManager> thermalManager;
    std::shared_ptr<detail::PowerManager> powerManager;
    std::shared_ptr<detail::HardwareAccelerator> hardwareAccelerator;
    
    // Initialization and cleanup
    void manageResources();
    void handleTaskQueue();
    void validateConfiguration() const;
    void cleanupResources();
    void handleException(const std::exception& e);
    
    // Thread management
    void startWorkerThreads();
    void stopWorkerThreads();
    void adjustThreadCount(size_t count);
    
    // Optimization
    void optimizeMemoryUsage();
    void optimizeThreadUsage();
    void optimizeCacheUsage();
    
    // Platform-specific optimizations
    #ifdef CLOUD_PLATFORM_APPLE_ARM
        void optimizeForAppleARM();
        void configureAppleARM();
        void monitorAppleARMMetrics();
    #elif defined(CLOUD_PLATFORM_LINUX_X64)
        void optimizeForLinuxX64();
        void configureLinuxX64();
        void monitorLinuxX64Metrics();
    #endif
    
    std::shared_ptr<core::PreloadManager> preloadManager_;
    std::shared_ptr<cloud::core::balancer::LoadBalancer> loadBalancer_;
    TaskCallback taskCallback_;
    std::unordered_map<std::string, EventCallback> eventCallbacks_;
    ExtendedKernelMetrics extendedMetrics_;
    
    void initializePreloadManager();
    void initializeLoadBalancer();
    void updateExtendedMetricsFromPerformance();
    void notifyEvent(const std::string& event, const std::any& data);
};

namespace detail {
    template<typename T>
    struct is_kernel_type : std::is_base_of<IKernel, T> {};
}

} // namespace kernel
} // namespace core
} // namespace cloud
