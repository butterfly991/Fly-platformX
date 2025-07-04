#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <locale>
#include <codecvt>
#include <random>
#include <ios>
#include <streambuf>
#include <shared_mutex>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "core/thread/ThreadPool.hpp"

#if defined(__APPLE__) && defined(__arm64__)
    #include <mach/mach.h>
    #include <sys/sysctl.h>
#endif

namespace cloud {
namespace core {
namespace recovery {

// Вложенные пространства имен для лучшей организации
namespace detail {
    class RecoveryLogger;
    class StateValidator;
    class CheckpointManager;
}

namespace config {
    struct RecoveryPointConfig {
        size_t maxSize;
        bool enableCompression;
        std::string storagePath;
        std::chrono::seconds retentionPeriod;
    };
}

namespace metrics {
    struct RecoveryMetrics {
        size_t totalPoints;
        size_t successfulRecoveries;
        size_t failedRecoveries;
        double averageRecoveryTime;
        std::chrono::steady_clock::time_point lastRecovery;
    };
}

// Улучшенная структура точки восстановления
struct RecoveryPoint {
    std::string id;
    std::chrono::steady_clock::time_point timestamp;
    std::vector<uint8_t> state;
    bool isConsistent;
    std::string checksum;
    size_t size;
    std::unordered_map<std::string, std::string> metadata;
    
    // Сериализация в JSON
    nlohmann::json toJson() const {
        return {
            {"id", id},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count()},
            {"size", size},
            {"isConsistent", isConsistent},
            {"checksum", checksum},
            {"metadata", metadata}
        };
    }
    
    // Десериализация из JSON
    static RecoveryPoint fromJson(const nlohmann::json& j) {
        RecoveryPoint point;
        point.id = j["id"];
        point.timestamp = std::chrono::steady_clock::time_point(
            std::chrono::milliseconds(j["timestamp"]));
        point.size = j["size"];
        point.isConsistent = j["isConsistent"];
        point.checksum = j["checksum"];
        point.metadata = j["metadata"].get<std::unordered_map<std::string, std::string>>();
        return point;
    }
};

// Улучшенная конфигурация
struct RecoveryConfig {
    size_t maxRecoveryPoints;
    std::chrono::seconds checkpointInterval;
    bool enableAutoRecovery;
    bool enableStateValidation;
    config::RecoveryPointConfig pointConfig;
    std::string logPath;
    size_t maxLogSize;
    size_t maxLogFiles;
    
    // Валидация конфигурации
    bool validate() const {
        if (maxRecoveryPoints == 0) return false;
        if (checkpointInterval.count() <= 0) return false;
        if (pointConfig.maxSize == 0) return false;
        if (pointConfig.storagePath.empty()) return false;
        return true;
    }
};

// Основной класс с улучшенной структурой
class RecoveryManager {
public:
    // RAII конструктор с улучшенной инициализацией
    explicit RecoveryManager(const RecoveryConfig& config = RecoveryConfig{});
    
    // Запрещаем копирование
    RecoveryManager(const RecoveryManager&) = delete;
    RecoveryManager& operator=(const RecoveryManager&) = delete;
    
    // Разрешаем перемещение
    RecoveryManager(RecoveryManager&&) noexcept;
    RecoveryManager& operator=(RecoveryManager&&) noexcept;
    
    // Виртуальный деструктор с логированием
    virtual ~RecoveryManager();

    // Основные методы с улучшенной обработкой ошибок
    bool initialize();
    void shutdown();
    
    // Методы управления точками восстановления
    std::string createRecoveryPoint();
    bool restoreFromPoint(const std::string& pointId);
    void deleteRecoveryPoint(const std::string& pointId);
    // std::vector<RecoveryPoint> getRecoveryPoints() const; // Удалено, если не реализовано
    
    // Методы валидации
    bool validateState(const std::vector<uint8_t>& state) const;
    // bool isStateConsistent(const std::string& pointId) const; // Удалено, если не реализовано
    
    // Методы конфигурации
    void setConfiguration(const RecoveryConfig& config);
    RecoveryConfig getConfiguration() const;
    
    // Методы мониторинга
    metrics::RecoveryMetrics getMetrics() const;
    std::chrono::steady_clock::time_point getLastCheckpointTime() const;
    bool isRecoveryInProgress() const;
    
    // Новые методы
    void setStateCaptureCallback(std::function<std::vector<uint8_t>()> callback);
    void setStateRestoreCallback(std::function<bool(const std::vector<uint8_t>&)> callback);
    void setErrorCallback(std::function<void(const std::string&)> callback);
    
    // Методы для работы с логами
    void setLogLevel(spdlog::level::level_enum level);
    void flushLogs();
    
private:
    // PIMPL идиома
    struct Impl {
        RecoveryConfig config;
        metrics::RecoveryMetrics metrics;
        std::chrono::steady_clock::time_point lastCheckpoint;
        std::mt19937 rng;
        std::function<std::vector<uint8_t>()> stateCaptureCallback;
        std::function<bool(const std::vector<uint8_t>&)> stateRestoreCallback;
        std::function<void(const std::string&)> errorCallback;
        std::unordered_map<std::string, RecoveryPoint> recoveryPoints;
    };
    std::unique_ptr<Impl> pImpl;
    
    // Внутренние компоненты
    std::shared_ptr<detail::RecoveryLogger> logger;
    std::shared_ptr<detail::StateValidator> validator;
    std::shared_ptr<detail::CheckpointManager> checkpointManager;
    
    // Пул потоков для асинхронных операций
    std::shared_ptr<cloud::core::thread::ThreadPool> threadPool;
    
    // Синхронизация
    mutable std::shared_mutex recoveryMutex;
    std::atomic<bool> initialized;
    std::atomic<bool> recoveryInProgress;
    
    // Приватные методы
    void initializeLogger();
    void initializeValidator();
    void initializeCheckpointManager();
    void cleanupOldPoints();
    void validateRecoveryPoints();
    std::string generatePointId() const;
    bool saveRecoveryPoint(const RecoveryPoint& point);
    bool loadRecoveryPoint(const std::string& pointId, RecoveryPoint& point);
    void handleError(const std::string& error);
    
    // Вспомогательные методы
    std::string calculateChecksum(const std::vector<uint8_t>& data) const;
    bool compressState(std::vector<uint8_t>& data) const;
    bool decompressState(std::vector<uint8_t>& data) const;
    
    // Методы для работы с файловой системой
    bool ensureDirectoryExists(const std::string& path) const;
    std::string getStoragePath() const;
    
    // Методы для работы с метриками
    void updateMetrics(const metrics::RecoveryMetrics& newMetrics);
    void logMetrics() const;
};

} // namespace recovery
} // namespace core
} // namespace cloud 