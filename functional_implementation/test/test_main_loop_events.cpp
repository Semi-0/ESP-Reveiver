#include <iostream>
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <cassert>
#include <chrono>

// Mock event structure for testing
struct MockEvent {
    uint16_t type;
    int32_t i32;
    void* ptr;
    
    MockEvent() : type(0), i32(0), ptr(nullptr) {}
    MockEvent(uint16_t t, int32_t data, void* p = nullptr) : type(t), i32(data), ptr(p) {}
};

// Mock event handler type
using MockEventHandler = std::function<void(const MockEvent&, void*)>;

// Mock event bus for testing
class MockEventBus {
public:
    std::vector<std::pair<MockEventHandler, uint32_t>> handlers;
    
    int subscribe(MockEventHandler handler, void* user_data, uint32_t mask) {
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
};

// Mock MQTT functions
class MockMqtt {
public:
    static bool isConnected() { return connected; }
    static void publish(const std::string& topic, const std::string& message) {
        published_messages.emplace_back(topic, message);
    }
    
    static void setConnected(bool status) { connected = status; }
    static std::vector<std::pair<std::string, std::string>> getPublishedMessages() { return published_messages; }
    static void clearMessages() { published_messages.clear(); }
    
private:
    static bool connected;
    static std::vector<std::pair<std::string, std::string>> published_messages;
};

bool MockMqtt::connected = false;
std::vector<std::pair<std::string, std::string>> MockMqtt::published_messages;

// Mock device ID function
std::string getMockDeviceId() {
    return "test_device_123";
}

// Mock uptime function
uint64_t getMockUptime() {
    static uint64_t uptime = 0;
    return uptime++;
}

// ===== PURE FUNCTIONS FOR DEVICE INFO =====

// Pure function to create device status JSON
std::string createDeviceStatusJson(const std::string& device_id, uint64_t uptime_seconds) {
    return "{\"device_id\":\"" + device_id + 
           "\",\"status\":\"online\",\"uptime\":" + 
           std::to_string(uptime_seconds) + "}";
}

// Pure function to check if device info should be published
bool shouldPublishDeviceInfo(uint64_t last_publish_time, uint64_t current_time, uint64_t interval_seconds = 60) {
    return (current_time - last_publish_time) >= interval_seconds;
}

// ===== CURRIED FUNCTIONS FOR DEVICE INFO PUBLISHING =====

// Curried function that takes MQTT publish function and returns a function that takes uptime
auto createMqttDeviceInfoPublisher(const std::function<void(const std::string&, const std::string&)>& mqtt_publish_func) {
    return [mqtt_publish_func](uint64_t uptime_seconds) {
        std::string device_id = getMockDeviceId();
        std::string status_json = createDeviceStatusJson(device_id, uptime_seconds);
        mqtt_publish_func("device/status", status_json);
    };
}

// Pure function that takes clock event and MQTT publish function
auto createDeviceInfoPublisher(const std::string& device_id) {
    return [device_id](uint64_t uptime_seconds, const std::function<void(const std::string&, const std::string&)>& publish_func) {
        std::string status_json = createDeviceStatusJson(device_id, uptime_seconds);
        publish_func("device/status", status_json);
    };
}

// ===== EVENT HANDLERS =====

void handleMainLoopEvent(const MockEvent& event, void* user_data) {
    // Extract current time from event
    uint64_t current_time = static_cast<uint64_t>(event.i32);
    
    // Check if MQTT is connected and we should publish device info
    if (MockMqtt::isConnected()) {
        // Create device status using pure function
        std::string device_id = getMockDeviceId();
        std::string status_json = createDeviceStatusJson(device_id, current_time);
        
        // Publish device info
        MockMqtt::publish("device/status", status_json);
    }
}

// ===== CLOCK EVENT HANDLER WITH CURRIED FUNCTIONS =====

// Curried clock handler factory that takes interval and returns a handler
auto createClockHandler(uint64_t interval_seconds) {
    static uint64_t last_publish_time = 0;
    
    return [interval_seconds](const MockEvent& event, void* user_data) {
        uint64_t current_time = static_cast<uint64_t>(event.i32);
        
        // Check if we should publish device info (pure function)
        if (shouldPublishDeviceInfo(last_publish_time, current_time, interval_seconds)) {
            // Check if MQTT is connected
            if (MockMqtt::isConnected()) {
                // Create device status using pure function
                std::string device_id = getMockDeviceId();
                std::string status_json = createDeviceStatusJson(device_id, current_time);
                
                // Publish device info
                MockMqtt::publish("device/status", status_json);
            }
            last_publish_time = current_time;
        }
    };
}

// Clock event handler that publishes device info every 3 seconds (for testing)
void handleClockEvent(const MockEvent& event, void* user_data) {
    // Use curried clock handler with 3-second interval for testing
    auto clock_handler = createClockHandler(3);
    clock_handler(event, user_data);
}

// ===== TEST FUNCTIONS =====

void testPureDeviceInfoFunctions() {
    std::cout << "\n=== Testing Pure Device Info Functions ===" << std::endl;
    
    // Test device status JSON creation
    std::string device_id = "test_device";
    uint64_t uptime = 12345;
    std::string status_json = createDeviceStatusJson(device_id, uptime);
    
    std::cout << "Device status JSON: " << status_json << std::endl;
    
    // Verify JSON format
    assert(status_json.find("\"device_id\":\"test_device\"") != std::string::npos);
    assert(status_json.find("\"status\":\"online\"") != std::string::npos);
    assert(status_json.find("\"uptime\":12345") != std::string::npos);
    
    // Test publish timing logic
    uint64_t last_publish = 0;
    uint64_t current_time = 30;
    
    assert(!shouldPublishDeviceInfo(last_publish, current_time, 60)); // Should not publish yet
    assert(shouldPublishDeviceInfo(last_publish, current_time, 30));  // Should publish now
    assert(shouldPublishDeviceInfo(last_publish, current_time, 20));  // Should publish now
}

void testCurriedDeviceInfoPublisher() {
    std::cout << "\n=== Testing Curried Device Info Publisher ===" << std::endl;
    
    MockMqtt::clearMessages();
    MockMqtt::setConnected(true);
    
    // Create curried publisher
    auto publishDeviceInfo = createMqttDeviceInfoPublisher(
        [](const std::string& topic, const std::string& message) {
            MockMqtt::publish(topic, message);
        }
    );
    
    // Test publishing
    uint64_t uptime = 54321;
    publishDeviceInfo(uptime);
    
    auto messages = MockMqtt::getPublishedMessages();
    assert(messages.size() == 1);
    assert(messages[0].first == "device/status");
    assert(messages[0].second.find("\"uptime\":54321") != std::string::npos);
    
    std::cout << "Published message: " << messages[0].second << std::endl;
}

void testDeviceInfoPublisherWithClockEvent() {
    std::cout << "\n=== Testing Device Info Publisher with Clock Event ===" << std::endl;
    
    MockMqtt::clearMessages();
    MockMqtt::setConnected(true);
    
    // Create device info publisher
    auto devicePublisher = createDeviceInfoPublisher("clock_test_device");
    
    // Test with clock event data
    uint64_t uptime = 98765;
    devicePublisher(uptime, [](const std::string& topic, const std::string& message) {
        MockMqtt::publish(topic, message);
    });
    
    auto messages = MockMqtt::getPublishedMessages();
    assert(messages.size() == 1);
    assert(messages[0].first == "device/status");
    assert(messages[0].second.find("clock_test_device") != std::string::npos);
    assert(messages[0].second.find("\"uptime\":98765") != std::string::npos);
    
    std::cout << "Clock event message: " << messages[0].second << std::endl;
}

void testEventDrivenMainLoop() {
    std::cout << "\n=== Testing Event-Driven Main Loop ===" << std::endl;
    
    MockEventBus bus;
    MockMqtt::clearMessages();
    
    // Subscribe to timer events
    bus.subscribe(handleMainLoopEvent, nullptr, 1u << 2); // TOPIC_TIMER
    
    // Test with MQTT disconnected
    MockMqtt::setConnected(false);
    MockEvent timer_event_disconnected(2, 1000); // TOPIC_TIMER
    bus.publish(timer_event_disconnected);
    
    auto messages_disconnected = MockMqtt::getPublishedMessages();
    assert(messages_disconnected.empty()); // Should not publish when disconnected
    
    // Test with MQTT connected
    MockMqtt::setConnected(true);
    MockEvent timer_event_connected(2, 2000); // TOPIC_TIMER
    bus.publish(timer_event_connected);
    
    auto messages_connected = MockMqtt::getPublishedMessages();
    assert(messages_connected.size() == 1);
    assert(messages_connected[0].first == "device/status");
    assert(messages_connected[0].second.find("\"uptime\":2000") != std::string::npos);
    
    std::cout << "Event-driven message: " << messages_connected[0].second << std::endl;
}

void testClockHandlerWithInterval() {
    std::cout << "\n=== Testing Clock Handler with Interval ===" << std::endl;
    
    MockEventBus bus;
    MockMqtt::clearMessages();
    MockMqtt::setConnected(true);
    
    // Subscribe to timer events with clock handler
    bus.subscribe(handleClockEvent, nullptr, 1u << 2); // TOPIC_TIMER
    
    // Test clock events with 3-second interval
    for (int i = 0; i < 10; i++) {
        MockEvent clock_event(2, i); // TOPIC_TIMER with time i
        bus.publish(clock_event);
    }
    
    auto messages = MockMqtt::getPublishedMessages();
    std::cout << "Total messages published: " << messages.size() << std::endl;
    
    // Should publish every 3 seconds (at times 0, 3, 6, 9)
    assert(messages.size() >= 3);
    
    for (const auto& [topic, message] : messages) {
        std::cout << "Topic: " << topic << ", Message: " << message << std::endl;
    }
}

void testMainLoopSimulation() {
    std::cout << "\n=== Testing Main Loop Simulation ===" << std::endl;
    
    MockEventBus bus;
    MockMqtt::clearMessages();
    MockMqtt::setConnected(true);
    
    // Subscribe to timer events
    bus.subscribe(handleMainLoopEvent, nullptr, 1u << 2); // TOPIC_TIMER
    
    // Create curried device info publisher
    auto publishDeviceInfo = createMqttDeviceInfoPublisher(
        [](const std::string& topic, const std::string& message) {
            MockMqtt::publish(topic, message);
        }
    );
    
    uint64_t last_publish_time = 0;
    
    // Simulate main loop iterations
    for (int i = 0; i < 5; i++) {
        uint64_t current_time = getMockUptime();
        
        // Publish main loop event
        MockEvent main_loop_event(2, static_cast<int32_t>(current_time)); // TOPIC_TIMER
        bus.publish(main_loop_event);
        
        // Check if we should publish device info (every 3 seconds for testing)
        if (shouldPublishDeviceInfo(last_publish_time, current_time, 3)) {
            publishDeviceInfo(current_time);
            last_publish_time = current_time;
        }
    }
    
    auto messages = MockMqtt::getPublishedMessages();
    std::cout << "Total messages published: " << messages.size() << std::endl;
    
    // Should have published at least once (every 3 seconds)
    assert(messages.size() >= 1);
    
    for (const auto& [topic, message] : messages) {
        std::cout << "Topic: " << topic << ", Message: " << message << std::endl;
    }
}

int main() {
    std::cout << "Testing Event-Driven Main Loop and Device Info Publishing" << std::endl;
    
    testPureDeviceInfoFunctions();
    testCurriedDeviceInfoPublisher();
    testDeviceInfoPublisherWithClockEvent();
    testEventDrivenMainLoop();
    testClockHandlerWithInterval();
    testMainLoopSimulation();
    
    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
