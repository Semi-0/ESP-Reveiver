#include "message_processor.h"
#include "config.h"
#include "esp_log.h"
#include <cstring>
#include <sstream>

static const char* TAG = "MESSAGE_PROCESSOR";

MessageProcessingResult MessageProcessor::processMessageToDeviceCommands(const std::string& message) {
    std::vector<DeviceCommand> commands;
    
    if (!parseJsonMessage(message, commands)) {
        return MessageProcessingResult(false, commands, "Failed to parse JSON message");
    }
    
    // Validate all commands
    for (const auto& command : commands) {
        if (!validateDeviceCommand(command)) {
            return MessageProcessingResult(false, commands, "Invalid device command");
        }
    }
    
    return MessageProcessingResult(true, commands);
}

bool MessageProcessor::parseJsonMessage(const std::string& message, std::vector<DeviceCommand>& commands) {
    // Simple JSON parsing for device commands
    // Expected format: {"pin": 2, "value": 1, "action": "set"}
    // or array: [{"pin": 2, "value": 1}, {"pin": 3, "value": 0}]
    
    commands.clear();
    
    // Check if it's an array or single object
    if (message.find('[') != std::string::npos) {
        // Parse array of commands
        size_t start = message.find('[');
        size_t end = message.rfind(']');
        
        if (start == std::string::npos || end == std::string::npos) {
            ESP_LOGE(TAG, "Invalid JSON array format");
            return false;
        }
        
        std::string array_content = message.substr(start + 1, end - start - 1);
        
        // Simple parsing - split by },{
        size_t pos = 0;
        while (pos < array_content.length()) {
            size_t next_pos = array_content.find("},{", pos);
            if (next_pos == std::string::npos) {
                next_pos = array_content.length();
            }
            
            std::string command_str = array_content.substr(pos, next_pos - pos);
            if (command_str.find('{') != std::string::npos) {
                command_str = command_str.substr(command_str.find('{'));
            }
            if (command_str.find('}') != std::string::npos) {
                command_str = command_str.substr(0, command_str.find('}') + 1);
            }
            
            DeviceCommand cmd = createDeviceCommandFromJson(command_str);
            if (cmd.pin >= 0) {
                commands.push_back(cmd);
            }
            
            pos = next_pos + 3;
        }
    } else {
        // Parse single command
        DeviceCommand cmd = createDeviceCommandFromJson(message);
        if (cmd.pin >= 0) {
            commands.push_back(cmd);
        }
    }
    
    return !commands.empty();
}

bool MessageProcessor::validateDeviceCommand(const DeviceCommand& command) {
    // Validate pin range
    if (command.pin < 0 || command.pin >= PIN_COUNT) {
        ESP_LOGE(TAG, "Invalid pin number: %d", command.pin);
        return false;
    }
    
    // Validate value range
    if (command.value < 0 || command.value > 1) {
        ESP_LOGE(TAG, "Invalid pin value: %d", command.value);
        return false;
    }
    
    // Validate action
    if (command.action.empty()) {
        ESP_LOGE(TAG, "Empty action");
        return false;
    }
    
    return true;
}

DeviceCommand MessageProcessor::createDeviceCommandFromJson(const std::string& json_command) {
    DeviceCommand cmd;
    cmd.pin = -1; // Invalid default
    
    // Simple JSON parsing for {"pin": X, "value": Y, "action": "Z"}
    if (json_command.find("\"pin\"") != std::string::npos) {
        size_t pin_start = json_command.find("\"pin\"") + 6;
        size_t pin_end = json_command.find(",", pin_start);
        if (pin_end == std::string::npos) {
            pin_end = json_command.find("}", pin_start);
        }
        if (pin_end != std::string::npos) {
            std::string pin_str = json_command.substr(pin_start, pin_end - pin_start);
            cmd.pin = std::stoi(pin_str);
        }
    }
    
    if (json_command.find("\"value\"") != std::string::npos) {
        size_t value_start = json_command.find("\"value\"") + 8;
        size_t value_end = json_command.find(",", value_start);
        if (value_end == std::string::npos) {
            value_end = json_command.find("}", value_start);
        }
        if (value_end != std::string::npos) {
            std::string value_str = json_command.substr(value_start, value_end - value_start);
            cmd.value = std::stoi(value_str);
        }
    }
    
    if (json_command.find("\"action\"") != std::string::npos) {
        size_t action_start = json_command.find("\"action\"") + 10;
        size_t action_end = json_command.find("\"", action_start);
        if (action_end != std::string::npos) {
            cmd.action = json_command.substr(action_start, action_end - action_start);
        }
    }
    
    // Set default description
    if (cmd.action == "set") {
        cmd.description = "Set pin " + std::to_string(cmd.pin) + " to " + std::to_string(cmd.value);
    } else if (cmd.action == "read") {
        cmd.description = "Read pin " + std::to_string(cmd.pin);
    } else {
        cmd.description = cmd.action + " pin " + std::to_string(cmd.pin);
    }
    
    return cmd;
}

