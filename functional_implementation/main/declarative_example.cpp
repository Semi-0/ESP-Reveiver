#include "declarative_events.h"
#include "event_protocol.h"
#include <iostream>

// Example demonstrating the declarative event system with composable operators
// This shows how to create complex event flows using the when/do/if_succeeded/if_failed operators

void setupDeclarativeEventSystem() {
    std::cout << "Setting up Declarative Event System with Composable Operators" << std::endl;
    std::cout << "=============================================================" << std::endl;
    
    // Example 1: when(wifi_connected, do(query_mdns, if_succeeded => publish(mqtt_connect), if_failed => publish(fallback)))
    auto wifi_to_mqtt_chain = std::make_shared<EventChain>();
    wifi_to_mqtt_chain->do_action(EventActions::log("WiFi", [](const Event& event, void*) {
        return "WiFi connected, starting mDNS discovery";
    }))
    .do_action(EventActions::query("mdns", [](const Event&, void*) {
        // Simulate mDNS discovery
        return EventResult::success_result("MQTT broker found at 192.168.1.100:1883");
    }))
    .if_succeeded([](const EventResult& result) {
        std::cout << "  ✓ mDNS succeeded: " << result.message << std::endl;
        std::cout << "  → Publishing MQTT connect event" << std::endl;
    })
    .if_failed([](const EventResult& result) {
        std::cout << "  ✗ mDNS failed: " << result.message << std::endl;
        std::cout << "  → Publishing fallback broker event" << std::endl;
    });
    
    g_declarativeEvents.when(MASK_WIFI, wifi_to_mqtt_chain);
    
    // Example 2: when(wifi_connected, log_event) - Multiple chains for same event
    auto wifi_log_chain = std::make_shared<EventChain>();
    wifi_log_chain->do_action(EventActions::log("System", [](const Event& event, void*) {
        return "WiFi connected with IP: " + std::to_string(event.i32);
    }));
    
    g_declarativeEvents.when(MASK_WIFI, wifi_log_chain);
    
    // Example 3: when(mdns_success, do(mqtt_connect, if_succeeded => publish(device_status), if_failed => retry))
    auto mdns_to_mqtt_chain = std::make_shared<EventChain>();
    mdns_to_mqtt_chain->do_action(EventActions::log("mDNS", [](const Event&, void*) {
        return "mDNS discovery successful, connecting to MQTT broker";
    }))
    .do_action(EventActions::query("mqtt_connect", [](const Event&, void*) {
        // Simulate MQTT connection
        return EventResult::success_result("Connected to MQTT broker");
    }))
    .if_succeeded([](const EventResult& result) {
        std::cout << "  ✓ MQTT connected: " << result.message << std::endl;
        std::cout << "  → Publishing device status" << std::endl;
    })
    .if_failed([](const EventResult& result) {
        std::cout << "  ✗ MQTT failed: " << result.message << std::endl;
        std::cout << "  → Retrying connection..." << std::endl;
    });
    
    g_declarativeEvents.when(MASK_MDNS, mdns_to_mqtt_chain);
    
    // Example 4: when(mqtt_connected, do(publish_device_info, start_heartbeat))
    auto mqtt_connected_chain = std::make_shared<EventChain>();
    mqtt_connected_chain->do_action(EventActions::log("MQTT", [](const Event&, void*) {
        return "MQTT connected, initializing device";
    }))
    .do_action(EventActions::publish("device/status", [](const Event&, void*) {
        return "{\"status\":\"online\",\"device_id\":\"esp32_001\"}";
    }))
    .do_action(EventActions::publish("device/heartbeat", [](const Event&, void*) {
        return "{\"uptime\":0,\"free_heap\":123456}";
    }));
    
    g_declarativeEvents.when(MASK_MQTT, mqtt_connected_chain);
    
    // Example 5: Complex conditional flow with multiple conditions
    auto complex_flow_chain = std::make_shared<EventChain>();
    complex_flow_chain->do_action(EventActions::query("system_check", [](const Event& event, void*) {
        // Simulate system health check
        if (event.i32 > 1000) {
            return EventResult::success_result("System healthy");
        } else {
            return EventResult::failure_result("System needs attention");
        }
    }))
    .if_succeeded([](const EventResult& result) {
        std::cout << "  ✓ System check passed: " << result.message << std::endl;
        std::cout << "  → Starting normal operation" << std::endl;
    })
    .if_failed([](const EventResult& result) {
        std::cout << "  ✗ System check failed: " << result.message << std::endl;
        std::cout << "  → Entering maintenance mode" << std::endl;
    });
    
    g_declarativeEvents.when(MASK_SYSTEM, complex_flow_chain);
    
    std::cout << "Declarative event system configured with " << g_declarativeEvents.get_chains().size() << " event chains" << std::endl;
    std::cout << std::endl;
}

void simulateEventFlow() {
    std::cout << "Simulating Event Flow" << std::endl;
    std::cout << "====================" << std::endl;
    
    // Simulate WiFi connection
    std::cout << "1. WiFi Connected Event:" << std::endl;
    Event wifi_event(TOPIC_WIFI, 192168001);
    g_declarativeEvents.handle_event(wifi_event);
    std::cout << std::endl;
    
    // Simulate mDNS success
    std::cout << "2. mDNS Success Event:" << std::endl;
    Event mdns_event(TOPIC_MDNS, 1);
    g_declarativeEvents.handle_event(mdns_event);
    std::cout << std::endl;
    
    // Simulate MQTT connection
    std::cout << "3. MQTT Connected Event:" << std::endl;
    Event mqtt_event(TOPIC_MQTT, 1);
    g_declarativeEvents.handle_event(mqtt_event);
    std::cout << std::endl;
    
    // Simulate system event
    std::cout << "4. System Event (Healthy):" << std::endl;
    Event system_healthy_event(TOPIC_SYSTEM, 2000);
    g_declarativeEvents.handle_event(system_healthy_event);
    std::cout << std::endl;
    
    // Simulate system event (unhealthy)
    std::cout << "5. System Event (Unhealthy):" << std::endl;
    Event system_unhealthy_event(TOPIC_SYSTEM, 500);
    g_declarativeEvents.handle_event(system_unhealthy_event);
    std::cout << std::endl;
}

void demonstrateComposableOperators() {
    std::cout << "Demonstrating Composable Operators" << std::endl;
    std::cout << "=================================" << std::endl;
    
    // Show how operators can be composed
    std::cout << "Operator Composition Examples:" << std::endl;
    std::cout << std::endl;
    
    // Example 1: when(wifi_connected, do(query_mdns, if_succeeded => publish(mqtt_connect), if_failed => publish(fallback)))
    std::cout << "1. when(wifi_connected, do(query_mdns, if_succeeded => publish(mqtt_connect), if_failed => publish(fallback)))" << std::endl;
    std::cout << "   - Declares what to do when WiFi connects" << std::endl;
    std::cout << "   - Queries mDNS for MQTT broker" << std::endl;
    std::cout << "   - If successful, publishes MQTT connect event" << std::endl;
    std::cout << "   - If failed, publishes fallback broker event" << std::endl;
    std::cout << std::endl;
    
    // Example 2: when(wifi_connected, log_event) - Multiple chains for same event
    std::cout << "2. when(wifi_connected, log_event)" << std::endl;
    std::cout << "   - Multiple chains can be registered for the same event" << std::endl;
    std::cout << "   - This allows for separation of concerns" << std::endl;
    std::cout << "   - Each chain executes independently" << std::endl;
    std::cout << std::endl;
    
    // Example 3: Complex conditional flow
    std::cout << "3. Complex conditional flow with multiple conditions" << std::endl;
    std::cout << "   - when(system_event, do(system_check, if_succeeded => normal_operation, if_failed => maintenance_mode))" << std::endl;
    std::cout << "   - Shows how conditions can control different execution paths" << std::endl;
    std::cout << std::endl;
    
    // Example 4: Chained actions
    std::cout << "4. Chained actions" << std::endl;
    std::cout << "   - do(action1, action2, action3)" << std::endl;
    std::cout << "   - Multiple actions can be chained together" << std::endl;
    std::cout << "   - Each action executes in sequence" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Benefits of this approach:" << std::endl;
    std::cout << "- Declarative: Focus on what to do, not how to do it" << std::endl;
    std::cout << "- Composable: Operators can be combined in various ways" << std::endl;
    std::cout << "- Networked: Multiple chains can respond to the same event" << std::endl;
    std::cout << "- Maintainable: Clear separation of concerns" << std::endl;
    std::cout << "- Testable: Each chain can be tested independently" << std::endl;
    std::cout << std::endl;
}

int main() {
    std::cout << "Declarative Event System with Composable Operators" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;
    
    setupDeclarativeEventSystem();
    demonstrateComposableOperators();
    simulateEventFlow();
    
    std::cout << "Example completed successfully!" << std::endl;
    return 0;
}
