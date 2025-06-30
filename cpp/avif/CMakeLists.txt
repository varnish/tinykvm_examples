cmake_minimum_required (VERSION 3.11)
project (avifencode CXX C)

option(NATIVE   "Enable native performance" ON)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -std=c++17 -O2")
set(CMAKE_C_FLAGS "-Wall -Wno-unused-parameter -std=gnu11 -O2")

if (NATIVE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -Ofast -fno-fast-math")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -Ofast -fno-fast-math")
endif()

option(BUILD_SHARED_LIBS "" OFF)
option(AVIF_CODEC_AOM "" ON)
option(AVIF_LOCAL_AOM "" ON)
option(ENABLE_STATIC "" ON)
option(REQUIRE_SIMD "" ON)
add_subdirectory(libavif)

add_subdirectory(libjpeg-turbo)
add_subdirectory(../ext/json json)

set(SOURCES
	src/avif.cpp
)

add_executable(avifencode ${SOURCES})
set_target_properties(avifencode PROPERTIES CXX_STANDARD 17)
target_link_libraries(avifencode avif nlohmann_json turbojpeg -Wl,-s,-X)
target_link_libraries(avifencode -Wl,-Ttext-segment=0x1FF000)
target_include_directories(avifencode PRIVATE
	libjpeg-turbo ${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo
)

add_library(avif-ffi SHARED
	src/avif-ffi.cpp
)
set_target_properties(avif-ffi PROPERTIES CXX_STANDARD 17)
target_link_libraries(avif-ffi -rdynamic avif nlohmann_json turbojpeg)
target_include_directories(avif-ffi PRIVATE
	libjpeg-turbo ${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo
)
