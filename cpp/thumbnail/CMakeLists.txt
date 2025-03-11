cmake_minimum_required (VERSION 3.11)
project (thumbnails CXX)

option(NATIVE   "Enable native performance" ON)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -std=c++17 -O2 -g3")

set(SOURCES
	src/thumbnails.cpp
)
set(ARCH
	#-msse -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -mavx -mavx2 -mno-fma4 -mno-xop -mfma -maes -mpclmul
	-march=skylake
)

add_executable(thumbnails ${SOURCES})
set_target_properties(thumbnails PROPERTIES CXX_STANDARD 17)
if (NATIVE)
	target_compile_options(thumbnails PRIVATE ${ARCH} -Ofast -fno-fast-math)
endif()

add_subdirectory(../ext/json nlohmann)

add_subdirectory(Simd/prj/cmake)
target_compile_options(Simd PUBLIC -Wno-unused)
# Disable AVX-512 due to build errors
target_compile_definitions(Simd PUBLIC -DSIMD_AVX512_DISABLE=1)
target_compile_definitions(Simd PUBLIC -DSIMD_AVX512BW_DISABLE=1)
target_compile_definitions(Simd PUBLIC -DSIMD_AVX512BF16_DISABLE=1)
target_compile_definitions(Simd PUBLIC -DSIMD_AVX512VNNI_DISABLE=1)

target_link_libraries(thumbnails -static Simd nlohmann_json pthread m)
target_include_directories(thumbnails PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Simd/src)
