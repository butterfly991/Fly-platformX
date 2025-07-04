#include "core/security/SecurityManager.hpp"
#include <spdlog/spdlog.h>

namespace cloud {
namespace core {
namespace security {

SecurityManager::SecurityManager() : policy("default") {}
SecurityManager::~SecurityManager() { shutdown(); }

bool SecurityManager::initialize() {
    spdlog::info("SecurityManager: инициализация");
    return true;
}

void SecurityManager::shutdown() {
    spdlog::info("SecurityManager: завершение работы");
}

bool SecurityManager::checkPolicy(const std::string& p) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return p == policy;
}

void SecurityManager::setPolicy(const std::string& p) {
    std::lock_guard<std::mutex> lock(mutex_);
    policy = p;
    spdlog::debug("SecurityManager: установлена политика '{}'");
}

std::string SecurityManager::getPolicy() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return policy;
}

void SecurityManager::auditEvent(const std::string& event, const std::string& details) {
    spdlog::info("SecurityManager: аудит события '{}' — {}", event, details);
}

} // namespace security
} // namespace core
} // namespace cloud
