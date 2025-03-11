cmake_minimum_required (VERSION 3.11)
project (cpp_app CXX)

set(SRCFILE "src/hello_world.cpp" CACHE STRING "C++ source file")
option(NATIVE   "Enable native performance" ON)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -std=c++17 -O2 -g3")

set(SOURCES
	${SRCFILE}
)

add_executable(cpp_app ${SOURCES})
set_target_properties(cpp_app PROPERTIES CXX_STANDARD 17)
if (NATIVE)
	target_compile_options(cpp_app PRIVATE -march=skylake -Ofast -fno-fast-math)
endif()
if (SRCFILE STREQUAL "src/espeak.cpp")
	target_link_libraries(cpp_app espeak-ng sonic)
endif()

add_subdirectory(simdjson)
add_subdirectory(ext/json)
add_subdirectory(ext/Turbo-Base64)

target_link_libraries(cpp_app -static simdjson base64 nlohmann_json m)
