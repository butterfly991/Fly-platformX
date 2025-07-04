#include "core/kernel/advanced/CryptoMicroKernel.hpp"
#include <spdlog/spdlog.h>
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/cache/PlatformOptimizer.hpp"
#include "core/drivers/ARMDriver.hpp"
#include "core/balancer/TaskTypes.hpp"

namespace cloud {
namespace core {
namespace kernel {

CryptoMicroKernel::CryptoMicroKernel(const std::string& id_) : id(id_) {
    armDriver = std::make_unique<core::drivers::ARMDriver>();
    platformOptimizer = std::make_unique<core::cache::PlatformOptimizer>();
    auto cacheConfig = platformOptimizer->getOptimalConfig();
    dynamicCache = std::make_unique<core::DefaultDynamicCache>(cacheConfig.initialSize);
    auto threadPoolConfig = platformOptimizer->getThreadPoolConfig();
    threadPool = std::make_shared<core::thread::ThreadPool>(threadPoolConfig);
    recoveryManager = std::make_unique<core::recovery::RecoveryManager>();
}

CryptoMicroKernel::~CryptoMicroKernel() {
    shutdown();
}

bool CryptoMicroKernel::initialize() {
    spdlog::info("CryptoMicroKernel[{}]: инициализация", id);
    bool accel = armDriver->initialize();
    return accel;
}

void CryptoMicroKernel::shutdown() {
    spdlog::info("CryptoMicroKernel[{}]: завершение работы", id);
    armDriver->shutdown();
    if (dynamicCache) dynamicCache->clear();
}

bool CryptoMicroKernel::executeCryptoTask(const std::vector<uint8_t>& data, std::vector<uint8_t>& result) {
    spdlog::debug("CryptoMicroKernel[{}]: выполнение криптографической задачи", id);
    recoveryManager->createRecoveryPoint("before_crypto", data);
    if (armDriver->accelerate(data, result)) {
        dynamicCache->put("crypto", result);
        return true;
    }
    return false;
}

void CryptoMicroKernel::updateMetrics() {
    auto json = getMetrics().toJson();
    spdlog::debug("CryptoMicroKernel metrics: {}", json.dump());
}

std::string CryptoMicroKernel::getId() const {
    return id;
}

cloud::core::PerformanceMetrics CryptoMicroKernel::getMetrics() const {
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