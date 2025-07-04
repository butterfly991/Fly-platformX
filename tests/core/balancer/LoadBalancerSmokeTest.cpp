#include <cassert>
#include <iostream>
#include <memory>
#include "core/balancer/LoadBalancer.hpp"
#include "core/kernel/base/MicroKernel.hpp"
#include "core/balancer/TaskTypes.hpp"

class DummyKernel : public IKernel {
public:
    bool initialize() override { return true; }
    void shutdown() override {}
    bool isRunning() const override { return true; }
    metrics::PerformanceMetrics getMetrics() const override { return {0.5, 0.1, 0.0, 0.0, 0, std::chrono::steady_clock::now()}; }
    void updateMetrics() override {}
    void setResourceLimit(const std::string&, double) override {}
    double getResourceUsage(const std::string&) const override { return 0.5; }
    KernelType getType() const override { return KernelType::MICRO; }
    std::string getId() const override { return "dummy"; }
    void pause() override {}
    void resume() override {}
    void reset() override {}
    std::vector<std::string> getSupportedFeatures() const override { return {}; }
    void scheduleTask(std::function<void()> task, int priority) override {}
};

void smokeTestLoadBalancer() {
    LoadBalancer lb;
    std::vector<std::shared_ptr<cloud::core::kernel::IKernel>> kernels = {std::make_shared<DummyKernel>(), std::make_shared<DummyKernel>()};
    std::vector<cloud::core::balancer::TaskDescriptor> tasks;
    cloud::core::balancer::TaskDescriptor t1;
    t1.data = {1,2,3};
    t1.priority = 10;
    t1.enqueueTime = std::chrono::steady_clock::now();
    tasks.push_back(t1);
    cloud::core::balancer::TaskDescriptor t2;
    t2.data = {4,5,6};
    t2.priority = 1;
    t2.enqueueTime = std::chrono::steady_clock::now();
    tasks.push_back(t2);
    std::vector<cloud::core::balancer::KernelMetrics> metrics = {{0.5, 0.1, 0.9, 1.0, 1}, {0.2, 0.2, 0.8, 1.0, 1}};
    lb.balance(kernels, tasks, metrics);
    std::cout << "[OK] LoadBalancer smoke test\n";
}

void stressTestLoadBalancer() {
    LoadBalancer lb;
    std::vector<std::shared_ptr<cloud::core::kernel::IKernel>> kernels;
    for (int i = 0; i < 32; ++i) kernels.push_back(std::make_shared<DummyKernel>());
    std::vector<cloud::core::balancer::TaskDescriptor> tasks;
    for (int i = 0; i < 10000; ++i) {
        cloud::core::balancer::TaskDescriptor t;
        t.data = std::vector<uint8_t>(100, i % 256);
        t.priority = i % 10;
        t.enqueueTime = std::chrono::steady_clock::now();
        tasks.push_back(t);
    }
    std::vector<cloud::core::balancer::KernelMetrics> metrics(32, {0.5, 0.1, 0.9, 1.0, 1});
    lb.balance(kernels, tasks, metrics);
    std::cout << "[OK] LoadBalancer stress test\n";
}

int main() {
    smokeTestLoadBalancer();
    stressTestLoadBalancer();
    std::cout << "All LoadBalancer tests passed!\n";
    return 0;
} 