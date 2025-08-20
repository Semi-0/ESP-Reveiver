#include "device_monitor.h"
#include "config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <sstream>

static const char* TAG = "DEVICE_MONITOR";

std::vector<DeviceCommandResult> DeviceMonitor::executeDeviceCommands(const std::vector<DeviceCommand>& commands) {
    std::vector<DeviceCommandResult> results;
    
    for (const auto& command : commands) {
        DeviceCommandResult result = executeDeviceCommand(command);
        results.push_back(result);
    }
    
    return results;
}

DeviceCommandResult DeviceMonitor::executeDeviceCommand(const DeviceCommand& command) {
    if (command.action == "set") {
        return setPinValue(command.pin, command.value);
    } else if (command.action == "read") {
        return readPinValue(command.pin);
    } else {
        return DeviceCommandResult(command.pin, command.value, false, 
                                 command.description, "Unknown action: " + command.action);
    }
}

DeviceCommandResult DeviceMonitor::setPinValue(int pin, int value) {
    gpio_num_t gpio_pin = static_cast<gpio_num_t>(pin);
    
    // Configure pin as output if not already configured
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        std::string error = "Failed to configure GPIO pin " + std::to_string(pin);
        return DeviceCommandResult(pin, value, false, "Set pin " + std::to_string(pin), error);
    }
    
    // Set pin value
    ret = gpio_set_level(gpio_pin, value);
    if (ret != ESP_OK) {
        std::string error = "Failed to set GPIO pin " + std::to_string(pin) + " to " + std::to_string(value);
        return DeviceCommandResult(pin, value, false, "Set pin " + std::to_string(pin), error);
    }
    
    std::string description = "Set pin " + std::to_string(pin) + " to " + std::to_string(value);
    return DeviceCommandResult(pin, value, true, description);
}

DeviceCommandResult DeviceMonitor::readPinValue(int pin) {
    gpio_num_t gpio_pin = static_cast<gpio_num_t>(pin);
    
    // Configure pin as input if not already configured
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        std::string error = "Failed to configure GPIO pin " + std::to_string(pin) + " as input";
        return DeviceCommandResult(pin, 0, false, "Read pin " + std::to_string(pin), error);
    }
    
    // Read pin value
    int value = gpio_get_level(gpio_pin);
    
    std::string description = "Read pin " + std::to_string(pin) + " = " + std::to_string(value);
    return DeviceCommandResult(pin, value, true, description);
}

void DeviceMonitor::initializePins() {
    ESP_LOGI(TAG, "Initializing GPIO pins");
    
    // Initialize default pins as outputs
    for (int i = 0; i < PIN_COUNT; i++) {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << i);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        
        esp_err_t ret = gpio_config(&io_conf);
        if (ret == ESP_OK) {
            gpio_set_level(static_cast<gpio_num_t>(i), 0); // Set to low initially
        }
    }
    
    ESP_LOGI(TAG, "GPIO pins initialized");
}

