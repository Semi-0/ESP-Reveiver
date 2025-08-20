#include "device_monitor.h"
#include "pin_controller.h"
#include <algorithm>

// Pure function to validate pin number
bool DeviceMonitor::isValidPin(int pin) {
    return pin >= 0 && pin <= 40;
}

// Pure function to validate pin value for digital operations
bool DeviceMonitor::isValidDigitalValue(int value) {
    return value == 0 || value == 1;
}

// Pure function to validate pin value for analog operations
bool DeviceMonitor::isValidAnalogValue(int value) {
    return value >= 0 && value <= 255;
}

// Pure function to create success result
DeviceCommandResult DeviceMonitor::createSuccessResult(const DevicePinCommand& command, const std::string& action) {
    return DeviceCommandResult::success_result(action, command.pin, command.value);
}

// Pure function to create failure result
DeviceCommandResult DeviceMonitor::createFailureResult(const DevicePinCommand& command, const std::string& error) {
    return DeviceCommandResult::failure_result(error, command.pin);
}

// Pure function to execute a single device command
DeviceCommandResult DeviceMonitor::executeDeviceCommand(const DevicePinCommand& command) {
    // Validate command first
    if (!isValidDeviceCommand(command)) {
        return createFailureResult(command, "Invalid device command");
    }
    
    // Validate pin
    if (!isValidPin(command.pin)) {
        return createFailureResult(command, "Invalid pin number");
    }
    
    // Execute based on command type
    switch (command.type) {
        case PIN_SET:
            // Validate value for digital/analog operations
            if (!isValidDigitalValue(command.value) && !isValidAnalogValue(command.value)) {
                return createFailureResult(command, "Invalid pin value");
            }
            
            // In a real implementation, this would call PinController
            // For now, we'll simulate success
            return createSuccessResult(command, "Pin set successfully");
            
        case PIN_READ:
            // In a real implementation, this would read from PinController
            // For now, we'll simulate success
            return createSuccessResult(command, "Pin read successfully");
            
        case PIN_MODE:
            if (!isValidDigitalValue(command.value)) {
                return createFailureResult(command, "Invalid mode value (0=INPUT, 1=OUTPUT)");
            }
            // In a real implementation, this would set pin mode
            return createSuccessResult(command, "Pin mode set successfully");
            
        default:
            return createFailureResult(command, "Unknown command type");
    }
}

// Pure function to execute multiple device commands
std::vector<DeviceCommandResult> DeviceMonitor::executeDeviceCommands(const std::vector<DevicePinCommand>& commands) {
    std::vector<DeviceCommandResult> results;
    results.reserve(commands.size());
    
    for (const auto& command : commands) {
        results.push_back(executeDeviceCommand(command));
    }
    
    return results;
}
