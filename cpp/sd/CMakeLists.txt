cmake_minimum_required (VERSION 3.11)
project (sdcpp CXX C)

option(NATIVE   "Enable native performance" ON)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -std=c++17 -O2")
set(CMAKE_C_FLAGS "-Wall -Wno-unused-parameter -std=gnu11 -O2")

set(SOURCES
	src/sd.cpp
)

add_executable(sdpp ${SOURCES})
set_target_properties(sdpp PROPERTIES CXX_STANDARD 17)

add_subdirectory(../ext/json nlohmann)

add_subdirectory(stable-diffusion.cpp sdcpp)
if (NATIVE)
	target_compile_options(stable-diffusion PUBLIC -march=native -Ofast -fno-fast-math)
	target_compile_options(ggml PUBLIC -march=native -Ofast -fno-fast-math)
endif()

target_link_libraries(sdpp -static nlohmann_json stable-diffusion -Wl,-s,-X)
