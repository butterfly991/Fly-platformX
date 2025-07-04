#include "core/balancer/TaskOrchestrator.hpp"
#include <spdlog/spdlog.h>

namespace cloud {
namespace core {
namespace balancer {

TaskOrchestrator::TaskOrchestrator() : orchestrationPolicy("fifo") {}
TaskOrchestrator::~TaskOrchestrator() { }

void TaskOrchestrator::enqueueTask(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    taskQueue.push_back(data);
    spdlog::debug("TaskOrchestrator: задача добавлена в очередь (policy: {})", orchestrationPolicy);
}

bool TaskOrchestrator::dequeueTask(std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!taskQueue.empty()) {
        data = std::move(taskQueue.front());
        taskQueue.erase(taskQueue.begin());
        spdlog::debug("TaskOrchestrator: задача извлечена из очереди");
        return true;
    }
    return false;
}

size_t TaskOrchestrator::queueSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return taskQueue.size();
}

void TaskOrchestrator::setOrchestrationPolicy(const std::string& policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    orchestrationPolicy = policy;
    spdlog::debug("TaskOrchestrator: установлена политика оркестрации '{}'");
}

std::string TaskOrchestrator::getOrchestrationPolicy() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return orchestrationPolicy;
}

} // namespace balancer
} // namespace core
} // namespace cloud
