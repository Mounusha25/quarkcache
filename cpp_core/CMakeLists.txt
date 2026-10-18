cmake_minimum_required(VERSION 3.16)
project(QuarkCache VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(QUARKCACHE_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)

# Produce compile_commands.json for IDE support.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --------------------------------------------------------------------------- #
#  Compiler flags
# --------------------------------------------------------------------------- #
add_compile_options(
    -Wall -Wextra -Wpedantic
    -O2
)

if(QUARKCACHE_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# --------------------------------------------------------------------------- #
#  Library: quarkcache_core
# --------------------------------------------------------------------------- #
add_library(quarkcache_core STATIC
    src/aof.cpp
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

# --------------------------------------------------------------------------- #
#  Tests
# --------------------------------------------------------------------------- #
include(CTest)

if(BUILD_TESTING)
    add_executable(quarkcache_unit_tests tests/cache_tests.cpp)
    target_link_libraries(quarkcache_unit_tests PRIVATE quarkcache_core)
    add_test(NAME quarkcache_unit_tests COMMAND quarkcache_unit_tests)

    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(
        NAME quarkcache_protocol_tests
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/protocol_test.py
                $<TARGET_FILE:quarkcache_server>
    )
    set_tests_properties(quarkcache_protocol_tests PROPERTIES TIMEOUT 20)
endif()
