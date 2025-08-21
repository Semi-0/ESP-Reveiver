# Test CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Include the main project's CMakeLists.txt to get component definitions
include(${CMAKE_CURRENT_SOURCE_DIR}/../CMakeLists.txt)

# Test executable for eventbus implementation
idf_component_register(
    SRCS "test_eventbus_implementation.cpp"
    INCLUDE_DIRS "../main"
    REQUIRES unity freertos esp_common
)

# Set up the test
idf_build_executable(
    "test_eventbus_implementation"
    "test_eventbus_implementation.cpp"
)
