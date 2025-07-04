#include "core/cache/experimental/PreloadManager.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <unordered_set>

namespace cloud {
namespace core {

// Реализация PIMPL
struct PreloadManager::Impl {
    PreloadConfig config;             // Конфигурация предварительной загрузки
    std::queue<PreloadTask> tasks;    // Очередь задач
    std::mutex tasksMutex;            // Мьютекс для задач
    std::condition_variable condition; // Условная переменная
    std::atomic<bool> stop;           // Флаг остановки
    std::atomic<size_t> activeTasks;  // Количество активных задач
    std::chrono::steady_clock::time_point lastMetricsUpdate; // Время последнего обновления метрик
    std::unordered_set<std::string> accessHistory;
    size_t totalTasks = 0;
    size_t successfulTasks = 0;
    size_t predictionCount = 0;
    size_t correctPredictions = 0;
    
    Impl(const PreloadConfig& cfg)
        : config(cfg)
        , stop(false)
        , activeTasks(0)
        , lastMetricsUpdate(std::chrono::steady_clock::now()) {
        // Инициализация логгера
        try {
            auto logger = spdlog::get("preloadmanager");
            if (!logger) {
                logger = spdlog::rotating_logger_mt("preloadmanager", "logs/preloadmanager.log",
                                                   1024 * 1024 * 5, 3);
            }
            logger->set_level(spdlog::level::debug);
        } catch (const std::exception& e) {
            spdlog::error("Ошибка инициализации логгера: {}", e.what());
        }
    }
};

// Конструктор
PreloadManager::PreloadManager(const PreloadConfig& config)
    : pImpl(std::make_unique<Impl>(config)) {
}

// Деструктор
PreloadManager::~PreloadManager() = default;

// Инициализация менеджера предварительной загрузки
bool PreloadManager::initialize() {
    std::lock_guard<std::mutex> lock(pImpl->tasksMutex);
    
    try {
        // Проверка конфигурации
        if (!pImpl->config.validate()) {
            throw std::runtime_error("Некорректная конфигурация предварительной загрузки");
        }
        
        // Запуск обработчика задач
        startTaskProcessor();
        
        spdlog::get("preloadmanager")->info("PreloadManager успешно инициализирован");
        return true;
    } catch (const std::exception& e) {
        spdlog::get("preloadmanager")->error("Ошибка инициализации: {}", e.what());
        return false;
    }
}

// Предварительная загрузка данных
bool PreloadManager::preloadData(const std::string& key, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(pImpl->tasksMutex);
    try {
        // Проверка размера данных
        if (data.size() > pImpl->config.maxBatchSize) {
            throw std::runtime_error("Размер данных превышает максимально допустимый");
        }
        
        // Проверка размера очереди
        if (pImpl->tasks.size() >= pImpl->config.maxQueueSize) {
            throw std::runtime_error("Очередь задач переполнена");
        }
        
        // Создание задачи
        PreloadTask task{
            key,
            data,
            std::chrono::steady_clock::now(),
            1.0
        };
        
        // Добавление задачи в очередь
        pImpl->tasks.push(std::move(task));
        
        // Уведомление обработчика задач
        pImpl->condition.notify_one();
        
        spdlog::get("preloadmanager")->debug(
            "Задача добавлена в очередь: key={}, size={}",
            key, data.size()
        );
        return true;
    } catch (const std::exception& e) {
        spdlog::get("preloadmanager")->error("Ошибка добавления задачи: {}", e.what());
        return false;
    }
}

// Получение метрик
cache::experimental::PreloadMetrics PreloadManager::getMetrics() const {
    std::lock_guard<std::mutex> lock(pImpl->tasksMutex);
    cache::experimental::PreloadMetrics metrics;
    metrics.queueSize = pImpl->tasks.size();
    metrics.activeTasks = pImpl->activeTasks;
    // Расчет эффективности
    if (pImpl->totalTasks == 0) {
        metrics.efficiency = 0.0;
    } else {
        metrics.efficiency = static_cast<double>(pImpl->successfulTasks) / pImpl->totalTasks;
    }
    // Расчет точности предсказания
    if (pImpl->predictionCount == 0) {
        metrics.predictionAccuracy = 0.0;
    } else {
        metrics.predictionAccuracy = static_cast<double>(pImpl->correctPredictions) / pImpl->predictionCount;
    }
    return metrics;
}

// Обновление метрик
void PreloadManager::updateMetrics() {
    auto now = std::chrono::steady_clock::now();
    if (now - pImpl->lastMetricsUpdate > std::chrono::seconds(1)) {
        try {
            auto metrics = getMetrics();
            spdlog::get("preloadmanager")->debug(
                "Метрики предварительной загрузки: очередь={}, активных задач={}, эффективность={}, точность={}",
                metrics.queueSize, metrics.activeTasks,
                metrics.efficiency, metrics.predictionAccuracy
            );
            pImpl->lastMetricsUpdate = now;
        } catch (const std::exception& e) {
            spdlog::get("preloadmanager")->error("Ошибка обновления метрик: {}", e.what());
        }
    }
}

// Установка конфигурации
void PreloadManager::setConfiguration(const cache::experimental::PreloadConfig& config) {
    std::lock_guard<std::mutex> lock(pImpl->tasksMutex);
    try {
        if (!config.validate()) {
            throw std::runtime_error("Некорректная конфигурация предварительной загрузки");
        }
        
        pImpl->config = config;
        
        spdlog::get("preloadmanager")->info("Конфигурация предварительной загрузки обновлена");
    } catch (const std::exception& e) {
        spdlog::get("preloadmanager")->error("Ошибка обновления конфигурации: {}", e.what());
        throw;
    }
}

// Получение конфигурации
cache::experimental::PreloadConfig PreloadManager::getConfiguration() const {
    std::lock_guard<std::mutex> lock(pImpl->tasksMutex);
    return pImpl->config;
}

// Остановка менеджера предварительной загрузки
void PreloadManager::stop() {
    std::lock_guard<std::mutex> lock(pImpl->tasksMutex);
    try {
        pImpl->stop = true;
        pImpl->condition.notify_all();
        
        spdlog::get("preloadmanager")->debug("PreloadManager остановлен");
    } catch (const std::exception& e) {
        spdlog::get("preloadmanager")->error("Ошибка остановки: {}", e.what());
        throw;
    }
}

// Запуск обработчика задач
void PreloadManager::startTaskProcessor() {
    try {
        std::thread([this] {
            while (!pImpl->stop) {
                PreloadTask task;
                {
                    std::unique_lock<std::mutex> lock(pImpl->tasksMutex);
                    pImpl->condition.wait(lock, [this] {
                        return pImpl->stop || !pImpl->tasks.empty();
                    });
                    
                    if (pImpl->stop && pImpl->tasks.empty()) {
                        break;
                    }
                    
                    task = std::move(pImpl->tasks.front());
                    pImpl->tasks.pop();
                }
                
                try {
                    ++pImpl->activeTasks;
                    processTask(task);
                } catch (const std::exception& e) {
                    spdlog::get("preloadmanager")->error("Ошибка обработки задачи: {}", e.what());
                }
                --pImpl->activeTasks;
            }
        }).detach();
        
        spdlog::get("preloadmanager")->debug("Обработчик задач запущен");
    } catch (const std::exception& e) {
        spdlog::get("preloadmanager")->error("Ошибка запуска обработчика задач: {}", e.what());
        throw;
    }
}

// Обработка задачи
void PreloadManager::processTask(const PreloadTask& task) {
    try {
        // Проверка задачи
        if (task.data.empty()) {
            throw std::runtime_error("Пустые данные задачи");
        }
        
        // Загрузка данных
        std::vector<uint8_t> loadedData;
        if (!loadData(task.key, loadedData)) {
            throw std::runtime_error("Ошибка загрузки данных");
        }
        
        spdlog::get("preloadmanager")->debug(
            "Задача обработана: key={}, size={}",
            task.key, loadedData.size()
        );
    } catch (const std::exception& e) {
        spdlog::get("preloadmanager")->error("Ошибка обработки задачи: {}", e.what());
        throw;
    }
}

// Предсказание следующего доступа
bool PreloadManager::predictNextAccess(const std::string& key) {
    ++pImpl->predictionCount;
    bool predicted = pImpl->accessHistory.count(key) > 0;
    // Для теста: если ключ есть в истории, считаем предсказание верным
    if (predicted) ++pImpl->correctPredictions;
    return predicted;
}

// Загрузка данных
bool PreloadManager::loadData(const std::string& key, std::vector<uint8_t>& data) {
    // Эмулируем загрузку: генерируем данные на основе ключа
    data.clear();
    for (char c : key) {
        data.push_back(static_cast<uint8_t>(c));
    }
    // Считаем, что загрузка всегда успешна
    pImpl->accessHistory.insert(key);
    ++pImpl->totalTasks;
    ++pImpl->successfulTasks;
    return true;
}

std::vector<std::string> PreloadManager::getAllKeys() const {
    std::lock_guard<std::mutex> lock(pImpl->tasksMutex);
    std::vector<std::string> keys;
    // Ключи из очереди задач
    std::queue<PreloadTask> copy = pImpl->tasks;
    while (!copy.empty()) {
        keys.push_back(copy.front().key);
        copy.pop();
    }
    // Ключи из истории доступа
    for (const auto& k : pImpl->accessHistory) {
        if (std::find(keys.begin(), keys.end(), k) == keys.end())
            keys.push_back(k);
    }
    return keys;
}

bool PreloadManager::getDataForKey(const std::string& key, std::vector<uint8_t>& data) const {
    std::lock_guard<std::mutex> lock(pImpl->tasksMutex);
    // Сначала ищем в очереди задач
    std::queue<PreloadTask> copy = pImpl->tasks;
    while (!copy.empty()) {
        if (copy.front().key == key) {
            data = copy.front().data;
            return true;
        }
        copy.pop();
    }
    // Если не найдено — пробуем загрузить через loadData
    return const_cast<PreloadManager*>(this)->loadData(key, data);
}

} // namespace core
} // namespace cloud 