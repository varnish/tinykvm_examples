cmake_minimum_required (VERSION 3.5.1)
project (gamebro CXX)

set(CMAKE_CXX_STANDARD 17)
set(COMMON "-static -O2 -march=native -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON}")

option(GAMEBRO_INDEXED_FRAME "" ON)
add_subdirectory(gamebro libgbc)
option(SPNG_SHARED "" OFF)
option(SPNG_STATIC "" ON)
add_subdirectory(gamebro/ext/libspng libspng)

add_executable(gbcemu
	"src/main.cpp"
)
target_link_libraries(gbcemu gbc spng_static)

target_include_directories(gbcemu PRIVATE ${CMAKE_SOURCE_DIR}/..)
target_include_directories(gbcemu PRIVATE ${CMAKE_SOURCE_DIR}/../ext)
