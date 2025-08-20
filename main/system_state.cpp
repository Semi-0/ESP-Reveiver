#include "system_state.h"
#include "config.h"
#include "esp_log.h"
#include <sstream>

static const char* TAG = "SYSTEM_STATE";

// Static member initialization
SystemState SystemStateManager::current_state_;

SystemState SystemStateManager::getCurrentState() {
    return current_state_;
}

void SystemStateManager::updateWifiState(bool connected) {
    current_state_.wifi_connected = connected;
    ESP_LOGI(TAG, "WiFi state updated: %s", connected ? "connected" : "disconnected");
}

void SystemStateManager::updateMqttState(bool connected) {
    current_state_.mqtt_connected = connected;
    ESP_LOGI(TAG, "MQTT state updated: %s", connected ? "connected" : "disconnected");
}

void SystemStateManager::updateMdnsState(bool available) {
    current_state_.mdns_available = available;
    ESP_LOGI(TAG, "mDNS state updated: %s", available ? "available" : "unavailable");
}

void SystemStateManager::updateBrokerInfo(const std::string& broker, int port) {
    current_state_.current_broker = broker;
    current_state_.current_broker_port = port;
    ESP_LOGI(TAG, "Broker info updated: %s:%d", broker.c_str(), port);
}

void SystemStateManager::updateUptime(uint64_t uptime_seconds) {
    current_state_.uptime_seconds = uptime_seconds;
}

void SystemStateManager::incrementErrorCount() {
    current_state_.error_count++;
    ESP_LOGW(TAG, "Error count incremented: %d", current_state_.error_count);
}

void SystemStateManager::incrementMessageCount() {
    current_state_.message_count++;
    ESP_LOGI(TAG, "Message count incremented: %d", current_state_.message_count);
}

std::string SystemStateManager::createDeviceStatusJson() {
    std::string device_id = get_esp32_device_id();
    
    std::ostringstream json;
    json << "{";
    json << "\"device_id\":\"" << device_id << "\",";
    json << "\"status\":\"" << (current_state_.mqtt_connected ? "online" : "offline") << "\",";
    json << "\"uptime\":" << current_state_.uptime_seconds << ",";
    json << "\"system\":{";
    json << "\"wifi_connected\":" << (current_state_.wifi_connected ? "true" : "false") << ",";
    json << "\"mqtt_connected\":" << (current_state_.mqtt_connected ? "true" : "false") << ",";
    json << "\"mdns_available\":" << (current_state_.mdns_available ? "true" : "false") << ",";
    json << "\"current_broker\":\"" << current_state_.current_broker << "\",";
    json << "\"current_broker_port\":" << current_state_.current_broker_port << ",";
    json << "\"error_count\":" << current_state_.error_count << ",";
    json << "\"message_count\":" << current_state_.message_count;
    json << "}";
    json << "}";
    
    return json.str();
}

void SystemStateManager::reset() {
    current_state_ = SystemState();
    ESP_LOGI(TAG, "System state reset");
}

