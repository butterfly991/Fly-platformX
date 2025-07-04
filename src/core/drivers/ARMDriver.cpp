#include "core/drivers/ARMDriver.hpp"
#include <spdlog/spdlog.h>
#if defined(__APPLE__) && defined(__arm64__)
    #include <sys/sysctl.h>
    #include <arm_neon.h>
    #include <chrono>
#endif

namespace cloud {
namespace core {
namespace drivers {

ARMDriver::ARMDriver() { detectCapabilities(); }
ARMDriver::~ARMDriver() { shutdown(); }

bool ARMDriver::initialize() {
    spdlog::info("ARMDriver: initialization");
    detectCapabilities();
    return neonSupported || amxSupported || sveSupported || neuralEngineSupported;
}

void ARMDriver::shutdown() {
    spdlog::info("ARMDriver: shutdown");
    // Очистка ресурсов, если появятся
}

void ARMDriver::detectCapabilities() {
#if defined(__APPLE__) && defined(__arm64__)
    int neon = 0, amx = 0;
    size_t size = sizeof(int);
    sysctlbyname("hw.optional.neon", &neon, &size, nullptr, 0);
    sysctlbyname("hw.optional.amx", &amx, &size, nullptr, 0);
    neonSupported = (neon != 0);
    amxSupported = (amx != 0);
    sveSupported = false;
    neuralEngineSupported = false;
    platformInfo = "Apple Silicon (M1-M4)";
#elif defined(__linux__) && defined(__aarch64__)
    neonSupported = true;
    amxSupported = false;
    sveSupported = false;
    neuralEngineSupported = false;
    platformInfo = "Linux ARM64";
#else
    neonSupported = false;
    amxSupported = false;
    sveSupported = false;
    neuralEngineSupported = false;
    platformInfo = "Unknown/Unsupported";
#endif
}

bool ARMDriver::isNeonSupported() const { return neonSupported; }
bool ARMDriver::isAMXSupported() const { return amxSupported; }
bool ARMDriver::isSVEAvailable() const { return sveSupported; }
bool ARMDriver::isNeuralEngineAvailable() const { return neuralEngineSupported; }
std::string ARMDriver::getPlatformInfo() const { return platformInfo; }

// NEON-ускоренный memcpy с трассировкой
bool ARMDriver::accelerateCopy(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
#if defined(__APPLE__) && defined(__arm64__)
    if (!neonSupported) return false;
    auto start = std::chrono::high_resolution_clock::now();
    size_t n = input.size();
    output.resize(n);
    size_t i = 0;
    constexpr size_t block = 64; // 64 байта = 4 x uint8x16_t
    for (; i + block <= n; i += block) {
        uint8x16_t v0 = vld1q_u8(&input[i]);
        uint8x16_t v1 = vld1q_u8(&input[i+16]);
        uint8x16_t v2 = vld1q_u8(&input[i+32]);
        uint8x16_t v3 = vld1q_u8(&input[i+48]);
        vst1q_u8(&output[i], v0);
        vst1q_u8(&output[i+16], v1);
        vst1q_u8(&output[i+32], v2);
        vst1q_u8(&output[i+48], v3);
    }
    for (; i < n; ++i) {
        output[i] = input[i];
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    spdlog::trace("ARMDriver::accelerateCopy (NEON): {} bytes in {} ns", n, dur);
    return true;
#else
    (void)input; (void)output;
    return false;
#endif
}

// NEON-ускоренное сложение с трассировкой
bool ARMDriver::accelerateAdd(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, std::vector<uint8_t>& result) {
#if defined(__APPLE__) && defined(__arm64__)
    if (!neonSupported || a.size() != b.size()) return false;
    auto start = std::chrono::high_resolution_clock::now();
    size_t n = a.size();
    result.resize(n);
    size_t i = 0;
    constexpr size_t block = 64;
    for (; i + block <= n; i += block) {
        uint8x16_t va0 = vld1q_u8(&a[i]);
        uint8x16_t vb0 = vld1q_u8(&b[i]);
        uint8x16_t va1 = vld1q_u8(&a[i+16]);
        uint8x16_t vb1 = vld1q_u8(&b[i+16]);
        uint8x16_t va2 = vld1q_u8(&a[i+32]);
        uint8x16_t vb2 = vld1q_u8(&b[i+32]);
        uint8x16_t va3 = vld1q_u8(&a[i+48]);
        uint8x16_t vb3 = vld1q_u8(&b[i+48]);
        vst1q_u8(&result[i], vaddq_u8(va0, vb0));
        vst1q_u8(&result[i+16], vaddq_u8(va1, vb1));
        vst1q_u8(&result[i+32], vaddq_u8(va2, vb2));
        vst1q_u8(&result[i+48], vaddq_u8(va3, vb3));
    }
    for (; i < n; ++i) {
        result[i] = a[i] + b[i];
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    spdlog::trace("ARMDriver::accelerateAdd (NEON): {} bytes in {} ns", n, dur);
    return true;
#else
    (void)a; (void)b; (void)result;
    return false;
#endif
}

// NEON-ускоренное умножение с трассировкой
bool ARMDriver::accelerateMul(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, std::vector<uint8_t>& result) {
#if defined(__APPLE__) && defined(__arm64__)
    if (!neonSupported || a.size() != b.size()) return false;
    auto start = std::chrono::high_resolution_clock::now();
    size_t n = a.size();
    result.resize(n);
    size_t i = 0;
    constexpr size_t block = 64;
    for (; i + block <= n; i += block) {
        uint8x16_t va0 = vld1q_u8(&a[i]);
        uint8x16_t vb0 = vld1q_u8(&b[i]);
        uint8x16_t va1 = vld1q_u8(&a[i+16]);
        uint8x16_t vb1 = vld1q_u8(&b[i+16]);
        uint8x16_t va2 = vld1q_u8(&a[i+32]);
        uint8x16_t vb2 = vld1q_u8(&b[i+32]);
        uint8x16_t va3 = vld1q_u8(&a[i+48]);
        uint8x16_t vb3 = vld1q_u8(&b[i+48]);
        vst1q_u8(&result[i], vmulq_u8(va0, vb0));
        vst1q_u8(&result[i+16], vmulq_u8(va1, vb1));
        vst1q_u8(&result[i+32], vmulq_u8(va2, vb2));
        vst1q_u8(&result[i+48], vmulq_u8(va3, vb3));
    }
    for (; i < n; ++i) {
        result[i] = a[i] * b[i];
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    spdlog::trace("ARMDriver::accelerateMul (NEON): {} bytes in {} ns", n, dur);
    return true;
#else
    (void)a; (void)b; (void)result;
    return false;
#endif
}

bool ARMDriver::customAccelerate(const std::string& op, const std::vector<uint8_t>& in, std::vector<uint8_t>& out) {
    spdlog::warn("ARMDriver: custom operation '{}' not implemented", op);
    return false;
}

} // namespace drivers
} // namespace core
} // namespace cloud
