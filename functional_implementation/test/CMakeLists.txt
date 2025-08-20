cmake_minimum_required(VERSION 3.16)

# Set project name
project(mqtt-receiver-functional-tests)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)

# Add executable for curried events test
add_executable(test_curried_events test_curried_events.cpp)

# Set compiler flags
target_compile_options(test_curried_events PRIVATE -Wall -Wextra -std=c++17)

# Link libraries
target_link_libraries(test_curried_events)

# Add executable for main loop events test
add_executable(test_main_loop_events test_main_loop_events.cpp)

# Set compiler flags
target_compile_options(test_main_loop_events PRIVATE -Wall -Wextra -std=c++17)

# Link libraries
target_link_libraries(test_main_loop_events)

# Add executable for SRP architecture test
add_executable(test_srp_architecture test_srp_architecture.cpp)

# Set compiler flags
target_compile_options(test_srp_architecture PRIVATE -Wall -Wextra -std=c++17)

# Link libraries
target_link_libraries(test_srp_architecture)

# Add executable for declarative events test
add_executable(test_declarative_events 
    test_declarative_events.cpp
    ../main/declarative_events.cpp
)

# Set compiler flags
target_compile_options(test_declarative_events PRIVATE -Wall -Wextra -std=c++17)

# Include directories
target_include_directories(test_declarative_events PRIVATE ../main)

# Link libraries
target_link_libraries(test_declarative_events)

# Add test targets
add_custom_target(run_tests
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_curried_events
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_main_loop_events
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_srp_architecture
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_declarative_events
    DEPENDS test_curried_events test_main_loop_events test_srp_architecture test_declarative_events
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running functional implementation tests"
)
