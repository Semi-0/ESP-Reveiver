#pragma once

#include <string>

// WiFi Configuration
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
#define WIFI_MAX_RETRIES 10
#define WIFI_RETRY_DELAY_MS 5000

// MQTT Configuration
#define MQTT_BROKER_HOST "192.168.1.100"
#define MQTT_BROKER_PORT 1883
#define MQTT_CLIENT_ID_PREFIX "esp32_mqtt_receiver_"
#define MQTT_CONTROL_TOPIC "device/control"
#define MQTT_STATUS_TOPIC "device/status"
#define MQTT_MAX_RETRIES 5
#define MQTT_RETRY_INTERVAL_MS 30000

// mDNS Configuration
#define MDNS_SERVICE_TYPE "_mqtt"
#define MDNS_PROTOCOL "_tcp"
#define MDNS_QUERY_TIMEOUT_MS 3000
#define MDNS_MAX_RESULTS 20

// System Configuration
#define DEVICE_STATUS_PUBLISH_INTERVAL_MS 10000
#define MAIN_LOOP_DELAY_MS 1000
#define EVENT_BUS_TASK_STACK_SIZE 4096
#define EVENT_BUS_TASK_PRIORITY (tskIDLE_PRIORITY + 2)

// Pin Configuration
#define DEFAULT_PIN 2
#define PIN_COUNT 8

// Event Bus Configuration
#define EBUS_MAX_LISTENERS 16
#define EBUS_DISPATCH_QUEUE_LEN 64

// Machine Configuration
#define MACHINE_ID "ESP32_MQTT_RECEIVER"

// Helper functions
std::string get_esp32_device_id();
std::string get_mqtt_control_topic();
std::string get_mqtt_status_topic();


