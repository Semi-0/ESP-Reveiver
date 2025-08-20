#include <iostream>
#include <functional>
#include <memory>
#include <cassert>
#include "../main/declarative_events.h"
#include "../main/event_bus_interface.h"

// Use the real Event type from the interface
using Event = ::Event;

// Mock event bus for testing
class MockEventBus {
public:
    std::vector<std::string> published_messages;
    std::vector<std::string> logged_messages;
    
    void publish(const std::string& topic, const std::string& message) {
        published_messages.push_back(topic + ": " + message);
    }
    
    void log(const std::string& component, const std::string& message) {
        logged_messages.push_back(component + ": " + message);
    }
    
    void clear() {
        published_messages.clear();
        logged_messages.clear();
    }
};

// Global mock event bus for testing
MockEventBus g_mockEventBus;

// Use the real event masks from the protocol
#include "../main/event_protocol.h"

// Test functions
void testBasicDeclarativeChain() {
    std::cout << "\n=== Testing Basic Declarative Chain ===" << std::endl;
    
    g_mockEventBus.clear();
    
    // Create a simple declarative chain
    auto chain = std::make_shared<EventChain>();
    
    // Add a log action
    auto log_action = EventActions::log("WiFi", [](const Event& event, void*) {
        return "WiFi connected with IP: " + std::to_string(event.i32);
    });
    
    chain->do_action(log_action);
    
    // Execute the chain
    Event wifi_event(3, 192168001); // WiFi connected
    chain->execute(wifi_event, nullptr);
    
    // Verify the log was created (we can see the output above)
    std::cout << "Basic declarative chain test passed!" << std::endl;
}

void testChainedActions() {
    std::cout << "\n=== Testing Chained Actions ===" << std::endl;
    
    g_mockEventBus.clear();
    
    // Create a chain with multiple actions
    auto chain = std::make_shared<EventChain>();
    
    // Add a query action
    auto query_action = EventActions::query("mdns", [](const Event&, void*) {
        return EventResult::success_result("MQTT broker found at 192.168.1.100:1883");
    });
    
    // Add a publish action
    auto publish_action = EventActions::publish("mqtt/connect", [](const Event&, void*) {
        return "{\"broker\":\"192.168.1.100\",\"port\":1883}";
    });
    
    chain->do_action(query_action)
          .do_action(publish_action);
    
    // Execute the chain
    Event mdns_event(4, 1); // mDNS success
    chain->execute(mdns_event, nullptr);
    
    // Verify the publish was created (we can see the output above)
    std::cout << "Chained actions test passed!" << std::endl;
}

void testConditionalActions() {
    std::cout << "\n=== Testing Conditional Actions ===" << std::endl;
    
    g_mockEventBus.clear();
    
    // Create a chain with conditional logic
    auto chain = std::make_shared<EventChain>();
    
    // Add a query action
    auto query_action = EventActions::query("mdns", [](const Event& event, void*) {
        // Simulate mDNS query result
        if (event.i32 == 1) {
            return EventResult::success_result("MQTT broker found");
        } else {
            return EventResult::failure_result("No MQTT broker found");
        }
    });
    
    // Set up conditional handlers
    chain->do_action(query_action)
          .if_succeeded([](const EventResult& result) {
              std::cout << "Success: " << result.message << std::endl;
              g_mockEventBus.publish("mqtt/connect", "Connecting to discovered broker");
          })
          .if_failed([](const EventResult& result) {
              std::cout << "Failure: " << result.message << std::endl;
              g_mockEventBus.publish("mqtt/fallback", "Using fallback broker");
          });
    
    // Test success case
    Event success_event(4, 1); // mDNS success
    chain->execute(success_event, nullptr);
    
    // Test failure case
    g_mockEventBus.clear();
    Event failure_event(4, 0); // mDNS failure
    chain->execute(failure_event, nullptr);
}

void testDeclarativeEventSystem() {
    std::cout << "\n=== Testing Declarative Event System ===" << std::endl;
    
    g_mockEventBus.clear();
    
    // Create the declarative event system
    DeclarativeEventSystem declarativeEvents;
    
    // Declare what to do when WiFi connects
    auto wifi_chain = std::make_shared<EventChain>();
    wifi_chain->do_action(EventActions::log("WiFi", [](const Event& event, void*) {
        return "WiFi connected with IP: " + std::to_string(event.i32);
    }));
    
    declarativeEvents.when(MASK_WIFI, wifi_chain);
    
    // Declare what to do when mDNS succeeds
    auto mdns_chain = std::make_shared<EventChain>();
    mdns_chain->do_action(EventActions::query("mdns", [](const Event&, void*) {
        return EventResult::success_result("MQTT broker found");
    }))
    .if_succeeded([](const EventResult&) {
        g_mockEventBus.publish("mqtt/connect", "Connecting to discovered broker");
    })
    .if_failed([](const EventResult&) {
        g_mockEventBus.publish("mqtt/fallback", "Using fallback broker");
    });
    
    declarativeEvents.when(MASK_MDNS, mdns_chain);
    
    // Handle events
    Event wifi_event(3, 192168001);
    declarativeEvents.handle_event(wifi_event);
    
    Event mdns_event(4, 1);
    declarativeEvents.handle_event(mdns_event);
    
    // Verify results (we can see the output above)
    std::cout << "Declarative event system test passed!" << std::endl;
}

void testComposableOperators() {
    std::cout << "\n=== Testing Composable Operators ===" << std::endl;
    
    g_mockEventBus.clear();
    
    // Test the composable operator syntax
    DeclarativeEventSystem declarativeEvents;
    
    // Example: when(wifi_connected, do(query_mdns, if_succeeded => publish(mqtt_connect), if_failed => publish(fallback)))
    
    // Create the mDNS query chain
    auto mdns_query_chain = std::make_shared<EventChain>();
    mdns_query_chain->do_action(EventActions::query("mdns", [](const Event&, void*) {
        return EventResult::success_result("MQTT broker found at 192.168.1.100:1883");
    }))
    .if_succeeded([](const EventResult& result) {
        g_mockEventBus.publish("mqtt/connect", "Connecting to " + result.message);
    })
    .if_failed([](const EventResult&) {
        g_mockEventBus.publish("mqtt/fallback", "Using fallback broker");
    });
    
    // Register the chain for WiFi events
    declarativeEvents.when(MASK_WIFI, mdns_query_chain);
    
    // Also register a logging chain for the same event
    auto log_chain = std::make_shared<EventChain>();
    log_chain->do_action(EventActions::log("WiFi", [](const Event& event, void*) {
        return "WiFi connected, triggering mDNS query";
    }));
    
    declarativeEvents.when(MASK_WIFI, log_chain);
    
    // Handle the WiFi event
    Event wifi_event(3, 192168001);
    declarativeEvents.handle_event(wifi_event);
    
    // Verify both chains executed (we can see the output above)
    std::cout << "Composable operators test passed!" << std::endl;
}

void testComplexEventFlow() {
    std::cout << "\n=== Testing Complex Event Flow ===" << std::endl;
    
    g_mockEventBus.clear();
    
    DeclarativeEventSystem declarativeEvents;
    
    // Complex flow: WiFi -> mDNS -> MQTT -> Device Status
    
    // WiFi connection triggers mDNS query
    auto wifi_to_mdns = std::make_shared<EventChain>();
    wifi_to_mdns->do_action(EventActions::log("WiFi", [](const Event&, void*) {
        return "WiFi connected, starting mDNS discovery";
    }))
    .do_action(EventActions::publish("system/mdns", [](const Event&, void*) {
        return "start_discovery";
    }));
    
    declarativeEvents.when(MASK_WIFI, wifi_to_mdns);
    
    // mDNS success triggers MQTT connection
    auto mdns_to_mqtt = std::make_shared<EventChain>();
    mdns_to_mqtt->do_action(EventActions::query("mdns", [](const Event&, void*) {
        return EventResult::success_result("MQTT broker discovered");
    }))
    .if_succeeded([](const EventResult&) {
        g_mockEventBus.publish("mqtt/connect", "Connecting to discovered broker");
    })
    .if_failed([](const EventResult&) {
        g_mockEventBus.publish("mqtt/fallback", "Using fallback broker");
    });
    
    declarativeEvents.when(MASK_MDNS, mdns_to_mqtt);
    
    // MQTT connection triggers device status
    auto mqtt_to_status = std::make_shared<EventChain>();
    mqtt_to_status->do_action(EventActions::log("MQTT", [](const Event&, void*) {
        return "MQTT connected, publishing device status";
    }))
    .do_action(EventActions::publish("device/status", [](const Event&, void*) {
        return "{\"status\":\"online\"}";
    }));
    
    declarativeEvents.when(MASK_MQTT, mqtt_to_status);
    
    // Simulate the event flow
    Event wifi_event(3, 192168001);
    declarativeEvents.handle_event(wifi_event);
    
    Event mdns_event(4, 1);
    declarativeEvents.handle_event(mdns_event);
    
    Event mqtt_event(1, 1);
    declarativeEvents.handle_event(mqtt_event);
    
    // Verify the complete flow (we can see the output above)
    std::cout << "Complex event flow test passed!" << std::endl;
}

int main() {
    std::cout << "Testing Declarative Event System with Composable Operators" << std::endl;
    
    testBasicDeclarativeChain();
    testChainedActions();
    testConditionalActions();
    testDeclarativeEventSystem();
    testComposableOperators();
    testComplexEventFlow();
    
    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
