#include "core/kernel/ParentKernel.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>
#include "core/balancer/LoadBalancer.hpp"
#include "core/balancer/EnergyController.hpp"
#include "core/balancer/TaskOrchestrator.hpp"
#include "core/cache/DynamicCache.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/cache/PlatformOptimizer.hpp"
#include "core/balancer/TaskTypes.hpp"

namespace cloud {
namespace core {
namespace kernel {

ParentKernel::ParentKernel() = default;
ParentKernel::~ParentKernel() {
    shutdown();
}

bool ParentKernel::initialize() {
    spdlog::info("ParentKernel: инициализация");
    energyController->initialize();
    loadBalancer->setStrategy("round_robin");
    platformOptimizer = std::make_unique<core::cache::PlatformOptimizer>();
    auto cacheConfig = platformOptimizer->getOptimalConfig();
    dynamicCache = std::make_unique<core::DefaultDynamicCache>(cacheConfig.initialSize);
    auto threadPoolConfig = platformOptimizer->getThreadPoolConfig();
    threadPool = std::make_shared<core::thread::ThreadPool>(threadPoolConfig);
    for (auto& child : children) {
        if (!child->initialize()) {
            spdlog::error("ParentKernel: ошибка инициализации дочернего ядра");
            return false;
        }
    }
    return true;
}

void ParentKernel::shutdown() {
    spdlog::info("ParentKernel: завершение работы");
    if (energyController) {
        energyController->shutdown();
        energyController.reset();
    }
    if (orchestrationKernel) {
        orchestrationKernel->shutdown();
        orchestrationKernel.reset();
    }
    if (recoveryManager) {
        recoveryManager->shutdown();
        recoveryManager.reset();
    }
    if (threadPool) {
        threadPool->waitForCompletion();
        threadPool.reset();
    }
    if (loadBalancer) {
        loadBalancer.reset();
    }
    if (platformOptimizer) {
        platformOptimizer.reset();
    }
    if (dynamicCache) {
        dynamicCache->clear();
        dynamicCache.reset();
    }
    for (auto& child : children) {
        if (child) child->shutdown();
    }
    children.clear();
}

void ParentKernel::addChild(std::shared_ptr<IKernel> child) {
    if (child) {
        children.push_back(child);
        spdlog::info("ParentKernel: добавлено дочернее ядро");
    }
}

void ParentKernel::removeChild(const std::string& id) {
    auto it = std::remove_if(children.begin(), children.end(), [&](const std::shared_ptr<IKernel>& k) {
        return k->getId() == id;
    });
    if (it != children.end()) {
        children.erase(it, children.end());
        spdlog::info("ParentKernel: удалено дочернее ядро");
    }
}

void ParentKernel::balanceLoad() {
    if (loadBalancer) {
        loadBalancer->balance(children);
        spdlog::debug("ParentKernel: балансировка нагрузки");
    }
}

void ParentKernel::orchestrateTasks() {
    if (orchestrationKernel) {
        orchestrationKernel->orchestrate(getChildren());
        spdlog::debug("ParentKernel: вызвана оркестрация задач для всех дочерних ядер");
    }
    if (taskOrchestrator) {
        std::vector<uint8_t> task;
        if (taskOrchestrator->dequeueTask(task)) {
            dynamicCache->put("last_task", task);
        }
    }
}

void ParentKernel::updateMetrics() {
    // Агрегируем метрики всех дочерних ядер
    double totalLoad = 0.0, totalEfficiency = 0.0;
    size_t count = 0;
    for (auto& child : children) {
        child->updateMetrics();
        auto m = child->getMetrics();
        totalLoad += m.cpu_usage;
        totalEfficiency += m.efficiencyScore;
        ++count;
    }
    energyController->updateMetrics();
    if (recoveryManager) recoveryManager->updateMetrics();
    // Адаптация thread pool и кэша
    if (threadPool && count > 0) {
        auto config = threadPool->getConfiguration();
        double avgLoad = totalLoad / count;
        if (avgLoad > 0.8 && config.maxThreads < 32) {
            config.maxThreads += 2;
            threadPool->setConfiguration(config);
            spdlog::info("ParentKernel: увеличено число потоков до {} (avgLoad={})", config.maxThreads, avgLoad);
        } else if (avgLoad < 0.3 && config.maxThreads > 2) {
            config.maxThreads -= 1;
            threadPool->setConfiguration(config);
            spdlog::info("ParentKernel: уменьшено число потоков до {} (avgLoad={})", config.maxThreads, avgLoad);
        }
    }
    if (dynamicCache && count > 0) {
        auto cacheMetrics = dynamicCache->getMetrics();
        if (cacheMetrics.hitRate < 0.8) {
            dynamicCache->resize(dynamicCache->allocatedSize() * 1.2);
            spdlog::info("ParentKernel: увеличен размер кэша до {}", dynamicCache->allocatedSize());
        } else if (cacheMetrics.hitRate > 0.95 && dynamicCache->allocatedSize() > 16) {
            dynamicCache->resize(dynamicCache->allocatedSize() * 0.8);
            spdlog::info("ParentKernel: уменьшен размер кэша до {}", dynamicCache->allocatedSize());
        }
    }
    // Агрегируем и кэшируем метрики
    auto json = getMetrics().toJson();
    spdlog::debug("ParentKernel metrics: {}", json.dump());
    std::vector<uint8_t> metricsData(json.dump().begin(), json.dump().end());
    dynamicCache->put("metrics", metricsData);
}

std::vector<std::shared_ptr<IKernel>> ParentKernel::getChildren() const {
    return children;
}

bool ParentKernel::isRunning() const { return true; }
cloud::core::PerformanceMetrics ParentKernel::getMetrics() const { return {}; }
void ParentKernel::setResourceLimit(const std::string& resource, double limit) {
    if (resource == "threads" && threadPool) {
        auto config = threadPool->getConfiguration();
        config.maxThreads = static_cast<size_t>(limit);
        threadPool->setConfiguration(config);
        spdlog::info("ParentKernel: установлен лимит потоков {}", limit);
    } else if (resource == "cache" && dynamicCache) {
        dynamicCache->resize(static_cast<size_t>(limit));
        spdlog::info("ParentKernel: установлен лимит кэша {}", limit);
    } else {
        spdlog::warn("ParentKernel: неизвестный ресурс '{}'", resource);
    }
}

double ParentKernel::getResourceUsage(const std::string& resource) const {
    if (resource == "threads" && threadPool) {
        return threadPool->getMetrics().activeThreads;
    } else if (resource == "cache" && dynamicCache) {
        return dynamicCache->allocatedSize();
    } else {
        spdlog::warn("ParentKernel: неизвестный ресурс '{}'", resource);
        return 0.0;
    }
}

cloud::core::kernel::KernelType ParentKernel::getType() const { return cloud::core::kernel::KernelType::PARENT; }
std::string ParentKernel::getId() const { return "parent_kernel"; }
void ParentKernel::pause() {}
void ParentKernel::resume() {}
void ParentKernel::reset() {}
std::vector<std::string> ParentKernel::getSupportedFeatures() const {
    return {"dynamic_thread_pool", "dynamic_cache", "energy_management", "task_orchestration"};
}

} // namespace kernel
} // namespace core
} // namespace cloud
