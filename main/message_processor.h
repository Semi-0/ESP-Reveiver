#ifndef MESSAGE_PROCESSOR_H
#define MESSAGE_PROCESSOR_H

#include <string>
#include <vector>
#include <optional>

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

struct ExecutionResult {
    bool success;
    std::string error_message;
    std::string action_description;
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
