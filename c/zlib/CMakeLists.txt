cmake_minimum_required (VERSION 3.11)
project (cpp_app C)

option(NATIVE   "Enable native performance" ON)

set(CMAKE_C_FLAGS "-Wall -Wextra -Wno-unused-parameter -std=gnu17 -O2 -g3")

set(SOURCES
	zlib.c
)

add_executable(zlibng ${SOURCES})
if (NATIVE)
	target_compile_options(zlibng PRIVATE -march=native -Ofast -fno-fast-math)
endif()

add_subdirectory(zlib-ng)

target_link_libraries(zlibng -static zlib)
