# Set the minimum version of CMake that can be used
# To find the cmake version run
# $ cmake --version
cmake_minimum_required(VERSION 3.16)

# Set the project name
project(alsrac)

# Collect source files
aux_source_directory(${PROJECT_SOURCE_DIR}/src sources)

# Add include paths
include_directories(${TOP_DIR}/abc/src)
include_directories(${TOP_DIR}/espresso/src)
include_directories(${PROJECT_SOURCE_DIR}/src)

# Add definitions
add_definitions(-DLIN64 -g -Wall -O3 -std=c++2a)

# Set output path
set(EXECUTABLE_OUTPUT_PATH ${TOP_DIR})
message(STATUS "EXECUTABLE_OUTPUT_PATH = ${EXECUTABLE_OUTPUT_PATH}")

# Add an executable
add_executable(${PROJECT_NAME}.out ${sources})
target_link_libraries(${PROJECT_NAME}.out espresso libabc)