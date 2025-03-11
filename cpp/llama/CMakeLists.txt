cmake_minimum_required (VERSION 3.11)
project (llamacpp CXX C)

option(NATIVE   "Enable native performance" ON)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -std=c++17 -O2")
set(CMAKE_C_FLAGS "-Wall -Wno-unused-parameter -std=gnu11 -O2")

set(SOURCES
	src/llama.cpp
	llama.cpp/common/common.cpp
	llama.cpp/common/grammar-parser.cpp
)

add_executable(llamapp ${SOURCES})
set_target_properties(llamapp PROPERTIES CXX_STANDARD 17)

add_subdirectory(../ext/json nlohmann)

add_subdirectory(llama.cpp llamacpp)
target_compile_definitions(llama PUBLIC LOG_DEFAULT_FILE_NAME="")
if (NATIVE)
	target_compile_options(llama PUBLIC -march=native -Ofast -ffast-math)
	target_compile_options(ggml PUBLIC -march=native -Ofast -ffast-math)
endif()

target_link_libraries(llamapp -static nlohmann_json llama -Wl,-s,-X -fuse-ld=lld)
