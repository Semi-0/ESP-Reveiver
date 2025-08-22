#include "device_monitor.h"
#include "pin_controller.h"
#include "driver/gpio.h"
#include <algorithm>
#include <stdexcept>

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
            
            try {
                // Determine if this is digital (0,1) or analog (0-255) write
                if (isValidDigitalValue(command.value)) {
                    // Digital write
                    PinController::digital_write(command.pin, command.value == 1);
                    return createSuccessResult(command, "Digital pin set successfully");
                } else {
                    // Analog write (PWM)
                    PinController::analog_write(command.pin, command.value);
                    return createSuccessResult(command, "Analog pin set successfully");
                }
            } catch (const std::exception& e) {
                return createFailureResult(command, std::string("Failed to set pin: ") + e.what());
            } catch (...) {
                return createFailureResult(command, "Failed to set pin: unknown error");
            }
            
        case PIN_READ:
            try {
                // Configure pin as input if needed
                PinController::configure_pin_if_needed(command.pin, GPIO_MODE_INPUT);
                
                // Read digital value from pin
                int pin_value = gpio_get_level((gpio_num_t)command.pin);
                
                // Return result with actual read value
                return DeviceCommandResult::success_result("Pin read successfully", command.pin, pin_value);
            } catch (const std::exception& e) {
                return createFailureResult(command, std::string("Failed to read pin: ") + e.what());
            } catch (...) {
                return createFailureResult(command, "Failed to read pin: unknown error");
            }
            
        case PIN_MODE:
            if (!isValidDigitalValue(command.value)) {
                return createFailureResult(command, "Invalid mode value (0=INPUT, 1=OUTPUT)");
            }
            
            try {
                // Set pin mode: 0 = INPUT, 1 = OUTPUT
                gpio_mode_t mode = (command.value == 1) ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT;
                PinController::configure_pin_if_needed(command.pin, mode);
                
                std::string mode_desc = (command.value == 1) ? "OUTPUT" : "INPUT";
                return createSuccessResult(command, "Pin mode set to " + mode_desc);
            } catch (const std::exception& e) {
                return createFailureResult(command, std::string("Failed to set pin mode: ") + e.what());
            } catch (...) {
                return createFailureResult(command, "Failed to set pin mode: unknown error");
            }
            
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
