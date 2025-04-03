cmake_minimum_required (VERSION 3.11)
project (cpp_examples CXX)

option(NATIVE   "Enable native performance" ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -std=c++17")

function (create_program name)
	add_executable(${name} ${ARGN})
	set_target_properties(${name} PROPERTIES CXX_STANDARD 17)
	if (NATIVE)
		target_compile_options(${name} PRIVATE -march=skylake -O3 -fno-fast-math)
	endif()
	target_link_libraries(${name} -static)
endfunction()

add_subdirectory(ext/libdeflate)
add_subdirectory(simdjson)
add_subdirectory(ext/json)
add_subdirectory(ext/Turbo-Base64)

create_program(deflate
	src/deflate.cpp
)
target_link_libraries(deflate libdeflate_static nlohmann_json)

create_program(espeak
	src/espeak.cpp
)
target_link_libraries(espeak espeak-ng sonic)

create_program(hello_world
	src/hello_world.cpp
)

create_program(hello_frontend
	src/hello_frontend.cpp
)

create_program(minify
	src/minify.cpp
)
target_link_libraries(minify simdjson)

create_program(basicauth
	src/basicauth.cpp
)
target_link_libraries(basicauth base64)

create_program(fetch
	src/fetch.cpp
)
target_link_libraries(fetch nlohmann_json)

create_program(to_string
	src/to_string.cpp
)
