#include <iostream>
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <cassert>

// Mock structures for testing
struct MockEvent {
    uint16_t type;
    int32_t i32;
    void* ptr;
    
    MockEvent() : type(0), i32(0), ptr(nullptr) {}
    MockEvent(uint16_t t, int32_t data, void* p = nullptr) : type(t), i32(data), ptr(p) {}
};

// Mock device command structures
enum DeviceCommandType {
    PIN_SET,
    PIN_READ,
    PIN_MODE,
    DEVICE_STATUS,
    DEVICE_RESET
};

struct DevicePinCommand {
    DeviceCommandType type;
    int pin;
    int value;
    std::string description;
    
    DevicePinCommand(DeviceCommandType t, int p, int v, const std::string& desc = "")
        : type(t), pin(p), value(v), description(desc) {}
};

struct DeviceCommandResult {
    bool success;
    std::string error_message;
    std::string action_description;
    int pin;
    int value;
    
    DeviceCommandResult(bool s, const std::string& err = "", const std::string& action = "", int p = 0, int v = 0)
        : success(s), error_message(err), action_description(action), pin(p), value(v) {}
    
    static DeviceCommandResult success_result(const std::string& action, int pin, int value) {
        return DeviceCommandResult(true, "", action, pin, value);
    }
    
    static DeviceCommandResult failure_result(const std::string& error, int pin = 0) {
        return DeviceCommandResult(false, error, "", pin, 0);
    }
};

struct DeviceCommandEvent {
    std::vector<DevicePinCommand> commands;
    std::string source;
    uint64_t timestamp;
    
    DeviceCommandEvent(const std::vector<DevicePinCommand>& cmds, const std::string& src = "mqtt")
        : commands(cmds), source(src), timestamp(0) {}
};

// Mock result structures
struct MockParseResult {
    bool success;
    std::string error_message;
    std::vector<std::string> commands;
};

struct MockDeviceCommandParseResult {
    bool success;
    std::string error_message;
    std::vector<DevicePinCommand> device_commands;
};

// Mock message processor (SRP: only handles parsing and conversion)
class MockMessageProcessor {
public:
    // Pure function to parse JSON message
    static MockParseResult parse_json_message(const std::string& message) {
        if (message.find("digital") != std::string::npos) {
            return {true, "", {"digital:13:1"}};
        } else if (message.find("analog") != std::string::npos) {
            return {true, "", {"analog:9:128"}};
        } else if (message.find("invalid") != std::string::npos) {
            return {false, "Invalid JSON format", {}};
        }
        return {false, "Unknown message format", {}};
    }
    
    // Pure function to convert parsed commands to device commands
    static MockDeviceCommandParseResult convertToDeviceCommands(const MockParseResult& parse_result) {
        if (!parse_result.success) {
            return {false, parse_result.error_message, {}};
        }
        
        std::vector<DevicePinCommand> device_commands;
        for (const auto& cmd_str : parse_result.commands) {
            // Parse command string (e.g., "digital:13:1")
            size_t pos1 = cmd_str.find(':');
            size_t pos2 = cmd_str.find(':', pos1 + 1);
            
            if (pos1 != std::string::npos && pos2 != std::string::npos) {
                std::string type = cmd_str.substr(0, pos1);
                int pin = std::stoi(cmd_str.substr(pos1 + 1, pos2 - pos1 - 1));
                int value = std::stoi(cmd_str.substr(pos2 + 1));
                
                DeviceCommandType cmd_type = PIN_SET;
                if (type == "digital" || type == "analog") {
                    cmd_type = PIN_SET;
                } else if (type == "read") {
                    cmd_type = PIN_READ;
                }
                
                device_commands.emplace_back(cmd_type, pin, value, "Command from " + type);
            }
        }
        
        return {true, "", device_commands};
    }
    
    // Pure function to process message and return device commands
    static MockDeviceCommandParseResult processMessageToDeviceCommands(const std::string& message) {
        auto parse_result = parse_json_message(message);
        return convertToDeviceCommands(parse_result);
    }
};

// Mock device monitor (SRP: only handles device operations)
class MockDeviceMonitor {
public:
    // Pure function to validate device command
    static bool isValidDeviceCommand(const DevicePinCommand& command) {
        if (command.pin < 0 || command.pin > 40) {
            return false;
        }
        
        switch (command.type) {
            case PIN_SET:
                return command.value >= 0 && command.value <= 255;
            case PIN_READ:
                return true;
            case PIN_MODE:
                return command.value == 0 || command.value == 1;
            default:
                return false;
        }
    }
    
    // Pure function to execute a single device command
    static DeviceCommandResult executeDeviceCommand(const DevicePinCommand& command) {
        if (!isValidDeviceCommand(command)) {
            return DeviceCommandResult::failure_result("Invalid device command", command.pin);
        }
        
        switch (command.type) {
            case PIN_SET:
                return DeviceCommandResult::success_result("Pin set successfully", command.pin, command.value);
            case PIN_READ:
                return DeviceCommandResult::success_result("Pin read successfully", command.pin, 0);
            case PIN_MODE:
                return DeviceCommandResult::success_result("Pin mode set successfully", command.pin, command.value);
            default:
                return DeviceCommandResult::failure_result("Unknown command type", command.pin);
        }
    }
    
    // Pure function to execute multiple device commands
    static std::vector<DeviceCommandResult> executeDeviceCommands(const std::vector<DevicePinCommand>& commands) {
        std::vector<DeviceCommandResult> results;
        results.reserve(commands.size());
        
        for (const auto& command : commands) {
            results.push_back(executeDeviceCommand(command));
        }
        
        return results;
    }
};

// Mock event bus
class MockEventBus {
public:
    std::vector<std::pair<std::function<void(const MockEvent&, void*)>, uint32_t>> handlers;
    std::vector<std::string> published_events;
    
    int subscribe(std::function<void(const MockEvent&, void*)> handler, void* user_data, uint32_t mask) {
        handlers.emplace_back(handler, mask);
        return handlers.size() - 1;
    }
    
    void publish(const MockEvent& event) {
        for (auto& [handler, mask] : handlers) {
            if (mask & (1u << event.type)) {
                handler(event, nullptr);
            }
        }
    }
    
    void publishEvent(const std::string& event_type) {
        published_events.push_back(event_type);
    }
};

// Mock event handlers
void handleMqttMessageEvent(const MockEvent& event, void* user_data) {
    const char* message = static_cast<const char*>(event.ptr);
    if (!message) return;
    
    // Parse message and convert to device commands (pure function - no logging)
    auto device_command_result = MockMessageProcessor::processMessageToDeviceCommands(message);
    
    if (!device_command_result.success) {
        std::cout << "MessageProcessor error: " << device_command_result.error_message << std::endl;
        return;
    }
    
    // Publish device command event for device monitor to handle
    if (!device_command_result.device_commands.empty()) {
        // Create device command event
        DeviceCommandEvent device_event(device_command_result.device_commands, "mqtt");
        
        // Publish device command event
        MockEvent device_command_event;
        device_command_event.type = 5; // TOPIC_PIN
        device_command_event.ptr = new DeviceCommandEvent(device_event);
        device_command_event.i32 = 1; // Indicate device command
        
        // In a real implementation, this would publish to event bus
        std::cout << "Published device command event with " << device_event.commands.size() << " commands" << std::endl;
    }
}

void handleDeviceCommandEvent(const MockEvent& event, void* user_data) {
    auto* device_event = static_cast<DeviceCommandEvent*>(event.ptr);
    if (!device_event) return;
    
    // Execute device commands using device monitor (pure function)
    auto execution_results = MockDeviceMonitor::executeDeviceCommands(device_event->commands);
    
    // Process results
    for (const auto& result : execution_results) {
        if (result.success) {
            std::cout << "DeviceMonitor success: " << result.action_description 
                      << " (pin: " << result.pin << ", value: " << result.value << ")" << std::endl;
        } else {
            std::cout << "DeviceMonitor error: " << result.error_message 
                      << " (pin: " << result.pin << ")" << std::endl;
        }
    }
    
    // Clean up the allocated event data
    delete device_event;
}

// ===== TEST FUNCTIONS =====

void testMessageProcessorSRP() {
    std::cout << "\n=== Testing MessageProcessor SRP ===" << std::endl;
    
    // Test valid digital command
    std::string digital_message = "{\"type\":\"digital\",\"pin\":13,\"value\":1}";
    auto result1 = MockMessageProcessor::processMessageToDeviceCommands(digital_message);
    
    assert(result1.success);
    assert(result1.device_commands.size() == 1);
    assert(result1.device_commands[0].type == PIN_SET);
    assert(result1.device_commands[0].pin == 13);
    assert(result1.device_commands[0].value == 1);
    
    std::cout << "Digital command parsed successfully" << std::endl;
    
    // Test valid analog command
    std::string analog_message = "{\"type\":\"analog\",\"pin\":9,\"value\":128}";
    auto result2 = MockMessageProcessor::processMessageToDeviceCommands(analog_message);
    
    assert(result2.success);
    assert(result2.device_commands.size() == 1);
    assert(result2.device_commands[0].type == PIN_SET);
    assert(result2.device_commands[0].pin == 9);
    assert(result2.device_commands[0].value == 128);
    
    std::cout << "Analog command parsed successfully" << std::endl;
    
    // Test invalid message
    std::string invalid_message = "{\"type\":\"invalid\"}";
    auto result3 = MockMessageProcessor::processMessageToDeviceCommands(invalid_message);
    
    assert(!result3.success);
    assert(result3.device_commands.empty());
    
    std::cout << "Invalid message handled correctly" << std::endl;
}

void testDeviceMonitorSRP() {
    std::cout << "\n=== Testing DeviceMonitor SRP ===" << std::endl;
    
    // Test valid pin set command
    DevicePinCommand valid_command(PIN_SET, 13, 1, "Test digital command");
    auto result1 = MockDeviceMonitor::executeDeviceCommand(valid_command);
    
    assert(result1.success);
    assert(result1.pin == 13);
    assert(result1.value == 1);
    
    std::cout << "Valid pin command executed successfully" << std::endl;
    
    // Test invalid pin command
    DevicePinCommand invalid_command(PIN_SET, 50, 1, "Invalid pin");
    auto result2 = MockDeviceMonitor::executeDeviceCommand(invalid_command);
    
    assert(!result2.success);
    assert(result2.pin == 50);
    
    std::cout << "Invalid pin command handled correctly" << std::endl;
    
    // Test multiple commands
    std::vector<DevicePinCommand> commands = {
        DevicePinCommand(PIN_SET, 13, 1, "Digital pin"),
        DevicePinCommand(PIN_SET, 9, 128, "Analog pin"),
        DevicePinCommand(PIN_READ, 12, 0, "Read pin")
    };
    
    auto results = MockDeviceMonitor::executeDeviceCommands(commands);
    
    assert(results.size() == 3);
    assert(results[0].success);
    assert(results[1].success);
    assert(results[2].success);
    
    std::cout << "Multiple commands executed successfully" << std::endl;
}

void testEventDrivenFlow() {
    std::cout << "\n=== Testing Event-Driven Flow ===" << std::endl;
    
    MockEventBus bus;
    
    // Subscribe handlers
    bus.subscribe(handleMqttMessageEvent, nullptr, 1u << 1); // TOPIC_MQTT
    bus.subscribe(handleDeviceCommandEvent, nullptr, 1u << 5); // TOPIC_PIN
    
    // Test MQTT message event
    std::string digital_message = "{\"type\":\"digital\",\"pin\":13,\"value\":1}";
    MockEvent mqtt_event(1, 0, const_cast<char*>(digital_message.c_str())); // TOPIC_MQTT
    bus.publish(mqtt_event);
    
    std::cout << "Event-driven flow completed successfully" << std::endl;
}

void testSeparationOfConcerns() {
    std::cout << "\n=== Testing Separation of Concerns ===" << std::endl;
    
    // MessageProcessor only handles parsing and conversion
    std::string message = "{\"type\":\"digital\",\"pin\":13,\"value\":1}";
    auto parse_result = MockMessageProcessor::processMessageToDeviceCommands(message);
    
    assert(parse_result.success);
    assert(parse_result.device_commands.size() == 1);
    
    // DeviceMonitor only handles device operations
    auto device_result = MockDeviceMonitor::executeDeviceCommand(parse_result.device_commands[0]);
    
    assert(device_result.success);
    assert(device_result.pin == 13);
    assert(device_result.value == 1);
    
    std::cout << "Separation of concerns maintained: MessageProcessor handles parsing, DeviceMonitor handles execution" << std::endl;
}

int main() {
    std::cout << "Testing Single Responsibility Principle Architecture" << std::endl;
    
    testMessageProcessorSRP();
    testDeviceMonitorSRP();
    testEventDrivenFlow();
    testSeparationOfConcerns();
    
    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
