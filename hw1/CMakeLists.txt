cmake_minimum_required(VERSION 3.20)

project(hw1_parallel_apply CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

add_library(parallel_lib_interface INTERFACE)
target_include_directories(parallel_lib_interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(run_tests tests.cpp)
target_link_libraries(run_tests PRIVATE GTest::gtest_main parallel_lib_interface)

enable_testing()
add_test(NAME hw1_tests COMMAND run_tests)

add_executable(run_benchmarks benchmarks.cpp)
target_link_libraries(run_benchmarks PRIVATE benchmark::benchmark parallel_lib_interface)