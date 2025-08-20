#include "device_commands.h"
#include <algorithm>

// Pure function to create device command from pin command
DevicePinCommand createDevicePinCommand(const std::string& message_type, int pin, int value, const std::string& description) {
    DeviceCommandType type = PIN_SET; // Default
    
    if (message_type == "digital") {
        type = PIN_SET;
    } else if (message_type == "analog") {
        type = PIN_SET;
    } else if (message_type == "read") {
        type = PIN_READ;
    } else if (message_type == "mode") {
        type = PIN_MODE;
    }
    
    return DevicePinCommand(type, pin, value, description);
}

// Pure function to validate device command
bool isValidDeviceCommand(const DevicePinCommand& command) {
    // Basic validation
    if (command.pin < 0 || command.pin > 40) {
        return false;
    }
    
    // Value validation based on command type
    switch (command.type) {
        case PIN_SET:
            return command.value >= 0 && command.value <= 255;
        case PIN_READ:
            return true; // Read doesn't need value validation
        case PIN_MODE:
            return command.value == 0 || command.value == 1; // INPUT or OUTPUT
        default:
            return false;
    }
}

// Pure function to create device command event
DeviceCommandEvent createDeviceCommandEvent(const std::vector<DevicePinCommand>& commands, const std::string& source) {
    DeviceCommandEvent event(commands, source);
    // In a real implementation, you might set timestamp here
    return event;
}
