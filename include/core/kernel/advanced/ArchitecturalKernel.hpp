#pragma once
#include <memory>
#include "core/kernel/base/CoreKernel.hpp"
#include "core/drivers/ARMDriver.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/thread/ThreadPool.hpp"

namespace cloud {
namespace core {
namespace kernel {

// Архитектурное ядро — отвечает за топологию, оптимизацию размещения задач, взаимодействие между ядрами
class ArchitecturalKernel : public IKernel {
public:
    ArchitecturalKernel();
    ~ArchitecturalKernel() override;

    bool initialize() override;
    void shutdown() override;

    // Оптимизация архитектуры
    void optimizeTopology();
    void optimizePlacement();

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
    std::unique_ptr<core::drivers::ARMDriver> hardwareAccelerator;
    std::unique_ptr<core::DefaultDynamicCache> dynamicCache; // Динамический кэш для архитектурных оптимизаций и промежуточных данных
};

} // namespace kernel
} // namespace core
} // namespace cloud
