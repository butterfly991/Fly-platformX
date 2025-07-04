#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include "core/balancer/TaskTypes.hpp"

namespace cloud {
namespace core {
namespace balancer {

// TaskOrchestrator — управление очередями задач, поддержка расширенных стратегий оркестрации
class TaskOrchestrator {
public:
    TaskOrchestrator();
    ~TaskOrchestrator();

    // Добавление задачи
    void enqueueTask(const std::vector<uint8_t>& data);
    // Извлечение задачи
    bool dequeueTask(std::vector<uint8_t>& data);
    // Получение размера очереди
    size_t queueSize() const;

    // Расширенные стратегии оркестрации
    void setOrchestrationPolicy(const std::string& policy);
    std::string getOrchestrationPolicy() const;

private:
    std::vector<std::vector<uint8_t>> taskQueue;
    std::string orchestrationPolicy;
    mutable std::mutex mutex_;
};

} // namespace balancer
} // namespace core
} // namespace cloud
