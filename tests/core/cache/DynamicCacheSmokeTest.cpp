#include <cassert>
#include <iostream>
#include "core/cache/dynamic/DynamicCache.hpp"
#include <string>
#include <vector>

void smokeTestDynamicCache() {
    cloud::core::cache::DefaultDynamicCache cache(4);
    cache.put("a", {1});
    cache.put("b", {2});
    cache.put("c", {3});
    cache.put("d", {4});
    assert(cache.size() == 4);
    cache.put("e", {5}); // LRU eviction
    assert(cache.size() == 4);
    auto v = cache.get("e");
    assert(v && (*v)[0] == 5);
    cache.remove("e");
    assert(!cache.get("e"));
    cache.clear();
    assert(cache.size() == 0);
    std::cout << "[OK] DynamicCache smoke test\n";
}

void stressTestDynamicCache() {
    cloud::core::cache::DefaultDynamicCache cache(128);
    for (int i = 0; i < 10000; ++i) {
        cache.put(std::to_string(i), {uint8_t(i % 256)});
    }
    assert(cache.size() <= 128);
    for (int i = 0; i < 10000; ++i) {
        cache.remove(std::to_string(i));
    }
    assert(cache.size() == 0);
    std::cout << "[OK] DynamicCache stress test\n";
}

int main() {
    smokeTestDynamicCache();
    stressTestDynamicCache();
    std::cout << "All DynamicCache tests passed!\n";
    return 0;
} 