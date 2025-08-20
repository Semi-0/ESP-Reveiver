#include <iostream>
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <cassert>

// Mock event structure for testing
struct MockEvent {
    uint16_t type;
    int32_t i32;
    void* ptr;
    
    MockEvent() : type(0), i32(0), ptr(nullptr) {}
    MockEvent(uint16_t t, int32_t data, void* p = nullptr) : type(t), i32(data), ptr(p) {}
};

// Mock data structures
struct ServiceDiscoveryData {
    std::string service_name;
    std::string host;
    int port;
    bool valid;
    
    ServiceDiscoveryData() : port(0), valid(false) {}
    ServiceDiscoveryData(const std::string& name, const std::string& h, int p) 
        : service_name(name), host(h), port(p), valid(true) {}
    
    static ServiceDiscoveryData invalid() {
        return ServiceDiscoveryData();
    }
};

struct MqttConnectionData {
    std::string broker_host;
    int broker_port;
    std::string client_id;
    
    MqttConnectionData() : broker_port(0) {}
    MqttConnectionData(const std::string& host, int port, const std::string& id)
        : broker_host(host), broker_port(port), client_id(id) {}
};

// Mock event handler type
using MockEventHandler = std::function<void(const MockEvent&, void*)>;

// Mock event bus for testing
class MockEventBus {
public:
    std::vector<std::pair<MockEventHandler, uint32_t>> handlers;
    
    int subscribe(MockEventHandler handler, void* user_data, uint32_t mask) {
        handlers.emplace_back(handler, mask);
        return handlers.size() - 1; // Return handler ID
    }
    
    void publish(const MockEvent& event) {
        for (auto& [handler, mask] : handlers) {
            if (mask & (1u << event.type)) {
                handler(event, nullptr);
            }
        }
    }
};

// ===== CURRIED EVENT HANDLER FACTORIES (TEST VERSION) =====

// Curried event handler factory - takes component name and returns event handler
auto createLoggingHandler(const std::string& component) {
    return [component](const MockEvent& event, void* user_data) {
        const char* status = static_cast<const char*>(event.ptr);
        std::cout << "System event: " << component << " - " << (status ? status : "unknown") << std::endl;
    };
}

// Curried error handler factory
auto createErrorHandler(const std::string& component) {
    return [component](const MockEvent& event, void* user_data) {
        const char* message = static_cast<const char*>(event.ptr);
        int error_code = event.i32;
        std::cout << "Error in " << component << ": " << (message ? message : "unknown error") 
                  << " (code: " << error_code << ")" << std::endl;
    };
}

// Curried WiFi logging handler
auto createWifiLoggingHandler() {
    return [](const MockEvent& event, void* user_data) {
        bool connected = (event.i32 != 0);
        std::cout << "WiFi " << (connected ? "connected" : "disconnected") << std::endl;
    };
}

// Curried mDNS logging handler
auto createMdnsLoggingHandler() {
    return [](const MockEvent& event, void* user_data) {
        bool discovered = (event.i32 != 0);
        std::cout << "mDNS discovery: " << (discovered ? "success" : "failed") << std::endl;
    };
}

// Curried MQTT logging handler
auto createMqttLoggingHandler() {
    return [](const MockEvent& event, void* user_data) {
        bool connected = (event.i32 != 0);
        std::cout << "MQTT " << (connected ? "connected" : "disconnected") << std::endl;
    };
}

// ===== CURRIED SUBSCRIPTION HELPER =====

// Curried subscription function that takes mask and returns a function that takes handler
auto subscribeTo(MockEventBus& bus, const uint32_t mask) {
    return [&bus, mask](MockEventHandler handler, void* user_data = nullptr) {
        return bus.subscribe(handler, user_data, mask);
    };
}

// ===== PURE FUNCTIONS FOR TESTING =====

// Pure function to create MQTT connection data from service discovery
MqttConnectionData createMqttConnectionFromService(const ServiceDiscoveryData& service, const std::string& client_id) {
    if (service.valid) {
        return MqttConnectionData(service.host, service.port, client_id);
    } else {
        // Return invalid connection data for invalid service
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

// ===== TEST FUNCTIONS =====

void testCurriedLoggingHandlers() {
    std::cout << "\n=== Testing Curried Logging Handlers ===" << std::endl;
    
    MockEventBus bus;
    
    // Create curried subscription functions
    auto subscribeToWifi = subscribeTo(bus, 1u << 3);  // MASK_WIFI
    auto subscribeToMdns = subscribeTo(bus, 1u << 4);  // MASK_MDNS
    auto subscribeToMqtt = subscribeTo(bus, 1u << 1);  // MASK_MQTT
    auto subscribeToSystem = subscribeTo(bus, 1u << 6); // MASK_SYSTEM
    auto subscribeToErrors = subscribeTo(bus, 1u << 7); // MASK_ERRORS
    
    // Register handlers using currying
    subscribeToWifi(createWifiLoggingHandler());
    subscribeToMdns(createMdnsLoggingHandler());
    subscribeToMqtt(createMqttLoggingHandler());
    subscribeToSystem(createLoggingHandler("system"));
    subscribeToErrors(createErrorHandler("error"));
    
    // Test events
    MockEvent wifi_event(3, 1); // WiFi connected
    MockEvent mdns_event(4, 0); // mDNS failed
    MockEvent mqtt_event(1, 1); // MQTT connected
    MockEvent system_event(6, 0, const_cast<char*>("start_discovery"));
    MockEvent error_event(7, 404, const_cast<char*>("connection_timeout"));
    
    std::cout << "Publishing events..." << std::endl;
    bus.publish(wifi_event);
    bus.publish(mdns_event);
    bus.publish(mqtt_event);
    bus.publish(system_event);
    bus.publish(error_event);
}

void testExplicitDataFlow() {
    std::cout << "\n=== Testing Explicit Data Flow ===" << std::endl;
    
    // Test service discovery data
    ServiceDiscoveryData valid_service("mqtt", "broker.local", 1883);
    ServiceDiscoveryData invalid_service = ServiceDiscoveryData::invalid();
    
    // Test pure functions
    auto mqtt_data_valid = createMqttConnectionFromService(valid_service, "test_client");
    auto mqtt_data_invalid = createMqttConnectionFromService(invalid_service, "test_client");
    
    std::cout << "Valid service -> MQTT data: " << mqtt_data_valid.broker_host 
              << ":" << mqtt_data_valid.broker_port << std::endl;
    std::cout << "Invalid service -> MQTT data: " << mqtt_data_invalid.broker_host 
              << ":" << mqtt_data_invalid.broker_port << std::endl;
    
    // Test validation
    assert(isValidServiceDiscovery(valid_service));
    assert(!isValidServiceDiscovery(invalid_service));
    
    // Test processing
    std::string result_valid = processMqttConnection(mqtt_data_valid);
    std::string result_invalid = processMqttConnection(mqtt_data_invalid);
    
    std::cout << "Processing valid data: " << result_valid << std::endl;
    std::cout << "Processing invalid data: " << result_invalid << std::endl;
    
    assert(result_valid.find("connecting_to_broker.local:1883") != std::string::npos);
    assert(result_invalid == "invalid_connection_data");
}

void testFunctionalComposition() {
    std::cout << "\n=== Testing Functional Composition ===" << std::endl;
    
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
    
    // Test with valid service
    ServiceDiscoveryData valid_service("mqtt", "broker.local", 1883);
    std::string result = pipeline(valid_service);
    std::cout << "Pipeline result: " << result << std::endl;
    
    assert(result.find("connecting_to_broker.local:1883") != std::string::npos);
    
    // Test with invalid service
    ServiceDiscoveryData invalid_service = ServiceDiscoveryData::invalid();
    std::string result_invalid = pipeline(invalid_service);
    std::cout << "Pipeline result (invalid): " << result_invalid << std::endl;
    
    assert(result_invalid == "invalid_service");
}

void testEventDrivenFlow() {
    std::cout << "\n=== Testing Event-Driven Flow ===" << std::endl;
    
    MockEventBus bus;
    
    // Create a simple event-driven pipeline
    auto handleServiceDiscovery = [&bus](const MockEvent& event, void* user_data) {
        // Simulate service discovery
        ServiceDiscoveryData service("mqtt", "discovered.broker.com", 1883);
        
        if (service.valid) {
            // Create MQTT connection data
            auto mqtt_data = createMqttConnectionFromService(service, "event_client");
            
            // Publish MQTT connection event
            MockEvent mqtt_event(1, 1, &mqtt_data); // MQTT topic
            bus.publish(mqtt_event);
        }
    };
    
    auto handleMqttConnection = [](const MockEvent& event, void* user_data) {
        auto* mqtt_data = static_cast<MqttConnectionData*>(event.ptr);
        if (mqtt_data) {
            std::string result = processMqttConnection(*mqtt_data);
            std::cout << "Event-driven MQTT connection: " << result << std::endl;
        }
    };
    
    // Subscribe handlers
    bus.subscribe(handleServiceDiscovery, nullptr, 1u << 4); // mDNS topic
    bus.subscribe(handleMqttConnection, nullptr, 1u << 1);   // MQTT topic
    
    // Trigger the flow
    MockEvent discovery_event(4, 1); // mDNS success
    std::cout << "Triggering service discovery..." << std::endl;
    bus.publish(discovery_event);
}

int main() {
    std::cout << "Testing Curried Event Handlers and Explicit Data Flow" << std::endl;
    
    testCurriedLoggingHandlers();
    testExplicitDataFlow();
    testFunctionalComposition();
    testEventDrivenFlow();
    
    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
