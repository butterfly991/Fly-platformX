#include "core/kernel/TunnelManager.hpp"
#include <spdlog/spdlog.h>

namespace cloud {
namespace core {
namespace kernel {

TunnelManager::TunnelManager() {}
TunnelManager::~TunnelManager() { shutdown(); }

bool TunnelManager::initialize() {
    spdlog::info("TunnelManager: инициализация");
    return true;
}

void TunnelManager::shutdown() {
    spdlog::info("TunnelManager: завершение работы");
    tunnels.clear();
}

bool TunnelManager::createTunnel(const std::string& from, const std::string& to) {
    tunnels.emplace_back(from, to);
    spdlog::debug("TunnelManager: создан туннель {} -> {}", from, to);
    return true;
}

void TunnelManager::removeTunnel(const std::string& from, const std::string& to) {
    tunnels.erase(std::remove_if(tunnels.begin(), tunnels.end(), [&](const auto& t) {
        return t.first == from && t.second == to;
    }), tunnels.end());
    spdlog::debug("TunnelManager: удалён туннель {} -> {}", from, to);
}

std::vector<std::pair<std::string, std::string>> TunnelManager::getTunnels() const {
    return tunnels;
}

} // namespace kernel
} // namespace core
} // namespace cloud 