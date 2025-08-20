# MQTT Integration with Event Bus Architecture

## Overview

This document describes the complete MQTT integration with the event bus architecture in the ESP32 MQTT receiver system. The integration follows functional programming principles and maintains a clean separation of concerns.

## Architecture Components

### 1. MQTT Client (`eventbus/main/mqtt_client.h/cpp`)

The MQTT client is implemented as a static class that provides:
- Connection management to MQTT brokers
- Topic subscription and publishing
- Event bus integration for message handling
- Proper memory management for message data

**Key Features:**
- **Pure Functions**: Connection operations return `MqttConnectionResult` with success/failure status
- **Event Bus Integration**: Automatically publishes events to the event bus for connection state changes
- **Message Callbacks**: Supports custom message handling with lambda functions
- **Memory Safety**: Proper cleanup of message data to prevent memory leaks

### 2. Data Structures (`eventbus/main/data_structures.h`)

Clean, immutable data structures for MQTT operations:

```cpp
struct MqttConnectionData {
    std::string broker_host;
    int broker_port;
    std::string client_id;
};

struct MqttMessageData {
    std::string topic;
    std::string payload;
    int qos;
};
```

### 3. Event Protocol (`eventbus/include/eventbus/EventProtocol.h`)

Defines MQTT-related event topics:
- `TOPIC_MQTT_CONNECTED` (4)
- `TOPIC_MQTT_DISCONNECTED` (5)
- `TOPIC_MQTT_MESSAGE` (6)

## Integration Flow

### 1. WiFi Connection → mDNS Discovery → MQTT Connection

The system follows a declarative flow:

```cpp
// Flow 1: When WiFi connects, do mDNS lookup
G.when(TOPIC_WIFI_CONNECTED,
    G.async_blocking("mdns-query", mdns_query_worker,
        FlowGraph::publish(TOPIC_MDNS_FOUND),
        FlowGraph::publish(TOPIC_MDNS_FAILED)
    )
);

// Flow 2: When mDNS succeeds, connect to MQTT
G.when(TOPIC_MDNS_FOUND,
    G.async_blocking("mqtt-connect", mqtt_connect_worker,
        FlowGraph::publish(TOPIC_MQTT_CONNECTED),
        FlowGraph::publish(TOPIC_MQTT_DISCONNECTED)
    )
);
```

### 2. MQTT Connection Worker

The `mqtt_connect_worker` function:
1. Creates MQTT connection data from discovered broker
2. Sets up event bus integration
3. Connects to MQTT broker
4. Subscribes to command topics
5. Sets up message callback for event bus publishing

```cpp
static bool mqtt_connect_worker(void** out) {
    const char* host = static_cast<const char*>(*out);
    MqttConnectionData connection_data(host, MQTT_DEFAULT_PORT, MQTT_CLIENT_ID);
    
    MqttClient::setEventBus(&BUS);
    auto result = MqttClient::connect(connection_data);
    
    if (result.success) {
        MqttClient::subscribe(MQTT_SUBSCRIBE_TOPIC, 1);
        MqttClient::setMessageCallback([](const MqttMessageData& message) {
            auto* msg_data = new MqttMessageData(message);
            Event message_event{TOPIC_MQTT_MESSAGE, 0, msg_data};
            BUS.publish(message_event);
        });
    }
    
    return result.success;
}
```

### 3. Message Handling

Incoming MQTT messages are automatically:
1. Received by the MQTT client
2. Converted to `MqttMessageData` structures
3. Published as events to the event bus
4. Processed by event handlers

```cpp
void mqtt_message_handler(const Event& e, void* user) {
    const MqttMessageData* msg_data = static_cast<const MqttMessageData*>(e.ptr);
    if (msg_data) {
        // Process the message
        std::string ack_msg = "{\"received\":\"" + msg_data->payload + "\"}";
        MqttClient::publish("esp32/ack", ack_msg, 1);
        
        // Clean up memory
        delete msg_data;
    }
}
```

## Configuration

### MQTT Topics
- **Subscribe**: `esp32/commands` - Receives commands from external systems
- **Publish**: `esp32/status` - Publishes device status and acknowledgments
- **Acknowledgment**: `esp32/ack` - Confirms received commands

### Connection Settings
- **Client ID**: `esp32_eventbus_client`
- **Default Port**: 1883
- **Keepalive**: 60 seconds
- **Clean Session**: Enabled

## Testing

### Test Suite (`test/test_mqtt_integration.cpp`)

Comprehensive test coverage including:
- MQTT connection testing
- Topic subscription testing
- Message publishing testing
- Message callback testing
- Event bus integration testing

### Running Tests

```bash
# Build and run tests
bun run test

# Or manually
./test/build_mqtt_test.sh
./test/build/test_mqtt_integration
```

## Functional Programming Principles Applied

### 1. Immutable Data Structures
- `MqttConnectionData` and `MqttMessageData` are read-only after construction
- No mutable state in data structures

### 2. Pure Functions
- Connection operations return `MqttConnectionResult` with explicit success/failure
- No side effects in data transformation functions

### 3. Function Composition
- Event handlers are composed using the FlowGraph system
- Message processing follows a pipeline pattern

### 4. Separation of Concerns
- MQTT client handles only MQTT operations
- Event bus handles only event distribution
- Message processing is separate from transport

## Memory Management

### Automatic Cleanup
- MQTT message events are automatically cleaned up after processing
- Event handlers are responsible for deleting message data

### RAII Principles
- MQTT client uses RAII for connection management
- Automatic cleanup on disconnect

## Error Handling

### Graceful Degradation
- mDNS failures trigger fallback mechanisms
- MQTT connection failures are handled by event bus
- System continues operation even with MQTT issues

### Error Events
- Connection failures publish `TOPIC_SYSTEM_ERROR` events
- Disconnection events are properly propagated

## Usage Example

```cpp
// The system automatically:
// 1. Connects to WiFi
// 2. Discovers MQTT broker via mDNS
// 3. Connects to MQTT broker
// 4. Subscribes to command topics
// 5. Publishes status messages
// 6. Processes incoming commands

// To send a command to the device:
// Publish to topic: esp32/commands
// Message: {"command": "set_pin", "pin": 2, "value": 1}

// Device will respond with:
// Topic: esp32/ack
// Message: {"received": "{\"command\": \"set_pin\", \"pin\": 2, \"value\": 1}"}
```

## Benefits

1. **Modularity**: MQTT client is completely separate from event bus
2. **Testability**: All components can be tested independently
3. **Maintainability**: Clear separation of concerns
4. **Reliability**: Proper error handling and memory management
5. **Extensibility**: Easy to add new MQTT topics and handlers

## Future Enhancements

1. **TLS Support**: Add secure MQTT connections
2. **Authentication**: Support for username/password authentication
3. **Message Queuing**: Persistent message storage for offline scenarios
4. **Multiple Brokers**: Support for connecting to multiple MQTT brokers
5. **Message Validation**: JSON schema validation for incoming messages
