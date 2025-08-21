#!/bin/bash

# Build script for EventBus implementation tests
set -e

echo "=== Building EventBus Implementation Tests ==="

# Set up environment
export IDF_PATH=${IDF_PATH:-$HOME/esp/esp-idf}
source $IDF_PATH/export.sh

# Build the test
cd test
idf.py build

echo "=== EventBus Tests Built Successfully ==="
echo "To run tests:"
echo "  idf.py flash monitor"
echo ""
echo "Or to run specific test:"
echo "  idf.py flash monitor --target esp32"
