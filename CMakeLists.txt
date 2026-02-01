cmake_minimum_required(VERSION 3.10)
project(MiniShell)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Static compilation flags
set(CMAKE_EXE_LINKER_FLAGS "-static")

# Source files
file(GLOB SOURCES "src/*.cpp")

# Executable
add_executable(minsh ${SOURCES})

# Output directory
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set_target_properties(minsh PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
