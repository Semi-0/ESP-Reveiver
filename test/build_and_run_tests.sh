#!/bin/bash

# Build and run TinyEventBus memory corruption tests
set -e

echo "🔍 Building TinyEventBus Memory Corruption Tests"
echo "================================================"

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "📋 Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build the tests
echo "🔨 Building tests..."
make -j$(nproc)

# Run the tests
echo "🧪 Running tests..."
echo "=================="

# Run with verbose output and capture results
./test_tiny_eventbus_memory_corruption --gtest_color=yes --gtest_output=xml:test_results.xml

# Check if tests passed
if [ $? -eq 0 ]; then
    echo "✅ All tests passed!"
else
    echo "❌ Some tests failed!"
    echo "📊 Test results saved to test_results.xml"
    exit 1
fi

echo ""
echo "🎉 Test execution completed successfully!"
