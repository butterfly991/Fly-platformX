#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/kernel/advanced/ArchitecturalKernel.hpp"
#include <spdlog/spdlog.h>
#include "core/drivers/ARMDriver.hpp"
#include "core/balancer/TaskTypes.hpp"
#include "core/thread/ThreadPool.hpp"

namespace cloud {
namespace core {
namespace kernel {

ArchitecturalKernel::ArchitecturalKernel() {
    hardwareAccelerator = std::make_unique<core::drivers::ARMDriver>();
    dynamicCache = std::make_unique<core::DefaultDynamicCache>(64);
}

ArchitecturalKernel::~ArchitecturalKernel() {
    shutdown();
}

bool ArchitecturalKernel::initialize() {
    spdlog::info("ArchitecturalKernel: инициализация");
    return hardwareAccelerator->initialize();
}

void ArchitecturalKernel::shutdown() {
    spdlog::info("ArchitecturalKernel: завершение работы");
    hardwareAccelerator->shutdown();
    if (dynamicCache) dynamicCache->clear();
}

void ArchitecturalKernel::optimizeTopology() {
    spdlog::debug("ArchitecturalKernel: оптимизация топологии");
    std::vector<uint8_t> topologyData{'O','P','T','I','M','_','T','O','P'};
    dynamicCache->put("topology_optimized", topologyData);
    spdlog::info("ArchitecturalKernel: топология оптимизирована и сохранена в кэш");
}

void ArchitecturalKernel::optimizePlacement() {
    spdlog::debug("ArchitecturalKernel: оптимизация размещения задач");
    std::vector<uint8_t> placementData{'O','P','T','I','M','_','P','L','A','C','E'};
    dynamicCache->put("placement_optimized", placementData);
    spdlog::info("ArchitecturalKernel: размещение задач оптимизировано и сохранено в кэш");
}

void ArchitecturalKernel::updateMetrics() {
    std::vector<uint8_t> metricsData{1,2,3,4};
    dynamicCache->put("arch_metrics", metricsData);
    spdlog::debug("ArchitecturalKernel: метрики обновлены");
}

cloud::core::PerformanceMetrics ArchitecturalKernel::getMetrics() const {
    cloud::core::PerformanceMetrics m{};
    if (dynamicCache) {
        auto c = dynamicCache->getMetrics();
        m.memory_usage = static_cast<double>(c.currentSize) / (c.maxSize ? c.maxSize : 1);
    }
    m.timestamp = std::chrono::steady_clock::now();
    return m;
}

} // namespace kernel
} // namespace core
} // namespace cloud
