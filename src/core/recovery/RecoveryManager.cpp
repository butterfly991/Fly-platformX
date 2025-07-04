#include "core/recovery/RecoveryManager.hpp"
#include <random>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <zlib.h>
#include <filesystem>
#include <chrono>
#include <cmath>
#include <thread>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include "core/thread/ThreadPool.hpp"
#include "core/cache/dynamic/PlatformOptimizer.hpp"
#include <iostream>

namespace cloud {
namespace core {
namespace recovery {

namespace detail {

// Реализация логгера
class RecoveryLogger {
public:
    explicit RecoveryLogger(const std::string& logPath, size_t maxSize, size_t maxFiles)
        : logger_(spdlog::rotating_logger_mt("recovery", logPath, maxSize, maxFiles)) {
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
    }

    void log(spdlog::level::level_enum level, const std::string& message) {
        logger_->log(level, message);
    }

    void flush() {
        logger_->flush();
    }

private:
    std::shared_ptr<spdlog::logger> logger_;
};

// Реализация валидатора состояния
class StateValidator {
public:
    bool validateState(const std::vector<uint8_t>& state) const {
        if (state.empty()) return false;
        
        // Проверка целостности данных
        std::string checksum = calculateChecksum(state);
        return !checksum.empty();
    }

private:
    std::string calculateChecksum(const std::vector<uint8_t>& data) const {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, data.data(), data.size());
        SHA256_Final(hash, &sha256);
        
        std::stringstream ss;
        for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return ss.str();
    }
};

// Реализация менеджера контрольных точек
class CheckpointManager {
public:
    explicit CheckpointManager(const config::RecoveryPointConfig& config)
        : config_(config) {}

    bool saveCheckpoint(const RecoveryPoint& point) {
        try {
            std::filesystem::path path = config_.storagePath;
            path /= point.id + ".json";
            
            std::ofstream file(path);
            if (!file) return false;
            
            file << point.toJson().dump(4);
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    bool loadCheckpoint(const std::string& id, RecoveryPoint& point) {
        try {
            std::filesystem::path path = config_.storagePath;
            path /= id + ".json";
            
            std::ifstream file(path);
            if (!file) return false;
            
            nlohmann::json j;
            file >> j;
            point = RecoveryPoint::fromJson(j);
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

private:
    config::RecoveryPointConfig config_;
};

} // namespace detail

RecoveryManager::RecoveryManager(const RecoveryConfig& config)
    : pImpl(std::make_unique<Impl>(config))
    , recoveryInProgress(false) {
    initializeLogger();
    initializeValidator();
    initializeCheckpointManager();
}

RecoveryManager::RecoveryManager(RecoveryManager&& other) noexcept
    : pImpl(std::move(other.pImpl))
    , logger(std::move(other.logger))
    , validator(std::move(other.validator))
    , checkpointManager(std::move(other.checkpointManager))
    , threadPool(std::move(other.threadPool)) {
}

RecoveryManager& RecoveryManager::operator=(RecoveryManager&& other) noexcept {
    if (this != &other) {
        pImpl = std::move(other.pImpl);
        logger = std::move(other.logger);
        validator = std::move(other.validator);
        checkpointManager = std::move(other.checkpointManager);
        threadPool = std::move(other.threadPool);
    }
    return *this;
}

RecoveryManager::~RecoveryManager() {
    try {
        shutdown();
        logger->log(spdlog::level::info, "RecoveryManager destroyed");
        logger->flush();
    } catch (const std::exception& e) {
        // Логируем ошибку при уничтожении
        if (logger) {
            logger->log(spdlog::level::err, 
                "Error during RecoveryManager destruction: " + std::string(e.what()));
            logger->flush();
        }
    }
}

void RecoveryManager::initializeLogger() {
    logger = std::make_shared<detail::RecoveryLogger>(
        pImpl->config.logPath,
        pImpl->config.maxLogSize,
        pImpl->config.maxLogFiles
    );
    logger->log(spdlog::level::info, "Logger initialized");
}

void RecoveryManager::initializeValidator() {
    validator = std::make_shared<detail::StateValidator>();
    logger->log(spdlog::level::info, "State validator initialized");
}

void RecoveryManager::initializeCheckpointManager() {
    checkpointManager = std::make_shared<detail::CheckpointManager>(
        pImpl->config.pointConfig
    );
    logger->log(spdlog::level::info, "Checkpoint manager initialized");
}

bool RecoveryManager::initialize() {
    try {
        if (!pImpl->config.validate()) {
            throw std::runtime_error("Invalid configuration");
        }
        
        if (!ensureDirectoryExists(pImpl->config.pointConfig.storagePath)) {
            throw std::runtime_error("Failed to create storage directory");
        }
        
        threadPool = std::make_shared<cloud::core::thread::ThreadPool>(cloud::core::thread::ThreadPoolConfig{/* заполнить параметры */});
        
        logger->log(spdlog::level::info, "RecoveryManager initialized successfully");
        return true;
    } catch (const std::exception& e) {
        handleError("Initialization failed: " + std::string(e.what()));
        return false;
    }
}

void RecoveryManager::shutdown() {
    try {
        if (threadPool) {
            threadPool->stop();
        }
        
        flushLogs();
        
        logger->log(spdlog::level::info, "RecoveryManager shut down successfully");
    } catch (const std::exception& e) {
        handleError("Shutdown failed: " + std::string(e.what()));
    }
}

std::string RecoveryManager::createRecoveryPoint() {
    try {
        auto startTime = std::chrono::steady_clock::now();
        
        RecoveryPoint point;
        point.id = generatePointId();
        point.timestamp = std::chrono::steady_clock::now();
        
        if (pImpl->config.enableStateValidation) {
            point.state = pImpl->stateCaptureCallback();
            point.checksum = calculateChecksum(point.state);
            point.isConsistent = validator->validateState(point.state);
        }
        
        if (pImpl->config.pointConfig.enableCompression) {
            compressState(point.state);
        }
        
        point.size = point.state.size();
        
        if (!checkpointManager->saveCheckpoint(point)) {
            throw std::runtime_error("Failed to save recovery point");
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
        
        logger->log(spdlog::level::info, 
            "Created recovery point " + point.id + " in " + 
            std::to_string(duration) + "ms");
        
        return point.id;
    } catch (const std::exception& e) {
        handleError("Failed to create recovery point: " + std::string(e.what()));
        return "";
    }
}

bool RecoveryManager::restoreFromPoint(const std::string& pointId) {
    try {
        recoveryInProgress = true;
        auto startTime = std::chrono::steady_clock::now();
        
        RecoveryPoint point;
        if (!checkpointManager->loadCheckpoint(pointId, point)) {
            throw std::runtime_error("Failed to load recovery point");
        }
        
        if (pImpl->config.pointConfig.enableCompression) {
            decompressState(point.state);
        }
        
        if (pImpl->config.enableStateValidation) {
            if (!validator->validateState(point.state)) {
                throw std::runtime_error("Invalid state data");
            }
        }
        
        if (!pImpl->stateRestoreCallback(point.state)) {
            throw std::runtime_error("Failed to restore state");
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
        
        metrics::RecoveryMetrics newMetrics = pImpl->metrics;
        newMetrics.successfulRecoveries++;
        newMetrics.averageRecoveryTime = 
            (newMetrics.averageRecoveryTime * (newMetrics.successfulRecoveries - 1) + 
             duration) / newMetrics.successfulRecoveries;
        newMetrics.lastRecovery = endTime;
        updateMetrics(newMetrics);
        
        logger->log(spdlog::level::info, 
            "Restored from point " + pointId + " in " + 
            std::to_string(duration) + "ms");
        
        recoveryInProgress = false;
        return true;
    } catch (const std::exception& e) {
        handleError("Failed to restore from point: " + std::string(e.what()));
        recoveryInProgress = false;
        return false;
    }
}

void RecoveryManager::deleteRecoveryPoint(const std::string& pointId) {
    // recoveryPoints.erase(pointId);
}

bool RecoveryManager::validateState(const std::vector<uint8_t>& state) const {
    if (!pImpl->config.enableStateValidation) return true;
    
    // TODO: Реализовать валидацию состояния
    return !state.empty();
}

void RecoveryManager::setConfiguration(const RecoveryConfig& config) {
    pImpl->config = config;
    cleanupOldPoints();
}

RecoveryConfig RecoveryManager::getConfiguration() const {
    return pImpl->config;
}

std::chrono::steady_clock::time_point RecoveryManager::getLastCheckpointTime() const {
    return pImpl->lastCheckpoint;
}

bool RecoveryManager::isRecoveryInProgress() const {
    return recoveryInProgress;
}

void RecoveryManager::cleanupOldPoints() {
    if (pImpl->recoveryPoints.size() <= pImpl->config.maxRecoveryPoints) return;
    
    std::vector<std::pair<std::string, RecoveryPoint>> points(
        pImpl->recoveryPoints.begin(), pImpl->recoveryPoints.end());
    
    std::sort(points.begin(), points.end(),
        [](const auto& a, const auto& b) {
            return a.second.timestamp < b.second.timestamp;
        });
    
    size_t toRemove = points.size() - pImpl->config.maxRecoveryPoints;
    for (size_t i = 0; i < toRemove; ++i) {
        pImpl->recoveryPoints.erase(points[i].first);
    }
}

void RecoveryManager::validateRecoveryPoints() {
    for (auto& pair : pImpl->recoveryPoints) {
        pair.second.isConsistent = validateState(pair.second.state);
    }
}

std::string RecoveryManager::generatePointId() const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    for (int i = 0; i < 8; ++i) {
        ss << std::setw(2) << (pImpl->rng() & 0xFF);
    }
    
    return ss.str();
}

bool RecoveryManager::saveRecoveryPoint(const RecoveryPoint& point) {
    // TODO: Реализовать сохранение точки восстановления
    return true;
}

bool RecoveryManager::loadRecoveryPoint(const std::string& pointId, RecoveryPoint& point) {
    // TODO: Реализовать загрузку точки восстановления
    return true;
}

void RecoveryManager::handleError(const std::string& error) {
    logger->log(spdlog::level::err, error);
    if (pImpl->errorCallback) {
        pImpl->errorCallback(error);
    }
}

void RecoveryManager::updateMetrics(const metrics::RecoveryMetrics& newMetrics) {
    pImpl->metrics = newMetrics;
    logMetrics();
}

void RecoveryManager::logMetrics() const {
    nlohmann::json metricsJson = {
        {"totalPoints", pImpl->metrics.totalPoints},
        {"successfulRecoveries", pImpl->metrics.successfulRecoveries},
        {"failedRecoveries", pImpl->metrics.failedRecoveries},
        {"averageRecoveryTime", pImpl->metrics.averageRecoveryTime},
        {"lastRecovery", std::chrono::duration_cast<std::chrono::milliseconds>(
            pImpl->metrics.lastRecovery.time_since_epoch()).count()}
    };
    
    logger->log(spdlog::level::info, "Metrics updated: " + metricsJson.dump());
}

} // namespace recovery
} // namespace core
} // namespace cloud 