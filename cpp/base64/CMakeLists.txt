cmake_minimum_required (VERSION 3.11)
project (base64 CXX C)

option(NATIVE   "Enable native performance" ON)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -std=c++17 -O2")
set(CMAKE_C_FLAGS "-Wall -Wno-unused-parameter -std=gnu11 -O2")

set(SOURCES
	src/base64.cpp
)

add_executable(base64pp ${SOURCES})
set_target_properties(base64pp PROPERTIES CXX_STANDARD 17)
if (NATIVE)
	target_compile_options(base64pp PRIVATE -march=skylake -Ofast -fno-fast-math)
endif()

add_subdirectory(../ext/json nlohmann)

add_subdirectory(../ext/Turbo-Base64 Turbo-Base64)

target_link_libraries(base64pp -static nlohmann_json base64 -Wl,-s,-X)
