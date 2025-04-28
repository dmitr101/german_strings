cmake_minimum_required(VERSION 3.14)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "Vcpkg toolchain file")
endif()

project(german_strings_talk
        VERSION 0.1.0
        DESCRIPTION "German Strings Talk Demo"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define the library target
file(GLOB_RECURSE LIB_SOURCES "src/german_string.cpp")
add_library(${PROJECT_NAME}_lib ${LIB_SOURCES})

# Ensure the include directory is properly set for the library target
target_include_directories(${PROJECT_NAME}_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Ensure the benchmark library is found
find_package(benchmark REQUIRED)
find_package(GTest CONFIG REQUIRED)

# Define the benchmark target
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

# Install targets (optional)
install(TARGETS ${PROJECT_NAME}_lib ${PROJECT_NAME}_benchmarks
        RUNTIME DESTINATION bin)