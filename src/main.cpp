#include <iostream>
#include <memory>
#include <signal.h>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

// Core components
#include "core/kernel/base/CoreKernel.hpp"
#include "core/kernel/base/MicroKernel.hpp"
#include "core/kernel/advanced/OrchestrationKernel.hpp"
#include "core/balancer/LoadBalancer.hpp"
#include "core/cache/experimental/PreloadManager.hpp"
#include "core/security/SecurityManager.hpp"
#include "core/recovery/RecoveryManager.hpp"
#include "core/thread/ThreadPool.hpp"
#include "core/drivers/ARMDriver.hpp"
#include "core/balancer/TaskTypes.hpp"

using namespace cloud::core;

// Global variables for graceful shutdown
std::atomic<bool> g_running{true};
std::vector<std::shared_ptr<kernel::IKernel>> g_kernels;
std::shared_ptr<balancer::LoadBalancer> g_loadBalancer;
std::shared_ptr<PreloadManager> g_preloadManager;
std::shared_ptr<security::SecurityManager> g_securityManager;
std::shared_ptr<recovery::RecoveryManager> g_recoveryManager;
std::shared_ptr<thread::ThreadPool> g_threadPool;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    spdlog::info("Received signal {}, initiating graceful shutdown...", signal);
    g_running = false;
}

// Initialize logging system
void initializeLogging() {
    try {
        // Create logs directory
        std::filesystem::create_directories("logs");
        
        // Console sink
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
        
        // File sink
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "logs/cloud_service.log", 1024*1024*10, 5);
        file_sink->set_level(spdlog::level::debug);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
        
        // Create logger
        auto logger = std::make_shared<spdlog::logger>("cloud_service", 
            spdlog::sinks_init_list{console_sink, file_sink});
        
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::debug);
        
        spdlog::info("=== Cloud IaaS Service Starting ===");
        spdlog::info("Logging system initialized");
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize logging: " << e.what() << std::endl;
        throw;
    }
}

// Initialize core components
void initializeComponents() {
    spdlog::info("Initializing core components...");
    
    try {
        // Initialize thread pool
        thread::ThreadPoolConfig threadConfig;
        threadConfig.minThreads = 4;
        threadConfig.maxThreads = std::thread::hardware_concurrency();
        threadConfig.queueSize = 1000;
        threadConfig.stackSize = 1024 * 1024;
        
        #ifdef CLOUD_PLATFORM_APPLE_ARM
            threadConfig.usePerformanceCores = true;
            threadConfig.useEfficiencyCores = true;
            threadConfig.performanceCoreCount = 4;
            threadConfig.efficiencyCoreCount = 4;
        #endif
        
        g_threadPool = std::make_shared<thread::ThreadPool>(threadConfig);
        spdlog::info("Thread pool initialized with {} threads", threadConfig.maxThreads);
        
        // Initialize security manager
        g_securityManager = std::make_shared<security::SecurityManager>();
        if (!g_securityManager->initialize()) {
            throw std::runtime_error("Failed to initialize security manager");
        }
        g_securityManager->setPolicy("production");
        spdlog::info("Security manager initialized");
        
        // Initialize recovery manager
        recovery::RecoveryConfig recoveryConfig;
        recoveryConfig.maxRecoveryPoints = 10;
        recoveryConfig.checkpointInterval = std::chrono::seconds(30);
        recoveryConfig.enableAutoRecovery = true;
        recoveryConfig.enableStateValidation = true;
        recoveryConfig.pointConfig.maxSize = 1024 * 1024 * 100; // 100MB
        recoveryConfig.pointConfig.enableCompression = true;
        recoveryConfig.pointConfig.storagePath = "recovery_points";
        recoveryConfig.pointConfig.retentionPeriod = std::chrono::hours(24);
        recoveryConfig.logPath = "logs/recovery.log";
        recoveryConfig.maxLogSize = 1024 * 1024 * 5;
        recoveryConfig.maxLogFiles = 3;
        
        g_recoveryManager = std::make_shared<recovery::RecoveryManager>(recoveryConfig);
        if (!g_recoveryManager->initialize()) {
            throw std::runtime_error("Failed to initialize recovery manager");
        }
        spdlog::info("Recovery manager initialized");
        
        // Initialize preload manager
        PreloadConfig preloadConfig;
        preloadConfig.maxQueueSize = 1000;
        preloadConfig.maxConcurrentTasks = 10;
        preloadConfig.predictionThreshold = 0.7;
        preloadConfig.enableAdaptivePrediction = true;
        preloadConfig.enableMetricsCollection = true;
        
        g_preloadManager = std::make_shared<PreloadManager>(preloadConfig);
        if (!g_preloadManager->initialize()) {
            throw std::runtime_error("Failed to initialize preload manager");
        }
        spdlog::info("Preload manager initialized");
        
        // Initialize load balancer
        g_loadBalancer = std::make_shared<balancer::LoadBalancer>();
        g_loadBalancer->setStrategy(balancer::BalancingStrategy::HybridAdaptive);
        g_loadBalancer->setResourceWeights(0.3, 0.25, 0.25, 0.2); // CPU, Memory, Network, Energy
        g_loadBalancer->setAdaptiveThresholds(0.8, 0.7);
        spdlog::info("Load balancer initialized with hybrid adaptive strategy");
        
        // Initialize kernels
        spdlog::info("Initializing kernels...");
        
        // Core kernel
        auto coreKernel = std::make_shared<kernel::CoreKernel>("core_main");
        coreKernel->setPreloadManager(g_preloadManager);
        coreKernel->setLoadBalancer(g_loadBalancer);
        if (!coreKernel->initialize()) {
            throw std::runtime_error("Failed to initialize core kernel");
        }
        g_kernels.push_back(coreKernel);
        spdlog::info("Core kernel initialized");
        
        // Micro kernels
        for (int i = 0; i < 4; ++i) {
            auto microKernel = std::make_shared<kernel::MicroKernel>("micro_" + std::to_string(i));
            microKernel->setPreloadManager(g_preloadManager);
            microKernel->setLoadBalancer(g_loadBalancer);
            if (!microKernel->initialize()) {
                throw std::runtime_error("Failed to initialize micro kernel " + std::to_string(i));
            }
            g_kernels.push_back(microKernel);
        }
        spdlog::info("{} micro kernels initialized", 4);
        
        // Orchestration kernel
        auto orchestrationKernel = std::make_shared<kernel::OrchestrationKernel>();
        if (!orchestrationKernel->initialize()) {
            throw std::runtime_error("Failed to initialize orchestration kernel");
        }
        g_kernels.push_back(orchestrationKernel);
        spdlog::info("Orchestration kernel initialized");
        
        spdlog::info("All components initialized successfully");
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize components: {}", e.what());
        throw;
    }
}

// Main service loop
void runServiceLoop() {
    spdlog::info("Starting service loop...");
    
    auto lastMetricsUpdate = std::chrono::steady_clock::now();
    auto lastRecoveryCheckpoint = std::chrono::steady_clock::now();
    
    while (g_running) {
        try {
            auto now = std::chrono::steady_clock::now();
            
            // Update metrics every 5 seconds
            if (now - lastMetricsUpdate > std::chrono::seconds(5)) {
                for (auto& kernel : g_kernels) {
                    kernel->updateMetrics();
                }
                g_preloadManager->updateMetrics();
                g_threadPool->updateMetrics();
                lastMetricsUpdate = now;
                
                spdlog::debug("Metrics updated");
            }
            
            // Create recovery checkpoint every 30 seconds
            if (now - lastRecoveryCheckpoint > std::chrono::seconds(30)) {
                std::string checkpointId = g_recoveryManager->createRecoveryPoint();
                if (!checkpointId.empty()) {
                    spdlog::info("Recovery checkpoint created: {}", checkpointId);
                }
                lastRecoveryCheckpoint = now;
            }
            
            // Process some sample tasks
            g_threadPool->enqueue([]() {
                // Simulate some background work
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            });
            
            // Sleep for a short time
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
        } catch (const std::exception& e) {
            spdlog::error("Error in service loop: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    spdlog::info("Service loop stopped");
}

// Graceful shutdown
void shutdown() {
    spdlog::info("Initiating graceful shutdown...");
    
    try {
        // Stop accepting new tasks
        g_running = false;
        
        // Create final recovery checkpoint
        std::string finalCheckpoint = g_recoveryManager->createRecoveryPoint();
        if (!finalCheckpoint.empty()) {
            spdlog::info("Final recovery checkpoint created: {}", finalCheckpoint);
        }
        
        // Shutdown kernels
        spdlog::info("Shutting down kernels...");
        for (auto& kernel : g_kernels) {
            kernel->shutdown();
        }
        g_kernels.clear();
        
        // Shutdown components
        if (g_preloadManager) {
            g_preloadManager->shutdown();
        }
        
        if (g_securityManager) {
            g_securityManager->shutdown();
        }
        
        if (g_recoveryManager) {
            g_recoveryManager->shutdown();
        }
        
        if (g_threadPool) {
            g_threadPool->stop();
        }
        
        spdlog::info("All components shut down successfully");
        
    } catch (const std::exception& e) {
        spdlog::error("Error during shutdown: {}", e.what());
    }
}

int main(int argc, char* argv[]) {
    try {
        // Set up signal handlers
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        
        // Initialize logging
        initializeLogging();
        
        // Initialize components
        initializeComponents();
        
        // Run service loop
        runServiceLoop();
        
        // Graceful shutdown
        shutdown();
        
        spdlog::info("=== Cloud IaaS Service Shutdown Complete ===");
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        if (spdlog::get("cloud_service")) {
            spdlog::critical("Fatal error: {}", e.what());
        }
        return 1;
    }
} 