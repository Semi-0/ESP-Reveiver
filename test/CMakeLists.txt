cmake_minimum_required(VERSION 3.16)

# Set project name
project(mqtt-receiver-tests)

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

# Add existing tests
add_executable(test_device_id test_device_id.cpp)
target_compile_options(test_device_id PRIVATE -Wall -Wextra -std=c++17)
target_link_libraries(test_device_id)

add_executable(test_device_id_standalone test_device_id_standalone.cpp)
target_compile_options(test_device_id_standalone PRIVATE -Wall -Wextra -std=c++17)
target_link_libraries(test_device_id_standalone)

add_executable(test_functional_utils test_functional_utils.cpp)
target_compile_options(test_functional_utils PRIVATE -Wall -Wextra -std=c++17)
target_link_libraries(test_functional_utils)

add_executable(test_lightweight_reactive test_lightweight_reactive.cpp)
target_compile_options(test_lightweight_reactive PRIVATE -Wall -Wextra -std=c++17)
target_link_libraries(test_lightweight_reactive)

add_executable(test_lightweight_standalone test_lightweight_standalone.cpp)
target_compile_options(test_lightweight_standalone PRIVATE -Wall -Wextra -std=c++17)
target_link_libraries(test_lightweight_standalone)

add_executable(test_message_processor test_message_processor.cpp)
target_compile_options(test_message_processor PRIVATE -Wall -Wextra -std=c++17)
target_link_libraries(test_message_processor)

# Add test targets
add_custom_target(run_tests
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_curried_events
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_main_loop_events
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_srp_architecture
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_device_id
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_device_id_standalone
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_functional_utils
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_lightweight_reactive
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_lightweight_standalone
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_message_processor
    DEPENDS test_curried_events test_main_loop_events test_srp_architecture test_device_id test_device_id_standalone test_functional_utils test_lightweight_reactive test_lightweight_standalone test_message_processor
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running all tests"
)
