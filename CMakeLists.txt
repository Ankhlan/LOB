cmake_minimum_required(VERSION 3.15)
project(LOB VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include directories
include_directories(include)
include_directories(src)

# Source files
set(SOURCES
    src/main.cpp
    src/orderbook.cpp
    src/matching_engine.cpp
    src/risk_engine.cpp
)

# Main executable
add_executable(lob_engine ${SOURCES})

# Link libraries (add as needed)
# find_package(Threads REQUIRED)
# target_link_libraries(lob_engine PRIVATE Threads::Threads)

# Enable warnings
if(MSVC)
    target_compile_options(lob_engine PRIVATE /W4)
else()
    target_compile_options(lob_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()
