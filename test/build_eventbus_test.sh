#!/bin/bash
set -e

echo "Building Event Bus and Flow Graph Unit Test..."

# Create build directory
mkdir -p test/build

# Build the test
g++ -std=c++17 \
    -Wall -Wextra -Wpedantic \
    -I. \
    -o test/build/test_eventbus_flow \
    test/test_eventbus_flow.cpp \
    -lpthread

echo "Build complete!"
echo "Run with: ./test/build/test_eventbus_flow"
