#pragma once

#include <string>
#include <vector>
#include <memory>

// Service discovery result for explicit data passing
struct ServiceDiscoveryData {
    std::string service_name;
    std::string host;
    int port;
    bool valid;
    
    ServiceDiscoveryData() : port(0), valid(false) {}
    ServiceDiscoveryData(const std::string& name, const std::string& h, int p) 
        : service_name(name), host(h), port(p), valid(true) {}
    
    static ServiceDiscoveryData invalid() {
        return ServiceDiscoveryData();
    }
};

// MQTT connection data for explicit data passing
struct MqttConnectionData {
    std::string broker_host;
    int broker_port;
    std::string client_id;
    
    MqttConnectionData() : broker_port(0) {}
    MqttConnectionData(const std::string& host, int port, const std::string& id)
        : broker_host(host), broker_port(port), client_id(id) {}
};

// MQTT message data
struct MqttMessageData {
    std::string topic;
    std::string payload;
    int qos;
    
    MqttMessageData() : qos(0) {}
    MqttMessageData(const std::string& t, const std::string& p, int q = 0)
        : topic(t), payload(p), qos(q) {}
};

// Device command data
struct DeviceCommand {
    int pin;
    int value;
    std::string action;
    std::string description;
    
    DeviceCommand() : pin(0), value(0) {}
    DeviceCommand(int p, int v, const std::string& a, const std::string& desc = "")
        : pin(p), value(v), action(a), description(desc) {}
};

// Device command result
struct DeviceCommandResult {
    int pin;
    int value;
    bool success;
    std::string action_description;
    std::string error_message;
    
    DeviceCommandResult() : pin(0), value(0), success(false) {}
    DeviceCommandResult(int p, int v, bool s, const std::string& desc, const std::string& err = "")
        : pin(p), value(v), success(s), action_description(desc), error_message(err) {}
};

// System state data
struct SystemState {
    bool wifi_connected;
    bool mqtt_connected;
    bool mdns_available;
    std::string current_broker;
    int current_broker_port;
    uint64_t uptime_seconds;
    int error_count;
    int message_count;
    
    SystemState() : wifi_connected(false), mqtt_connected(false), mdns_available(false),
                   current_broker_port(0), uptime_seconds(0), error_count(0), message_count(0) {}
};

// Device status data
struct DeviceStatus {
    std::string device_id;
    std::string status;
    uint64_t uptime_seconds;
    SystemState system_state;
    
    DeviceStatus() : uptime_seconds(0) {}
    DeviceStatus(const std::string& id, const std::string& s, uint64_t uptime)
        : device_id(id), status(s), uptime_seconds(uptime) {}
};


