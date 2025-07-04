#pragma once

#include <vector>
#include <queue>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

// Определение платформо-зависимых макросов
#if defined(__APPLE__) && defined(__arm64__)
    #define CLOUD_PLATFORM_APPLE_ARM
    #include <mach/thread_policy.h>
    #include <mach/thread_act.h>
#elif defined(__linux__) && defined(__x86_64__)
    #define CLOUD_PLATFORM_LINUX_X64
    #include <pthread.h>
#endif

namespace cloud {
namespace core {
namespace thread {

// Структура для хранения метрик пула потоков
struct ThreadPoolMetrics {
    size_t activeThreads;    // Количество активных потоков
    size_t queueSize;        // Размер очереди задач
    size_t totalThreads;     // Общее количество потоков
};

// Структура для конфигурации пула потоков
struct ThreadPoolConfig {
    size_t minThreads;       // Минимальное количество потоков
    size_t maxThreads;       // Максимальное количество потоков
    size_t queueSize;        // Максимальный размер очереди
    size_t stackSize;        // Размер стека для каждого потока
    
    #ifdef CLOUD_PLATFORM_APPLE_ARM
        bool usePerformanceCores;    // Использовать ядра производительности
        bool useEfficiencyCores;     // Использовать энергоэффективные ядра
        size_t performanceCoreCount; // Количество ядер производительности
        size_t efficiencyCoreCount;  // Количество энергоэффективных ядер
    #elif defined(CLOUD_PLATFORM_LINUX_X64)
        bool useHyperthreading;      // Использовать гипертрейдинг
        size_t physicalCoreCount;    // Количество физических ядер
        size_t logicalCoreCount;     // Количество логических ядер
    #endif
    
    bool validate() const {
        if (minThreads > maxThreads) return false;
        if (minThreads == 0) return false;
        if (stackSize == 0) return false;
        
        #ifdef CLOUD_PLATFORM_APPLE_ARM
            if (usePerformanceCores && performanceCoreCount == 0) return false;
            if (useEfficiencyCores && efficiencyCoreCount == 0) return false;
        #elif defined(CLOUD_PLATFORM_LINUX_X64)
            if (useHyperthreading && logicalCoreCount <= physicalCoreCount) return false;
        #endif
        
        return true;
    }
};

// Пул потоков
class ThreadPool {
public:
    // Конструктор с конфигурацией
    explicit ThreadPool(const ThreadPoolConfig& config);
    
    // Деструктор
    ~ThreadPool();
    
    // Запрет копирования
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    // Добавление задачи в очередь
    void enqueue(std::function<void()> task);
    
    // Получение количества активных потоков
    size_t getActiveThreadCount() const;
    
    // Получение размера очереди
    size_t getQueueSize() const;
    
    // Проверка пустоты очереди
    bool isQueueEmpty() const;
    
    // Ожидание завершения всех задач
    void waitForCompletion();
    
    // Остановка пула потоков
    void stop();
    
    // Перезапуск пула потоков
    void restart();
    
    // Получение метрик
    ThreadPoolMetrics getMetrics() const;
    
    // Обновление метрик
    void updateMetrics();
    
    // Установка конфигурации
    void setConfiguration(const ThreadPoolConfig& config);
    
    // Получение текущей конфигурации
    ThreadPoolConfig getConfiguration() const;

private:
    // Реализация PIMPL
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace thread
} // namespace core
} // namespace cloud