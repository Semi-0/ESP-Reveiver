#pragma once

#include "data_structures.h"
#include "eventbus/EventBus.h"
#include "mqtt_client.h" // ESP-IDF MQTT client header
#include <string>
#include <functional>

// MQTT connection result
struct MqttConnectionResult {
    bool success;
    std::string error_message;
    int error_code;
    
    MqttConnectionResult() : success(false), error_code(0) {}
    MqttConnectionResult(bool s, const std::string& err = "", int code = 0)
        : success(s), error_message(err), error_code(code) {}
};

class MqttClient {
    // Friend function to allow event handler to access private members
    friend void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

public:
    // Initialize MQTT client
    static MqttConnectionResult initialize(const MqttConnectionData& connection_data);
    
    // Connect to MQTT broker
    static MqttConnectionResult connect(const MqttConnectionData& connection_data);
    
    // Disconnect from MQTT broker
    static void disconnect();
    
    // Check if connected
    static bool isConnected();
    
    // Subscribe to topic
    static bool subscribe(const std::string& topic, int qos = 0);
    
    // Publish message
    static bool publish(const std::string& topic, const std::string& message, int qos = 0);
    
    // Set message callback
    static void setMessageCallback(std::function<void(const MqttMessageData&)> callback);
    
    // Get current broker info
    static std::string getCurrentBroker();
    static int getCurrentPort();
    
    // Set event bus for publishing events
    static void setEventBus(IEventBus* event_bus);
    
private:
    static IEventBus* event_bus_;
    static std::function<void(const MqttMessageData&)> message_callback_;
    static std::string current_broker_;
    static int current_port_;
    static bool connected_;
};

