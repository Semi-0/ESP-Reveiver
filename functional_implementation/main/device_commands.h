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

// Device command result
struct DeviceCommandResult {
    bool success;
    std::string error_message;
    std::string action_description;
    int pin;
    int value;
    
    DeviceCommandResult(bool s, const std::string& err = "", const std::string& action = "", int p = 0, int v = 0)
        : success(s), error_message(err), action_description(action), pin(p), value(v) {}
    
    static DeviceCommandResult success_result(const std::string& action, int pin, int value) {
        return DeviceCommandResult(true, "", action, pin, value);
    }
    
    static DeviceCommandResult failure_result(const std::string& error, int pin = 0) {
        return DeviceCommandResult(false, error, "", pin, 0);
    }
};

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
