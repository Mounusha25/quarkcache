cmake_minimum_required(VERSION 3.16)
project(QuarkCache VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Produce compile_commands.json for IDE support.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --------------------------------------------------------------------------- #
#  Compiler flags
# --------------------------------------------------------------------------- #
add_compile_options(
    -Wall -Wextra -Wpedantic
    -O2
)

# --------------------------------------------------------------------------- #
#  Library: quarkcache_core
# --------------------------------------------------------------------------- #
add_library(quarkcache_core STATIC
    src/lru_cache.cpp
    src/kv_store.cpp
    src/server.cpp
)

target_include_directories(quarkcache_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# epoll is Linux-only; link pthreads.
find_package(Threads REQUIRED)
target_link_libraries(quarkcache_core PUBLIC Threads::Threads)

# --------------------------------------------------------------------------- #
#  Executable: quarkcache_server
# --------------------------------------------------------------------------- #
add_executable(quarkcache_server src/main.cpp)
target_link_libraries(quarkcache_server PRIVATE quarkcache_core)

# --------------------------------------------------------------------------- #
#  Install target (optional)
# --------------------------------------------------------------------------- #
install(TARGETS quarkcache_server DESTINATION bin)
