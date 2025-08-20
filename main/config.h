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

// mDNS settings
extern const char* MDNS_SERVICE_NAME;

// Event Bus Configuration
#define EVENT_BUS_TASK_STACK_SIZE 4096
#define EVENT_BUS_TASK_PRIORITY (tskIDLE_PRIORITY + 2)

// mDNS Configuration
#define MDNS_SERVICE_TYPE "_mqtt"
#define MDNS_PROTOCOL "_tcp"
#define MDNS_QUERY_TIMEOUT_MS 3000
#define MDNS_MAX_RESULTS 20

// System Configuration
#define DEVICE_STATUS_PUBLISH_INTERVAL_MS 10000
#define MAIN_LOOP_DELAY_MS 1000

// MQTT Configuration constants
#define MQTT_BROKER_HOST "192.168.1.100"
#define MQTT_BROKER_PORT 1883

#endif // CONFIG_H
