cmake_minimum_required (VERSION 3.11)
project (xml CXX C)

option(NATIVE   "Enable native performance" ON)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -std=c++17 -O2")
set(CMAKE_C_FLAGS "-Wall -Wno-unused-parameter -std=gnu11 -O2")

set(SOURCES
	src/xml.cpp
)

add_executable(xmlpp ${SOURCES})
set_target_properties(xmlpp PROPERTIES CXX_STANDARD 17)
if (NATIVE)
	target_compile_options(xmlpp PRIVATE -march=skylake -Ofast -fno-fast-math)
endif()

add_subdirectory(../ext/json nlohmann)

add_subdirectory(../ext/tinyxml2 tinyxml2)

target_link_libraries(xmlpp -static nlohmann_json tinyxml2 -Wl,-s,-X)
