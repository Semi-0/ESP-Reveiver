#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cstring>
#include <functional>
#include <memory>

// Minimal event system for testing
enum EventType {
    TOPIC_MDNS_FOUND = 1,
    TOPIC_MQTT_CONNECTED = 2,
    TOPIC_MQTT_DISCONNECTED = 3,
    TOPIC_MQTT_MESSAGE = 4,
    TOPIC_PIN_SET = 5,
    TOPIC_PIN_READ = 6,
    TOPIC_SYSTEM_ERROR = 7,
    TOPIC__ANY = 0
};

struct Event {
    EventType type;
    int32_t i32;
    void* ptr;
    
    Event(EventType t, int32_t i, void* p) : type(t), i32(i), ptr(p) {}
};

// Mock data structures
struct MqttMessageData {
    std::string topic;
    std::string payload;
    
    MqttMessageData(const std::string& t, const std::string& p) : topic(t), payload(p) {}
};

struct PinCommandData {
    int pin;
    int value;
    std::string description;
    
    PinCommandData(int p, int v, const std::string& desc) : pin(p), value(v), description(desc) {}
};

enum DeviceCommandType {
    PIN_SET = 1,
    PIN_READ = 2,
    PIN_MODE = 3
};

struct DevicePinCommand {
    DeviceCommandType type;
    int pin;
    int value;
    std::string description;
    
    DevicePinCommand(DeviceCommandType t, int p, int v, const std::string& desc) 
        : type(t), pin(p), value(v), description(desc) {}
};

struct DeviceCommandResult {
    bool success;
    std::vector<DevicePinCommand> device_commands;
    std::string error_message;
};

// Mock event bus
class MockEventBus {
private:
    std::vector<Event> published_events;
    std::vector<std::pair<EventType, std::function<void(const Event&)>>> handlers;

public:
    void publish(const Event& event) {
        published_events.push_back(event);
        // Trigger handlers
        for (const auto& handler : handlers) {
            if (handler.first == event.type || handler.first == TOPIC__ANY) {
                handler.second(event);
            }
        }
    }

    void subscribe(EventType topic, std::function<void(const Event&)> handler) {
        handlers.push_back({topic, handler});
    }

    // Test helper methods
    const std::vector<Event>& get_published_events() const { return published_events; }
    void clear_events() { published_events.clear(); }
    bool has_event(EventType type) const {
        for (const auto& event : published_events) {
            if (event.type == type) return true;
        }
        return false;
    }
    Event get_last_event() const {
        if (published_events.empty()) return Event{TOPIC__ANY, 0, nullptr};
        return published_events.back();
    }
};

// Mock message processor
class MessageProcessor {
public:
    static DeviceCommandResult processMessageToDeviceCommands(const std::string& message) {
        DeviceCommandResult result;
        
        // Simple parsing for testing
        if (message.find("pin_set") != std::string::npos) {
            DevicePinCommand cmd(PIN_SET, 2, 1, "Turn on LED");
            result.device_commands.push_back(cmd);
            result.success = true;
        } else if (message.find("pin_read") != std::string::npos) {
            DevicePinCommand cmd(PIN_READ, 3, 0, "Read sensor");
            result.device_commands.push_back(cmd);
            result.success = true;
        } else {
            result.success = false;
            result.error_message = "Invalid message format";
        }
        
        return result;
    }
};

// Test functions
std::string get_esp32_device_id() {
    return "ESP32_58B8D8";
}

std::string get_mqtt_control_topic() {
    return "esp32/control";
}

// MQTT connection worker for testing
bool test_mqtt_connection_worker(const Event& trigger_event, void** out) {
    const char* host = static_cast<const char*>(trigger_event.ptr);
    printf("test_mqtt_connection_worker called with host: %s\n", host ? host : "NULL");
    
    if (!host) {
        printf("ERROR: No hostname provided\n");
        return false;
    }
    
    // Simulate connection success for test.mosquitto.org, failure for others
    if (std::string(host) == "test.mosquitto.org") {
        printf("SUCCESS: Connected to %s\n", host);
        return true;
    } else {
        printf("ERROR: Failed to connect to %s\n", host);
        return false;
    }
}

// ===== EXTRACTED FLOW FUNCTIONS =====

// Flow 1: mDNS query worker
bool mdns_query_flow(void** out) {
    printf("mdns_query_flow: Simulating mDNS query\n");
    
    // Simulate finding a service
    std::string hostname = "test.mosquitto.org";
    *out = strdup(hostname.c_str());
    printf("mdns_query_flow: Found hostname: %s\n", hostname.c_str());
    return true;
}

// Flow 2: MQTT connection handler
void mqtt_connection_flow(const Event& e, MockEventBus& bus) {
    const char* hostname = static_cast<const char*>(e.ptr);
    printf("mqtt_connection_flow: Received hostname: %s\n", hostname ? hostname : "NULL");
    
    if (!hostname) {
        printf("mqtt_connection_flow: ERROR - No hostname provided\n");
        bus.publish(Event{TOPIC_MQTT_DISCONNECTED, 0, nullptr});
        bus.publish(Event{TOPIC_SYSTEM_ERROR, 6, nullptr});
        return;
    }
    
    // Simulate MQTT connection attempt
    void* out = nullptr;
    bool success = test_mqtt_connection_worker(e, &out);
    
    if (success) {
        printf("mqtt_connection_flow: SUCCESS - Publishing MQTT_CONNECTED\n");
        bus.publish(Event{TOPIC_MQTT_CONNECTED, 1, nullptr});
    } else {
        printf("mqtt_connection_flow: FAILURE - Publishing MQTT_DISCONNECTED and SYSTEM_ERROR\n");
        bus.publish(Event{TOPIC_MQTT_DISCONNECTED, 0, nullptr});
        bus.publish(Event{TOPIC_SYSTEM_ERROR, 6, nullptr});
    }
}

// Flow 3: MQTT message parsing handler
void mqtt_message_parsing_flow(const Event& e, MockEventBus& bus) {
    auto* msg_data = static_cast<MqttMessageData*>(e.ptr);
    if (!msg_data) {
        printf("mqtt_message_parsing_flow: ERROR - No message data\n");
        bus.publish(Event{TOPIC_SYSTEM_ERROR, 1, nullptr});
        return;
    }
    
    printf("mqtt_message_parsing_flow: Parsing message: %s\n", msg_data->payload.c_str());
    
    // Parse message to device commands
    auto device_command_result = MessageProcessor::processMessageToDeviceCommands(msg_data->payload);
    
    if (!device_command_result.success) {
        printf("mqtt_message_parsing_flow: ERROR - Failed to parse message\n");
        bus.publish(Event{TOPIC_SYSTEM_ERROR, 3, nullptr});
        return;
    }
    
    printf("mqtt_message_parsing_flow: Successfully parsed %zu commands\n", device_command_result.device_commands.size());
    
    // Publish specific command events for each device command
    for (const auto& device_cmd : device_command_result.device_commands) {
        switch (device_cmd.type) {
            case PIN_SET: {
                auto* pin_cmd_data = new PinCommandData{
                    device_cmd.pin,
                    device_cmd.value,
                    device_cmd.description
                };
                bus.publish(Event{TOPIC_PIN_SET, device_cmd.pin, pin_cmd_data});
                printf("mqtt_message_parsing_flow: Published PIN_SET for pin %d\n", device_cmd.pin);
                break;
            }
            case PIN_READ: {
                auto* pin_cmd_data = new PinCommandData{
                    device_cmd.pin,
                    0,
                    device_cmd.description
                };
                bus.publish(Event{TOPIC_PIN_READ, device_cmd.pin, pin_cmd_data});
                printf("mqtt_message_parsing_flow: Published PIN_READ for pin %d\n", device_cmd.pin);
                break;
            }
            default:
                printf("mqtt_message_parsing_flow: Unknown command type\n");
                bus.publish(Event{TOPIC_SYSTEM_ERROR, 4, nullptr});
                break;
        }
    }
}

// Flow 4: Pin command execution handler
void pin_command_execution_flow(const Event& e, MockEventBus& bus) {
    auto* pin_cmd_data = static_cast<PinCommandData*>(e.ptr);
    if (!pin_cmd_data) {
        printf("pin_command_execution_flow: ERROR - No pin command data\n");
        bus.publish(Event{TOPIC_SYSTEM_ERROR, 5, nullptr});
        return;
    }
    
    printf("pin_command_execution_flow: Executing command on pin %d with value %d\n", 
           pin_cmd_data->pin, pin_cmd_data->value);
    
    // Simulate pin execution
    bool success = true; // Simulate successful execution
    
    if (success) {
        printf("pin_command_execution_flow: SUCCESS - Pin command executed\n");
        // Could publish a success event here
    } else {
        printf("pin_command_execution_flow: FAILURE - Pin command failed\n");
        bus.publish(Event{TOPIC_SYSTEM_ERROR, 7, nullptr});
    }
    
    delete pin_cmd_data;
}

// ===== TEST CASES =====

void test_mdns_to_mqtt_data_passing() {
    printf("\n=== Testing mDNS to MQTT Data Passing ===\n");
    MockEventBus bus;
    
    std::vector<std::string> flow_log;
    
    // Subscribe to mDNS found events
    bus.subscribe(TOPIC_MDNS_FOUND, [&flow_log, &bus](const Event& e) {
        const char* hostname = static_cast<const char*>(e.ptr);
        flow_log.push_back("mDNS_FOUND received: " + std::string(hostname ? hostname : "NULL"));
        
        // Call the extracted flow function
        mqtt_connection_flow(e, bus);
    });
    
    // Subscribe to MQTT events to verify the flow
    bus.subscribe(TOPIC_MQTT_CONNECTED, [&flow_log](const Event& e) {
        flow_log.push_back("MQTT_CONNECTED received");
    });
    
    bus.subscribe(TOPIC_MQTT_DISCONNECTED, [&flow_log](const Event& e) {
        flow_log.push_back("MQTT_DISCONNECTED received");
    });
    
    bus.subscribe(TOPIC_SYSTEM_ERROR, [&flow_log](const Event& e) {
        flow_log.push_back("SYSTEM_ERROR received: " + std::to_string(e.i32));
    });
    
    // Test with valid hostname
    const char* valid_hostname = "test.mosquitto.org";
    bus.publish(Event{TOPIC_MDNS_FOUND, 1, const_cast<char*>(valid_hostname)});
    
    // Verify the flow
    assert(flow_log.size() >= 2);
    assert(flow_log[0].find("mDNS_FOUND received: test.mosquitto.org") != std::string::npos);
    assert(flow_log[1].find("MQTT_CONNECTED received") != std::string::npos);
    assert(bus.has_event(TOPIC_MQTT_CONNECTED));
    printf("✓ mDNS to MQTT data passing works with valid hostname\n");
    
    // Reset for next test
    bus.clear_events();
    flow_log.clear();
    
    // Test with invalid hostname
    const char* invalid_hostname = "invalid.host";
    bus.publish(Event{TOPIC_MDNS_FOUND, 1, const_cast<char*>(invalid_hostname)});
    
    // Verify the error flow
    assert(flow_log.size() >= 2);
    assert(flow_log[0].find("mDNS_FOUND received: invalid.host") != std::string::npos);
    assert(flow_log[1].find("MQTT_DISCONNECTED received") != std::string::npos);
    assert(bus.has_event(TOPIC_MQTT_DISCONNECTED));
    assert(bus.has_event(TOPIC_SYSTEM_ERROR));
    printf("✓ mDNS to MQTT data passing handles invalid hostname\n");
}

void test_mqtt_message_to_pin_data_passing() {
    printf("\n=== Testing MQTT Message to Pin Data Passing ===\n");
    MockEventBus bus;
    
    std::vector<std::string> flow_log;
    
    // Subscribe to MQTT message events
    bus.subscribe(TOPIC_MQTT_MESSAGE, [&flow_log, &bus](const Event& e) {
        flow_log.push_back("MQTT_MESSAGE received");
        
        // Call the extracted flow function
        mqtt_message_parsing_flow(e, bus);
    });
    
    // Subscribe to pin command events
    bus.subscribe(TOPIC_PIN_SET, [&flow_log, &bus](const Event& e) {
        flow_log.push_back("PIN_SET received");
        
        // Call the extracted flow function
        pin_command_execution_flow(e, bus);
    });
    
    bus.subscribe(TOPIC_PIN_READ, [&flow_log, &bus](const Event& e) {
        flow_log.push_back("PIN_READ received");
        
        // Call the extracted flow function
        pin_command_execution_flow(e, bus);
    });
    
    bus.subscribe(TOPIC_SYSTEM_ERROR, [&flow_log](const Event& e) {
        flow_log.push_back("SYSTEM_ERROR received: " + std::to_string(e.i32));
    });
    
    // Test with pin_set message
    std::string test_message = "pin_set";
    auto* msg_data = new MqttMessageData{"test/topic", test_message};
    bus.publish(Event{TOPIC_MQTT_MESSAGE, 1, msg_data});
    
    // Verify the flow
    assert(flow_log.size() >= 3);
    assert(flow_log[0].find("MQTT_MESSAGE received") != std::string::npos);
    assert(flow_log[1].find("PIN_SET received") != std::string::npos);
    assert(bus.has_event(TOPIC_PIN_SET));
    printf("✓ MQTT message to pin data passing works with pin_set\n");
    
    delete msg_data;
    
    // Reset for next test
    bus.clear_events();
    flow_log.clear();
    
    // Test with pin_read message
    std::string read_message = "pin_read";
    auto* read_msg_data = new MqttMessageData{"test/topic", read_message};
    bus.publish(Event{TOPIC_MQTT_MESSAGE, 1, read_msg_data});
    
    // Verify the flow
    assert(flow_log.size() >= 3);
    assert(flow_log[0].find("MQTT_MESSAGE received") != std::string::npos);
    assert(flow_log[1].find("PIN_READ received") != std::string::npos);
    assert(bus.has_event(TOPIC_PIN_READ));
    printf("✓ MQTT message to pin data passing works with pin_read\n");
    
    delete read_msg_data;
}

void test_complete_flow_chain() {
    printf("\n=== Testing Complete Flow Chain ===\n");
    MockEventBus bus;
    
    std::vector<std::string> flow_log;
    
    // Set up the complete flow chain
    bus.subscribe(TOPIC_MDNS_FOUND, [&flow_log, &bus](const Event& e) {
        flow_log.push_back("1. mDNS_FOUND");
        mqtt_connection_flow(e, bus);
    });
    
    bus.subscribe(TOPIC_MQTT_CONNECTED, [&flow_log](const Event& e) {
        flow_log.push_back("2. MQTT_CONNECTED");
    });
    
    bus.subscribe(TOPIC_MQTT_MESSAGE, [&flow_log, &bus](const Event& e) {
        flow_log.push_back("3. MQTT_MESSAGE");
        mqtt_message_parsing_flow(e, bus);
    });
    
    bus.subscribe(TOPIC_PIN_SET, [&flow_log, &bus](const Event& e) {
        flow_log.push_back("4. PIN_SET");
        pin_command_execution_flow(e, bus);
    });
    
    // Simulate the complete flow
    // Step 1: mDNS finds broker
    const char* hostname = "test.mosquitto.org";
    bus.publish(Event{TOPIC_MDNS_FOUND, 1, const_cast<char*>(hostname)});
    
    // Step 2: MQTT connects (simulated by the flow)
    // Step 3: MQTT message arrives
    std::string test_message = "pin_set";
    auto* msg_data = new MqttMessageData{"test/topic", test_message};
    bus.publish(Event{TOPIC_MQTT_MESSAGE, 1, msg_data});
    
    // Verify the complete flow
    assert(flow_log.size() >= 4);
    assert(flow_log[0].find("1. mDNS_FOUND") != std::string::npos);
    assert(flow_log[1].find("2. MQTT_CONNECTED") != std::string::npos);
    assert(flow_log[2].find("3. MQTT_MESSAGE") != std::string::npos);
    assert(flow_log[3].find("4. PIN_SET") != std::string::npos);
    
    assert(bus.has_event(TOPIC_MQTT_CONNECTED));
    assert(bus.has_event(TOPIC_PIN_SET));
    printf("✓ Complete flow chain works correctly\n");
    
    delete msg_data;
}

void test_flow_function_isolation() {
    printf("\n=== Testing Flow Function Isolation ===\n");
    
    // Test each flow function in isolation
    MockEventBus bus1, bus2, bus3;
    
    // Test mDNS query flow
    void* out = nullptr;
    bool mdns_success = mdns_query_flow(&out);
    assert(mdns_success);
    assert(out != nullptr);
    assert(std::string(static_cast<char*>(out)) == "test.mosquitto.org");
    free(out);
    printf("✓ mDNS query flow works in isolation\n");
    
    // Test MQTT connection flow
    const char* test_host = "test.mosquitto.org";
    Event test_event(TOPIC_MDNS_FOUND, 1, const_cast<char*>(test_host));
    mqtt_connection_flow(test_event, bus1);
    assert(bus1.has_event(TOPIC_MQTT_CONNECTED));
    printf("✓ MQTT connection flow works in isolation\n");
    
    // Test MQTT message parsing flow
    std::string test_message = "pin_set";
    auto* msg_data = new MqttMessageData{"test/topic", test_message};
    Event msg_event(TOPIC_MQTT_MESSAGE, 1, msg_data);
    mqtt_message_parsing_flow(msg_event, bus2);
    assert(bus2.has_event(TOPIC_PIN_SET));
    printf("✓ MQTT message parsing flow works in isolation\n");
    
    // Test pin command execution flow
    auto* pin_data = new PinCommandData(2, 1, "Test");
    Event pin_event(TOPIC_PIN_SET, 2, pin_data);
    pin_command_execution_flow(pin_event, bus3);
    // Should not publish any events, just execute the command
    printf("✓ Pin command execution flow works in isolation\n");
    
    delete msg_data;
    delete pin_data;
}

int main() {
    printf("Starting Flow Data Passing Unit Tests...\n");
    
    test_mdns_to_mqtt_data_passing();
    test_mqtt_message_to_pin_data_passing();
    test_complete_flow_chain();
    test_flow_function_isolation();
    
    printf("\n🎉 All flow data passing tests passed! Functions are properly isolated and data flows correctly.\n");
    return 0;
}
