# MQTT Receiver - Functional Implementation

This is the functional programming implementation of the MQTT receiver, featuring:

- **Curried Event Handlers** - Reduced visual clutter and improved composability
- **Explicit Data Flow** - No hidden dependencies or global state
- **Single Responsibility Principle** - Clear separation of concerns
- **Event-Driven Architecture** - Pure functions and functional composition
- **Comprehensive Testing** - Isolated component testing

## Architecture Overview

### Core Components

1. **MessageProcessor** - Handles JSON parsing and conversion to device commands
2. **DeviceMonitor** - Executes device/pin operations as pure functions
3. **DeviceCommands** - Defines command structures and validation
4. **Event Bus** - Coordinates communication between components
5. **Curried Handlers** - Composable event handlers with reduced boilerplate

### Event-Driven Flow

```
MQTT Message → MessageProcessor → Device Commands → Event Bus → DeviceMonitor → Pin Operations
     ↓              ↓                    ↓              ↓            ↓              ↓
   Parse        Convert to         Publish Event    Route to     Execute       Publish
   JSON         Device Commands     (TOPIC_PIN)     Handler      Commands      Results
```

## Key Features

### 1. Curried Event Handlers

```cpp
// Curried subscription helper
auto subscribeTo(const uint32_t mask) {
    return [mask](EventHandler handler, void* user_data = nullptr) {
        return g_eventBus->subscribe(handler, user_data, mask);
    };
}

// Clean, composable subscription
auto subscribeToWifi = subscribeTo(MASK_WIFI);
auto subscribeToMqtt = subscribeTo(MASK_MQTT);

subscribeToWifi(createWifiLoggingHandler());
subscribeToMqtt(createMqttLoggingHandler());
```

### 2. Explicit Data Flow

```cpp
// Explicit data structures
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

// Pure functions with explicit inputs/outputs
ServiceDiscoveryData discoverMqttServicePure(const MdnsConfig& config, int max_retries = 3);
MqttResult setupMqttPure(const MqttConnectionData& connection_data, int max_retries = 3);
```

### 3. Single Responsibility Principle

```cpp
// MessageProcessor: Only handles parsing and conversion
auto device_command_result = MessageProcessor::processMessageToDeviceCommands(message);

// DeviceMonitor: Only handles device operations
auto execution_results = DeviceMonitor::executeDeviceCommands(device_event->commands);
```

### 4. Event-Driven Main Loop

```cpp
// Event-driven main loop with clock events
while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Publish clock event - device info handled by clock handler
    Event clock_event;
    clock_event.type = TOPIC_TIMER;
    clock_event.i32 = static_cast<int32_t>(getCurrentUptimeSeconds());
    g_eventBus->publish(clock_event);
}
```

## Building and Testing

### Prerequisites

- ESP-IDF development environment
- CMake 3.16 or higher
- C++17 compatible compiler

### Building

```bash
cd functional_implementation
mkdir build && cd build
cmake ..
make
```

### Running Tests

```bash
cd functional_implementation/test
mkdir build && cd build
cmake ..
make run_tests
```

### Available Tests

1. **test_curried_events** - Tests curried event handlers and explicit data flow
2. **test_main_loop_events** - Tests event-driven main loop and device info publishing
3. **test_srp_architecture** - Tests Single Responsibility Principle implementation

## File Structure

```
functional_implementation/
├── main/
│   ├── main.cpp                 # Main application with event-driven architecture
│   ├── device_commands.h/cpp    # Device command structures and validation
│   ├── device_monitor.h/cpp     # Device operation execution
│   ├── message_processor.h/cpp  # JSON parsing and conversion
│   ├── event_bus_interface.h    # Event bus interface
│   ├── event_protocol.h         # Event types and protocols
│   ├── config.h/cpp             # Configuration management
│   ├── wifi_pure.h              # WiFi pure functions
│   ├── mdns_pure.h              # mDNS pure functions
│   ├── mqtt_pure.h              # MQTT pure functions
│   └── pin_controller.h/cpp     # Pin control operations
├── test/
│   ├── test_curried_events.cpp      # Curried event handler tests
│   ├── test_main_loop_events.cpp    # Main loop event tests
│   ├── test_srp_architecture.cpp    # SRP architecture tests
│   └── CMakeLists.txt               # Test build configuration
├── CMakeLists.txt                   # Main build configuration
└── README.md                        # This file
```

## Benefits

### 1. Reduced Visual Cluttering
- Curried subscription functions hide repetitive `g_eventBus->subscribe` calls
- Handler factories eliminate duplicate handler function definitions
- Cleaner main function with less boilerplate code

### 2. Explicit Data Flow
- No hidden dependencies on global state or environment variables
- Data is passed explicitly through function arguments and events
- Easier to debug as inputs and outputs are clearly visible
- Better testability with pure functions

### 3. Functional Programming Principles
- Pure functions with no side effects
- Immutable data structures for data flow
- Function composition for complex operations
- Explicit error handling through return values

### 4. Improved Maintainability
- Modular design with clear separation of concerns
- Reusable components through curried functions
- Type safety with explicit data structures
- Easier to extend with new event types

## Comparison with Original Implementation

| Aspect | Original | Functional Implementation |
|--------|----------|---------------------------|
| **Event Handlers** | Repetitive boilerplate | Curried factory functions |
| **Data Flow** | Hidden global state | Explicit function arguments |
| **Responsibilities** | Mixed concerns | Single responsibility |
| **Testing** | Difficult to isolate | Easy component testing |
| **Maintainability** | Tight coupling | Loose coupling |
| **Debugging** | Hard to trace data | Clear input/output flow |

## Migration Guide

To migrate from the original implementation:

1. **Replace event handler registration** with curried functions
2. **Extract data structures** for explicit data flow
3. **Separate concerns** into MessageProcessor and DeviceMonitor
4. **Update main loop** to use event-driven approach
5. **Add comprehensive tests** for each component

## Contributing

When adding new features:

1. **Follow SRP** - Each component should have a single responsibility
2. **Use pure functions** - Avoid side effects and global state
3. **Add explicit data structures** - Don't rely on environment variables
4. **Create curried handlers** - Use functional composition
5. **Write tests** - Ensure each component is testable in isolation

## License

This implementation follows the same license as the original project.

