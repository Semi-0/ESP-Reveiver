#ifndef DEVICE_MONITOR_H
#define DEVICE_MONITOR_H

#include "device_commands.h"
#include <vector>

// Device monitor interface for pin operations
class DeviceMonitor {
public:
    // Pure function to execute a single device command
    static DeviceCommandResult executeDeviceCommand(const DevicePinCommand& command);
    
    // Pure function to execute multiple device commands
    static std::vector<DeviceCommandResult> executeDeviceCommands(const std::vector<DevicePinCommand>& commands);
    
    // Pure function to validate pin number
    static bool isValidPin(int pin);
    
    // Pure function to validate pin value for digital operations
    static bool isValidDigitalValue(int value);
    
    // Pure function to validate pin value for analog operations
    static bool isValidAnalogValue(int value);
    
    // Pure function to create success result
    static DeviceCommandResult createSuccessResult(const DevicePinCommand& command, const std::string& action);
    
    // Pure function to create failure result
    static DeviceCommandResult createFailureResult(const DevicePinCommand& command, const std::string& error);
};

#endif // DEVICE_MONITOR_H
