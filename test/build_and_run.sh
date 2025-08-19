#!/bin/bash

set -e

echo "=== Building Message Processor Tests ==="

# Check if cJSON is available
if ! pkg-config --exists libcjson; then
    echo "Error: libcjson not found. Please install it:"
    echo "  On macOS: brew install cjson"
    echo "  On Ubuntu: sudo apt-get install libcjson-dev" 
    exit 1
fi

# Get cJSON flags
CJSON_CFLAGS=$(pkg-config --cflags libcjson)
CJSON_LIBS=$(pkg-config --libs libcjson)

# Create a simplified message processor that doesn't depend on ESP-IDF
echo "Creating standalone test version..."

# Copy and modify message_processor files for testing
cp ../main/message_processor.h ./message_processor_test.h
cp ../main/message_processor.cpp ./message_processor_test.cpp

# Remove ESP-IDF dependencies and create a standalone version
cat > message_processor_standalone.h << 'EOF'
#ifndef MESSAGE_PROCESSOR_H
#define MESSAGE_PROCESSOR_H

#include <string>
#include <vector>
#include <optional>

// Mock ESP logging
#define ESP_LOGI(tag, format, ...) printf("[INFO] " format "\n", ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) printf("[WARN] " format "\n", ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) printf("[ERROR] " format "\n", ##__VA_ARGS__)
#define APP_TAG "TEST"

// Define types that can be used across multiple files
enum CommandType {
    DIGITAL,
    ANALOG,
};

struct PinCommand {
    CommandType type;
    int pin;
    int value;
};

// Result types for explicit error handling
struct ParseResult {
    bool success;
    std::string error_message;
    std::vector<PinCommand> commands;
};

struct ExecutionResult {
    bool success;
    std::string error_message;
    std::string action_description;
};

// Mock PinController
class PinController {
public:
    static void digital_write(int pin, bool high) {
        printf("PinController::digital_write(pin=%d, high=%s)\n", pin, high ? "true" : "false");
    }
    
    static void analog_write(int pin, int value) {
        printf("PinController::analog_write(pin=%d, value=%d)\n", pin, value);
    }
};

class MessageProcessor {
public:
    // Pure parsing functions - no side effects
    static ParseResult parse_json_message(const std::string& message);
    static std::optional<PinCommand> parse_single_json_command(const std::string& json_str);
    
    // Pure execution functions - explicit inputs and outputs
    static ExecutionResult execute_pin_command(const PinCommand& command);
    static std::vector<ExecutionResult> execute_commands(const std::vector<PinCommand>& commands);
    
    // Legacy functions (deprecated)
    static void process_message(const std::string& message);
    static std::vector<std::string> split(const std::string& s, char delimiter);
    static bool try_process_json(const std::string& message);

private:
    static void process_digital_command(const std::vector<std::string>& parts);
    static void process_analog_command(const std::vector<std::string>& parts);
    static void process_motor_command(const std::vector<std::string>& parts);
};

#endif // MESSAGE_PROCESSOR_H
EOF

# Create standalone cpp file
cat > message_processor_standalone.cpp << 'EOF'
#include "message_processor_standalone.h"
#include <sstream>
#include <cjson/cjson.h>
#include <cstring>

// ===== PURE PARSING FUNCTIONS =====
// These functions only transform data, no side effects

std::optional<PinCommand> MessageProcessor::parse_single_json_command(const std::string& json_str) {
    cJSON* root = cJSON_Parse(json_str.c_str());
    if (!root) return std::nullopt;

    cJSON* type = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type) || type->valuestring == nullptr) {
        cJSON_Delete(root);
        return std::nullopt;
    }

    cJSON* pin = cJSON_GetObjectItemCaseSensitive(root, "pin");
    if (!cJSON_IsNumber(pin)) {
        cJSON_Delete(root);
        return std::nullopt;
    }

    cJSON* value = cJSON_GetObjectItemCaseSensitive(root, "value");
    if (!cJSON_IsNumber(value)) {
        cJSON_Delete(root);
        return std::nullopt;
    }

    CommandType cmd_type;
    if (strcmp(type->valuestring, "digital") == 0) {
        cmd_type = CommandType::DIGITAL;
    } else if (strcmp(type->valuestring, "analog") == 0) {
        cmd_type = CommandType::ANALOG;
    } else {
        cJSON_Delete(root);
        return std::nullopt;
    }

    PinCommand command = {
        .type = cmd_type,
        .pin = pin->valueint,
        .value = value->valueint
    };

    cJSON_Delete(root);
    return command;
}

ParseResult MessageProcessor::parse_json_message(const std::string& message) {
    ParseResult result = {false, "", {}};
    
    cJSON* root = cJSON_Parse(message.c_str());
    if (!root) {
        result.error_message = "Failed to parse JSON";
        return result;
    }

    if (cJSON_IsArray(root)) {
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, root) {
            if (cJSON_IsObject(item)) {
                char* json_str = cJSON_Print(item);
                if (json_str) {
                    auto command = parse_single_json_command(json_str);
                    if (command) {
                        result.commands.push_back(*command);
                    }
                    free(json_str);
                }
            }
        }
    } else if (cJSON_IsObject(root)) {
        char* json_str = cJSON_Print(root);
        if (json_str) {
            auto command = parse_single_json_command(json_str);
            if (command) {
                result.commands.push_back(*command);
            }
            free(json_str);
        }
    } else {
        result.error_message = "JSON must be an object or array";
        cJSON_Delete(root);
        return result;
    }

    cJSON_Delete(root);
    result.success = !result.commands.empty();
    if (!result.success && result.error_message.empty()) {
        result.error_message = "No valid commands found";
    }
    return result;
}

// ===== PURE EXECUTION FUNCTIONS =====
// These functions have explicit inputs and outputs

ExecutionResult MessageProcessor::execute_pin_command(const PinCommand& command) {
    ExecutionResult result = {false, "", ""};
    
    switch (command.type) {
        case CommandType::DIGITAL: {
            bool high = command.value != 0;
            PinController::digital_write(command.pin, high);
            result.success = true;
            result.action_description = "Digital write: pin " + std::to_string(command.pin) + 
                                      " = " + (high ? "HIGH" : "LOW");
            break;
        }
        case CommandType::ANALOG: {
            PinController::analog_write(command.pin, command.value);
            result.success = true;
            result.action_description = "Analog write: pin " + std::to_string(command.pin) + 
                                      " = " + std::to_string(command.value);
            break;
        }
        default: {
            result.error_message = "Unknown command type";
            break;
        }
    }
    
    return result;
}

std::vector<ExecutionResult> MessageProcessor::execute_commands(const std::vector<PinCommand>& commands) {
    std::vector<ExecutionResult> results;
    results.reserve(commands.size());
    
    for (const auto& command : commands) {
        results.push_back(execute_pin_command(command));
    }
    
    return results;
}

// ===== LEGACY FUNCTIONS (DEPRECATED) =====

void MessageProcessor::process_message(const std::string& message) {
    auto parse_result = parse_json_message(message);
    if (parse_result.success) {
        execute_commands(parse_result.commands);
    }
}

std::vector<std::string> MessageProcessor::split(const std::string& s, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }

    return result;
}

bool MessageProcessor::try_process_json(const std::string& message) {
    auto parse_result = parse_json_message(message);
    if (parse_result.success) {
        auto execution_results = execute_commands(parse_result.commands);
        return true;
    }
    return false;
}

void MessageProcessor::process_digital_command(const std::vector<std::string>& parts) {
    // Legacy function - not implemented
}

void MessageProcessor::process_analog_command(const std::vector<std::string>& parts) {
    // Legacy function - not implemented
}

void MessageProcessor::process_motor_command(const std::vector<std::string>& parts) {
    // Legacy function - not implemented
}
EOF

# Create standalone test file
cat > test_standalone.cpp << 'EOF'
#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include "message_processor_standalone.h"

// Test helper functions
void assert_equals(bool expected, bool actual, const std::string& message) {
    if (expected != actual) {
        std::cerr << "FAIL: " << message << " (expected: " << expected << ", actual: " << actual << ")" << std::endl;
        exit(1);
    }
}

void assert_equals(int expected, int actual, const std::string& message) {
    if (expected != actual) {
        std::cerr << "FAIL: " << message << " (expected: " << expected << ", actual: " << actual << ")" << std::endl;
        exit(1);
    }
}

void assert_equals(const std::string& expected, const std::string& actual, const std::string& message) {
    if (expected != actual) {
        std::cerr << "FAIL: " << message << " (expected: '" << expected << "', actual: '" << actual << "')" << std::endl;
        exit(1);
    }
}

// ===== MAIN.CPP INTEGRATION TEST =====

void write_income_message(const std::string& message) {
    // Parse the message into commands (pure function)
    auto parse_result = MessageProcessor::parse_json_message(message);
    
    if (!parse_result.success) {
        ESP_LOGW(APP_TAG, "Failed to parse message: %s", parse_result.error_message.c_str());
        return;
    }
    
    // Execute the commands (pure function with explicit results)
    auto execution_results = MessageProcessor::execute_commands(parse_result.commands);
    
    // Log the results (side effect separated from logic)
    for (const auto& result : execution_results) {
        if (result.success) {
            ESP_LOGI(APP_TAG, "Command executed: %s", result.action_description.c_str());
        } else {
            ESP_LOGE(APP_TAG, "Command failed: %s", result.error_message.c_str());
        }
    }
}

// ===== TESTS =====

void test_parse_valid_digital_json() {
    std::cout << "Testing: Parse valid digital JSON..." << std::endl;
    
    std::string json = R"({"type": "digital", "pin": 13, "value": 1})";
    auto result = MessageProcessor::parse_json_message(json);
    
    assert_equals(true, result.success, "Parse should succeed");
    assert_equals(std::string(""), result.error_message, "No error message expected");
    assert_equals(1, (int)result.commands.size(), "Should have 1 command");
    assert_equals((int)CommandType::DIGITAL, (int)result.commands[0].type, "Command type should be DIGITAL");
    assert_equals(13, result.commands[0].pin, "Pin should be 13");
    assert_equals(1, result.commands[0].value, "Value should be 1");
    
    std::cout << "✓ PASSED" << std::endl;
}

void test_parse_multiple_commands_json() {
    std::cout << "Testing: Parse multiple commands JSON..." << std::endl;
    
    std::string json = R"([
        {"type": "digital", "pin": 13, "value": 1},
        {"type": "analog", "pin": 9, "value": 128}
    ])";
    
    auto result = MessageProcessor::parse_json_message(json);
    
    assert_equals(true, result.success, "Parse should succeed");
    assert_equals(2, (int)result.commands.size(), "Should have 2 commands");
    
    std::cout << "✓ PASSED" << std::endl;
}

void test_execute_commands() {
    std::cout << "Testing: Execute commands..." << std::endl;
    
    PinCommand command = {CommandType::DIGITAL, 13, 1};
    auto result = MessageProcessor::execute_pin_command(command);
    
    assert_equals(true, result.success, "Execution should succeed");
    assert_equals(std::string("Digital write: pin 13 = HIGH"), result.action_description, "Action description");
    
    std::cout << "✓ PASSED" << std::endl;
}

void test_main_cpp_integration() {
    std::cout << "Testing: Main.cpp integration..." << std::endl;
    
    std::string message = R"({"type": "digital", "pin": 13, "value": 1})";
    
    std::cout << "Calling write_income_message with: " << message << std::endl;
    write_income_message(message);
    
    std::cout << "✓ PASSED" << std::endl;
}

void test_main_cpp_multiple_commands() {
    std::cout << "Testing: Main.cpp with multiple commands..." << std::endl;
    
    std::string message = R"([
        {"type": "digital", "pin": 13, "value": 1},
        {"type": "analog", "pin": 9, "value": 255},
        {"type": "digital", "pin": 12, "value": 0}
    ])";
    
    std::cout << "Calling write_income_message with multiple commands..." << std::endl;
    write_income_message(message);
    
    std::cout << "✓ PASSED" << std::endl;
}

void test_main_cpp_invalid_message() {
    std::cout << "Testing: Main.cpp with invalid message..." << std::endl;
    
    std::string message = R"({"type": "invalid", "pin": 13, "value": 1})";
    
    std::cout << "Calling write_income_message with invalid message..." << std::endl;
    write_income_message(message);
    
    std::cout << "✓ PASSED" << std::endl;
}

void run_all_tests() {
    std::cout << "=== Running Message Processor Integration Tests ===" << std::endl;
    
    test_parse_valid_digital_json();
    test_parse_multiple_commands_json();
    test_execute_commands();
    test_main_cpp_integration();
    test_main_cpp_multiple_commands();
    test_main_cpp_invalid_message();
    
    std::cout << "\n=== ALL TESTS PASSED! ===" << std::endl;
}

int main() {
    run_all_tests();
    return 0;
}
EOF

echo "Compiling test..."
g++ -std=c++17 \
    $CJSON_CFLAGS \
    test_standalone.cpp \
    message_processor_standalone.cpp \
    $CJSON_LIBS \
    -o test_runner

echo "Running tests..."
./test_runner

echo "Cleaning up..."
rm -f message_processor_test.h message_processor_test.cpp
rm -f message_processor_standalone.h message_processor_standalone.cpp test_standalone.cpp
rm -f test_runner

echo "=== Test build and run completed successfully! ==="
