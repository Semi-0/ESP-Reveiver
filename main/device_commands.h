#ifndef DEVICE_COMMANDS_H
#define DEVICE_COMMANDS_H

#include <string>
#include <vector>
#include <optional>

// Device command types
enum DeviceCommandType {
    PIN_SET,
    PIN_READ,
    PIN_MODE,
    DEVICE_STATUS,
    DEVICE_RESET
};

// Pin command structure for device operations
struct DevicePinCommand {
    DeviceCommandType type;
    int pin;
    int value;
    std::string description;
    
    DevicePinCommand(DeviceCommandType t, int p, int v, const std::string& desc = "")
        : type(t), pin(p), value(v), description(desc) {}
};

// DeviceCommandResult is defined in data_structures.h

// Device command event data
struct DeviceCommandEvent {
    std::vector<DevicePinCommand> commands;
    std::string source;
    uint64_t timestamp;
    
    DeviceCommandEvent(const std::vector<DevicePinCommand>& cmds, const std::string& src = "mqtt")
        : commands(cmds), source(src), timestamp(0) {}
};

// Pure function to create device command from pin command
DevicePinCommand createDevicePinCommand(const std::string& message_type, int pin, int value, const std::string& description = "");

// Pure function to validate device command
bool isValidDeviceCommand(const DevicePinCommand& command);

// Pure function to create device command event
DeviceCommandEvent createDeviceCommandEvent(const std::vector<DevicePinCommand>& commands, const std::string& source = "mqtt");

#endif // DEVICE_COMMANDS_H
