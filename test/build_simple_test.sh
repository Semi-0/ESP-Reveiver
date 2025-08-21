#!/bin/bash

# Simple test build script for EventBus implementation
set -e

echo "=== Building Simple EventBus Test ==="

# Set up environment
export IDF_PATH=${IDF_PATH:-$HOME/esp/esp-idf}
source $IDF_PATH/export.sh

# Create a simple test project structure
mkdir -p test_simple/main
mkdir -p test_simple/main/eventbus

# Copy the EventBus files
cp main/eventbus/EventBus.h test_simple/main/eventbus/
cp main/eventbus/TinyEventBus.h test_simple/main/eventbus/
cp main/eventbus/EventProtocol.h test_simple/main/eventbus/

# Copy the test file
cp test/test_eventbus_simple.cpp test_simple/main/main.cpp

# Create CMakeLists.txt for the test
cat > test_simple/CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(eventbus_test)
EOF

# Create component CMakeLists.txt
cat > test_simple/main/CMakeLists.txt << 'EOF'
idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES unity freertos esp_common
)
EOF

# Build the test
cd test_simple
idf.py build

echo "=== Simple EventBus Test Built Successfully ==="
echo "To flash and test:"
echo "  idf.py flash monitor"
echo ""
echo "Or to run on specific port:"
echo "  idf.py -p /dev/cu.usbserial-1120 flash monitor"
