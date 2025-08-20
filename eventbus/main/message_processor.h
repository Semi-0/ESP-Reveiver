#pragma once

#include "data_structures.h"
#include <string>
#include <vector>

// Message processing result
struct MessageProcessingResult {
    bool success;
    std::vector<DeviceCommand> commands;
    std::string error_message;
    
    MessageProcessingResult() : success(false) {}
    MessageProcessingResult(bool s, const std::vector<DeviceCommand>& cmds, const std::string& err = "")
        : success(s), commands(cmds), error_message(err) {}
};

class MessageProcessor {
public:
    // Parse JSON message and convert to device commands (pure function)
    static MessageProcessingResult processMessageToDeviceCommands(const std::string& message);
    
    // Parse JSON message (pure function)
    static bool parseJsonMessage(const std::string& message, std::vector<DeviceCommand>& commands);
    
    // Validate device command (pure function)
    static bool validateDeviceCommand(const DeviceCommand& command);
    
    // Create device command from JSON object (pure function)
    static DeviceCommand createDeviceCommandFromJson(const std::string& json_command);
};

