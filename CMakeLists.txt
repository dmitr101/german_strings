cmake_minimum_required(VERSION 3.14)

# Need to switch to a custom toolchain file for vcpkg and presets, as well as triplet overlays/custom triblet files to switch between libstdc++ and libc++
# https://learn.microsoft.com/en-us/vcpkg/users/buildsystems/cmake-integration
# https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html
# https://learn.microsoft.com/en-us/vcpkg/users/buildsystems/cmake-integration#vcpkg_overlay_triplets

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "Vcpkg toolchain file")
endif()

project(german_strings_talk
        VERSION 0.1.0
        DESCRIPTION "German String Implementation"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE LIB_SOURCES "src/german_string.cpp")
add_library(${PROJECT_NAME}_lib ${LIB_SOURCES})

target_include_directories(${PROJECT_NAME}_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

find_package(benchmark REQUIRED)
find_package(GTest CONFIG REQUIRED)

file(GLOB_RECURSE BENCHMARK_SOURCES "benchmark/*.cpp")
add_executable(${PROJECT_NAME}_benchmarks ${BENCHMARK_SOURCES})
target_link_libraries(${PROJECT_NAME}_benchmarks PRIVATE ${PROJECT_NAME}_lib benchmark::benchmark)

# Define the test target (placeholder, no test files found)
file(GLOB_RECURSE TEST_SOURCES "test/*.cpp")
if(TEST_SOURCES)
    add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})
    target_link_libraries(${PROJECT_NAME}_tests PRIVATE ${PROJECT_NAME}_lib)
    target_link_libraries(${PROJECT_NAME}_tests PRIVATE GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)
endif()

# Update compiler options for all targets
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME}_lib PRIVATE -Wall -Wextra -Wpedantic) # -fsanitize=address -static-libsan)
    target_compile_options(${PROJECT_NAME}_benchmarks PRIVATE -Wall -Wextra -Wpedantic)
    if(TARGET ${PROJECT_NAME}_tests)
        target_compile_options(${PROJECT_NAME}_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(${PROJECT_NAME}_tests PUBLIC -fsanitize=address,undefined)
        target_link_options(${PROJECT_NAME}_tests PUBLIC -fsanitize=address,undefined)
    endif()
elseif(MSVC)
    target_compile_options(${PROJECT_NAME}_lib PRIVATE /W4)
    target_compile_options(${PROJECT_NAME}_benchmarks PRIVATE /W4)
    if(TARGET ${PROJECT_NAME}_tests)
        target_compile_options(${PROJECT_NAME}_tests PRIVATE /W4)
    endif()
endif()

# # Use libc++ if available
# if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
#     set(LIBCXX_INCLUDE_DIR "/usr/include/llvm-20/libc++")
#     set(LIBCXXABI_INCLUDE_DIR "/usr/include/llvm-20/libc++abi")
#     target_include_directories(${PROJECT_NAME}_lib PRIVATE ${LIBCXX_INCLUDE_DIR} ${LIBCXXABI_INCLUDE_DIR})
#     target_include_directories(${PROJECT_NAME}_benchmarks PRIVATE ${LIBCXX_INCLUDE_DIR} ${LIBCXXABI_INCLUDE_DIR})
#     if(TARGET ${PROJECT_NAME}_tests)
#         target_include_directories(${PROJECT_NAME}_tests PRIVATE ${LIBCXX_INCLUDE_DIR} ${LIBCXXABI_INCLUDE_DIR})
#     endif()
# endif()

# Install targets (optional)
install(TARGETS ${PROJECT_NAME}_lib ${PROJECT_NAME}_benchmarks
        RUNTIME DESTINATION bin)