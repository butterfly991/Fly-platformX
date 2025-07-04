cmake_minimum_required(VERSION 3.15)
project(CloudIaaS VERSION 1.0.0 LANGUAGES CXX C)

# Установка стандарта C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Опции компиляции
option(CLOUD_ENABLE_LOGGING "Enable logging" ON)
option(CLOUD_ENABLE_METRICS "Enable metrics collection" ON)
option(CLOUD_ENABLE_PERFORMANCE_OPTIMIZATIONS "Enable performance optimizations" ON)
option(BUILD_TESTING "Build tests" ON)

# Флаги компилятора
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
    
    # Включаем предупреждения
    add_compile_options(-Wall -Wextra -Wpedantic)
    
    # Включаем оптимизации
    if(CLOUD_ENABLE_PERFORMANCE_OPTIMIZATIONS)
        add_compile_options(-O3 -march=native)
    endif()
endif()

# Поиск необходимых пакетов
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem thread)

# Добавление исходных файлов
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.c"
)

# Добавление заголовочных файлов
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OPENSSL_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)

# Создание исполняемого файла
add_executable(${PROJECT_NAME} ${SOURCES})

# Линковка с библиотеками
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    ${Boost_LIBRARIES}
)

# Установка выходной директории
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Установка директории для объектных файлов
set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Включение тестов
if(BUILD_TESTING)
    enable_testing()
    include_directories(include src)

    # Собираем все исходники для тестов
    file(GLOB KERNEL_SOURCES src/core/kernel/*.cpp)
    file(GLOB CACHE_SOURCES src/core/cache/*.cpp)
    file(GLOB BALANCER_SOURCES src/core/balancer/*.cpp)
    file(GLOB SECURITY_SOURCES src/core/security/*.cpp)
    file(GLOB DRIVER_SOURCES src/core/drivers/*.cpp)
    file(GLOB RECOVERY_SOURCES src/core/recovery/*.cpp)
    file(GLOB THREAD_SOURCES src/core/thread/*.cpp)

    set(ALL_CORE_SOURCES
        ${KERNEL_SOURCES}
        ${CACHE_SOURCES}
        ${BALANCER_SOURCES}
        ${SECURITY_SOURCES}
        ${DRIVER_SOURCES}
        ${RECOVERY_SOURCES}
        ${THREAD_SOURCES}
    )

    add_executable(KernelSmokeTest tests/core/kernel/KernelSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(KernelSmokeTest PRIVATE include src)
    target_link_libraries(KernelSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES})
    add_test(NAME KernelSmokeTest COMMAND KernelSmokeTest)

    add_executable(DynamicCacheSmokeTest tests/core/cache/DynamicCacheSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(DynamicCacheSmokeTest PRIVATE include src)
    target_link_libraries(DynamicCacheSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES})
    add_test(NAME DynamicCacheSmokeTest COMMAND DynamicCacheSmokeTest)

    add_executable(LoadBalancerSmokeTest tests/core/balancer/LoadBalancerSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(LoadBalancerSmokeTest PRIVATE include src)
    target_link_libraries(LoadBalancerSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES})
    add_test(NAME LoadBalancerSmokeTest COMMAND LoadBalancerSmokeTest)

    add_executable(SecurityManagerSmokeTest tests/core/security/SecurityManagerSmokeTest.cpp ${ALL_CORE_SOURCES})
    target_include_directories(SecurityManagerSmokeTest PRIVATE include src)
    target_link_libraries(SecurityManagerSmokeTest PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${Boost_LIBRARIES})
    add_test(NAME SecurityManagerSmokeTest COMMAND SecurityManagerSmokeTest)
endif()

# Установка
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Установка заголовочных файлов
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

# Генерация документации
find_package(Doxygen)
if(DOXYGEN_FOUND)
    # configure_file(${CMAKE_SOURCE_DIR}/docs/Doxyfile.in ${CMAKE_BINARY_DIR}/Doxyfile @ONLY)
    
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif() 