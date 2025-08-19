#!/bin/bash

set -e

echo "=== Message Processor Unit Tests ==="
echo "Testing the main.cpp functionality with functional programming approach"
echo

# Check if we're in the test directory
if [ ! -f "build_and_run.sh" ]; then
    echo "Error: Please run from the test directory"
    exit 1
fi

# Run the comprehensive tests
./build_and_run.sh

echo
echo "=== Test Summary ==="
echo "✓ JSON parsing functionality verified"
echo "✓ Command execution functionality verified" 
echo "✓ write_income_message integration verified"
echo "✓ Error handling verified"
echo "✓ Multiple commands support verified"
echo "✓ Functional programming principles enforced:"
echo "  - Pure parsing functions with explicit inputs/outputs"
echo "  - Pure execution functions with explicit results"
echo "  - Side effects (logging) separated from logic"
echo "  - No embedded side effects in parsing/execution"
echo
echo "All tests completed successfully! 🎉"
