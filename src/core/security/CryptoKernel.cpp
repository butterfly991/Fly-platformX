#include "core/security/CryptoKernel.hpp"
#include <spdlog/spdlog.h>
#include "core/cache/manager/CacheManager.hpp"
#include "core/cache/dynamic/DynamicCache.hpp"
#include "core/cache/metrics/CacheConfig.hpp"

namespace cloud {
namespace core {
namespace security {

CryptoKernel::CryptoKernel(const std::string& id_) : id(id_) {
    armDriver = std::make_unique<drivers::ARMDriver>();
    cache = std::make_unique<cache::CacheManager>(cache::CacheConfig{/* параметры по умолчанию */});
    dynamicCache = std::make_unique<cache::DefaultDynamicCache>(64);
}

CryptoKernel::~CryptoKernel() {
    shutdown();
}

bool CryptoKernel::initialize() {
    spdlog::info("CryptoKernel[{}]: инициализация", id);
    bool accel = armDriver->initialize();
    bool cacheInit = cache->initialize();
    return accel && cacheInit;
}

void CryptoKernel::shutdown() {
    spdlog::info("CryptoKernel[{}]: завершение работы", id);
    armDriver->shutdown();
    cache.reset();
    if (dynamicCache) dynamicCache->clear();
}

bool CryptoKernel::execute(const std::vector<uint8_t>& data, std::vector<uint8_t>& result) {
    spdlog::debug("CryptoKernel[{}]: выполнение криптографической задачи", id);
    result = data;
    cache->putData("crypto", result);
    dynamicCache->put("crypto", result);
    return true;
}

void CryptoKernel::updateMetrics() {
    cache->updateMetrics();
}

std::string CryptoKernel::getId() const {
    return id;
}

} // namespace security
} // namespace core
} // namespace cloud
