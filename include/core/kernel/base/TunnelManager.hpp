#pragma once
#include <vector>
#include <string>
#include <memory>

namespace cloud {
namespace core {
namespace kernel {

// TunnelManager — управление ускоряющими туннелями между ядрами и устройствами
class TunnelManager {
public:
    TunnelManager();
    ~TunnelManager();

    // Инициализация туннелей
    bool initialize();
    void shutdown();

    // Создание туннеля между ядрами
    bool createTunnel(const std::string& from, const std::string& to);
    // Удаление туннеля
    void removeTunnel(const std::string& from, const std::string& to);
    // Получение списка туннелей
    std::vector<std::pair<std::string, std::string>> getTunnels() const;

private:
    std::vector<std::pair<std::string, std::string>> tunnels;
};

} // namespace kernel
} // namespace core
} // namespace cloud 