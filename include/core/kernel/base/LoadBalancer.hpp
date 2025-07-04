#pragma once
#include <vector>
#include <memory>
#include <string>
#include "core/kernel/CoreKernel.hpp"

namespace cloud {
namespace core {
namespace kernel {

// LoadBalancer — балансировка нагрузки между ядрами и задачами
class LoadBalancer {
public:
    LoadBalancer();
    ~LoadBalancer();

    // Балансировка между ядрами
    void balance(const std::vector<std::shared_ptr<IKernel>>& kernels);

    // Балансировка задач внутри ядра
    void balanceTasks(std::vector<std::vector<uint8_t>>& taskQueues);
};

} // namespace kernel
} // namespace core
} // namespace cloud 