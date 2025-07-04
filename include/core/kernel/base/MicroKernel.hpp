#pragma once
#include <string>
#include <vector>
#include <memory>
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/cache/dynamic/PlatformOptimizer.hpp"
#include "core/balancer/LoadBalancer.hpp"
#include "core/cache/dynamic/PlatformOptimizer.hpp"
#include "core/cache/experimental/PreloadManager.hpp"
#include "core/balancer/TaskTypes.hpp"

namespace cloud {
namespace core {
namespace kernel {

// Микроядро — минимальный набор функций, может быть криптографическим или сервисным
class MicroKernel : public IKernel {
public:
    explicit MicroKernel(const std::string& id);
    ~MicroKernel() override;

    bool initialize() override;
    void shutdown() override;

    // Выполнение задачи
    virtual bool executeTask(const std::vector<uint8_t>& data);

    // Обновление метрик
    void updateMetrics() override;

    bool isRunning() const override;
    metrics::PerformanceMetrics getMetrics() const override;
    void setResourceLimit(const std::string& resource, double limit) override;
    double getResourceUsage(const std::string& resource) const override;
    KernelType getType() const override;
    std::string getId() const override;
    void pause() override;
    void resume() override;
    void reset() override;
    std::vector<std::string> getSupportedFeatures() const override;

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
    void scheduleTask(std::function<void()> task, int priority) override;
    
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
    
    /**
     * @brief Вызвать event
     */
    void triggerEvent(const std::string& event, const std::any& data);

protected:
    std::string id;
    std::unique_ptr<core::cache::DefaultDynamicCache> dynamicCache;
    std::shared_ptr<cloud::core::thread::ThreadPool> threadPool;
    std::unique_ptr<core::recovery::RecoveryManager> recoveryManager;
    std::unique_ptr<core::cache::PlatformOptimizer> platformOptimizer;
    
    // Интеграция с новой балансировкой
    std::shared_ptr<core::PreloadManager> preloadManager_;
    std::shared_ptr<cloud::core::balancer::LoadBalancer> loadBalancer_;
    TaskCallback taskCallback_;
    std::unordered_map<std::string, EventCallback> eventCallbacks_;
    ExtendedKernelMetrics extendedMetrics_;
    mutable std::shared_mutex kernelMutex_;
    
    // Вспомогательные методы
    void initializePreloadManager();
    void initializeLoadBalancer();
    void updateExtendedMetricsFromPerformance();
    void notifyEvent(const std::string& event, const std::any& data);
};

} // namespace kernel
} // namespace core
} // namespace cloud
