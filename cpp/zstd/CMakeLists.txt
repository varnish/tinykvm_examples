cmake_minimum_required (VERSION 3.11)
project (zstd CXX C)

option(NATIVE   "Enable native performance" ON)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -std=c++17 -O2")
set(CMAKE_C_FLAGS "-Wall -Wno-unused-parameter -std=gnu11 -O2")

set(SOURCES
	src/zstd.cpp
)

add_executable(zstdpp ${SOURCES})
set_target_properties(zstdpp PROPERTIES CXX_STANDARD 17)
if (NATIVE)
	target_compile_options(zstdpp PRIVATE -march=skylake -Ofast -fno-fast-math)
endif()

add_subdirectory(../ext/json nlohmann)

option(ZSTD_MULTITHREAD_SUPPORT "" OFF)
option(ZSTD_BUILD_SHARED "" OFF)
option(ZSTD_BUILD_STATIC "" ON)
add_subdirectory(zstd/build/cmake)

target_link_libraries(zstdpp -static nlohmann_json libzstd_static -Wl,-s,-X)
