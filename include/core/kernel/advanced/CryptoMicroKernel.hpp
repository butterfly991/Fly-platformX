#pragma once
#include <string>
#include <vector>
#include <memory>
#include "core/kernel/base/CoreKernel.hpp"
#include "core/drivers/ARMDriver.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/cache/PlatformOptimizer.hpp"

namespace cloud {
namespace core {
namespace kernel {

// Криптографическое микроядро — специализировано для криптографических операций
class CryptoMicroKernel : public IKernel {
public:
    explicit CryptoMicroKernel(const std::string& id);
    ~CryptoMicroKernel() override;

    bool initialize() override;
    void shutdown() override;

    // Выполнение криптографической задачи
    bool executeCryptoTask(const std::vector<uint8_t>& data, std::vector<uint8_t>& result);

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
    std::string id;
    std::unique_ptr<core::drivers::ARMDriver> armDriver;
    std::unique_ptr<core::DefaultDynamicCache> dynamicCache;
    std::shared_ptr<core::thread::ThreadPool> threadPool;
    std::unique_ptr<core::recovery::RecoveryManager> recoveryManager;
    std::unique_ptr<core::cache::PlatformOptimizer> platformOptimizer;
};

} // namespace kernel
} // namespace core
} // namespace cloud 