#pragma once

#include "data_structures.h"
#include <vector>

class DeviceMonitor {
public:
    // Execute device commands (pure function)
    static std::vector<DeviceCommandResult> executeDeviceCommands(const std::vector<DeviceCommand>& commands);
    
    // Execute single device command (pure function)
    static DeviceCommandResult executeDeviceCommand(const DeviceCommand& command);
    
    // Set pin value (pure function)
    static DeviceCommandResult setPinValue(int pin, int value);
    
    // Read pin value (pure function)
    static DeviceCommandResult readPinValue(int pin);
    
    // Initialize GPIO pins
    static void initializePins();
};

