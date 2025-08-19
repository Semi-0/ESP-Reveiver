# Message Processor Unit Tests

This directory contains comprehensive unit tests for the main.cpp functionality, specifically testing the JSON parsing and command execution parts with a functional programming approach.

## What We Test

### 1. JSON Parsing Functionality
- ✅ Parse valid digital commands: `{"type": "digital", "pin": 13, "value": 1}`
- ✅ Parse valid analog commands: `{"type": "analog", "pin": 9, "value": 255}`
- ✅ Parse multiple commands in arrays
- ✅ Handle invalid command types
- ✅ Handle malformed JSON
- ✅ Handle missing required fields
- ✅ Handle empty arrays

### 2. Command Execution Functionality
- ✅ Execute digital commands (HIGH/LOW)
- ✅ Execute analog commands with PWM values
- ✅ Execute multiple commands in sequence
- ✅ Proper conversion of non-zero values to HIGH for digital commands

### 3. Integration Testing
- ✅ Test the exact `write_income_message` function from main.cpp
- ✅ Verify the functional programming approach:
  - Pure parsing functions with explicit inputs/outputs
  - Pure execution functions with explicit results
  - Side effects (logging) separated from logic
  - No embedded side effects in parsing/execution

### 4. Error Handling
- ✅ Invalid JSON format handling
- ✅ Unsupported command types
- ✅ Missing or malformed fields
- ✅ Proper error reporting through explicit result types

## Functional Programming Principles Verified

### ✅ Explicit Inputs and Outputs
- `parse_json_message(string) → ParseResult`
- `execute_commands(vector<PinCommand>) → vector<ExecutionResult>`

### ✅ Pure Functions
- Parsing functions only transform data, no side effects
- Execution functions return explicit results instead of embedded logging

### ✅ Separated Concerns
- **Parsing**: Pure data transformation
- **Execution**: Pure function with explicit results  
- **Logging**: Side effects handled separately in main.cpp

### ✅ Composability
- Functions can be easily tested independently
- Clear data flow from input → parse → execute → log

## Running the Tests

### Quick Test Run
```bash
cd test
./run_tests.sh
```

### Manual Test Build
```bash
cd test  
./build_and_run.sh
```

## Test Output Example

```
=== Running Message Processor Integration Tests ===
Testing: Parse valid digital JSON...
✓ PASSED
Testing: Parse multiple commands JSON...
✓ PASSED
Testing: Execute commands...
PinController::digital_write(pin=13, high=true)
✓ PASSED
Testing: Main.cpp integration...
Calling write_income_message with: {"type": "digital", "pin": 13, "value": 1}
PinController::digital_write(pin=13, high=true)
[INFO] Command executed: Digital write: pin 13 = HIGH
✓ PASSED
Testing: Main.cpp with multiple commands...
Calling write_income_message with multiple commands...
PinController::digital_write(pin=13, high=true)
PinController::analog_write(pin=9, value=255)
PinController::digital_write(pin=12, high=false)
[INFO] Command executed: Digital write: pin 13 = HIGH
[INFO] Command executed: Analog write: pin 9 = 255
[INFO] Command executed: Digital write: pin 12 = LOW
✓ PASSED
Testing: Main.cpp with invalid message...
Calling write_income_message with invalid message...
[WARN] Failed to parse message: No valid commands found
✓ PASSED

=== ALL TESTS PASSED! ===
```

## Architecture Benefits Demonstrated

1. **Testability**: Each function can be tested independently
2. **Maintainability**: Clear separation of concerns
3. **Readability**: Explicit data flow
4. **Reliability**: Comprehensive error handling
5. **Functional Style**: Pure functions with explicit inputs/outputs

The tests prove that the refactored code follows functional programming principles while maintaining full functionality for the ESP32 MQTT receiver project.
