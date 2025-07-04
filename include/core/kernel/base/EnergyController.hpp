#pragma once
#include <string>
#include <vector>
#include <memory>

namespace cloud {
namespace core {
namespace kernel {

// EnergyController — управление энергопотреблением и мониторинг состояния питания
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

private:
    double powerLimit;
    double currentPower;
};

} // namespace kernel
} // namespace core
} // namespace cloud 