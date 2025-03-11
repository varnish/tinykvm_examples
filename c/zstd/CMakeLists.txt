cmake_minimum_required (VERSION 3.11)
project (zstd_app C)

option(NATIVE   "Enable native performance" ON)

set(CMAKE_C_FLAGS "-Wall -Wextra -Wno-unused-parameter -std=gnu17 -O2 -g3")

set(SOURCES
	zstd.c
)

add_executable(zstddecompr ${SOURCES})
if (NATIVE)
	target_compile_options(zstddecompr PRIVATE -march=native -Ofast -fno-fast-math)
endif()

option(ZSTD_MULTITHREAD_SUPPORT "" OFF)
option(ZSTD_BUILD_SHARED "" OFF)
option(ZSTD_BUILD_STATIC "" ON)
add_subdirectory(zstd/build/cmake)

target_link_libraries(zstddecompr -static libzstd_static)
