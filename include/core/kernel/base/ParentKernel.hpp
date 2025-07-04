#pragma once
#include <vector>
#include <memory>
#include <string>
#include "core/kernel/base/MicroKernel.hpp"
#include "core/balancer/LoadBalancer.hpp"
#include "core/balancer/EnergyController.hpp"
#include "core/cache/DefaultDynamicCache.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/cache/PlatformOptimizer.hpp"
#include "core/balancer/TaskTypes.hpp"

namespace cloud {
namespace core {
namespace kernel {

// Родительское ядро — управляет сетью дочерних ядер и оркестрацией задач
class ParentKernel : public IKernel {
public:
    ParentKernel();
    ~ParentKernel() override;

    // Инициализация и завершение работы ядра
    bool initialize() override;
    void shutdown() override;

    // Добавление/удаление дочерних ядер
    void addChild(std::shared_ptr<IKernel> child);
    void removeChild(const std::string& id);

    // Балансировка и оркестрация
    void balanceLoad();
    void orchestrateTasks();

    // Обновление метрик
    void updateMetrics() override;

    // Новый метод для получения списка дочерних ядер
    std::vector<std::shared_ptr<IKernel>> getChildren() const;

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

private:
    std::vector<std::shared_ptr<IKernel>> children; // Список дочерних ядер
    std::unique_ptr<LoadBalancer> loadBalancer;     // Балансировщик нагрузки
    std::unique_ptr<EnergyController> energyController; // Энергоадаптивный контроллер
    std::unique_ptr<OrchestrationKernel> orchestrationKernel; // Оркестратор задач
    std::unique_ptr<core::DefaultDynamicCache> dynamicCache; // Динамический кэш для агрегированных метрик и состояния дочерних ядер
    std::shared_ptr<core::thread::ThreadPool> threadPool;
    std::unique_ptr<core::recovery::RecoveryManager> recoveryManager;
    std::unique_ptr<core::cache::PlatformOptimizer> platformOptimizer;
};

} // namespace kernel
} // namespace core
} // namespace cloud
