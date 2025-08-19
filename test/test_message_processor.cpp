#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include "../main/message_processor.h"

// Mock PinController for testing (we don't want to actually control pins in tests)
class MockPinController {
public:
    struct DigitalCall {
        int pin;
        bool value;
    };
    
    struct AnalogCall {
        int pin;
        int value;
    };
    
    static std::vector<DigitalCall> digital_calls;
    static std::vector<AnalogCall> analog_calls;
    
    static void clear_calls() {
        digital_calls.clear();
        analog_calls.clear();
    }
    
    static void digital_write(int pin, bool high) {
        digital_calls.push_back({pin, high});
    }
    
    static void analog_write(int pin, int value) {
        analog_calls.push_back({pin, value});
    }
};

// Initialize static members
std::vector<MockPinController::DigitalCall> MockPinController::digital_calls;
std::vector<MockPinController::AnalogCall> MockPinController::analog_calls;

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

// ===== JSON PARSING TESTS =====

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

void test_parse_valid_analog_json() {
    std::cout << "Testing: Parse valid analog JSON..." << std::endl;
    
    std::string json = R"({"type": "analog", "pin": 9, "value": 255})";
    auto result = MessageProcessor::parse_json_message(json);
    
    assert_equals(true, result.success, "Parse should succeed");
    assert_equals(1, (int)result.commands.size(), "Should have 1 command");
    assert_equals((int)CommandType::ANALOG, (int)result.commands[0].type, "Command type should be ANALOG");
    assert_equals(9, result.commands[0].pin, "Pin should be 9");
    assert_equals(255, result.commands[0].value, "Value should be 255");
    
    std::cout << "✓ PASSED" << std::endl;
}

void test_parse_multiple_commands_json() {
    std::cout << "Testing: Parse multiple commands JSON..." << std::endl;
    
    std::string json = R"([
        {"type": "digital", "pin": 13, "value": 1},
        {"type": "analog", "pin": 9, "value": 128},
        {"type": "digital", "pin": 12, "value": 0}
    ])";
    
    auto result = MessageProcessor::parse_json_message(json);
    
    assert_equals(true, result.success, "Parse should succeed");
    assert_equals(3, (int)result.commands.size(), "Should have 3 commands");
    
    // Check first command
    assert_equals((int)CommandType::DIGITAL, (int)result.commands[0].type, "First command type");
    assert_equals(13, result.commands[0].pin, "First command pin");
    assert_equals(1, result.commands[0].value, "First command value");
    
    // Check second command
    assert_equals((int)CommandType::ANALOG, (int)result.commands[1].type, "Second command type");
    assert_equals(9, result.commands[1].pin, "Second command pin");
    assert_equals(128, result.commands[1].value, "Second command value");
    
    // Check third command
    assert_equals((int)CommandType::DIGITAL, (int)result.commands[2].type, "Third command type");
    assert_equals(12, result.commands[2].pin, "Third command pin");
    assert_equals(0, result.commands[2].value, "Third command value");
    
    std::cout << "✓ PASSED" << std::endl;
}

void test_parse_invalid_json() {
    std::cout << "Testing: Parse invalid JSON..." << std::endl;
    
    std::string json = R"({"type": "invalid", "pin": 13, "value": 1})";
    auto result = MessageProcessor::parse_json_message(json);
    
    assert_equals(false, result.success, "Parse should fail");
    assert_equals(0, (int)result.commands.size(), "Should have 0 commands");
    
    std::cout << "✓ PASSED" << std::endl;
}

void test_parse_malformed_json() {
    std::cout << "Testing: Parse malformed JSON..." << std::endl;
    
    std::string json = R"({"type": "digital", "pin":})";
    auto result = MessageProcessor::parse_json_message(json);
    
    assert_equals(false, result.success, "Parse should fail");
    assert_equals(std::string("Failed to parse JSON"), result.error_message, "Should have error message");
    
    std::cout << "✓ PASSED" << std::endl;
}

void test_parse_missing_fields() {
    std::cout << "Testing: Parse JSON with missing fields..." << std::endl;
    
    std::string json = R"({"type": "digital", "pin": 13})";  // missing value
    auto result = MessageProcessor::parse_json_message(json);
    
    assert_equals(false, result.success, "Parse should fail");
    
    std::cout << "✓ PASSED" << std::endl;
}

// ===== COMMAND EXECUTION TESTS =====

void test_execute_digital_command_high() {
    std::cout << "Testing: Execute digital command (HIGH)..." << std::endl;
    
    PinCommand command = {CommandType::DIGITAL, 13, 1};
    auto result = MessageProcessor::execute_pin_command(command);
    
    assert_equals(true, result.success, "Execution should succeed");
    assert_equals(std::string(""), result.error_message, "No error message expected");
    assert_equals(std::string("Digital write: pin 13 = HIGH"), result.action_description, "Action description");
    
    std::cout << "✓ PASSED" << std::endl;
}

void test_execute_digital_command_low() {
    std::cout << "Testing: Execute digital command (LOW)..." << std::endl;
    
    PinCommand command = {CommandType::DIGITAL, 12, 0};
    auto result = MessageProcessor::execute_pin_command(command);
    
    assert_equals(true, result.success, "Execution should succeed");
    assert_equals(std::string("Digital write: pin 12 = LOW"), result.action_description, "Action description");
    
    std::cout << "✓ PASSED" << std::endl;
}

void test_execute_analog_command() {
    std::cout << "Testing: Execute analog command..." << std::endl;
    
    PinCommand command = {CommandType::ANALOG, 9, 255};
    auto result = MessageProcessor::execute_pin_command(command);
    
    assert_equals(true, result.success, "Execution should succeed");
    assert_equals(std::string("Analog write: pin 9 = 255"), result.action_description, "Action description");
    
    std::cout << "✓ PASSED" << std::endl;
}

void test_execute_multiple_commands() {
    std::cout << "Testing: Execute multiple commands..." << std::endl;
    
    std::vector<PinCommand> commands = {
        {CommandType::DIGITAL, 13, 1},
        {CommandType::ANALOG, 9, 128},
        {CommandType::DIGITAL, 12, 0}
    };
    
    auto results = MessageProcessor::execute_commands(commands);
    
    assert_equals(3, (int)results.size(), "Should have 3 results");
    
    // Check all commands succeeded
    for (const auto& result : results) {
        assert_equals(true, result.success, "All commands should succeed");
    }
    
    assert_equals(std::string("Digital write: pin 13 = HIGH"), results[0].action_description, "First result");
    assert_equals(std::string("Analog write: pin 9 = 128"), results[1].action_description, "Second result");
    assert_equals(std::string("Digital write: pin 12 = LOW"), results[2].action_description, "Third result");
    
    std::cout << "✓ PASSED" << std::endl;
}

// ===== INTEGRATION TESTS =====

void test_parse_single_json_command() {
    std::cout << "Testing: Parse single JSON command..." << std::endl;
    
    std::string json = R"({"type": "digital", "pin": 13, "value": 1})";
    auto command = MessageProcessor::parse_single_json_command(json);
    
    assert_equals(true, command.has_value(), "Should have value");
    assert_equals((int)CommandType::DIGITAL, (int)command->type, "Command type");
    assert_equals(13, command->pin, "Pin number");
    assert_equals(1, command->value, "Pin value");
    
    std::cout << "✓ PASSED" << std::endl;
}

void test_parse_invalid_single_json_command() {
    std::cout << "Testing: Parse invalid single JSON command..." << std::endl;
    
    std::string json = R"({"type": "unknown", "pin": 13, "value": 1})";
    auto command = MessageProcessor::parse_single_json_command(json);
    
    assert_equals(false, command.has_value(), "Should not have value");
    
    std::cout << "✓ PASSED" << std::endl;
}

// ===== EDGE CASES =====

void test_digital_command_with_non_zero_value() {
    std::cout << "Testing: Digital command with non-zero value (should be HIGH)..." << std::endl;
    
    PinCommand command = {CommandType::DIGITAL, 13, 42};  // any non-zero should be HIGH
    auto result = MessageProcessor::execute_pin_command(command);
    
    assert_equals(true, result.success, "Execution should succeed");
    assert_equals(std::string("Digital write: pin 13 = HIGH"), result.action_description, "Non-zero should be HIGH");
    
    std::cout << "✓ PASSED" << std::endl;
}

void test_empty_array_json() {
    std::cout << "Testing: Empty array JSON..." << std::endl;
    
    std::string json = R"([])";
    auto result = MessageProcessor::parse_json_message(json);
    
    assert_equals(false, result.success, "Empty array should fail");
    assert_equals(0, (int)result.commands.size(), "Should have 0 commands");
    
    std::cout << "✓ PASSED" << std::endl;
}

// ===== TEST RUNNER =====

void run_all_tests() {
    std::cout << "=== Running Message Processor Unit Tests ===" << std::endl;
    
    // JSON Parsing Tests
    test_parse_valid_digital_json();
    test_parse_valid_analog_json();
    test_parse_multiple_commands_json();
    test_parse_invalid_json();
    test_parse_malformed_json();
    test_parse_missing_fields();
    
    // Command Execution Tests
    test_execute_digital_command_high();
    test_execute_digital_command_low();
    test_execute_analog_command();
    test_execute_multiple_commands();
    
    // Integration Tests
    test_parse_single_json_command();
    test_parse_invalid_single_json_command();
    
    // Edge Cases
    test_digital_command_with_non_zero_value();
    test_empty_array_json();
    
    std::cout << "\n=== ALL TESTS PASSED! ===" << std::endl;
}

int main() {
    run_all_tests();
    return 0;
}
