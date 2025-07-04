#include "core/cache/base/AdaptiveCache.hpp"
#include <spdlog/spdlog.h>

namespace cloud {
namespace core {

AdaptiveCache::AdaptiveCache(size_t maxSize) : maxSize_(maxSize) {}
AdaptiveCache::~AdaptiveCache() { clear(); }

bool AdaptiveCache::get(const std::string& key, std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        data = it->second;
        return true;
    }
    return false;
}

void AdaptiveCache::put(const std::string& key, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (cache_.size() >= maxSize_) {
        // Простая стратегия: удаляем первый элемент
        cache_.erase(cache_.begin());
    }
    cache_[key] = data;
}

void AdaptiveCache::adapt(size_t newMaxSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxSize_ = newMaxSize;
    while (cache_.size() > maxSize_) {
        cache_.erase(cache_.begin());
    }
    spdlog::debug("AdaptiveCache: адаптирован размер до {}", maxSize_);
}

void AdaptiveCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

size_t AdaptiveCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

size_t AdaptiveCache::maxSize() const {
    return maxSize_;
}

} // namespace core
} // namespace cloud
