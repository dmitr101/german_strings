cmake_minimum_required(VERSION 3.14)

# Need to switch to a custom toolchain file for vcpkg and presets, as well as triplet overlays/custom triblet files to switch between libstdc++ and libc++
# https://learn.microsoft.com/en-us/vcpkg/users/buildsystems/cmake-integration
# https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html
# https://learn.microsoft.com/en-us/vcpkg/users/buildsystems/cmake-integration#vcpkg_overlay_triplets

# https://github.com/Neumann-A/my-vcpkg-triplets/ seems to be a good source for custom triplets
# https://github.com/qietv/vcpkg-ohos-overlay/blob/main/triplets/arm64-ohos-static.cmake

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

# Enable folder organization in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set default build type for single-config generators
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Default build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

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
add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})
target_link_libraries(${PROJECT_NAME}_tests PRIVATE ${PROJECT_NAME}_lib)
target_link_libraries(${PROJECT_NAME}_tests PRIVATE GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)

# Add test to CTest
enable_testing()
add_test(NAME ${PROJECT_NAME}_tests COMMAND ${PROJECT_NAME}_tests)

add_executable(1brc_base 1brc_base/main.cpp)
add_executable(1brc_base_views 1brc_base_views/main.cpp)
add_executable(1brc_gs 1brc_gs/main.cpp)
target_link_libraries(1brc_gs PRIVATE ${PROJECT_NAME}_lib)

# Set up folder organization for IDE
set_target_properties(${PROJECT_NAME}_lib PROPERTIES FOLDER "Libraries")
set_target_properties(${PROJECT_NAME}_benchmarks PROPERTIES FOLDER "Benchmarks")
set_target_properties(${PROJECT_NAME}_tests PROPERTIES FOLDER "Tests")
set_target_properties(1brc_base PROPERTIES FOLDER "1BRC")
set_target_properties(1brc_base_views PROPERTIES FOLDER "1BRC")
set_target_properties(1brc_gs PROPERTIES FOLDER "1BRC")

# Function to apply common compiler options to targets
function(apply_compiler_options target_name)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target_name} PRIVATE 
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wnon-virtual-dtor -Wcast-align
            -Wunused -Woverloaded-virtual -Wpedantic -Wconversion -Wsign-conversion
            -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches
            -Wlogical-op -Wnull-dereference -Wuseless-cast -Wdouble-promotion
            -Wformat=2
        )
        
        # Additional options for Clang
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target_name} PRIVATE
                -Wlifetime -Wmost -Wextra-semi -Wunreachable-code
            )
        endif()
        
        # Debug configuration options
        target_compile_options(${target_name} PRIVATE
            $<$<CONFIG:Debug>:-O0 -g3 -DDEBUG>
        )
        
        # Release configuration options
        target_compile_options(${target_name} PRIVATE
            $<$<CONFIG:Release>:-O3 -DNDEBUG -march=native>
            $<$<CONFIG:RelWithDebInfo>:-O2 -g -DNDEBUG>
        )
        
    elseif(MSVC)
        target_compile_options(${target_name} PRIVATE
            /W4                     # High warning level
            /permissive-            # Strict conformance mode
            /Zc:__cplusplus         # Report correct __cplusplus value
            /Zc:externConstexpr     # Enable extern constexpr variables
            /Zc:inline              # Remove unreferenced inline functions
            /Zc:preprocessor        # Use conforming preprocessor
            /Zc:throwingNew         # Assume operator new throws
            /EHsc                   # Exception handling model
            /utf-8                  # Use UTF-8 for source and execution character sets
            /MP                     # Multi-processor compilation
            /wd4996                 # Disable deprecated function warnings
        )
        
        # Release optimizations
        target_compile_options(${target_name} PRIVATE $<$<CONFIG:Release>:/O2 /Ob2 /arch:AVX2>)
        
        # Linker optimizations for release builds
        target_link_options(${target_name} PRIVATE
            $<$<CONFIG:Release>:/OPT:REF /OPT:ICF>
            $<$<CONFIG:RelWithDebInfo>:/OPT:REF /OPT:ICF /DEBUG>
        )
        
        # Enable /MP for faster compilation
        target_compile_options(${target_name} PRIVATE /MP)
        
        # Set runtime library consistently
        set_target_properties(${target_name} PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        )
    endif()
endfunction()

# Apply compiler options to all targets
apply_compiler_options(${PROJECT_NAME}_lib)
apply_compiler_options(${PROJECT_NAME}_benchmarks)
apply_compiler_options(${PROJECT_NAME}_tests)
apply_compiler_options(1brc_base)
apply_compiler_options(1brc_base_views)
apply_compiler_options(1brc_gs)

# Special sanitizer options for tests (only for GCC/Clang, not Windows)  
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT WIN32)
    target_compile_options(${PROJECT_NAME}_tests PUBLIC -fsanitize=address,undefined)
    target_link_options(${PROJECT_NAME}_tests PUBLIC -fsanitize=address,undefined)
endif()

# Windows-specific definitions to avoid common issues
if(WIN32)
    target_compile_definitions(${PROJECT_NAME}_lib PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(${PROJECT_NAME}_benchmarks PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(${PROJECT_NAME}_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(1brc_base PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(1brc_base_views PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(1brc_gs PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Install targets (optional)
install(TARGETS ${PROJECT_NAME}_lib ${PROJECT_NAME}_benchmarks 1brc_base 1brc_base_views 1brc_gs
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)