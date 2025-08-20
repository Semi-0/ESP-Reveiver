cmake_minimum_required(VERSION 3.16)

# Set project name
project(mqtt-receiver-functional)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(main)

# Add main executable
add_executable(mqtt-receiver-functional
    main/main.cpp
    main/device_commands.cpp
    main/device_monitor.cpp
    main/message_processor.cpp
    main/config.cpp
    main/pin_controller.cpp
    main/declarative_events.cpp
)

# Add declarative example executable
add_executable(declarative-example
    main/declarative_example.cpp
    main/declarative_events.cpp
)

# Set compiler flags
target_compile_options(mqtt-receiver-functional PRIVATE -Wall -Wextra -std=c++17)
target_compile_options(declarative-example PRIVATE -Wall -Wextra -std=c++17)

# Link libraries (you may need to adjust these based on your ESP-IDF setup)
target_link_libraries(mqtt-receiver-functional)
target_link_libraries(declarative-example)

# Add test directory
add_subdirectory(test)
