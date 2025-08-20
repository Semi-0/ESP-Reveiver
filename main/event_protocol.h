#pragma once
#include <stdint.h>

// Reserve 0..31 for hot-path bitmask delivery
enum : uint16_t {
    TOPIC_BUTTON = 0,
    TOPIC_MQTT   = 1,
    TOPIC_TIMER  = 2,
    TOPIC_WIFI   = 3,
    TOPIC_MDNS   = 4,
    TOPIC_PIN    = 5,
    TOPIC_SYSTEM = 6,
    TOPIC_ERROR  = 7,
    // add more up to 31 for cheap mask filtering
};

constexpr uint32_t bit(uint16_t topic) {
    return (topic < 32) ? (1u << topic) : 0u;
}

// Common masks
constexpr uint32_t MASK_ALL      = 0xFFFFFFFFu;
constexpr uint32_t MASK_BUTTONS  = bit(TOPIC_BUTTON);
constexpr uint32_t MASK_MQTT     = bit(TOPIC_MQTT);
constexpr uint32_t MASK_TIMERS   = bit(TOPIC_TIMER);
constexpr uint32_t MASK_WIFI     = bit(TOPIC_WIFI);
constexpr uint32_t MASK_MDNS     = bit(TOPIC_MDNS);
constexpr uint32_t MASK_PINS     = bit(TOPIC_PIN);
constexpr uint32_t MASK_SYSTEM   = bit(TOPIC_SYSTEM);
constexpr uint32_t MASK_ERRORS   = bit(TOPIC_ERROR);

// Event data structures for different topics
struct MqttEvent {
    const char* topic;
    const char* message;
    int length;
};

struct WifiEvent {
    bool connected;
    const char* ssid;
    const char* ip_address;
};

struct MdnsEvent {
    bool discovered;
    const char* service_name;
    const char* host;
    int port;
};

struct PinEvent {
    int pin;
    int value;
    const char* action;
};

struct SystemEvent {
    const char* status;
    const char* component;
};

struct ErrorEvent {
    const char* component;
    const char* message;
    int error_code;
};
