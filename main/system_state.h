#pragma once

#include "data_structures.h"
#include <string>

class SystemStateManager {
public:
    // Get current system state
    static SystemState getCurrentState();
    
    // Update system state
    static void updateWifiState(bool connected);
    static void updateMqttState(bool connected);
    static void updateMdnsState(bool available);
    static void updateBrokerInfo(const std::string& broker, int port);
    static void updateUptime(uint64_t uptime_seconds);
    static void incrementErrorCount();
    static void incrementMessageCount();
    
    // Create device status JSON
    static std::string createDeviceStatusJson();
    
    // Reset system state
    static void reset();
    
private:
    static SystemState current_state_;
};

