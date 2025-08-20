#ifndef MESSAGE_PROCESSOR_H
#define MESSAGE_PROCESSOR_H

#include <string>
#include <vector>
#include <optional>
#include "device_commands.h"

// Define types that can be used across multiple files
enum CommandType {
    DIGITAL,  // Fixed typo from DIGITAl
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

// Execution result type
struct ExecutionResult {
    bool success;
    std::string error_message;
    std::string action_description;
    
    ExecutionResult(bool s, const std::string& err = "", const std::string& action = "")
        : success(s), error_message(err), action_description(action) {}
};

// Device command result for message processor
struct DeviceCommandParseResult {
    bool success;
    std::string error_message;
    std::vector<DevicePinCommand> device_commands;
    
    DeviceCommandParseResult(bool s, const std::string& err = "", const std::vector<DevicePinCommand>& cmds = {})
        : success(s), error_message(err), device_commands(cmds) {}
    
    static DeviceCommandParseResult success_result(const std::vector<DevicePinCommand>& commands) {
        return DeviceCommandParseResult(true, "", commands);
    }
    
    static DeviceCommandParseResult failure_result(const std::string& error) {
        return DeviceCommandParseResult(false, error);
    }
};

class MessageProcessor {
public:
    // Pure parsing functions - no side effects
    static ParseResult parse_json_message(const std::string& message);
    static std::optional<PinCommand> parse_single_json_command(const std::string& json_str);
    
    // Pure function to convert parsed commands to device commands
    static DeviceCommandParseResult convertToDeviceCommands(const ParseResult& parse_result);
    
    // Pure function to process message and return device commands
    static DeviceCommandParseResult processMessageToDeviceCommands(const std::string& message);
    
    // Pure function to execute commands
    static std::vector<ExecutionResult> execute_commands(const std::vector<PinCommand>& commands);
    static ExecutionResult execute_pin_command(const PinCommand& command);
    
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
