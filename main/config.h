#ifndef CONFIG_H
#define CONFIG_H

#include <string>

// WiFi settings
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;
extern const int UDP_LOCAL_PORT;

// Machine configuration
extern const std::string MACHINE_ID;

// ESP32 Device ID functions
std::string get_esp32_device_id();
std::string get_mqtt_topic_prefix();

// MQTT settings
extern const char* MQTT_SERVER;
extern const int MQTT_PORT;
extern const char* MQTT_USERNAME;
extern const char* MQTT_PASSWORD;

// MQTT topics - dynamically generated from ESP32 ID
std::string get_mqtt_status_topic();
std::string get_mqtt_control_topic();
std::string get_mqtt_response_topic();

// Application settings
extern const char* APP_TAG;

#endif // CONFIG_H
