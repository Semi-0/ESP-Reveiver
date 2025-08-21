#include "device_monitor.h"
#include "pin_controller.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <algorithm>

static const char* TAG = "DEVICE_MONITOR";

// Initialize GPIO pins
void DeviceMonitor::initializePins() {
    // Configure default pin as output
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << 2); // GPIO2
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    ESP_LOGI(TAG, "GPIO pins initialized");
}

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
        case PIN_SET: {
            // Validate value for digital/analog operations
            if (!isValidDigitalValue(command.value) && !isValidAnalogValue(command.value)) {
                return createFailureResult(command, "Invalid pin value");
            }
            
            // Map digital values: 0 = LOW, 1 = HIGH
            if (isValidDigitalValue(command.value)) {
                bool high = (command.value == 1);
                PinController::digital_write(command.pin, high);
                std::string action = "Digital write: pin " + std::to_string(command.pin) + 
                                   " = " + (high ? "HIGH" : "LOW");
                return createSuccessResult(command, action);
            } else {
                // Analog value (0-255)
                PinController::analog_write(command.pin, command.value);
                std::string action = "Analog write: pin " + std::to_string(command.pin) + 
                                   " = " + std::to_string(command.value);
                return createSuccessResult(command, action);
            }
        }
            
        case PIN_READ: {
            // Configure pin as input and read value
            PinController::configure_pin_if_needed(command.pin, GPIO_MODE_INPUT);
            int value = gpio_get_level((gpio_num_t)command.pin);
            std::string action = "Pin read: pin " + std::to_string(command.pin) + 
                               " = " + std::to_string(value);
            return DeviceCommandResult::success_result(action, command.pin, value);
        }
            
        case PIN_MODE: {
            if (!isValidDigitalValue(command.value)) {
                return createFailureResult(command, "Invalid mode value (0=INPUT, 1=OUTPUT)");
            }
            // Set pin mode: 0 = INPUT, 1 = OUTPUT
            gpio_mode_t mode = (command.value == 0) ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT;
            PinController::configure_pin_if_needed(command.pin, mode);
            std::string action = "Pin mode set: pin " + std::to_string(command.pin) + 
                               " = " + (mode == GPIO_MODE_INPUT ? "INPUT" : "OUTPUT");
            return createSuccessResult(command, action);
        }
            
        case DEVICE_STATUS:
            return createSuccessResult(command, "Device status requested");
            
        case DEVICE_RESET:
            return createSuccessResult(command, "Device reset requested");
            
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
