cmake_minimum_required(VERSION 3.20)
project(itmo_notification LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
    message(FATAL_ERROR "GCC >= 11 required, got ${CMAKE_CXX_COMPILER_VERSION}")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
    message(FATAL_ERROR "Clang >= 14 required, got ${CMAKE_CXX_COMPILER_VERSION}")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_library(itmo_notification_warnings INTERFACE)
target_compile_options(itmo_notification_warnings INTERFACE
    -Wall -Wextra -Wpedantic
)

option(ITMO_NOTIFICATION_TSAN "Build with ThreadSanitizer" OFF)
if(ITMO_NOTIFICATION_TSAN)
    message(STATUS "ThreadSanitizer enabled")
    add_compile_options(-fsanitize=thread -g -O1 -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.15.3
)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

add_library(notification_lib STATIC
    src/notification.cpp
    src/notification_service.cpp
)
target_include_directories(notification_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(notification_lib PUBLIC itmo_notification_warnings)

add_executable(notification_service app/main.cpp)
target_link_libraries(notification_service
    PRIVATE notification_lib
            httplib::httplib
            nlohmann_json::nlohmann_json
            itmo_notification_warnings
)

enable_testing()
add_executable(notification_tests
    tests/test_notification_service.cpp
)
target_link_libraries(notification_tests
    PRIVATE notification_lib
            GTest::gtest_main
            itmo_notification_warnings
)
include(GoogleTest)
gtest_discover_tests(notification_tests)

add_executable(notification_bench bench/bench_notification_service.cpp)
target_link_libraries(notification_bench
    PRIVATE notification_lib
            benchmark::benchmark
            benchmark::benchmark_main
            nlohmann_json::nlohmann_json
            itmo_notification_warnings
)
target_compile_definitions(notification_bench PRIVATE
    ITMO_NOTIFICATION_DEFAULT_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data/notifications.jsonl"
    ITMO_NOTIFICATION_DEFAULT_META="${CMAKE_CURRENT_SOURCE_DIR}/data/meta.json"
)
