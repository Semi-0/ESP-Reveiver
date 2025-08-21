#pragma once
#include <stdint.h>

// Reserve hot topics 0..31 for bitmask (fast path)
enum : uint16_t {
    TOPIC_WIFI_CONNECTED = 0,
    TOPIC_WIFI_DISCONNECTED = 1,
    TOPIC_MDNS_FOUND     = 2,
    TOPIC_MDNS_FAILED    = 3,
    TOPIC_MQTT_CONNECTED = 4,
    TOPIC_MQTT_DISCONNECTED = 5,
    TOPIC_MQTT_MESSAGE   = 6,
    TOPIC_PIN_SET        = 7,
    TOPIC_PIN_READ       = 8,
    TOPIC_PIN_MODE       = 9,
    TOPIC_PIN_SUCCESS    = 10,
    TOPIC_DEVICE_STATUS  = 11,
    TOPIC_DEVICE_RESET   = 12,
    TOPIC_SYSTEM_ERROR   = 13,
    TOPIC_SYSTEM_WARNING = 14,
    TOPIC_TIMER          = 15,
    TOPIC_STATUS_PUBLISH_SUCCESS = 16,
    TOPIC_MQTT_SUBSCRIBED = 17,

    // Recovery and Safe Mode Topics
    TOPIC_RETRY_RESOLVE = 2001,
    TOPIC_SAFE_MODE_ENTER,
    TOPIC_SAFE_MODE_EXIT,
    TOPIC_BROKER_PERSISTED,
    TOPIC_LINK_READY,

    // Reserved internal topic for async continuation:
    TOPIC__ASYNC_RESULT  = 31
};

constexpr uint32_t bit(uint16_t topic) {
    return (topic < 32) ? (1u << topic) : 0u;
}

constexpr uint32_t MASK_ALL = 0xFFFFFFFFu;
