#include "core/kernel/advanced/ComputationalKernel.hpp"
#include <spdlog/spdlog.h>
#include "core/balancer/EnergyController.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/drivers/ARMDriver.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/cache/PlatformOptimizer.hpp"
#include "core/balancer/TaskTypes.hpp"

namespace cloud {
namespace core {
namespace kernel {

ComputationalKernel::ComputationalKernel() {
    hardwareAccelerator = std::make_unique<core::drivers::ARMDriver>();
    platformOptimizer = std::make_unique<core::cache::PlatformOptimizer>();
    auto cacheConfig = platformOptimizer->getOptimalConfig();
    dynamicCache = std::make_unique<core::DefaultDynamicCache>(cacheConfig.initialSize);
    auto threadPoolConfig = platformOptimizer->getThreadPoolConfig();
    threadPool = std::make_shared<core::thread::ThreadPool>(threadPoolConfig);
    recoveryManager = std::make_unique<core::recovery::RecoveryManager>();
}

ComputationalKernel::~ComputationalKernel() {
    shutdown();
}

bool ComputationalKernel::initialize() {
    spdlog::info("ComputationalKernel: инициализация");
    bool accel = hardwareAccelerator->initialize();
    dynamicCache->resize(256);
    return accel;
}

void ComputationalKernel::shutdown() {
    spdlog::info("ComputationalKernel: завершение работы");
    hardwareAccelerator->shutdown();
    dynamicCache->clear();
}

bool ComputationalKernel::compute(const std::vector<uint8_t>& data) {
    spdlog::debug("ComputationalKernel: выполнение вычислений");
    recoveryManager->createRecoveryPoint("before_compute", data);
    std::vector<uint8_t> result;
    if (hardwareAccelerator->accelerate(data, result)) {
        dynamicCache->put("compute", result);
        return true;
    }
    return false;
}

void ComputationalKernel::updateMetrics() {
    auto json = getMetrics().toJson();
    spdlog::debug("ComputationalKernel metrics: {}", json.dump());
}

cloud::core::PerformanceMetrics ComputationalKernel::getMetrics() const {
    cloud::core::PerformanceMetrics m{};
    if (threadPool) {
        auto t = threadPool->getMetrics();
        m.cpu_usage = static_cast<double>(t.activeThreads) / t.totalThreads;
    }
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
