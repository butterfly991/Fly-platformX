#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace cloud {
namespace core {
namespace security {

// SecurityManager — управление политиками безопасности, аудит, интеграция с ядрами
class SecurityManager {
public:
    SecurityManager();
    ~SecurityManager();

    // Инициализация и завершение работы
    bool initialize();
    void shutdown();

    // Проверка политики безопасности
    bool checkPolicy(const std::string& policy) const;
    void setPolicy(const std::string& policy);
    std::string getPolicy() const;

    // Аудит событий
    void auditEvent(const std::string& event, const std::string& details);

private:
    std::string policy;
    mutable std::mutex mutex_;
};

} // namespace security
} // namespace core
} // namespace cloud
