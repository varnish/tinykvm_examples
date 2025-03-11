cmake_minimum_required (VERSION 3.11)
project (webpencoder CXX C)

option(NATIVE   "Enable native performance" ON)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -std=c++17 -O2")
set(CMAKE_C_FLAGS "-Wall -Wno-unused-parameter -std=gnu11 -O2")

set(SOURCES
	src/webp.cpp
	src/decode_webp.cpp
	src/decode_jpeg.cpp
)

add_executable(webpencoder ${SOURCES})
set_target_properties(webpencoder PROPERTIES CXX_STANDARD 17)
if (NATIVE)
	target_compile_options(webpencoder PRIVATE -march=skylake -Ofast -fno-fast-math)
endif()

option(BUILD_SHARED_LIBS "" OFF)
option(ENABLE_SHARED "" OFF)
option(WEBP_LINK_STATIC "" ON)
option(WEBP_ENABLE_SIMD "" ON)
option(WEBP_USE_THREAD "" OFF)
add_subdirectory(libwebp)

add_subdirectory(libjpeg-turbo)
add_subdirectory(../ext/json json)

target_link_libraries(webpencoder -static webp nlohmann_json turbojpeg-static -Wl,-s,-X)
target_include_directories(webpencoder PRIVATE
	libjpeg-turbo ${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo
)
