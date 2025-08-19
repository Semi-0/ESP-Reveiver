cmake_minimum_required(VERSION 3.16)

project(message_processor_tests)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(CJSON REQUIRED libcjson)

# Include directories
include_directories(../main)
include_directories(${CJSON_INCLUDE_DIRS})

# Add source files
set(SOURCES
    test_message_processor.cpp
    ../main/message_processor.cpp
)

# Create test executable
add_executable(test_message_processor ${SOURCES})

# Link libraries
target_link_libraries(test_message_processor ${CJSON_LIBRARIES})

# Add compiler flags
target_compile_options(test_message_processor PRIVATE ${CJSON_CFLAGS_OTHER})

# Mock definitions to avoid ESP-IDF dependencies
target_compile_definitions(test_message_processor PRIVATE 
    -DESP_LOGI=printf
    -DESP_LOGW=printf
    -DESP_LOGE=printf
    -DAPP_TAG="TEST"
)

# Override PinController with mock
target_compile_definitions(test_message_processor PRIVATE 
    -DPinController=MockPinController
)
