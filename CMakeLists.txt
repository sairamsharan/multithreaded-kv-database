cmake_minimum_required(VERSION 3.15)
project(KVStore CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files for the core library (everything except main)
set(CORE_SOURCES
    src/kv_store.cpp
    src/thread_pool.cpp
    src/wal.cpp
    src/tcp_server.cpp
)

# Optional: Add threading
find_package(Threads REQUIRED)

# The server executable
add_executable(server src/main.cpp ${CORE_SOURCES})
target_link_libraries(server PRIVATE Threads::Threads)
if(WIN32)
    target_link_libraries(server PRIVATE ws2_32)
endif()

# The benchmark executable
add_executable(benchmark src/benchmark.cpp ${CORE_SOURCES})
target_link_libraries(benchmark PRIVATE Threads::Threads)
if(WIN32)
    target_link_libraries(benchmark PRIVATE ws2_32)
endif()
