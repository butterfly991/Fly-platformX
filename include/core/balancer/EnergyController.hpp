#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace cloud {
namespace core {
namespace balancer {

// EnergyController — управление энергопотреблением, поддержка стратегий энергосбережения и мониторинга
class EnergyController {
public:
    EnergyController();
    ~EnergyController();

    // Инициализация контроллера
    bool initialize();
    void shutdown();

    // Установка лимита мощности
    void setPowerLimit(double watts);
    double getPowerLimit() const;

    // Получение текущего энергопотребления
    double getCurrentPower() const;

    // Обновление метрик
    void updateMetrics();

    // Расширенные стратегии энергоменеджмента
    void enableDynamicScaling(bool enable);
    void setEnergyPolicy(const std::string& policy);
    std::string getEnergyPolicy() const;

private:
    double powerLimit;
    double currentPower;
    std::string energyPolicy;
    bool dynamicScalingEnabled;
    mutable std::mutex mutex_;
};

} // namespace balancer
} // namespace core
} // namespace cloud
