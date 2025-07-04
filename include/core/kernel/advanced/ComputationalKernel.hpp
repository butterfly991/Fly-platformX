#pragma once
#include <memory>
#include <vector>
#include "core/kernel/base/CoreKernel.hpp"
#include "core/drivers/ARMDriver.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/cache/PlatformOptimizer.hpp"

namespace cloud {
namespace core {
namespace kernel {

// Вычислительное ядро — оптимизировано для вычислительных задач, использует аппаратное ускорение
class ComputationalKernel : public IKernel {
public:
    ComputationalKernel();
    ~ComputationalKernel() override;

    bool initialize() override;
    void shutdown() override;

    // Выполнение вычислительной задачи
    bool compute(const std::vector<uint8_t>& data);

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
    std::unique_ptr<core::DefaultDynamicCache> dynamicCache;
    std::shared_ptr<core::thread::ThreadPool> threadPool;
    std::unique_ptr<core::recovery::RecoveryManager> recoveryManager;
    std::unique_ptr<core::cache::PlatformOptimizer> platformOptimizer;
    std::unique_ptr<core::drivers::ARMDriver> hardwareAccelerator;
};

} // namespace kernel
} // namespace core
} // namespace cloud
