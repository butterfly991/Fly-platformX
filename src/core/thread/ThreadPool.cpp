#include "core/thread/ThreadPool.hpp"
#include <algorithm>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <iostream>

namespace cloud {
namespace core {
namespace thread {

// Реализация PIMPL
struct ThreadPool::Impl {
    std::vector<std::thread> workers;           // Рабочие потоки
    std::queue<std::function<void()>> tasks;    // Очередь задач
    std::mutex queueMutex;                      // Мьютекс для очереди
    std::condition_variable condition;          // Условная переменная
    std::atomic<bool> stop;                     // Флаг остановки
    std::atomic<size_t> activeThreads;          // Количество активных потоков
    ThreadPoolConfig config;                    // Конфигурация пула потоков
    
    Impl(const ThreadPoolConfig& cfg) : stop(false), activeThreads(0), config(cfg) {
        // Инициализация логгера
        try {
            auto logger = spdlog::get("threadpool");
            if (!logger) {
                logger = spdlog::rotating_logger_mt("threadpool", "logs/threadpool.log",
                                                   1024 * 1024 * 5, 3);
            }
            logger->set_level(spdlog::level::debug);
        } catch (const std::exception& e) {
            std::cerr << "Ошибка инициализации логгера: " << e.what() << std::endl;
        }
        
        // Создание рабочих потоков с оптимизацией под платформу
        #ifdef CLOUD_PLATFORM_APPLE_ARM
            // Для Apple Silicon используем привязку к конкретным ядрам
            size_t threadCount = 0;
            
            // Создаем потоки для ядер производительности
            if (config.usePerformanceCores) {
                for (size_t i = 0; i < config.performanceCoreCount; ++i) {
                    createWorker(threadCount++, true);
                }
            }
            
            // Создаем потоки для энергоэффективных ядер
            if (config.useEfficiencyCores) {
                for (size_t i = 0; i < config.efficiencyCoreCount; ++i) {
                    createWorker(threadCount++, false);
                }
            }
            
            // Если не указаны конкретные ядра, создаем стандартные потоки
            if (threadCount == 0) {
                threadCount = std::min(config.minThreads, config.maxThreads);
                for (size_t i = 0; i < threadCount; ++i) {
                    createWorker(i, true);
                }
            }
        #elif defined(CLOUD_PLATFORM_LINUX_X64)
            // Для Linux x86-64 используем стандартное создание потоков
            size_t threadCount = config.minThreads;
            
            if (config.useHyperthreading) {
                threadCount = std::min(config.logicalCoreCount, config.maxThreads);
            } else {
                threadCount = std::min(config.physicalCoreCount, config.maxThreads);
            }
            
            for (size_t i = 0; i < threadCount; ++i) {
                createWorker(i);
            }
        #endif
        
        spdlog::get("threadpool")->debug(
            "Пул потоков инициализирован: {} потоков",
            workers.size()
        );
    }
    
    ~Impl() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
public:
    #ifdef CLOUD_PLATFORM_APPLE_ARM
    void createWorker(size_t coreIndex, bool isPerformanceCore) {
        workers.emplace_back([this, coreIndex, isPerformanceCore] {
            // Привязка потока к конкретному ядру
            thread_affinity_policy_data_t policy = { static_cast<integer_t>(coreIndex) };
            thread_policy_set(pthread_mach_thread_np(pthread_self()),
                            THREAD_AFFINITY_POLICY,
                            (thread_policy_t)&policy,
                            THREAD_AFFINITY_POLICY_COUNT);
            
            // Установка приоритета потока
            thread_extended_policy_data_t extPolicy;
            extPolicy.timeshare = isPerformanceCore ? 0 : 1;
            thread_policy_set(pthread_mach_thread_np(pthread_self()),
                            THREAD_EXTENDED_POLICY,
                            (thread_policy_t)&extPolicy,
                            THREAD_EXTENDED_POLICY_COUNT);
            
            processTasks();
        });
    }
    #elif defined(CLOUD_PLATFORM_LINUX_X64)
    void createWorker(size_t coreIndex) {
        workers.emplace_back([this] {
            processTasks();
        });
    }
    #endif
    
    void processTasks() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                condition.wait(lock, [this] {
                    return stop || !tasks.empty();
                });
                
                if (stop && tasks.empty()) {
                    return;
                }
                
                task = std::move(tasks.front());
                tasks.pop();
            }
            
            try {
                ++activeThreads;
                
                if (task) {
                    task();
                }
            } catch (const std::exception& e) {
                spdlog::get("threadpool")->error("Ошибка выполнения задачи: {}", e.what());
            }
            --activeThreads;
        }
    }
};

// Конструктор
ThreadPool::ThreadPool(const ThreadPoolConfig& config)
    : pImpl(std::make_unique<Impl>(config)) {
}

// Деструктор
ThreadPool::~ThreadPool() = default;

// Добавление задачи в очередь
void ThreadPool::enqueue(std::function<void()> task) {
    if (!task) return;
    
    try {
        std::unique_lock<std::mutex> lock(pImpl->queueMutex);
        
        // Проверка размера очереди
        if (pImpl->tasks.size() >= pImpl->config.queueSize) {
            throw std::runtime_error("Очередь задач переполнена");
        }
        
        pImpl->tasks.push(std::move(task));
        pImpl->condition.notify_one();
        
        spdlog::get("threadpool")->debug(
            "Задача добавлена в очередь: активных потоков={}, размер очереди={}",
            pImpl->activeThreads.load(), pImpl->tasks.size()
        );
    } catch (const std::exception& e) {
        spdlog::get("threadpool")->error("Ошибка добавления задачи: {}", e.what());
        throw;
    }
}

// Получение количества активных потоков
size_t ThreadPool::getActiveThreadCount() const {
    return pImpl->activeThreads.load();
}

// Получение размера очереди
size_t ThreadPool::getQueueSize() const {
    std::unique_lock<std::mutex> lock(pImpl->queueMutex);
    return pImpl->tasks.size();
}

// Проверка пустоты очереди
bool ThreadPool::isQueueEmpty() const {
    std::unique_lock<std::mutex> lock(pImpl->queueMutex);
    return pImpl->tasks.empty();
}

// Ожидание завершения всех задач
void ThreadPool::waitForCompletion() {
    try {
        while (true) {
            std::unique_lock<std::mutex> lock(pImpl->queueMutex);
            if (pImpl->tasks.empty() && pImpl->activeThreads.load() == 0) {
                break;
            }
            pImpl->condition.wait(lock);
        }
        
        spdlog::get("threadpool")->debug("Ожидание завершения задач выполнено");
    } catch (const std::exception& e) {
        spdlog::get("threadpool")->error("Ошибка ожидания завершения: {}", e.what());
        throw;
    }
}

// Остановка пула потоков
void ThreadPool::stop() {
    try {
        {
            std::unique_lock<std::mutex> lock(pImpl->queueMutex);
            pImpl->stop = true;
        }
        pImpl->condition.notify_all();
        
        for (auto& worker : pImpl->workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        
        spdlog::get("threadpool")->debug("Пул потоков остановлен");
    } catch (const std::exception& e) {
        spdlog::get("threadpool")->error("Ошибка остановки пула потоков: {}", e.what());
        throw;
    }
}

// Перезапуск пула потоков
void ThreadPool::restart() {
    try {
        stop();
        
        {
            std::unique_lock<std::mutex> lock(pImpl->queueMutex);
            pImpl->stop = false;
        }
        
        // Пересоздание рабочих потоков
        pImpl->workers.clear();
        
        #ifdef CLOUD_PLATFORM_APPLE_ARM
            size_t threadCount = 0;
            
            if (pImpl->config.usePerformanceCores) {
                for (size_t i = 0; i < pImpl->config.performanceCoreCount; ++i) {
                    pImpl->createWorker(threadCount++, true);
                }
            }
            
            if (pImpl->config.useEfficiencyCores) {
                for (size_t i = 0; i < pImpl->config.efficiencyCoreCount; ++i) {
                    pImpl->createWorker(threadCount++, false);
                }
            }
            
            if (threadCount == 0) {
                threadCount = std::min(pImpl->config.minThreads, pImpl->config.maxThreads);
                for (size_t i = 0; i < threadCount; ++i) {
                    pImpl->createWorker(i, true);
                }
            }
        #elif defined(CLOUD_PLATFORM_LINUX_X64)
            size_t threadCount = pImpl->config.minThreads;
            
            if (pImpl->config.useHyperthreading) {
                threadCount = std::min(pImpl->config.logicalCoreCount, pImpl->config.maxThreads);
            } else {
                threadCount = std::min(pImpl->config.physicalCoreCount, pImpl->config.maxThreads);
            }
            
            for (size_t i = 0; i < threadCount; ++i) {
                pImpl->createWorker(i);
            }
        #endif
        
        spdlog::get("threadpool")->debug("Пул потоков перезапущен");
    } catch (const std::exception& e) {
        spdlog::get("threadpool")->error("Ошибка перезапуска пула потоков: {}", e.what());
        throw;
    }
}

// Получение метрик
ThreadPoolMetrics ThreadPool::getMetrics() const {
    ThreadPoolMetrics metrics;
    metrics.activeThreads = pImpl->activeThreads.load();
    metrics.queueSize = getQueueSize();
    metrics.totalThreads = pImpl->workers.size();
    return metrics;
}

// Обновление метрик
void ThreadPool::updateMetrics() {
    try {
        auto metrics = getMetrics();
        spdlog::get("threadpool")->debug(
            "Метрики пула потоков: активных={}, очередь={}, всего={}",
            metrics.activeThreads, metrics.queueSize, metrics.totalThreads
        );
    } catch (const std::exception& e) {
        spdlog::get("threadpool")->error("Ошибка обновления метрик: {}", e.what());
    }
}

void ThreadPool::setConfiguration(const ThreadPoolConfig& config) {
    if (!config.validate()) {
        throw std::invalid_argument("Некорректная конфигурация пула потоков");
    }
    
    try {
        pImpl->config = config;
        restart();
        
        spdlog::get("threadpool")->debug("Конфигурация пула потоков обновлена");
    } catch (const std::exception& e) {
        spdlog::get("threadpool")->error("Ошибка обновления конфигурации: {}", e.what());
        throw;
    }
}

ThreadPoolConfig ThreadPool::getConfiguration() const {
    return pImpl->config;
}

} // namespace thread
} // namespace core
} // namespace cloud 