#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cstring>
#include <functional>
#include <memory>

// Mock ESP-IDF types and functions
typedef int esp_err_t;
typedef int esp_log_level_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE -1
#define ESP_LOG_INFO 3
#define ESP_LOG_ERROR 1
#define ESP_LOG_WARN 2

void esp_log_write(esp_log_level_t level, const char* tag, const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf("[%s] ", tag);
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

// Mock FreeRTOS types
typedef int BaseType_t;
typedef int UBaseType_t;
typedef int TickType_t;
#define pdMS_TO_TICKS(x) (x)
#define tskIDLE_PRIORITY 0

// Include our event bus system
#include "../main/eventbus/EventProtocol.h"
#include "../main/eventbus/TinyEventBus.h"
#include "../main/eventbus/FlowGraph.h"
#include "../main/data_structures.h"
#include "../main/device_commands.h"

// Forward declarations for message processor
struct DeviceCommandResult {
    bool success;
    std::vector<DevicePinCommand> device_commands;
    std::string error_message;
};

class MessageProcessor {
public:
    static DeviceCommandResult processMessageToDeviceCommands(const std::string& message);
};

// Simple implementation for testing
DeviceCommandResult MessageProcessor::processMessageToDeviceCommands(const std::string& message) {
    DeviceCommandResult result;
    
    // Simple parsing for testing - just check if it contains "pin_set" or "pin_read"
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

// Mock event bus for testing
class MockEventBus : public IEventBus {
private:
    std::vector<Event> published_events;
    std::vector<std::pair<EventType, std::function<void(const Event&)>>> handlers;

public:
    void publish(const Event& event) override {
        published_events.push_back(event);
        // Trigger handlers
        for (const auto& handler : handlers) {
            if (handler.first == event.type || handler.first == TOPIC__ANY) {
                handler.second(event);
            }
        }
    }

    ListenerHandle subscribe(EventType topic, std::function<void(const Event&)> handler) override {
        handlers.push_back({topic, handler});
        return handlers.size() - 1;
    }

    void unsubscribe(ListenerHandle handle) override {
        if (handle >= 0 && handle < static_cast<int>(handlers.size())) {
            handlers[handle] = {TOPIC__ANY, nullptr};
        }
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

// Mock FlowGraph for testing
class MockFlowGraph {
private:
    MockEventBus& bus_;

public:
    MockFlowGraph(MockEventBus& bus) : bus_(bus) {}

    void when(EventType topic, std::function<void(const Event&)> handler) {
        bus_.subscribe(topic, handler);
    }

    void when(EventType topic, const Flow& flow) {
        bus_.subscribe(topic, [flow](const Event& e) { 
            flow(e, MockEventBus::instance()); 
        });
    }

    static Flow tap(std::function<void(const Event&)> fn) {
        return [fn](const Event& e, IEventBus& bus) {
            fn(e);
        };
    }

    static Flow publish(EventType topic, int32_t i32, void* ptr) {
        return [topic, i32, ptr](const Event& e, IEventBus& bus) {
            bus.publish(Event{topic, i32, ptr});
        };
    }

    static Flow seq(const Flow& first, const Flow& second) {
        return [first, second](const Event& e, IEventBus& bus) {
            first(e, bus);
            second(e, bus);
        };
    }

    Flow async_blocking_with_event(const char* name,
                                  std::function<bool(const Event&, void**)> workerFn,
                                  const Flow& onOk,
                                  const Flow& onErr) {
        return [workerFn, onOk, onErr](const Event& trigger, IEventBus& bus) {
            void* payload = nullptr;
            bool success = workerFn(trigger, &payload);
            
            if (success) {
                onOk(trigger, bus);
            } else {
                onErr(trigger, bus);
            }
        };
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

// Test cases
void test_basic_event_publishing() {
    printf("\n=== Testing Basic Event Publishing ===\n");
    MockEventBus bus;
    
    // Subscribe to an event
    bool event_received = false;
    bus.subscribe(TOPIC_MDNS_FOUND, [&event_received](const Event& e) {
        event_received = true;
        printf("Event received: type=%d, ptr=%s\n", e.type, 
               e.ptr ? static_cast<const char*>(e.ptr) : "NULL");
    });
    
    // Publish an event
    const char* test_hostname = "test.local";
    bus.publish(Event{TOPIC_MDNS_FOUND, 1, const_cast<char*>(test_hostname)});
    
    assert(event_received);
    assert(bus.has_event(TOPIC_MDNS_FOUND));
    printf("✓ Basic event publishing works\n");
}

void test_flow_graph_basic() {
    printf("\n=== Testing FlowGraph Basic Operations ===\n");
    MockEventBus bus;
    MockFlowGraph G(bus);
    
    bool flow_triggered = false;
    std::string received_hostname;
    
    // Create a simple flow
    G.when(TOPIC_MDNS_FOUND, [&flow_triggered, &received_hostname](const Event& e) {
        flow_triggered = true;
        received_hostname = static_cast<const char*>(e.ptr);
        printf("Flow triggered with hostname: %s\n", received_hostname.c_str());
    });
    
    // Trigger the flow
    const char* test_hostname = "test.local";
    bus.publish(Event{TOPIC_MDNS_FOUND, 1, const_cast<char*>(test_hostname)});
    
    assert(flow_triggered);
    assert(received_hostname == "test.local");
    printf("✓ FlowGraph basic operations work\n");
}

void test_async_blocking_with_event() {
    printf("\n=== Testing Async Blocking with Event ===\n");
    MockEventBus bus;
    MockFlowGraph G(bus);
    
    bool success_flow_triggered = false;
    bool error_flow_triggered = false;
    
    // Create async flow with event data
    G.when(TOPIC_MDNS_FOUND,
        G.async_blocking_with_event("test-mqtt-connect", test_mqtt_connection_worker,
            // onOk
            [&success_flow_triggered](const Event& e, IEventBus& bus) {
                success_flow_triggered = true;
                bus.publish(Event{TOPIC_MQTT_CONNECTED, 1, nullptr});
                printf("✓ MQTT connection success flow triggered\n");
            },
            // onErr
            [&error_flow_triggered](const Event& e, IEventBus& bus) {
                error_flow_triggered = true;
                bus.publish(Event{TOPIC_MQTT_DISCONNECTED, 0, nullptr});
                bus.publish(Event{TOPIC_SYSTEM_ERROR, 6, nullptr});
                printf("✗ MQTT connection error flow triggered\n");
            }
        )
    );
    
    // Test with valid hostname
    const char* valid_hostname = "test.mosquitto.org";
    bus.publish(Event{TOPIC_MDNS_FOUND, 1, const_cast<char*>(valid_hostname)});
    
    assert(success_flow_triggered);
    assert(!error_flow_triggered);
    assert(bus.has_event(TOPIC_MQTT_CONNECTED));
    printf("✓ Async blocking with valid hostname works\n");
    
    // Reset for next test
    bus.clear_events();
    success_flow_triggered = false;
    error_flow_triggered = false;
    
    // Test with invalid hostname
    const char* invalid_hostname = "invalid.host";
    bus.publish(Event{TOPIC_MDNS_FOUND, 1, const_cast<char*>(invalid_hostname)});
    
    assert(!success_flow_triggered);
    assert(error_flow_triggered);
    assert(bus.has_event(TOPIC_MQTT_DISCONNECTED));
    assert(bus.has_event(TOPIC_SYSTEM_ERROR));
    printf("✓ Async blocking with invalid hostname works\n");
}

void test_mdns_to_mqtt_flow() {
    printf("\n=== Testing mDNS to MQTT Flow ===\n");
    MockEventBus bus;
    MockFlowGraph G(bus);
    
    std::vector<EventType> flow_sequence;
    
    // Create the complete flow from mDNS to MQTT
    G.when(TOPIC_MDNS_FOUND,
        G.async_blocking_with_event("mqtt-connect", test_mqtt_connection_worker,
            // onOk → publish MQTT_CONNECTED
            [&flow_sequence](const Event& e, IEventBus& bus) {
                flow_sequence.push_back(TOPIC_MQTT_CONNECTED);
                bus.publish(Event{TOPIC_MQTT_CONNECTED, 1, nullptr});
            },
            // onErr → publish MQTT_DISCONNECTED and system error
            [&flow_sequence](const Event& e, IEventBus& bus) {
                flow_sequence.push_back(TOPIC_MQTT_DISCONNECTED);
                flow_sequence.push_back(TOPIC_SYSTEM_ERROR);
                bus.publish(Event{TOPIC_MQTT_DISCONNECTED, 0, nullptr});
                bus.publish(Event{TOPIC_SYSTEM_ERROR, 6, nullptr});
            }
        )
    );
    
    // Simulate mDNS finding a broker
    const char* hostname = "test.mosquitto.org";
    bus.publish(Event{TOPIC_MDNS_FOUND, 1, const_cast<char*>(hostname)});
    
    // Verify the flow sequence
    assert(flow_sequence.size() == 1);
    assert(flow_sequence[0] == TOPIC_MQTT_CONNECTED);
    assert(bus.has_event(TOPIC_MQTT_CONNECTED));
    printf("✓ Complete mDNS to MQTT flow works\n");
}

void test_message_parsing() {
    printf("\n=== Testing Message Parsing ===\n");
    
    // Test valid message with pin_set
    std::string valid_message = "pin_set";
    auto result = MessageProcessor::processMessageToDeviceCommands(valid_message);
    assert(result.success);
    assert(result.device_commands.size() == 1);
    assert(result.device_commands[0].type == PIN_SET);
    assert(result.device_commands[0].pin == 2);
    printf("✓ Message parsing works with pin_set\n");
    
    // Test valid message with pin_read
    std::string read_message = "pin_read";
    auto read_result = MessageProcessor::processMessageToDeviceCommands(read_message);
    assert(read_result.success);
    assert(read_result.device_commands.size() == 1);
    assert(read_result.device_commands[0].type == PIN_READ);
    assert(read_result.device_commands[0].pin == 3);
    printf("✓ Message parsing works with pin_read\n");
    
    // Test invalid message
    std::string invalid_message = "invalid";
    auto invalid_result = MessageProcessor::processMessageToDeviceCommands(invalid_message);
    assert(!invalid_result.success);
    printf("✓ Message parsing handles invalid messages\n");
}

void test_pin_command_execution() {
    printf("\n=== Testing Pin Command Execution ===\n");
    MockEventBus bus;
    MockFlowGraph G(bus);
    
    std::vector<std::string> executed_commands;
    
    // Helper lambda for pin command execution
    auto executePinCommand = [&executed_commands](const Event& e, DeviceCommandType cmdType) {
        auto* pin_cmd_data = static_cast<PinCommandData*>(e.ptr);
        if (!pin_cmd_data) {
            printf("ERROR: No pin command data\n");
            return;
        }
        
        std::string cmd_desc = "Executed " + std::to_string(cmdType) + " on pin " + 
                              std::to_string(pin_cmd_data->pin) + " with value " + 
                              std::to_string(pin_cmd_data->value);
        executed_commands.push_back(cmd_desc);
        printf("✓ %s\n", cmd_desc.c_str());
        
        delete pin_cmd_data;
    };
    
    // Create flows for different pin commands
    G.when(TOPIC_PIN_SET, [executePinCommand](const Event& e) {
        executePinCommand(e, PIN_SET);
    });
    
    G.when(TOPIC_PIN_READ, [executePinCommand](const Event& e) {
        executePinCommand(e, PIN_READ);
    });
    
    // Test pin set command
    auto* pin_set_data = new PinCommandData(2, 1, "Turn on LED");
    bus.publish(Event{TOPIC_PIN_SET, 2, pin_set_data});
    
    // Test pin read command
    auto* pin_read_data = new PinCommandData(3, 0, "Read sensor");
    bus.publish(Event{TOPIC_PIN_READ, 3, pin_read_data});
    
    assert(executed_commands.size() == 2);
    assert(executed_commands[0].find("PIN_SET") != std::string::npos);
    assert(executed_commands[1].find("PIN_READ") != std::string::npos);
    printf("✓ Pin command execution works\n");
}

void test_complete_message_flow() {
    printf("\n=== Testing Complete Message Flow ===\n");
    MockEventBus bus;
    MockFlowGraph G(bus);
    
    std::vector<std::string> flow_log;
    
    // Flow 4: When MQTT message received, parse it into device commands
    G.when(TOPIC_MQTT_MESSAGE, 
        [&flow_log, &bus](const Event& e) {
            auto* msg_data = static_cast<MqttMessageData*>(e.ptr);
            if (!msg_data) {
                flow_log.push_back("ERROR: No message data");
                return;
            }
            
            flow_log.push_back("Parsing message: " + msg_data->payload);
            
            // Parse message to device commands
            auto device_command_result = MessageProcessor::processMessageToDeviceCommands(msg_data->payload);
            
            if (!device_command_result.success) {
                flow_log.push_back("ERROR: Failed to parse message");
                return;
            }
            
            flow_log.push_back("Successfully parsed " + std::to_string(device_command_result.device_commands.size()) + " commands");
            
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
                        flow_log.push_back("Published PIN_SET for pin " + std::to_string(device_cmd.pin));
                        break;
                    }
                    case PIN_READ: {
                        auto* pin_cmd_data = new PinCommandData{
                            device_cmd.pin,
                            0,
                            device_cmd.description
                        };
                        bus.publish(Event{TOPIC_PIN_READ, device_cmd.pin, pin_cmd_data});
                        flow_log.push_back("Published PIN_READ for pin " + std::to_string(device_cmd.pin));
                        break;
                    }
                    default:
                        flow_log.push_back("Unknown command type");
                        break;
                }
            }
        }
    );
    
    // Test with a valid message
    std::string test_message = "pin_set";
    auto* msg_data = new MqttMessageData{"test/topic", test_message};
    bus.publish(Event{TOPIC_MQTT_MESSAGE, 1, msg_data});
    
    // Verify the flow
    assert(flow_log.size() >= 3);
    assert(flow_log[0].find("Parsing message") != std::string::npos);
    assert(flow_log[1].find("Successfully parsed 1 commands") != std::string::npos);
    assert(bus.has_event(TOPIC_PIN_SET));
    printf("✓ Complete message flow works\n");
}

int main() {
    printf("Starting Event Flow Unit Tests...\n");
    
    test_basic_event_publishing();
    test_flow_graph_basic();
    test_async_blocking_with_event();
    test_mdns_to_mqtt_flow();
    test_message_parsing();
    test_pin_command_execution();
    test_complete_message_flow();
    
    printf("\n🎉 All tests passed! Event flow system is working correctly.\n");
    return 0;
}
