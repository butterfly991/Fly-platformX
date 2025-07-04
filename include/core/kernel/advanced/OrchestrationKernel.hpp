#pragma once
#include <memory>
#include <vector>
#include "core/balancer/LoadBalancer.hpp"
// #include "core/kernel/TunnelManager.hpp"
#include "core/cache/base/BaseCache.hpp"
#include "core/balancer/LoadBalancer.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/cache/dynamic/PlatformOptimizer.hpp"
#include "core/balancer/TaskTypes.hpp"

namespace cloud {
namespace core {
namespace kernel {

// Ядро-оркестратор — управляет очередями задач, распределяет задачи между ядрами
class OrchestrationKernel : public IKernel {
public:
    OrchestrationKernel();
    ~OrchestrationKernel() override;

    bool initialize() override;
    void shutdown() override;

    // Добавление задачи в очередь
    void enqueueTask(const std::vector<uint8_t>& data, int priority = 5);

    // Балансировка и ускорение
    void balanceTasks();
    void accelerateTunnels();

    // Новый метод: оркестрация задач с учётом приоритета и метрик
    void orchestrate(const std::vector<std::shared_ptr<IKernel>>& kernels);

    void updateMetrics() override;

    bool isRunning() const override;
    PerformanceMetrics getMetrics() const override;
    void setResourceLimit(const std::string& resource, double limit) override;
    double getResourceUsage(const std::string& resource) const override;
    KernelType getType() const override;
    std::string getId() const override;
    void pause() override;
    void resume() override;
    void reset() override;
    std::vector<std::string> getSupportedFeatures() const override;

private:
    std::unique_ptr<cloud::core::balancer::LoadBalancer> loadBalancer;
    // std::unique_ptr<TunnelManager> tunnelManager;
    std::vector<std::vector<uint8_t>> taskQueue;
    std::vector<balancer::TaskDescriptor> taskDescriptors;
    std::unique_ptr<core::DefaultDynamicCache> dynamicCache;
    std::shared_ptr<core::thread::ThreadPool> threadPool;
    std::unique_ptr<core::recovery::RecoveryManager> recoveryManager;
    std::unique_ptr<core::cache::PlatformOptimizer> platformOptimizer;
    std::vector<balancer::KernelMetrics> getKernelMetrics(const std::vector<std::shared_ptr<IKernel>>& kernels) const;
};

} // namespace kernel
} // namespace core
} // namespace cloud
