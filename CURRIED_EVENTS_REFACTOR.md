# Curried Events Refactoring and Explicit Data Flow

## Overview

This document describes the refactoring of the MQTT receiver application to use curried event handlers and explicit data flow, following functional programming principles.

## Key Improvements

### 1. Curried Event Handler Factories

**Before:**
```cpp
// Multiple separate handler functions with repetitive patterns
void handleWifiLoggingEvent(const Event& event, void* user_data) {
    bool connected = (event.i32 != 0);
    ESP_LOGI(TAG, "WiFi %s", connected ? "connected" : "disconnected");
}

void handleMdnsLoggingEvent(const Event& event, void* user_data) {
    bool discovered = (event.i32 != 0);
    ESP_LOGI(TAG, "mDNS discovery: %s", discovered ? "success" : "failed");
}

// Manual subscription with repetitive g_eventBus->subscribe calls
g_eventBus->subscribe(handleWifiLoggingEvent, nullptr, MASK_WIFI);
g_eventBus->subscribe(handleMdnsLoggingEvent, nullptr, MASK_MDNS);
```

**After:**
```cpp
// Curried handler factories that return event handlers
auto createWifiLoggingHandler() {
    return [](const Event& event, void* user_data) {
        bool connected = (event.i32 != 0);
        ESP_LOGI(TAG, "WiFi %s", connected ? "connected" : "disconnected");
    };
}

auto createMdnsLoggingHandler() {
    return [](const Event& event, void* user_data) {
        bool discovered = (event.i32 != 0);
        ESP_LOGI(TAG, "mDNS discovery: %s", discovered ? "success" : "failed");
    };
}

// Curried subscription helper
auto subscribeTo(const uint32_t mask) {
    return [mask](EventHandler handler, void* user_data = nullptr) {
        return g_eventBus->subscribe(handler, user_data, mask);
    };
}

// Clean, composable subscription
auto subscribeToWifi = subscribeTo(MASK_WIFI);
auto subscribeToMdns = subscribeTo(MASK_MDNS);

subscribeToWifi(createWifiLoggingHandler());
subscribeToMdns(createMdnsLoggingHandler());
```

### 2. Explicit Data Flow

**Before:**
```cpp
// Data passed through environment/global state
void handleStartMqttConnectionEvent(const Event& event, void* user_data) {
    // Try to get discovered service from mDNS result (environment dependency)
    auto services = MdnsPure::getAllServices();
    if (!services.empty()) {
        const auto& service = services[0];
        mqtt_config.broker_host = service.host;
        mqtt_config.broker_port = service.port;
    }
}
```

**After:**
```cpp
// Explicit data structures for data flow
struct ServiceDiscoveryData {
    std::string service_name;
    std::string host;
    int port;
    bool valid;
};

struct MqttConnectionData {
    std::string broker_host;
    int broker_port;
    std::string client_id;
};

// Pure functions with explicit inputs and outputs
ServiceDiscoveryData discoverMqttServicePure(const MdnsConfig& config, int max_retries = 3);
MqttResult setupMqttPure(const MqttConnectionData& connection_data, int max_retries = 3);

// Data passed explicitly through events
void handleMdnsSuccessEvent(const Event& event, void* user_data) {
    auto* discovery_data = static_cast<ServiceDiscoveryData*>(user_data);
    
    if (discovery_data && discovery_data->valid) {
        // Create MQTT connection data from discovered service
        MqttConnectionData mqtt_data(discovery_data->host, discovery_data->port, get_esp32_device_id());
        
        // Pass data explicitly through event
        auto* mqtt_data_ptr = new MqttConnectionData(mqtt_data);
        Event mqtt_event;
        mqtt_event.type = TOPIC_SYSTEM;
        mqtt_event.ptr = mqtt_data_ptr;
        mqtt_event.i32 = 1;
        
        g_eventBus->publish(mqtt_event);
    }
}
```

### 3. Functional Composition

**Pure Functions:**
```cpp
// Pure function to create MQTT connection data from service discovery
MqttConnectionData createMqttConnectionFromService(const ServiceDiscoveryData& service, const std::string& client_id) {
    if (service.valid) {
        return MqttConnectionData(service.host, service.port, client_id);
    } else {
        return MqttConnectionData("", 0, client_id);
    }
}

// Pure function to validate service discovery data
bool isValidServiceDiscovery(const ServiceDiscoveryData& service) {
    return service.valid && !service.host.empty() && service.port > 0;
}

// Pure function to process MQTT connection data
std::string processMqttConnection(const MqttConnectionData& data) {
    if (data.broker_host.empty() || data.broker_port <= 0) {
        return "invalid_connection_data";
    }
    return "connecting_to_" + data.broker_host + ":" + std::to_string(data.broker_port);
}
```

**Functional Pipeline:**
```cpp
// Create a pipeline of pure functions
auto pipeline = [](const ServiceDiscoveryData& service) {
    // Step 1: Validate service
    if (!isValidServiceDiscovery(service)) {
        return std::string("invalid_service");
    }
    
    // Step 2: Create MQTT connection data
    auto mqtt_data = createMqttConnectionFromService(service, "composed_client");
    
    // Step 3: Process connection
    return processMqttConnection(mqtt_data);
};
```

## Benefits

### 1. Reduced Visual Cluttering
- **Curried subscription functions** hide the repetitive `g_eventBus->subscribe` calls
- **Handler factories** eliminate duplicate handler function definitions
- **Cleaner main function** with less boilerplate code

### 2. Explicit Data Flow
- **No hidden dependencies** on global state or environment variables
- **Data is passed explicitly** through function arguments and events
- **Easier to debug** as inputs and outputs are clearly visible
- **Better testability** with pure functions

### 3. Functional Programming Principles
- **Pure functions** with no side effects
- **Immutable data structures** for data flow
- **Function composition** for complex operations
- **Explicit error handling** through return values

### 4. Improved Maintainability
- **Modular design** with clear separation of concerns
- **Reusable components** through curried functions
- **Type safety** with explicit data structures
- **Easier to extend** with new event types

## Testing

The refactored code includes comprehensive tests in `test/test_curried_events.cpp` that demonstrate:

1. **Curried logging handlers** - Testing the curried event handler factories
2. **Explicit data flow** - Testing pure functions with explicit inputs/outputs
3. **Functional composition** - Testing pipelines of pure functions
4. **Event-driven flow** - Testing the complete event-driven architecture

Run the tests with:
```bash
cd test
mkdir -p build && cd build
cmake .. && make test_curried_events
./test_curried_events
```

## Architecture Comparison

### Before (Imperative/Procedural)
- Global state dependencies
- Hidden data flow through environment
- Repetitive event handler patterns
- Difficult to test individual components

### After (Functional/Event-Driven)
- Explicit data structures
- Pure functions with clear inputs/outputs
- Curried event handlers for composition
- Easy to test and reason about

## Main Loop Refactoring

### Event-Driven Main Loop

**Before:**
```cpp
// Imperative main loop with direct MQTT calls
while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Publish status periodically (if MQTT is connected)
    if (MqttPure::isConnected()) {
        std::string status = "{\"device_id\":\"" + get_esp32_device_id() + 
                           "\",\"status\":\"online\",\"uptime\":" + 
                           std::to_string(esp_timer_get_time() / 1000000) + "}";
        MqttPure::publish(get_mqtt_status_topic(), status);
    }
}
```

**After:**
```cpp
// Event-driven main loop with clock events
while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Get current uptime
    uint64_t current_time = getCurrentUptimeSeconds();
    
    // Publish clock event with current time - device info will be handled by clock handler
    Event clock_event;
    clock_event.type = TOPIC_TIMER;
    clock_event.i32 = static_cast<int32_t>(current_time);
    clock_event.ptr = nullptr;
    
    g_eventBus->publish(clock_event);
}
```

### Curried Clock Handler

```cpp
// Curried clock handler factory that takes interval and returns a handler
auto createClockHandler(uint64_t interval_seconds) {
    static uint64_t last_publish_time = 0;
    
    return [interval_seconds](const Event& event, void* user_data) {
        uint64_t current_time = static_cast<uint64_t>(event.i32);
        
        // Check if we should publish device info (pure function)
        if (shouldPublishDeviceInfo(last_publish_time, current_time, interval_seconds)) {
            // Check if MQTT is connected
            if (MqttPure::isConnected()) {
                // Create device status using pure function
                std::string device_id = get_esp32_device_id();
                std::string status_json = createDeviceStatusJson(device_id, current_time);
                
                // Publish device info
                MqttPure::publish(get_mqtt_status_topic(), status_json);
            }
            last_publish_time = current_time;
        }
    };
}

// Clock event handler that publishes device info every 60 seconds
void handleClockEvent(const Event& event, void* user_data) {
    // Use curried clock handler with 60-second interval
    auto clock_handler = createClockHandler(60);
    clock_handler(event, user_data);
}
```

### Benefits of Event-Driven Main Loop

1. **Separation of Concerns**: Main loop only publishes events, handlers do the work
2. **Configurable Intervals**: Clock handler can be configured with different intervals
3. **Testable**: Clock events can be tested independently
4. **Extensible**: Easy to add more clock-based functionality
5. **Functional**: Uses pure functions for device info creation

## Conclusion

## Single Responsibility Principle (SRP) Refactoring

### Problem with Original Architecture

The original `handleMqttMessageEvent` violated the Single Responsibility Principle by:
- Parsing JSON messages
- Converting to internal commands
- Executing pin operations
- Publishing results

This created tight coupling and made testing difficult.

### Solution: Separation of Concerns

**Before (Violating SRP):**
```cpp
void handleMqttMessageEvent(const Event& event, void* user_data) {
    const char* message = static_cast<const char*>(event.ptr);
    
    // Parse and execute commands (violates SRP)
    auto parse_result = MessageProcessor::parse_json_message(message);
    auto execution_results = MessageProcessor::execute_commands(parse_result.commands);
    
    // Publish results directly
    for (const auto& result : execution_results) {
        if (result.success) {
            publishPinEvent(result.pin, result.value, result.action_description.c_str());
        } else {
            publishErrorEvent("PinController", result.error_message.c_str(), 0);
        }
    }
}
```

**After (Following SRP):**
```cpp
// MessageProcessor: Only handles parsing and conversion
void handleMqttMessageEvent(const Event& event, void* user_data) {
    const char* message = static_cast<const char*>(event.ptr);
    
    // Parse message and convert to device commands (pure function)
    auto device_command_result = MessageProcessor::processMessageToDeviceCommands(message);
    
    if (!device_command_result.success) {
        publishErrorEvent("MessageProcessor", device_command_result.error_message.c_str(), 0);
        return;
    }
    
    // Publish device command event for device monitor to handle
    if (!device_command_result.device_commands.empty()) {
        auto device_event = createDeviceCommandEvent(device_command_result.device_commands, "mqtt");
        
        Event device_command_event;
        device_command_event.type = TOPIC_PIN;
        device_command_event.ptr = new DeviceCommandEvent(device_event);
        device_command_event.i32 = 1;
        
        g_eventBus->publish(device_command_event);
    }
}

// DeviceMonitor: Only handles device operations
void handleDeviceCommandEvent(const Event& event, void* user_data) {
    auto* device_event = static_cast<DeviceCommandEvent*>(event.ptr);
    
    // Execute device commands using device monitor (pure function)
    auto execution_results = DeviceMonitor::executeDeviceCommands(device_event->commands);
    
    // Publish results as events
    for (const auto& result : execution_results) {
        if (result.success) {
            publishPinEvent(result.pin, result.value, result.action_description.c_str());
        } else {
            publishErrorEvent("DeviceMonitor", result.error_message.c_str(), result.pin);
        }
    }
    
    delete device_event;
}
```

### New Architecture Components

#### 1. Device Commands (`device_commands.h/cpp`)
```cpp
// Device command types
enum DeviceCommandType {
    PIN_SET,
    PIN_READ,
    PIN_MODE,
    DEVICE_STATUS,
    DEVICE_RESET
};

// Pin command structure for device operations
struct DevicePinCommand {
    DeviceCommandType type;
    int pin;
    int value;
    std::string description;
};

// Pure functions for device command operations
DevicePinCommand createDevicePinCommand(const std::string& message_type, int pin, int value, const std::string& description = "");
bool isValidDeviceCommand(const DevicePinCommand& command);
DeviceCommandEvent createDeviceCommandEvent(const std::vector<DevicePinCommand>& commands, const std::string& source = "mqtt");
```

#### 2. Device Monitor (`device_monitor.h/cpp`)
```cpp
class DeviceMonitor {
public:
    // Pure function to execute a single device command
    static DeviceCommandResult executeDeviceCommand(const DevicePinCommand& command);
    
    // Pure function to execute multiple device commands
    static std::vector<DeviceCommandResult> executeDeviceCommands(const std::vector<DevicePinCommand>& commands);
    
    // Pure validation functions
    static bool isValidPin(int pin);
    static bool isValidDigitalValue(int value);
    static bool isValidAnalogValue(int value);
};
```

#### 3. Message Processor (Refactored)
```cpp
class MessageProcessor {
public:
    // Pure parsing functions - no side effects
    static ParseResult parse_json_message(const std::string& message);
    
    // Pure function to convert parsed commands to device commands
    static DeviceCommandParseResult convertToDeviceCommands(const ParseResult& parse_result);
    
    // Pure function to process message and return device commands
    static DeviceCommandParseResult processMessageToDeviceCommands(const std::string& message);
};
```

### Benefits of SRP Architecture

1. **Single Responsibility**: Each component has one clear purpose
   - MessageProcessor: Parse and convert messages
   - DeviceMonitor: Execute device operations
   - Event Handlers: Coordinate between components

2. **Testability**: Each component can be tested in isolation
   - MessageProcessor tests focus on parsing logic
   - DeviceMonitor tests focus on device operations
   - Event handler tests focus on coordination

3. **Maintainability**: Changes to one component don't affect others
   - JSON format changes only affect MessageProcessor
   - Pin operations changes only affect DeviceMonitor
   - Event flow changes only affect handlers

4. **Reusability**: Components can be reused in different contexts
   - MessageProcessor can handle different message sources
   - DeviceMonitor can be used by different command sources
   - Event handlers can be composed differently

5. **Functional Programming**: Pure functions with explicit inputs/outputs
   - No hidden side effects
   - Clear data flow
   - Easy to reason about

### Event-Driven Flow

```
MQTT Message → MessageProcessor → Device Commands → Event Bus → DeviceMonitor → Pin Operations
     ↓              ↓                    ↓              ↓            ↓              ↓
   Parse        Convert to         Publish Event    Route to     Execute       Publish
   JSON         Device Commands     (TOPIC_PIN)     Handler      Commands      Results
```

The refactoring successfully applies functional programming principles to create a more maintainable, testable, and debuggable event-driven architecture. The use of currying reduces visual clutter while explicit data flow makes the system easier to understand and debug. The event-driven main loop and SRP architecture complete the transformation from imperative to functional programming style.
