#!/bin/bash

# Build script for MQTT integration test
# This can be run with: bun run build_mqtt_test.sh

set -e

echo "Building MQTT integration test..."

# Create build directory if it doesn't exist
mkdir -p test/build

# Compile the test
g++ -std=c++17 \
    -I. \
    -o test/build/test_mqtt_integration \
    test/test_mqtt_integration.cpp

echo "Build successful! Test binary created at: test/build/test_mqtt_integration"
echo ""
echo "To run the test:"
echo "  ./test/build/test_mqtt_integration"
echo ""
echo "Or with bun:"
echo "  bun run test_mqtt_integration"
