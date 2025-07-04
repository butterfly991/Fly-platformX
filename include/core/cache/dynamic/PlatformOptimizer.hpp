#pragma once

#include "core/cache/metrics/CacheConfig.hpp"
#include <memory>
#include <string>

namespace cloud {
namespace core {
namespace cache {

class PlatformOptimizer {
public:
    static PlatformOptimizer& getInstance();
    
    // Оптимизация кэша для конкретной платформы
    void optimizeCache(CacheConfig& config) const;
    
    // Получение оптимальных параметров для платформы
    CacheConfig getOptimalConfig() const;
    
    // Проверка поддержки платформы
    bool isPlatformSupported() const;
    
    // Получение информации о платформе
    std::string getPlatformInfo() const;
    
private:
    PlatformOptimizer() = default;
    ~PlatformOptimizer() = default;
    PlatformOptimizer(const PlatformOptimizer&) = delete;
    PlatformOptimizer& operator=(const PlatformOptimizer&) = delete;
    
    // Платформо-специфичные оптимизации
    #ifdef CLOUD_PLATFORM_APPLE_ARM
        void optimizeForAppleARM(CacheConfig& config);
        void configureMetalAcceleration(CacheConfig& config);
    #elif defined(CLOUD_PLATFORM_LINUX_X64)
        void optimizeForLinuxX64(CacheConfig& config);
        void configureAVXAcceleration(CacheConfig& config);
    #endif
    
    // Вспомогательные методы
    void detectHardwareCapabilities() const;
    void adjustConfigForHardware(CacheConfig& config) const;
};

} // namespace cache
} // namespace core
} // namespace cloud 