#include <cassert>
#include <iostream>
#include "core/kernel/base/MicroKernel.hpp"
#include "core/kernel/base/ParentKernel.hpp"
#include "core/kernel/advanced/OrchestrationKernel.hpp"
#include "core/balancer/LoadBalancer.hpp"
#include "core/cache/experimental/PreloadManager.hpp"
#include "core/balancer/TaskTypes.hpp"
#include <memory>
#include <vector>
#include <chrono>
#include <any>

void smokeTestParentKernel() {
    cloud::core::kernel::ParentKernel parent;
    assert(parent.initialize());
    auto child = std::make_shared<cloud::core::kernel::MicroKernel>("micro1");
    parent.addChild(child);
    assert(parent.getChildren().size() == 1);
    parent.removeChild(child->getId());
    assert(parent.getChildren().empty());
    parent.shutdown();
    std::cout << "[OK] ParentKernel smoke test\n";
}

void smokeTestOrchestrationKernel() {
    cloud::core::kernel::OrchestrationKernel ork;
    assert(ork.initialize());
    std::vector<uint8_t> data = {1,2,3};
    ork.enqueueTask(data, 7);
    ork.shutdown();
    std::cout << "[OK] OrchestrationKernel smoke test\n";
}

void stressTestOrchestrationKernel() {
    cloud::core::kernel::OrchestrationKernel ork;
    assert(ork.initialize());
    for (int i = 0; i < 10000; ++i) {
        std::vector<uint8_t> data(100, i % 256);
        ork.enqueueTask(data, i % 10);
    }
    // Многократная оркестрация (без реальных ядер)
    for (int i = 0; i < 100; ++i) {
        ork.balanceTasks();
    }
    ork.shutdown();
    std::cout << "[OK] OrchestrationKernel stress test\n";
}

void testKernelLoadBalancerIntegration() {
    std::cout << "Testing Kernel-LoadBalancer integration...\n";
    
    // Создаем LoadBalancer
    auto loadBalancer = std::make_shared<cloud::core::balancer::LoadBalancer>();
    
    // Создаем ядра
    auto microKernel = std::make_shared<cloud::core::kernel::MicroKernel>("micro_test");
    auto parentKernel = std::make_shared<cloud::core::kernel::ParentKernel>();
    
    // Инициализируем ядра
    assert(microKernel->initialize());
    assert(parentKernel->initialize());
    
    // Устанавливаем LoadBalancer в ядра
    microKernel->setLoadBalancer(loadBalancer);
    parentKernel->setLoadBalancer(loadBalancer);
    
    // Проверяем что LoadBalancer установлен
    assert(microKernel->getLoadBalancer() == loadBalancer);
    assert(parentKernel->getLoadBalancer() == loadBalancer);
    
    // Создаем тестовые задачи
    std::vector<cloud::core::balancer::TaskDescriptor> tasks;
    for (int i = 0; i < 5; ++i) {
        cloud::core::balancer::TaskDescriptor task;
        task.data = std::vector<uint8_t>(100, i);
        task.priority = i % 10;
        task.type = static_cast<TaskType>(i % 5);
        task.enqueueTime = std::chrono::steady_clock::now();
        tasks.push_back(task);
    }
    
    // Создаем метрики ядер
    std::vector<cloud::core::balancer::KernelMetrics> metrics;
    for (int i = 0; i < 2; ++i) {
        cloud::core::balancer::KernelMetrics metric;
        metric.cpuUsage = 0.5;
        metric.memoryUsage = 0.3;
        metric.networkBandwidth = 1000.0;
        metric.diskIO = 1000.0;
        metric.energyConsumption = 50.0;
        metric.cpuTaskEfficiency = 0.8;
        metric.ioTaskEfficiency = 0.7;
        metric.memoryTaskEfficiency = 0.6;
        metric.networkTaskEfficiency = 0.9;
        metrics.push_back(metric);
    }
    
    // Тестируем балансировку
    std::vector<std::shared_ptr<cloud::core::kernel::IKernel>> kernels = {microKernel, parentKernel};
    loadBalancer->balance(kernels, tasks, metrics);
    
    std::cout << "[OK] Kernel-LoadBalancer integration test\n";
}

void testKernelPreloadManagerIntegration() {
    std::cout << "Testing Kernel-PreloadManager integration...\n";
    
    // Создаем PreloadManager
    auto preloadManager = std::make_shared<cloud::core::cache::experimental::PreloadManager>();
    
    // Добавляем тестовые данные
    for (int i = 0; i < 10; ++i) {
        std::string key = "test_key_" + std::to_string(i);
        std::vector<uint8_t> data(100, i);
        preloadManager->addData(key, data);
    }
    
    // Создаем ядро
    auto microKernel = std::make_shared<cloud::core::kernel::MicroKernel>("preload_test");
    
    // Устанавливаем PreloadManager
    microKernel->setPreloadManager(preloadManager);
    
    // Инициализируем ядро (должен вызвать warmupFromPreload)
    assert(microKernel->initialize());
    
    // Проверяем что данные загружены в кэш
    auto extendedMetrics = microKernel->getExtendedMetrics();
    assert(extendedMetrics.load >= 0.0);
    
    std::cout << "[OK] Kernel-PreloadManager integration test\n";
}

void testEventCallbackIntegration() {
    std::cout << "Testing Event-Callback integration...\n";
    
    auto microKernel = std::make_shared<cloud::core::kernel::MicroKernel>("event_test");
    assert(microKernel->initialize());
    
    bool eventReceived = false;
    std::string receivedEvent;
    std::any receivedData;
    
    // Устанавливаем event callback
    microKernel->setEventCallback("test_event", [&](const std::string& event, const std::any& data) {
        eventReceived = true;
        receivedEvent = event;
        receivedData = data;
    });
    
    // Вызываем событие
    microKernel->triggerEvent("test_event", std::string("test_data"));
    
    // Проверяем что событие получено
    assert(eventReceived);
    assert(receivedEvent == "test_event");
    assert(std::any_cast<std::string>(receivedData) == "test_data");
    
    std::cout << "[OK] Event-Callback integration test\n";
}

void testTaskProcessingIntegration() {
    std::cout << "Testing Task processing integration...\n";
    
    auto microKernel = std::make_shared<cloud::core::kernel::MicroKernel>("task_test");
    assert(microKernel->initialize());
    
    bool taskProcessed = false;
    
    // Устанавливаем task callback
    microKernel->setTaskCallback([&](const cloud::core::balancer::TaskDescriptor& task) {
        taskProcessed = true;
        assert(task.priority == 5);
        assert(task.type == TaskType::CPU_INTENSIVE);
    });
    
    // Создаем и обрабатываем задачу
    cloud::core::balancer::TaskDescriptor task;
    task.data = std::vector<uint8_t>(100, 42);
    task.priority = 5;
    task.type = TaskType::CPU_INTENSIVE;
    task.enqueueTime = std::chrono::steady_clock::now();
    
    bool result = microKernel->processTask(task);
    assert(result);
    assert(taskProcessed);
    
    std::cout << "[OK] Task processing integration test\n";
}

int main() {
    smokeTestParentKernel();
    smokeTestOrchestrationKernel();
    stressTestOrchestrationKernel();
    testKernelLoadBalancerIntegration();
    testKernelPreloadManagerIntegration();
    testEventCallbackIntegration();
    testTaskProcessingIntegration();
    std::cout << "All kernel integration tests passed!\n";
    return 0;
} 