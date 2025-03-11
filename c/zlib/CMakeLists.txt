cmake_minimum_required (VERSION 3.11)
project (cpp_app C)

option(NATIVE   "Enable native performance" ON)

set(CMAKE_C_FLAGS "-Wall -Wextra -Wno-unused-parameter -std=gnu17 -O2 -g3")

set(SOURCES
	zlib.c
)

add_executable(inflate ${SOURCES})
if (NATIVE)
	target_compile_options(inflate PRIVATE -march=native -Ofast -fno-fast-math)
endif()

add_subdirectory(zlib-dougallj)

target_link_libraries(inflate -static zlib)
