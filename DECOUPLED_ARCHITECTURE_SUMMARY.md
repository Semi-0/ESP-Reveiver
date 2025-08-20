# Decoupled Architecture: Execution Logic Separate from Logging

## Overview

This document describes the refactored architecture where execution logic is completely decoupled from logging side effects, following functional programming principles and maintaining clean separation of concerns.

## Architecture Principles

### 1. **Pure Functions**
All core logic functions are pure - they have no side effects and their output depends only on their input.

### 2. **Separation of Concerns**
- **Core Logic**: Handles business logic, data transformation, and state management
- **Side Effects**: Logging, I/O operations, and external communications are isolated

### 3. **Optional Logging**
Logging can be enabled/disabled without affecting core functionality through compile-time configuration.

## Key Components

### 1. **Pure Core Logic Functions**

```cpp
// Pure function - no side effects
std::string create_status_message(int uptime_seconds) {
    return "{\"uptime\":" + std::to_string(uptime_seconds) + ",\"status\":\"running\"}";
}

// Pure function - processes data without logging
void process_mqtt_message(const MqttMessageData& message) {
    std::string ack_msg = "{\"received\":\"" + message.payload + "\"}";
    MockMqttClient::publish("esp32/ack", ack_msg, 1);
}

// Pure function - handles connection logic
void handle_mqtt_connected() {
    std::string status_msg = "{\"status\":\"online\",\"device\":\"esp32_eventbus\"}";
    MockMqttClient::publish("esp32/status", status_msg, 1);
}
```

### 2. **Event Handlers (Core Logic Only)**

```cpp
void mqtt_connected_handler(const Event& e, void* user) {
    // Core logic: MQTT connected, publish initial status
    std::string status_msg = "{\"status\":\"online\",\"device\":\"esp32_eventbus\"}";
    bool success = MqttClient::publish(MQTT_PUBLISH_TOPIC, status_msg, 1);
    
    // Optional logging - completely separate from core logic
    #if ENABLE_LOGGING
    if (success) {
        LoggingHandler::log_mqtt_publish_success(MQTT_PUBLISH_TOPIC);
    } else {
        LoggingHandler::log_mqtt_publish_failure(MQTT_PUBLISH_TOPIC);
    }
    #endif
}
```

### 3. **Optional Logging Handler**

```cpp
class LoggingHandler {
public:
    static void log_mqtt_connected(const Event& e, void* user) {
        ESP_LOGI("LOGGING", "MQTT connected event received");
    }
    
    static void log_mqtt_message(const Event& e, void* user) {
        const MqttMessageData* msg_data = static_cast<const MqttMessageData*>(e.ptr);
        if (msg_data) {
            ESP_LOGI("LOGGING", "MQTT message received - Topic: %s, Payload: %s", 
                     msg_data->topic.c_str(), msg_data->payload.c_str());
        }
    }
    
    // ... more logging methods
};
```

### 4. **Compile-Time Logging Configuration**

```cpp
// Optional logging configuration - can be enabled/disabled without affecting core logic
#define ENABLE_LOGGING 1

// In core logic functions:
#if ENABLE_LOGGING
LoggingHandler::log_mqtt_connection_attempt(host);
#endif
```

## Architecture Benefits

### 1. **Testability**
- Core logic can be tested independently of logging
- Pure functions are easy to unit test
- No side effects to mock or control

### 2. **Maintainability**
- Clear separation between business logic and debugging
- Changes to logging don't affect core functionality
- Easier to understand and modify core logic

### 3. **Performance**
- Logging can be completely disabled in production
- No logging overhead in core execution paths
- Conditional compilation removes logging code entirely

### 4. **Flexibility**
- Different logging strategies can be implemented without changing core logic
- Logging can be enabled/disabled per module or globally
- Easy to add new logging targets (file, network, etc.)

## Implementation Examples

### 1. **Pure Worker Functions**

```cpp
// Pure mDNS query worker - no logging
static bool mdns_query_worker(void** out) {
    #if ENABLE_LOGGING
    LoggingHandler::log_mdns_query_start();
    #endif
    
    // Core logic - no side effects
    mdns_result_t* results = nullptr;
    esp_err_t err = mdns_query_ptr("_mqtt", "_tcp", 3000, 20, &results);
    
    if (err != ESP_OK || results == nullptr) {
        #if ENABLE_LOGGING
        LoggingHandler::log_mdns_query_failure(esp_err_to_name(err));
        #endif
        *out = nullptr;
        return false;
    }
    
    // ... core logic continues
    return true;
}
```

### 2. **Event Handler Composition**

```cpp
// Subscribe to events for core logic
BUS.subscribe(wifi_connected_handler, nullptr, bit(TOPIC_WIFI_CONNECTED));
BUS.subscribe(mqtt_connected_handler, nullptr, bit(TOPIC_MQTT_CONNECTED));
BUS.subscribe(mqtt_message_handler, nullptr, bit(TOPIC_MQTT_MESSAGE));

// Optional: Subscribe to logging handlers (only if logging is enabled)
#if ENABLE_LOGGING
BUS.subscribe(LoggingHandler::log_wifi_connected, nullptr, bit(TOPIC_WIFI_CONNECTED));
BUS.subscribe(LoggingHandler::log_mqtt_connected, nullptr, bit(TOPIC_MQTT_CONNECTED));
BUS.subscribe(LoggingHandler::log_mqtt_message, nullptr, bit(TOPIC_MQTT_MESSAGE));
#endif
```

### 3. **MQTT Client (No Logging)**

```cpp
bool MqttClient::subscribe(const std::string& topic, int qos) {
    if (!isConnected()) {
        return false;  // Pure logic - no logging
    }
    
    esp_err_t err = esp_mqtt_client_subscribe(mqtt_client, topic.c_str(), qos);
    if (err != ESP_OK) {
        return false;  // Pure logic - no logging
    }
    
    return true;  // Pure logic - no logging
}
```

## Testing Strategy

### 1. **Pure Function Testing**

```cpp
void test_pure_functions() {
    // Test status message creation
    std::string status = create_status_message(100);
    assert(status == "{\"uptime\":100,\"status\":\"running\"}");
    
    // Test timer handler
    MockMqttClient::connected_ = true;
    handle_timer_tick(5);
    assert(MockMqttClient::published_messages.size() == 1);
}
```

### 2. **Decoupled Architecture Testing**

```cpp
void test_decoupled_architecture() {
    // Test that core logic functions work without any logging dependencies
    MockMqttClient::reset();
    MockMqttClient::connected_ = true;
    
    // Test pure function composition
    handle_mqtt_connected();
    handle_timer_tick(10);
    process_mqtt_message(MqttMessageData("test", "data"));
    
    // Verify all core operations completed successfully
    assert(MockMqttClient::published_messages.size() == 3);
}
```

## Configuration Options

### 1. **Global Logging Control**

```cpp
#define ENABLE_LOGGING 1  // Enable all logging
#define ENABLE_LOGGING 0  // Disable all logging
```

### 2. **Module-Specific Logging**

```cpp
#define ENABLE_MQTT_LOGGING 1
#define ENABLE_WIFI_LOGGING 0
#define ENABLE_MDNS_LOGGING 1
```

### 3. **Logging Levels**

```cpp
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DEBUG 4
```

## Best Practices

### 1. **Keep Core Logic Pure**
- Avoid side effects in core functions
- Use return values instead of global state
- Make functions predictable and testable

### 2. **Isolate Side Effects**
- Group all logging in separate handlers
- Use conditional compilation for optional features
- Keep I/O operations separate from business logic

### 3. **Use Function Composition**
- Compose pure functions to build complex behavior
- Avoid mixing concerns in single functions
- Use the event bus for decoupled communication

### 4. **Test Independently**
- Test core logic without logging
- Test logging separately from core logic
- Use mocks to isolate components

## Migration Guide

### From Coupled to Decoupled

1. **Identify Side Effects**
   ```cpp
   // Before: Mixed concerns
   void handler(const Event& e) {
       ESP_LOGI(TAG, "Processing event");  // Side effect
       process_data(e);                    // Core logic
       ESP_LOGI(TAG, "Event processed");   // Side effect
   }
   ```

2. **Separate Core Logic**
   ```cpp
   // After: Pure core logic
   void handler(const Event& e) {
       process_data(e);  // Core logic only
   }
   ```

3. **Add Optional Logging**
   ```cpp
   // After: Optional logging
   void handler(const Event& e) {
       #if ENABLE_LOGGING
       LoggingHandler::log_event_start(e);
       #endif
       
       process_data(e);  // Core logic
       
       #if ENABLE_LOGGING
       LoggingHandler::log_event_complete(e);
       #endif
   }
   ```

## Conclusion

The decoupled architecture provides:

- **Clean separation** between execution logic and side effects
- **Improved testability** through pure functions
- **Better performance** with optional logging
- **Enhanced maintainability** with clear concerns
- **Flexible configuration** for different deployment scenarios

This architecture follows functional programming principles and provides a solid foundation for building reliable, maintainable embedded systems.
