#pragma once

#include "eventbus/EventProtocol.h"
#include "data_structures.h"
#include "esp_log.h"

// Optional logging handlers - can be enabled/disabled without affecting core logic
class LoggingHandler {
public:
    static void log_wifi_connected(const Event& e, void* user) {
        ESP_LOGI("LOGGING", "WiFi connected event received");
    }
    
    static void log_mdns_found(const Event& e, void* user) {
        const char* host = static_cast<const char*>(e.ptr);
        ESP_LOGI("LOGGING", "mDNS found MQTT broker: %s", host ? host : "<null>");
    }
    
    static void log_mdns_failed(const Event& e, void* user) {
        ESP_LOGE("LOGGING", "mDNS query failed");
    }
    
    static void log_mqtt_connected(const Event& e, void* user) {
        ESP_LOGI("LOGGING", "MQTT connected event received");
    }
    
    static void log_mqtt_disconnected(const Event& e, void* user) {
        ESP_LOGI("LOGGING", "MQTT disconnected event received");
    }
    
    static void log_mqtt_message(const Event& e, void* user) {
        const MqttMessageData* msg_data = static_cast<const MqttMessageData*>(e.ptr);
        if (msg_data) {
            ESP_LOGI("LOGGING", "MQTT message received - Topic: %s, Payload: %s", 
                     msg_data->topic.c_str(), msg_data->payload.c_str());
        }
    }
    
    static void log_system_error(const Event& e, void* user) {
        ESP_LOGE("LOGGING", "System error: %d", e.i32);
    }
    
    static void log_timer_tick(const Event& e, void* user) {
        ESP_LOGI("LOGGING", "Timer tick: %d", e.i32);
    }
    
    // Logging for MQTT connection process
    static void log_mqtt_connection_attempt(const char* host) {
        ESP_LOGI("LOGGING", "Attempting MQTT connection to: %s", host);
    }
    
    static void log_mqtt_connection_success() {
        ESP_LOGI("LOGGING", "MQTT connection successful");
    }
    
    static void log_mqtt_connection_failure(const std::string& error) {
        ESP_LOGE("LOGGING", "MQTT connection failed: %s", error.c_str());
    }
    
    static void log_mqtt_subscription_success(const std::string& topic) {
        ESP_LOGI("LOGGING", "Subscribed to topic: %s", topic.c_str());
    }
    
    static void log_mqtt_subscription_failure(const std::string& topic) {
        ESP_LOGE("LOGGING", "Failed to subscribe to topic: %s", topic.c_str());
    }
    
    static void log_mqtt_publish_success(const std::string& topic) {
        ESP_LOGI("LOGGING", "Published to topic: %s", topic.c_str());
    }
    
    static void log_mqtt_publish_failure(const std::string& topic) {
        ESP_LOGE("LOGGING", "Failed to publish to topic: %s", topic.c_str());
    }
    
    static void log_mdns_query_start() {
        ESP_LOGI("LOGGING", "Starting mDNS query for MQTT broker...");
    }
    
    static void log_mdns_query_success(const char* host) {
        ESP_LOGI("LOGGING", "Found MQTT broker: %s", host);
    }
    
    static void log_mdns_query_failure(const char* error) {
        ESP_LOGE("LOGGING", "mDNS query failed: %s", error);
    }
    
    static void log_mdns_no_broker_found() {
        ESP_LOGE("LOGGING", "No MQTT broker found");
    }
    
    static void log_wifi_disconnect() {
        ESP_LOGI("LOGGING", "WiFi disconnected, attempting to reconnect...");
    }
    
    static void log_wifi_got_ip(const char* ip_str) {
        ESP_LOGI("LOGGING", "Got IP: %s", ip_str);
    }
    
    static void log_wifi_init_complete() {
        ESP_LOGI("LOGGING", "WiFi initialization complete");
    }
    
    static void log_eventbus_start() {
        ESP_LOGI("LOGGING", "EventBus system starting...");
    }
    
    static void log_eventbus_running() {
        ESP_LOGI("LOGGING", "EventBus system running...");
    }
};
