cmake_minimum_required(VERSION 3.25)

project(shared_mpsc_queue LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(mpsc_producer producer.cpp)
target_link_libraries(mpsc_producer PRIVATE Threads::Threads)

add_executable(mpsc_consumer consumer.cpp)
target_link_libraries(mpsc_consumer PRIVATE Threads::Threads)

