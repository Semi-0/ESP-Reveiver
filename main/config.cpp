#include "config.h"
#include "esp_system.h"
#include "esp_mac.h"
#include <sstream>
#include <iomanip>

// WiFi settings
const char* WIFI_SSID = "V2_ Lab";
const char* WIFI_PASSWORD = "end-of-file";
const int UDP_LOCAL_PORT = 12345;

// Machine configuration
const std::string MACHINE_ID = "2";

// ESP32 Device ID functions
std::string get_esp32_device_id() {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    
    std::stringstream ss;
    ss << "ESP32_";
    for (int i = 3; i < 6; i++) {  // Use last 3 bytes of MAC for shorter ID
        ss << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << (int)mac[i];
    }
    
    return ss.str();
}

std::string get_mqtt_topic_prefix() {
    return get_esp32_device_id();
}

// MQTT settings
const char* MQTT_SERVER = "192.168.178.32";
const int MQTT_PORT = 1883;
const char* MQTT_USERNAME = "";
const char* MQTT_PASSWORD = "";

// MQTT topics - dynamically generated from ESP32 ID
std::string get_mqtt_status_topic() {
    return get_mqtt_topic_prefix() + "/status";
}

std::string get_mqtt_control_topic() {
    return get_mqtt_topic_prefix() + "/control";
}

std::string get_mqtt_response_topic() {
    return get_mqtt_topic_prefix() + "/response";
}

// Application settings
const char* APP_TAG = "MQTT_RECEIVER";

// mDNS settings
const char* MDNS_SERVICE_NAME = "Local MQTT Controller";
