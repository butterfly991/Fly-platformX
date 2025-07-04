#pragma once
#include <string>
#include <vector>
#include <memory>
#include "core/drivers/ARMDriver.hpp"
#include "core/cache/manager/CacheManager.hpp"
#include "core/cache/base/BaseCache.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/cache/metrics/CacheConfig.hpp"

namespace cloud {
namespace core {
namespace security {

// CryptoKernel — ядро для криптографических операций
class CryptoKernel {
public:
    explicit CryptoKernel(const std::string& id);
    ~CryptoKernel();

    bool initialize();
    void shutdown();

    // Выполнение криптографической задачи
    bool execute(const std::vector<uint8_t>& data, std::vector<uint8_t>& result);

    // Метрики и кэш
    void updateMetrics();
    std::string getId() const;

private:
    std::string id;
    std::unique_ptr<drivers::ARMDriver> armDriver;
    std::unique_ptr<cache::CacheManager> cache;
    std::unique_ptr<cache::DefaultDynamicCache> dynamicCache;
};

} // namespace security
} // namespace core
} // namespace cloud
