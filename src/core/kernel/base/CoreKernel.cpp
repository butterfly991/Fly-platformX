#include "core/kernel/base/CoreKernel.hpp"
#include <chrono>
#include <thread>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include "core/cache/dynamic/DynamicCache.hpp"
#include <random>
#include <unordered_set>
#include "core/balancer/TaskTypes.hpp"

namespace cloud {
namespace core {
namespace kernel {

namespace detail {

class PerformanceMonitor {
public:
    explicit PerformanceMonitor(const config::OptimizationConfig& config)
        : config_(config) {
        initializeMetrics();
    }

    void updateMetrics() {
        std::shared_lock<std::shared_mutex> lock(metricsMutex_);
        try {
            #ifdef CLOUD_PLATFORM_APPLE_ARM
                updateAppleMetrics();
            #elif defined(CLOUD_PLATFORM_LINUX_X64)
                updateLinuxMetrics();
            #endif
            calculateEfficiency();
        } catch (const std::exception& e) {
            logger_->error("Failed to update metrics: {}", e.what());
        }
    }

    metrics::PerformanceMetrics getMetrics() const {
        std::shared_lock<std::shared_mutex> lock(metricsMutex_);
        return metrics_;
    }

private:
    void initializeMetrics() {
        metrics_ = metrics::PerformanceMetrics{
            .cpu_usage = 0.0,
            .memory_usage = 0.0,
            .power_consumption = 0.0,
            .temperature = 0.0,
            .instructions_per_second = 0,
            .timestamp = std::chrono::steady_clock::now()
        };
        
        #ifdef CLOUD_PLATFORM_APPLE_ARM
            metrics_.performance_core_usage = 0.0;
            metrics_.efficiency_core_usage = 0.0;
            metrics_.gpu_usage = 0.0;
            metrics_.neural_engine_usage = 0.0;
        #elif defined(CLOUD_PLATFORM_LINUX_X64)
            metrics_.physical_core_usage = 0.0;
            metrics_.logical_core_usage = 0.0;
            metrics_.gpu_usage = 0.0;
            metrics_.avx_usage = 0.0;
        #endif
    }

    #ifdef CLOUD_PLATFORM_APPLE_ARM
    void updateAppleMetrics() {
        processor_cpu_load_info_t cpuLoad;
        mach_msg_type_number_t count;
        host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &count,
                           reinterpret_cast<processor_info_t*>(&cpuLoad), &count);

        double perfUsage = 0.0, effUsage = 0.0;
        size_t perfCores = 0, effCores = 0;
        for (size_t i = 0; i < count; ++i) {
            double usage = (cpuLoad[i].cpu_ticks[CPU_STATE_USER] +
                          cpuLoad[i].cpu_ticks[CPU_STATE_SYSTEM]) /
                         static_cast<double>(cpuLoad[i].cpu_ticks[CPU_STATE_IDLE]);
            
            if (i < 4) {
                perfUsage += usage;
                ++perfCores;
            } else {
                effUsage += usage;
                ++effCores;
            }
        }

        metrics_.performance_core_usage = perfCores > 0 ? perfUsage / perfCores : 0.0;
        metrics_.efficiency_core_usage = effCores > 0 ? effUsage / effCores : 0.0;
        metrics_.cpu_usage = (perfUsage + effUsage) / count;
        
        vm_size_t pageSize;
        host_page_size(mach_host_self(), &pageSize);
        
        vm_statistics64_data_t vmStats;
        mach_msg_type_number_t infoCount = sizeof(vmStats) / sizeof(natural_t);
        host_statistics64(mach_host_self(), HOST_VM_INFO64,
                         reinterpret_cast<host_info64_t>(&vmStats), &infoCount);
        
        uint64_t totalMemory = vmStats.free_count + vmStats.active_count +
                              vmStats.inactive_count + vmStats.wire_count;
        metrics_.memory_usage = 1.0 - (static_cast<double>(vmStats.free_count) / totalMemory);
        
        int power, temp;
        size_t size = sizeof(power);
        if (sysctlbyname("machdep.cpu.power", &power, &size, nullptr, 0) == 0) {
            metrics_.power_consumption = power / 1000.0;
        }
        
        size = sizeof(temp);
        if (sysctlbyname("machdep.cpu.temperature", &temp, &size, nullptr, 0) == 0) {
            metrics_.temperature = temp / 100.0;
        }
        
        size_t neuralEngineUsage;
        size = sizeof(neuralEngineUsage);
        if (sysctlbyname("machdep.cpu.neural_engine_usage", &neuralEngineUsage, &size, nullptr, 0) == 0) {
            metrics_.neural_engine_usage = neuralEngineUsage / 100.0;
        }
    }
    #elif defined(CLOUD_PLATFORM_LINUX_X64)
    void updateLinuxMetrics() {
        struct sysinfo si;
        if (sysinfo(&si) == 0) {
            metrics_.memory_usage = 1.0 - (static_cast<double>(si.freeram) / si.totalram);
        }

        std::ifstream statFile("/proc/stat");
        std::string line;
        if (std::getline(statFile, line)) {
            std::istringstream iss(line);
            std::string cpu;
            uint64_t user, nice, system, idle, iowait, irq, softirq;
            iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;
            
            uint64_t total = user + nice + system + idle + iowait + irq + softirq;
            uint64_t idleTotal = idle + iowait;
            
            metrics_.cpu_usage = 1.0 - (static_cast<double>(idleTotal) / total);
        }

        std::ifstream cpuInfoFile("/proc/cpuinfo");
        size_t physicalCores = 0;
        size_t logicalCores = 0;
        std::unordered_set<std::string> physicalIds;
        
        while (std::getline(cpuInfoFile, line)) {
            if (line.find("processor") != std::string::npos) {
                ++logicalCores;
            } else if (line.find("physical id") != std::string::npos) {
                physicalIds.insert(line);
            }
        }
        
        physicalCores = physicalIds.size();
        metrics_.physical_core_usage = metrics_.cpu_usage * (static_cast<double>(physicalCores) / logicalCores);
        metrics_.logical_core_usage = metrics_.cpu_usage;
        
        std::ifstream memFile("/proc/meminfo");
        uint64_t totalMem = 0, freeMem = 0;
        while (std::getline(memFile, line)) {
            std::istringstream iss(line);
            std::string key;
            uint64_t value;
            iss >> key >> value;
            
            if (key == "MemTotal:") totalMem = value;
            else if (key == "MemFree:") freeMem = value;
        }
        
        if (totalMem > 0) {
            metrics_.memory_usage = 1.0 - (static_cast<double>(freeMem) / totalMem);
        }
        
        std::ifstream powerFile("/sys/class/power_supply/BAT0/power_now");
        if (powerFile) {
            int power;
            powerFile >> power;
            metrics_.power_consumption = power / 1000000.0;
        }
        
        std::ifstream tempFile("/sys/class/thermal/thermal_zone0/temp");
        if (tempFile) {
            int temp;
            tempFile >> temp;
            metrics_.temperature = temp / 1000.0;
        }
        
        std::ifstream avxFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
        if (avxFile) {
            int freq;
            avxFile >> freq;
            metrics_.avx_usage = freq > 2000000 ? 1.0 : 0.0;
        }
    }
    #endif

    void calculateEfficiency() {
        double efficiency = 0.0;
        
        #ifdef CLOUD_PLATFORM_APPLE_ARM
            efficiency = (metrics_.performance_core_usage * 0.4 +
                         metrics_.efficiency_core_usage * 0.3 +
                         metrics_.neural_engine_usage * 0.3);
        #elif defined(CLOUD_PLATFORM_LINUX_X64)
            efficiency = (metrics_.physical_core_usage * 0.4 +
                         metrics_.logical_core_usage * 0.3 +
                         metrics_.avx_usage * 0.3);
        #endif
        
        metrics_.instructions_per_second = static_cast<uint64_t>(efficiency * 1000000000);
    }

    config::OptimizationConfig config_;
    metrics::PerformanceMetrics metrics_;
    mutable std::shared_mutex metricsMutex_;
    std::shared_ptr<spdlog::logger> logger_;
};

class ResourceManager {
public:
    explicit ResourceManager(const config::ResourceLimits& config)
        : config_(config) {
        initializeResources();
    }

    bool allocateResource(const std::string& resource, double amount) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = resources_.find(resource);
        if (it == resources_.end()) return false;
        
        if (it->second.current + amount > it->second.limit) return false;
        
        it->second.current += amount;
        return true;
    }

    void deallocateResource(const std::string& resource, double amount) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = resources_.find(resource);
        if (it != resources_.end()) {
            it->second.current = std::max(0.0, it->second.current - amount);
        }
    }

    double getResourceEfficiency(const std::string& resource) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = resources_.find(resource);
        if (it == resources_.end()) return 0.0;
        
        return it->second.current / it->second.limit;
    }

private:
    void initializeResources() {
        resources_["cpu"] = {config_.maxCpuUsage, 0.0};
        resources_["memory"] = {static_cast<double>(config_.maxMemory), 0.0};
        resources_["power"] = {config_.maxPowerConsumption, 0.0};
        resources_["temperature"] = {config_.maxTemperature, 0.0};
    }

    struct Resource {
        double limit;
        double current;
    };

    config::ResourceLimits config_;
    std::unordered_map<std::string, Resource> resources_;
    mutable std::shared_mutex mutex_;
};

class AdaptiveController {
public:
    explicit AdaptiveController(const config::OptimizationConfig& config)
        : config_(config) {
        initializeController();
    }

    void update(const metrics::PerformanceMetrics& metrics) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        metricsHistory_.push_back(metrics);
        if (metricsHistory_.size() > config_.historySize) {
            metricsHistory_.pop_front();
        }

        if (shouldAdapt()) {
            adapt();
        }
    }

    std::vector<double> getAdaptationParameters() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return currentParameters_;
    }

private:
    void initializeController() {
        currentParameters_.resize(4); // CPU, Memory, Power, Temperature
        std::fill(currentParameters_.begin(), currentParameters_.end(), 0.5);
    }

    bool shouldAdapt() const {
        if (metricsHistory_.size() < 2) return false;

        const auto& current = metricsHistory_.back();
        const auto& previous = metricsHistory_[metricsHistory_.size() - 2];

        // Check if performance is below threshold
        if (current.efficiencyScore < config_.minPerformanceThreshold) return true;

        // Check if performance degradation is significant
        double degradation = previous.efficiencyScore - current.efficiencyScore;
        if (degradation > 0.1) return true;

        return false;
    }

    void adapt() {
        // Calculate gradient for each parameter
        double gradient = calculateGradient();
        
        // Update parameters using gradient descent
        for (auto& param : currentParameters_) {
            param = std::max(0.0, std::min(1.0, param - config_.learningRate * gradient));
        }

        // Add exploration
        if (std::uniform_real_distribution<>(0, 1)(rng_) < config_.explorationRate) {
            explore();
        }
    }

    double calculateGradient() const {
        if (metricsHistory_.size() < 2) return 0.0;

        const auto& current = metricsHistory_.back();
        const auto& previous = metricsHistory_[metricsHistory_.size() - 2];

        // Calculate gradient based on efficiency score change
        return (current.efficiencyScore - previous.efficiencyScore) /
               std::max(1e-6, std::abs(current.efficiencyScore - previous.efficiencyScore));
    }

    void explore() {
        std::uniform_real_distribution<> dist(-0.1, 0.1);
        for (auto& param : currentParameters_) {
            param = std::max(0.0, std::min(1.0, param + dist(rng_)));
        }
    }

    config::OptimizationConfig config_;
    std::deque<metrics::PerformanceMetrics> metricsHistory_;
    std::vector<double> currentParameters_;
    std::mt19937 rng_{std::random_device{}()};
    mutable std::shared_mutex mutex_;
};

} // namespace detail

// Constructor and destructor implementation
CoreKernel::CoreKernel()
    : pImpl(std::make_unique<Impl>()) {
    initializeLogger();
}

CoreKernel::CoreKernel(const std::string& id)
    : pImpl(std::make_unique<Impl>(id)) {
    dynamicCache = std::make_unique<core::DefaultDynamicCache>(128);
    initializeLogger();
}

CoreKernel::~CoreKernel() {
    try {
        shutdown();
    } catch (const std::exception& e) {
        spdlog::error("Error during kernel shutdown: {}", e.what());
    }
}

// Move operations
CoreKernel::CoreKernel(CoreKernel&& other) noexcept
    : pImpl(std::move(other.pImpl)) {
}

CoreKernel& CoreKernel::operator=(CoreKernel&& other) noexcept {
    if (this != &other) {
        pImpl = std::move(other.pImpl);
    }
    return *this;
}

// Core functionality implementation
bool CoreKernel::initialize() {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    if (pImpl->running) return false;
    
    try {
        initializeComponents();
        pImpl->running = true;
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize kernel: {}", e.what());
        return false;
    }
}

void CoreKernel::shutdown() {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    if (!pImpl->running) return;
    
    try {
        shutdownComponents();
        pImpl->running = false;
        if (dynamicCache) dynamicCache->clear();
    } catch (const std::exception& e) {
        spdlog::error("Error during kernel shutdown: {}", e.what());
        throw;
    }
}

bool CoreKernel::isRunning() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    return pImpl->running;
}

// Metrics implementation
PerformanceMetrics CoreKernel::getMetrics() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    return pImpl->currentMetrics;
}

void CoreKernel::updateMetrics() {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    pImpl->updateMetrics();
    std::vector<uint8_t> stateData{/* ... сериализация состояния ядра ... */};
    if (dynamicCache) dynamicCache->put("core_state", stateData);
}

// Resource management implementation
void CoreKernel::setResourceLimit(const std::string& resource, double limit) {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    pImpl->resourceLimits[resource] = limit;
}

double CoreKernel::getResourceUsage(const std::string& resource) const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    auto it = pImpl->resourceUsage.find(resource);
    return it != pImpl->resourceUsage.end() ? it->second : 0.0;
}

// Kernel information implementation
KernelType CoreKernel::getType() const {
    return KernelType::PARENT;
}

std::string CoreKernel::getId() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    return pImpl->id;
}

// Control implementation
void CoreKernel::pause() {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    pImpl->paused = true;
}

void CoreKernel::resume() {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    pImpl->paused = false;
}

void CoreKernel::reset() {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    shutdownComponents();
    initializeComponents();
}

// Features implementation
std::vector<std::string> CoreKernel::getSupportedFeatures() const {
    std::vector<std::string> features;
    
    #ifdef CLOUD_PLATFORM_APPLE_ARM
        features.push_back("neon");
        features.push_back("amx");
        features.push_back("metal");
        features.push_back("neural_engine");
    #elif defined(CLOUD_PLATFORM_LINUX_X64)
        features.push_back("avx2");
        features.push_back("avx512");
        features.push_back("perf_events");
    #endif
    
    return features;
}

// Child kernel management implementation
void CoreKernel::addChildKernel(std::shared_ptr<IKernel> kernel) {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    if (!kernel) return;
    
    pImpl->childKernels[kernel->getId()] = kernel;
}

void CoreKernel::removeChildKernel(const std::string& kernelId) {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    pImpl->childKernels.erase(kernelId);
}

std::vector<std::shared_ptr<IKernel>> CoreKernel::getChildKernels() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    std::vector<std::shared_ptr<IKernel>> kernels;
    kernels.reserve(pImpl->childKernels.size());
    
    for (const auto& [id, kernel] : pImpl->childKernels) {
        kernels.push_back(kernel);
    }
    
    return kernels;
}

// Task management implementation
void CoreKernel::scheduleTask(std::function<void()> task, int priority) {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    if (!pImpl->running) {
        spdlog::warn("CoreKernel[{}]: Попытка планирования задачи на остановленном ядре", pImpl->id);
        return;
    }
    pImpl->taskQueue.push({priority, std::move(task)});
    pImpl->taskCondition.notify_one();
    spdlog::debug("CoreKernel[{}]: Задача запланирована с приоритетом {}", pImpl->id, priority);
}

void CoreKernel::cancelTask(const std::string& taskId) {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    spdlog::info("CoreKernel: cancelling task id={}", taskId);
    pImpl->cancelledTasks.insert(taskId);
    // Реализуем удаление отменённых задач из очереди
    std::priority_queue<std::pair<int, std::function<void()>>> newQueue;
    while (!pImpl->taskQueue.empty()) {
        auto& [priority, task] = pImpl->taskQueue.top();
        std::string id = std::to_string(reinterpret_cast<uintptr_t>(&task));
        if (pImpl->cancelledTasks.count(id) == 0) {
            newQueue.emplace(priority, task);
        } else {
            spdlog::debug("CoreKernel: удалена отменённая задача id={}", id);
        }
        pImpl->taskQueue.pop();
    }
    pImpl->taskQueue = std::move(newQueue);
}

// Architecture optimization implementation
void CoreKernel::optimizeForArchitecture() {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    spdlog::info("CoreKernel: optimizeForArchitecture called");
#ifdef CLOUD_PLATFORM_APPLE_ARM
    if (platformOptimizer) {
        spdlog::info("CoreKernel: оптимизация под Apple ARM через PlatformOptimizer");
        // platformOptimizer->optimizeForAppleARM(); // если реализовано
    }
#elif defined(CLOUD_PLATFORM_LINUX_X64)
    if (platformOptimizer) {
        spdlog::info("CoreKernel: оптимизация под Linux x64 через PlatformOptimizer");
        // platformOptimizer->optimizeForLinuxX64(); // если реализовано
    }
#else
    spdlog::warn("CoreKernel: No platform-specific optimization available");
#endif
}

void CoreKernel::enableHardwareAcceleration() {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    spdlog::info("CoreKernel: enableHardwareAcceleration called");
#ifdef CLOUD_PLATFORM_APPLE_ARM
    // Пример: включаем NEON/AMX через ARMDriver, если доступно
    if (platformOptimizer) {
        spdlog::info("CoreKernel: включение NEON/AMX через PlatformOptimizer");
        // platformOptimizer->enableNeon(); // если реализовано
    }
#elif defined(CLOUD_PLATFORM_LINUX_X64)
    // Пример: включаем AVX/AVX2/AVX512 через PlatformOptimizer
    if (platformOptimizer) {
        spdlog::info("CoreKernel: включение AVX/AVX2/AVX512 через PlatformOptimizer");
        // platformOptimizer->enableAVX(); // если реализовано
    }
#else
    spdlog::warn("CoreKernel: No hardware acceleration available");
#endif
}

void CoreKernel::configureCache() {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    spdlog::info("CoreKernel: configureCache called");
    #ifdef CLOUD_PLATFORM_APPLE_ARM
        spdlog::info("CoreKernel: Apple Silicon cache configuration (stub)");
    #elif defined(CLOUD_PLATFORM_LINUX_X64)
        spdlog::info("CoreKernel: x86-64 cache configuration (stub)");
    #else
        spdlog::warn("CoreKernel: No cache configuration available");
    #endif
}

// Performance management implementation
void CoreKernel::setPerformanceMode(bool highPerformance) {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    pImpl->highPerformanceMode = highPerformance;
    
    #ifdef CLOUD_PLATFORM_APPLE_ARM
        if (highPerformance) {
            // Set performance cores
            // TODO: Implement performance core configuration
        } else {
            // Set efficiency cores
            // TODO: Implement efficiency core configuration
        }
    #elif defined(CLOUD_PLATFORM_LINUX_X64)
        if (highPerformance) {
            // Set high performance mode
            // TODO: Implement high performance mode configuration
        } else {
            // Set power saving mode
            // TODO: Implement power saving mode configuration
        }
    #endif
}

bool CoreKernel::isHighPerformanceMode() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    return pImpl->highPerformanceMode;
}

// Event handling implementation
void CoreKernel::registerEventHandler(const std::string& event, EventCallback callback) {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    pImpl->eventHandlers[event].push_back(std::move(callback));
}

void CoreKernel::unregisterEventHandler(const std::string& event) {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    pImpl->eventHandlers.erase(event);
}

void CoreKernel::triggerEvent(const std::string& event, const std::any& data) {
    notifyEvent(event, data);
}

void CoreKernel::setPreloadManager(std::shared_ptr<core::PreloadManager> preloadManager) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    preloadManager_ = preloadManager;
    spdlog::info("CoreKernel[{}]: PreloadManager установлен", pImpl->id);
}

void CoreKernel::warmupFromPreload() {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    if (!preloadManager_ || !dynamicCache) {
        spdlog::warn("CoreKernel[{}]: PreloadManager или DynamicCache недоступны для warm-up", pImpl->id);
        return;
    }
    
    try {
        spdlog::info("CoreKernel[{}]: Начинаем warm-up из PreloadManager", pImpl->id);
        
        // Получаем все ключи из PreloadManager
        auto keys = preloadManager_->getAllKeys();
        spdlog::debug("CoreKernel[{}]: Получено {} ключей для warm-up", pImpl->id, keys.size());
        
        // Получаем данные для ключей
        for (const auto& key : keys) {
            auto data = preloadManager_->getDataForKey(key);
            if (data) {
                dynamicCache->put(key, *data);
                spdlog::trace("CoreKernel[{}]: Загружен ключ '{}' в кэш", pImpl->id, key);
            }
        }
        
        spdlog::info("CoreKernel[{}]: Warm-up завершен, загружено {} элементов", pImpl->id, keys.size());
        notifyEvent("warmup_completed", keys.size());
        
    } catch (const std::exception& e) {
        spdlog::error("CoreKernel[{}]: Ошибка при warm-up: {}", pImpl->id, e.what());
        notifyEvent("warmup_failed", std::string(e.what()));
    }
}

ExtendedKernelMetrics CoreKernel::getExtendedMetrics() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    return extendedMetrics_;
}

void CoreKernel::updateExtendedMetrics() {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    updateExtendedMetricsFromPerformance();
}

bool CoreKernel::processTask(const balancer::TaskDescriptor& task) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    
    if (!pImpl->running) {
        spdlog::warn("CoreKernel[{}]: Попытка обработки задачи на остановленном ядре", pImpl->id);
        return false;
    }
    
    try {
        spdlog::debug("CoreKernel[{}]: Обработка задачи типа {} с приоритетом {}", 
                     pImpl->id, static_cast<int>(task.type), task.priority);
        
        // Вызываем callback если установлен
        if (taskCallback_) {
            taskCallback_(task);
        }
        
        // Обрабатываем данные через DynamicCache
        if (dynamicCache) {
            std::string key = "task_" + std::to_string(task.priority) + "_" + 
                             std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                 task.enqueueTime.time_since_epoch()).count());
            dynamicCache->put(key, task.data);
        }
        
        // Обновляем метрики
        updateExtendedMetrics();
        
        notifyEvent("task_processed", task);
        spdlog::debug("CoreKernel[{}]: Задача успешно обработана", pImpl->id);
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("CoreKernel[{}]: Ошибка обработки задачи: {}", pImpl->id, e.what());
        notifyEvent("task_failed", std::string(e.what()));
        return false;
    }
}

void CoreKernel::scheduleTask(const balancer::TaskDescriptor& task) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    
    if (!pImpl->running) {
        spdlog::warn("CoreKernel[{}]: Попытка планирования задачи на остановленном ядре", pImpl->id);
        return;
    }
    
    // Добавляем задачу в очередь с приоритетом
    pImpl->taskQueue.push({task.priority, [this, task]() {
        processTask(task);
    }});
    
    pImpl->taskCondition.notify_one();
    
    spdlog::debug("CoreKernel[{}]: Задача запланирована с приоритетом {}", 
                 pImpl->id, task.priority);
}

void CoreKernel::setTaskCallback(TaskCallback callback) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    taskCallback_ = callback;
    spdlog::debug("CoreKernel[{}]: TaskCallback установлен", pImpl->id);
}

void CoreKernel::setLoadBalancer(std::shared_ptr<balancer::LoadBalancer> loadBalancer) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    loadBalancer_ = loadBalancer;
    spdlog::info("CoreKernel[{}]: LoadBalancer установлен", pImpl->id);
}

std::shared_ptr<balancer::LoadBalancer> CoreKernel::getLoadBalancer() const {
    std::shared_lock<std::shared_mutex> lock(kernelMutex);
    return loadBalancer_;
}

void CoreKernel::setEventCallback(const std::string& event, EventCallback callback) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    eventCallbacks_[event] = callback;
    spdlog::debug("CoreKernel[{}]: EventCallback установлен для события '{}'", pImpl->id, event);
}

void CoreKernel::removeEventCallback(const std::string& event) {
    std::unique_lock<std::shared_mutex> lock(kernelMutex);
    eventCallbacks_.erase(event);
    spdlog::debug("CoreKernel[{}]: EventCallback удален для события '{}'", pImpl->id, event);
}

void CoreKernel::initializePreloadManager() {
    if (!preloadManager_) {
        spdlog::debug("CoreKernel[{}]: PreloadManager не установлен", pImpl->id);
        return;
    }
    
    try {
        if (preloadManager_->initialize()) {
            spdlog::info("CoreKernel[{}]: PreloadManager инициализирован", pImpl->id);
            warmupFromPreload();
        } else {
            spdlog::warn("CoreKernel[{}]: Не удалось инициализировать PreloadManager", pImpl->id);
        }
    } catch (const std::exception& e) {
        spdlog::error("CoreKernel[{}]: Ошибка инициализации PreloadManager: {}", pImpl->id, e.what());
    }
}

void CoreKernel::initializeLoadBalancer() {
    if (!loadBalancer_) {
        spdlog::debug("CoreKernel[{}]: LoadBalancer не установлен", pImpl->id);
        return;
    }
    
    try {
        spdlog::info("CoreKernel[{}]: LoadBalancer готов к работе", pImpl->id);
        notifyEvent("loadbalancer_ready", pImpl->id);
    } catch (const std::exception& e) {
        spdlog::error("CoreKernel[{}]: Ошибка инициализации LoadBalancer: {}", pImpl->id, e.what());
    }
}

void CoreKernel::updateExtendedMetricsFromPerformance() {
    try {
        auto perfMetrics = getMetrics();
        
        // Основные метрики
        extendedMetrics_.load = perfMetrics.cpu_usage;
        extendedMetrics_.latency = perfMetrics.latency;
        extendedMetrics_.cacheEfficiency = perfMetrics.cacheEfficiency;
        extendedMetrics_.tunnelBandwidth = perfMetrics.tunnelBandwidth;
        extendedMetrics_.activeTasks = pImpl->taskQueue.size();
        
        // Resource-Aware метрики
        extendedMetrics_.cpuUsage = perfMetrics.cpu_usage;
        extendedMetrics_.memoryUsage = perfMetrics.memory_usage;
        extendedMetrics_.networkBandwidth = 1000.0; // MB/s, можно получить из системных метрик
        extendedMetrics_.diskIO = 1000.0; // IOPS, можно получить из системных метрик
        extendedMetrics_.energyConsumption = perfMetrics.power_consumption;
        
        // Workload-Specific метрики (вычисляем на основе типа ядра и производительности)
        double baseEfficiency = perfMetrics.efficiencyScore;
        extendedMetrics_.cpuTaskEfficiency = baseEfficiency * (getType() == KernelType::COMPUTATIONAL ? 1.2 : 1.0);
        extendedMetrics_.ioTaskEfficiency = baseEfficiency * (getType() == KernelType::MICRO ? 1.1 : 1.0);
        extendedMetrics_.memoryTaskEfficiency = baseEfficiency * (getType() == KernelType::ARCHITECTURAL ? 1.15 : 1.0);
        extendedMetrics_.networkTaskEfficiency = baseEfficiency * (getType() == KernelType::ORCHESTRATION ? 1.25 : 1.0);
        
        spdlog::trace("CoreKernel[{}]: Расширенные метрики обновлены", pImpl->id);
        
    } catch (const std::exception& e) {
        spdlog::error("CoreKernel[{}]: Ошибка обновления расширенных метрик: {}", pImpl->id, e.what());
    }
}

void CoreKernel::notifyEvent(const std::string& event, const std::any& data) {
    try {
        auto it = eventCallbacks_.find(event);
        if (it != eventCallbacks_.end()) {
            it->second(pImpl->id, data);
            spdlog::trace("CoreKernel[{}]: Событие '{}' обработано", pImpl->id, event);
        }
    } catch (const std::exception& e) {
        spdlog::error("CoreKernel[{}]: Ошибка обработки события '{}': {}", pImpl->id, event, e.what());
    }
}

// Private implementation
void CoreKernel::initializeLogger() {
    try {
        auto logger = spdlog::rotating_logger_mt("kernel", "logs/kernel.log",
                                                1024 * 1024 * 5, 3);
        logger->set_level(spdlog::level::debug);
        spdlog::set_default_logger(logger);
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize logger: " << e.what() << std::endl;
    }
}

void CoreKernel::initializeComponents() {
    // Initialize platform-specific components
    #ifdef CLOUD_PLATFORM_APPLE_ARM
        appleConfig = std::make_unique<platform::AppleARMConfig>();
        configureAppleARM();
    #elif defined(CLOUD_PLATFORM_LINUX_X64)
        linuxConfig = std::make_unique<platform::LinuxX64Config>();
        configureLinuxX64();
    #endif
    
    // Initialize core components
    performanceMonitor = std::make_shared<detail::PerformanceMonitor>(config::OptimizationConfig{});
    resourceManager = std::make_shared<detail::ResourceManager>(config::ResourceLimits{});
    adaptiveController = std::make_shared<detail::AdaptiveController>(config::OptimizationConfig{});
    
    // Initialize thread pool
    threadPool = std::make_shared<ThreadPool>(std::thread::hardware_concurrency());
    
    // Start worker threads
    startWorkerThreads();
}

void CoreKernel::shutdownComponents() {
    // Stop worker threads
    stopWorkerThreads();
    
    // Clear task queue
    pImpl->taskQueue = std::priority_queue<std::pair<int, std::function<void()>>>();
    
    // Clear child kernels
    pImpl->childKernels.clear();
    
    // Clear event handlers
    pImpl->eventHandlers.clear();
    
    // Reset components
    performanceMonitor.reset();
    resourceManager.reset();
    adaptiveController.reset();
    threadPool.reset();
}

void CoreKernel::startWorkerThreads() {
    size_t numThreads = std::thread::hardware_concurrency();
    pImpl->workerThreads.reserve(numThreads);
    
    for (size_t i = 0; i < numThreads; ++i) {
        pImpl->workerThreads.emplace_back([this] {
            while (pImpl->running) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(pImpl->taskCondition);
                    pImpl->taskCondition.wait(lock, [this] {
                        return !pImpl->running || !pImpl->taskQueue.empty();
                    });
                    
                    if (!pImpl->running && pImpl->taskQueue.empty()) return;
                    
                    task = std::move(pImpl->taskQueue.top().second);
                    pImpl->taskQueue.pop();
                }
                
                if (pImpl->cancelledTasks.count(taskId)) continue;
                
                try {
                    task();
                } catch (const std::exception& e) {
                    spdlog::error("Error in worker thread: {}", e.what());
                }
            }
        });
    }
}

void CoreKernel::stopWorkerThreads() {
    pImpl->running = false;
    pImpl->taskCondition.notify_all();
    
    for (auto& thread : pImpl->workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    pImpl->workerThreads.clear();
}

#ifdef CLOUD_PLATFORM_APPLE_ARM
void CoreKernel::optimizeForAppleARM() {
    // Configure performance cores
    thread_affinity_policy_data_t policy = {1};
    thread_port_t thread = mach_thread_self();
    thread_policy_set(thread, THREAD_AFFINITY_POLICY,
                     reinterpret_cast<thread_policy_t>(&policy),
                     THREAD_AFFINITY_POLICY_COUNT);
    
    // Enable NEON
    // TODO: Implement NEON optimization
    
    // Enable AMX
    // TODO: Implement AMX optimization
    
    // Configure Neural Engine
    // TODO: Implement Neural Engine configuration
}

void CoreKernel::configureAppleARM() {
    // Get CPU information
    int numCPUs;
    size_t size = sizeof(numCPUs);
    if (sysctlbyname("hw.ncpu", &numCPUs, &size, nullptr, 0) == 0) {
        appleConfig->performanceCores = numCPUs / 2;
        appleConfig->efficiencyCores = numCPUs / 2;
    }
    
    // Enable features
    appleConfig->enableNeon = true;
    appleConfig->enableAMX = true;
    appleConfig->enablePowerManagement = true;
    appleConfig->enableThermalManagement = true;
}

void CoreKernel::monitorAppleARMMetrics() {
    // Monitor performance cores
    processor_cpu_load_info_t cpuLoad;
    mach_msg_type_number_t count;
    host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &count,
                       reinterpret_cast<processor_info_t*>(&cpuLoad), &count);
    
    double perfUsage = 0.0;
    for (size_t i = 0; i < appleConfig->performanceCores; ++i) {
        perfUsage += (cpuLoad[i].cpu_ticks[CPU_STATE_USER] +
                     cpuLoad[i].cpu_ticks[CPU_STATE_SYSTEM]) /
                    static_cast<double>(cpuLoad[i].cpu_ticks[CPU_STATE_IDLE]);
    }
    
    // Monitor efficiency cores
    double effUsage = 0.0;
    for (size_t i = appleConfig->performanceCores; i < count; ++i) {
        effUsage += (cpuLoad[i].cpu_ticks[CPU_STATE_USER] +
                    cpuLoad[i].cpu_ticks[CPU_STATE_SYSTEM]) /
                   static_cast<double>(cpuLoad[i].cpu_ticks[CPU_STATE_IDLE]);
    }
    
    // Update metrics
    pImpl->currentMetrics.performance_core_usage = perfUsage / appleConfig->performanceCores;
    pImpl->currentMetrics.efficiency_core_usage = effUsage / appleConfig->efficiencyCores;
}
#elif defined(CLOUD_PLATFORM_LINUX_X64)
void CoreKernel::optimizeForLinuxX64() {
    // Configure CPU affinity
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (size_t i = 0; i < linuxConfig->physicalCores; ++i) {
        CPU_SET(i, &cpuset);
    }
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    
    // Enable AVX2/AVX512
    // TODO: Implement AVX optimization
    
    // Configure CPU governor
    // TODO: Implement CPU governor configuration
}

void CoreKernel::configureLinuxX64() {
    // Get CPU information
    linuxConfig->physicalCores = sysconf(_SC_NPROCESSORS_ONLN);
    linuxConfig->logicalCores = sysconf(_SC_NPROCESSORS_CONF);
    
    // Check AVX support
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid(7, &eax, &ebx, &ecx, &edx)) {
        linuxConfig->enableAVX2 = (ebx & bit_AVX2) != 0;
        linuxConfig->enableAVX512 = (ebx & bit_AVX512F) != 0;
    }
    
    // Enable features
    linuxConfig->enablePowerManagement = true;
    linuxConfig->enableThermalManagement = true;
}

void CoreKernel::monitorLinuxX64Metrics() {
    // Monitor physical cores
    std::ifstream statFile("/proc/stat");
    std::string line;
    if (std::getline(statFile, line)) {
        std::istringstream iss(line);
        std::string cpu;
        uint64_t user, nice, system, idle, iowait, irq, softirq;
        iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;
        
        uint64_t total = user + nice + system + idle + iowait + irq + softirq;
        uint64_t idleTotal = idle + iowait;
        
        pImpl->currentMetrics.physical_core_usage = 1.0 - (static_cast<double>(idleTotal) / total);
    }
    
    // Monitor logical cores
    // TODO: Implement logical core monitoring
    
    // Monitor AVX usage
    // TODO: Implement AVX usage monitoring
}
#endif

} // namespace kernel
} // namespace core
} // namespace cloud

// Implementation of Impl methods
namespace cloud {
namespace core {
namespace kernel {

CoreKernel::Impl::Impl(const std::string& kernelId)
    : id(kernelId.empty() ? generateUniqueId() : kernelId)
    , paused(false)
    , highPerformanceMode(false)
    , lastOptimization(std::chrono::steady_clock::now())
    , running(false) {
    initializeMetrics();
}

std::string CoreKernel::Impl::generateUniqueId() {
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto value = now_ms.time_since_epoch().count();
    std::stringstream ss;
    ss << std::hex << value;
    return "kernel_" + ss.str();
}

void CoreKernel::Impl::initializeMetrics() {
    currentMetrics = PerformanceMetrics{
        .cpu_usage = 0.0,
        .memory_usage = 0.0,
        .power_consumption = 0.0,
        .temperature = 0.0,
        .instructions_per_second = 0,
        .timestamp = std::chrono::steady_clock::now()
    };
}

void CoreKernel::Impl::updateMetrics() {
    // Platform-specific metrics update
    #ifdef CLOUD_PLATFORM_APPLE_ARM
        updateAppleMetrics();
    #elif defined(CLOUD_PLATFORM_LINUX_X64)
        updateLinuxMetrics();
    #endif
    
    currentMetrics.timestamp = std::chrono::steady_clock::now();
}

#ifdef CLOUD_PLATFORM_APPLE_ARM
void CoreKernel::Impl::updateAppleMetrics() {
    // Get CPU usage
    processor_cpu_load_info_t cpuLoad;
    mach_msg_type_number_t count;
    host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &count,
                       reinterpret_cast<processor_info_t*>(&cpuLoad), &count);
    
    double totalUsage = 0.0;
    for (size_t i = 0; i < count; ++i) {
        totalUsage += (cpuLoad[i].cpu_ticks[CPU_STATE_USER] +
                      cpuLoad[i].cpu_ticks[CPU_STATE_SYSTEM]) /
                     static_cast<double>(cpuLoad[i].cpu_ticks[CPU_STATE_IDLE]);
    }
    currentMetrics.cpu_usage = totalUsage / count;
    
    // Get memory usage
    vm_size_t pageSize;
    host_page_size(mach_host_self(), &pageSize);
    
    vm_statistics64_data_t vmStats;
    mach_msg_type_number_t infoCount = sizeof(vmStats) / sizeof(natural_t);
    host_statistics64(mach_host_self(), HOST_VM_INFO64,
                     reinterpret_cast<host_info64_t>(&vmStats), &infoCount);
    
    uint64_t totalMemory = vmStats.free_count + vmStats.active_count +
                          vmStats.inactive_count + vmStats.wire_count;
    currentMetrics.memory_usage = 1.0 - (static_cast<double>(vmStats.free_count) / totalMemory);
    
    // Get power consumption (approximate)
    int power;
    size_t size = sizeof(power);
    if (sysctlbyname("machdep.cpu.power", &power, &size, nullptr, 0) == 0) {
        currentMetrics.power_consumption = power / 1000.0;
    }
    
    // Get temperature
    int temp;
    size = sizeof(temp);
    if (sysctlbyname("machdep.cpu.temperature", &temp, &size, nullptr, 0) == 0) {
        currentMetrics.temperature = temp / 100.0;
    }
}
#elif defined(CLOUD_PLATFORM_LINUX_X64)
void CoreKernel::Impl::updateLinuxMetrics() {
    // Get CPU usage from /proc/stat
    std::ifstream statFile("/proc/stat");
    std::string line;
    if (std::getline(statFile, line)) {
        std::istringstream iss(line);
        std::string cpu;
        uint64_t user, nice, system, idle, iowait, irq, softirq;
        iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;
        
        uint64_t total = user + nice + system + idle + iowait + irq + softirq;
        uint64_t idleTotal = idle + iowait;
        
        currentMetrics.cpu_usage = 1.0 - (static_cast<double>(idleTotal) / total);
    }
    
    // Get memory usage from /proc/meminfo
    std::ifstream memFile("/proc/meminfo");
    uint64_t totalMem = 0, freeMem = 0;
    while (std::getline(memFile, line)) {
        std::istringstream iss(line);
        std::string key;
        uint64_t value;
        iss >> key >> value;
        
        if (key == "MemTotal:") totalMem = value;
        else if (key == "MemFree:") freeMem = value;
    }
    
    if (totalMem > 0) {
        currentMetrics.memory_usage = 1.0 - (static_cast<double>(freeMem) / totalMem);
    }
    
    // Get power consumption (if available)
    std::ifstream powerFile("/sys/class/power_supply/BAT0/power_now");
    if (powerFile) {
        int power;
        powerFile >> power;
        currentMetrics.power_consumption = power / 1000000.0;
    }
    
    // Get temperature (if available)
    std::ifstream tempFile("/sys/class/thermal/thermal_zone0/temp");
    if (tempFile) {
        int temp;
        tempFile >> temp;
        currentMetrics.temperature = temp / 1000.0;
    }
}
#endif

} // namespace kernel
} // namespace core
} // namespace cloud
