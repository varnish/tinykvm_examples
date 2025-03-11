cmake_minimum_required (VERSION 3.11)
project (sqlite CXX)

option(NATIVE   "Enable native performance" OFF)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -std=c++17 -O2 -g3")

set(SOURCES
	src/sql.cpp
)

add_executable(sqlite ${SOURCES})
set_target_properties(sqlite PROPERTIES CXX_STANDARD 17)
if (NATIVE)
	target_compile_options(sqlite PRIVATE -march=native -Ofast -fno-fast-math)
endif()

add_subdirectory(../ext/json nlohmann)
add_subdirectory(SQLiteCpp)

target_link_libraries(sqlite -static SQLiteCpp sqlite3 nlohmann_json pthread m)
