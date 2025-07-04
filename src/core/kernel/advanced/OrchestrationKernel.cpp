#include "core/kernel/advanced/OrchestrationKernel.hpp"
#include <spdlog/spdlog.h>
#include "core/cache/dynamic/DynamicCache.hpp"
#include <chrono>
#include "core/cache/dynamic/PlatformOptimizer.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/balancer/TaskTypes.hpp"

namespace cloud {
namespace core {
namespace kernel {

OrchestrationKernel::OrchestrationKernel() {
    platformOptimizer = std::make_unique<core::cache::PlatformOptimizer>();
    auto cacheConfig = platformOptimizer->getOptimalConfig();
    dynamicCache = std::make_unique<core::DefaultDynamicCache>(cacheConfig.initialSize);
    auto threadPoolConfig = platformOptimizer->getThreadPoolConfig();
    threadPool = std::make_shared<core::thread::ThreadPool>(threadPoolConfig);
}
OrchestrationKernel::~OrchestrationKernel() = default;

bool OrchestrationKernel::initialize() {
    spdlog::info("OrchestrationKernel: инициализация");
    return true;
}

void OrchestrationKernel::shutdown() {
    spdlog::info("OrchestrationKernel: завершение работы");
    taskQueue.clear();
    if (dynamicCache) dynamicCache->clear();
}

void OrchestrationKernel::enqueueTask(const std::vector<uint8_t>& data, int priority) {
    recoveryManager->createRecoveryPoint("before_enqueue", data);
    taskQueue.push_back(data);
    balancer::TaskDescriptor desc;
    desc.data = data;
    desc.priority = priority;
    desc.enqueueTime = std::chrono::steady_clock::now();
    taskDescriptors.push_back(desc);
    if (dynamicCache) dynamicCache->put("last_enqueued_task", data);
    spdlog::debug("OrchestrationKernel: задача добавлена в очередь с приоритетом {}", priority);
}

void OrchestrationKernel::balanceTasks() {
    // Получаем реальные метрики ядер
    std::vector<std::shared_ptr<IKernel>> kernels = getActiveKernels(); // реализовать получение
    auto metrics = getKernelMetrics(kernels);
    // Сортируем задачи по приоритету
    std::sort(taskDescriptors.begin(), taskDescriptors.end(), [](const auto& a, const auto& b) {
        return a.priority > b.priority;
    });
    // Балансировка: задачи идут на менее загруженные ядра
    for (auto& task : taskDescriptors) {
        auto minIt = std::min_element(metrics.begin(), metrics.end(), [](const auto& a, const auto& b) {
            return a.load < b.load;
        });
        size_t idx = std::distance(metrics.begin(), minIt);
        if (idx < kernels.size()) {
            // Передаем задачу ядру (реализовать enqueueTask у IKernel или через каст)
            // kernels[idx]->enqueueTask(task.data, task.priority); // если реализовано
            spdlog::info("OrchestrationKernel: задача с приоритетом {} отправлена ядру {} (load={})", task.priority, kernels[idx]->getId(), minIt->load);
        }
        minIt->load += 0.1; // эмулируем рост загрузки
    }
    taskDescriptors.clear();
    spdlog::info("OrchestrationKernel: балансировка завершена");
}

void OrchestrationKernel::accelerateTunnels() {
    if (tunnelManager) {
        // Здесь можно реализовать ускорение передачи данных через туннели
        spdlog::debug("OrchestrationKernel: ускорение туннелей");
    }
}

void OrchestrationKernel::updateMetrics() {
    auto json = getMetrics().toJson();
    spdlog::debug("OrchestrationKernel metrics: {}", json.dump());
}

std::vector<balancer::KernelMetrics> OrchestrationKernel::getKernelMetrics(const std::vector<std::shared_ptr<IKernel>>& kernels) const {
    std::vector<balancer::KernelMetrics> metrics;
    for (const auto& k : kernels) {
        auto m = k->getMetrics();
        balancer::KernelMetrics km;
        km.load = m.cpu_usage;
        km.latency = m.memory_usage; // для примера, можно заменить на реальную задержку
        km.cacheEfficiency = 1.0 - m.memory_usage; // для примера
        // Получаем реальные значения tunnelBandwidth и activeTasks
        km.tunnelBandwidth = tunnelManager ? tunnelManager->getBandwidth(k->getId()) : 0.0;
        km.activeTasks = threadPool ? threadPool->getMetrics().queueSize : 0;
        metrics.push_back(km);
    }
    return metrics;
}

void OrchestrationKernel::orchestrate(const std::vector<std::shared_ptr<IKernel>>& kernels) {
    if (!loadBalancer) return;
    auto metrics = getKernelMetrics(kernels);
    loadBalancer->balance(kernels, taskDescriptors, metrics);
    taskDescriptors.clear();
    spdlog::info("OrchestrationKernel: оркестрация задач завершена");
}

bool OrchestrationKernel::isRunning() const { return true; }
cloud::core::PerformanceMetrics OrchestrationKernel::getMetrics() const { return {}; }
void OrchestrationKernel::setResourceLimit(const std::string&, double) {}
double OrchestrationKernel::getResourceUsage(const std::string&) const { return 0.0; }
cloud::core::kernel::KernelType OrchestrationKernel::getType() const { return cloud::core::kernel::KernelType::ORCHESTRATION; }
std::string OrchestrationKernel::getId() const { return "orchestration_kernel"; }
void OrchestrationKernel::pause() {}
void OrchestrationKernel::resume() {}
void OrchestrationKernel::reset() {}
std::vector<std::string> OrchestrationKernel::getSupportedFeatures() const { return {}; }

} // namespace kernel
} // namespace core
} // namespace cloud
