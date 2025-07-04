#include "core/kernel/EnergyController.hpp"
#include <spdlog/spdlog.h>

namespace cloud {
namespace core {
namespace kernel {

EnergyController::EnergyController() : powerLimit(100.0), currentPower(0.0) {}
EnergyController::~EnergyController() { shutdown(); }

bool EnergyController::initialize() {
    spdlog::info("EnergyController: initialization");
    currentPower = 0.0;
    return true;
}

void EnergyController::shutdown() {
    spdlog::info("EnergyController: shutting down");
}

void EnergyController::setPowerLimit(double watts) {
    powerLimit = watts;
    spdlog::debug("EnergyController: power limit set to {} W", watts);
}

double EnergyController::getPowerLimit() const {
    return powerLimit;
}

double EnergyController::getCurrentPower() const {
    return currentPower;
}

void EnergyController::updateMetrics() {
}

} // namespace kernel
} // namespace core
} // namespace cloud 