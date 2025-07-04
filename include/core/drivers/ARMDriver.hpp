#pragma once
#include <string>
#include <vector>
#include <optional>
#include <spdlog/spdlog.h>

namespace cloud {
namespace core {
namespace drivers {

// Драйвер ARM для Macbook M1-M4 с поддержкой NEON/AMX и аппаратного ускорения
class ARMDriver {
public:
    ARMDriver();
    virtual ~ARMDriver();

    // Инициализация и завершение работы
    virtual bool initialize();
    virtual void shutdown();

    // Проверка поддержки инструкций/ускорителей
    bool isNeonSupported() const;
    bool isAMXSupported() const;
    bool isSVEAvailable() const;
    bool isNeuralEngineAvailable() const;

    // Получение информации о платформе
    std::string getPlatformInfo() const;

    // Универсальный ускоренный memcpy (NEON/AMX)
    virtual bool accelerateCopy(const std::vector<uint8_t>& input, std::vector<uint8_t>& output);

    // Универсальное ускоренное сложение (пример)
    virtual bool accelerateAdd(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, std::vector<uint8_t>& result);

    // Универсальное ускоренное умножение (пример)
    virtual bool accelerateMul(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, std::vector<uint8_t>& result);

    // Расширяемый интерфейс для кастомных операций
    virtual bool customAccelerate(const std::string& op, const std::vector<uint8_t>& in, std::vector<uint8_t>& out);

protected:
    void detectCapabilities();
    bool neonSupported = false;
    bool amxSupported = false;
    bool sveSupported = false;
    bool neuralEngineSupported = false;
    std::string platformInfo;
};

} // namespace drivers
} // namespace core
} // namespace cloud
