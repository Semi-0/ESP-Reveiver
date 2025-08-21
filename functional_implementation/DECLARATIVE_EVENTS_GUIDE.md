# Declarative Event System with Composable Operators

## Overview

The Declarative Event System provides a way to hide the imperative nature of pub/sub patterns and make them declarative. Instead of manually subscribing to events and writing imperative handlers, you can use composable operators to declare what should happen when events occur.

## Key Concepts

### 1. Declarative vs Imperative

**Imperative (Traditional Pub/Sub):**
```cpp
// Manual subscription and imperative handling
g_eventBus->subscribe(handleWifiEvent, nullptr, MASK_WIFI);
g_eventBus->subscribe(handleMdnsEvent, nullptr, MASK_MDNS);

void handleWifiEvent(const Event& event, void* user_data) {
    // Imperative logic here
    if (event.i32 > 0) {
        startMdnsDiscovery();
        logWiFiConnection();
    }
}
```

**Declarative (Our System):**
```cpp
// Declarative composition of event flows
g_declarativeEvents
    .when(MASK_WIFI, 
        do_action(query_mdns)
        .if_succeeded(publish_mqtt_connect)
        .if_failed(publish_fallback)
    )
    .when(MASK_WIFI, 
        do_action(log_wifi_event)
    );
```

### 2. Composable Operators

The system provides several composable operators:

- **`when(event_mask, chain)`** - Declares what to do when an event occurs
- **`do_action(action)`** - Executes an action
- **`if_succeeded(handler)`** - Handles successful results
- **`if_failed(handler)`** - Handles failed results

## Core Components

### EventResult
```cpp
struct EventResult {
    bool success;
    std::string message;
    void* data;
    
    static EventResult success_result(const std::string& msg = "", void* data = nullptr);
    static EventResult failure_result(const std::string& msg = "", void* data = nullptr);
};
```

### EventAction
Base class for all actions that can be executed in a chain:
```cpp
class EventAction {
public:
    virtual ~EventAction() = default;
    virtual EventResult execute(const Event& event, void* user_data) = 0;
    virtual std::string get_name() const = 0;
};
```

### EventChain
Manages a sequence of actions with conditional logic:
```cpp
class EventChain {
public:
    EventChain& do_action(std::shared_ptr<EventAction> action);
    EventChain& if_succeeded(std::function<void(const EventResult&)> handler);
    EventChain& if_failed(std::function<void(const EventResult&)> handler);
    void execute(const Event& event, void* user_data);
};
```

### DeclarativeEventSystem
Main system that manages event chains:
```cpp
class DeclarativeEventSystem {
public:
    DeclarativeEventSystem& when(uint32_t event_mask, std::shared_ptr<EventChain> chain);
    void handle_event(const Event& event);
};
```

## Built-in Actions

### PublishAction
Publishes messages to topics:
```cpp
auto publish_action = EventActions::publish("mqtt/connect", [](const Event&, void*) {
    return "{\"broker\":\"192.168.1.100\",\"port\":1883}";
});
```

### LogAction
Logs messages for components:
```cpp
auto log_action = EventActions::log("WiFi", [](const Event& event, void*) {
    return "WiFi connected with IP: " + std::to_string(event.i32);
});
```

### QueryAction
Performs queries and returns results:
```cpp
auto query_action = EventActions::query("mdns", [](const Event&, void*) {
    return EventResult::success_result("MQTT broker found");
});
```

## Usage Examples

### Example 1: Basic Event Flow
```cpp
// when(wifi_connected, do(query_mdns, if_succeeded => publish(mqtt_connect), if_failed => publish(fallback)))
auto wifi_chain = std::make_shared<EventChain>();
wifi_chain->do_action(EventActions::log("WiFi", [](const Event&, void*) {
    return "WiFi connected, starting mDNS discovery";
}))
.do_action(EventActions::query("mdns", [](const Event&, void*) {
    return EventResult::success_result("MQTT broker found");
}))
.if_succeeded([](const EventResult& result) {
    std::cout << "✓ mDNS succeeded: " << result.message << std::endl;
    std::cout << "→ Publishing MQTT connect event" << std::endl;
})
.if_failed([](const EventResult& result) {
    std::cout << "✗ mDNS failed: " << result.message << std::endl;
    std::cout << "→ Publishing fallback broker event" << std::endl;
});

g_declarativeEvents.when(MASK_WIFI, wifi_chain);
```

### Example 2: Multiple Chains for Same Event
```cpp
// when(wifi_connected, log_event) - Multiple chains for same event
auto wifi_log_chain = std::make_shared<EventChain>();
wifi_log_chain->do_action(EventActions::log("System", [](const Event& event, void*) {
    return "WiFi connected with IP: " + std::to_string(event.i32);
}));

g_declarativeEvents.when(MASK_WIFI, wifi_log_chain);
```

### Example 3: Complex Conditional Flow
```cpp
// when(system_event, do(system_check, if_succeeded => normal_operation, if_failed => maintenance_mode))
auto system_chain = std::make_shared<EventChain>();
system_chain->do_action(EventActions::query("system_check", [](const Event& event, void*) {
    if (event.i32 > 1000) {
        return EventResult::success_result("System healthy");
    } else {
        return EventResult::failure_result("System needs attention");
    }
}))
.if_succeeded([](const EventResult& result) {
    std::cout << "✓ System check passed: " << result.message << std::endl;
    std::cout << "→ Starting normal operation" << std::endl;
})
.if_failed([](const EventResult& result) {
    std::cout << "✗ System check failed: " << result.message << std::endl;
    std::cout << "→ Entering maintenance mode" << std::endl;
});

g_declarativeEvents.when(MASK_SYSTEM, system_chain);
```

### Example 4: Chained Actions
```cpp
// do(action1, action2, action3) - Multiple actions in sequence
auto mqtt_chain = std::make_shared<EventChain>();
mqtt_chain->do_action(EventActions::log("MQTT", [](const Event&, void*) {
    return "MQTT connected, initializing device";
}))
.do_action(EventActions::publish("device/status", [](const Event&, void*) {
    return "{\"status\":\"online\",\"device_id\":\"esp32_001\"}";
}))
.do_action(EventActions::publish("device/heartbeat", [](const Event&, void*) {
    return "{\"uptime\":0,\"free_heap\":123456}";
}));

g_declarativeEvents.when(MASK_MQTT, mqtt_chain);
```

## Benefits

### 1. Declarative Nature
- **Focus on what to do, not how to do it**
- Clear intent in the code
- Easier to understand the business logic

### 2. Composability
- **Operators can be combined in various ways**
- Reusable action components
- Flexible event flow composition

### 3. Networked Events
- **Multiple chains can respond to the same event**
- Separation of concerns
- Independent execution of different flows

### 4. Maintainability
- **Clear separation of concerns**
- Easy to modify individual flows
- Testable components

### 5. Testability
- **Each chain can be tested independently**
- Mock actions for testing
- Isolated event flow testing

## Comparison with Traditional Pub/Sub

| Aspect | Traditional Pub/Sub | Declarative Events |
|--------|-------------------|-------------------|
| **Syntax** | Imperative handlers | Declarative operators |
| **Composability** | Manual composition | Built-in composition |
| **Readability** | Scattered logic | Centralized flows |
| **Testing** | Hard to isolate | Easy to test |
| **Maintenance** | Tight coupling | Loose coupling |
| **Reusability** | Duplicate code | Reusable actions |

## Integration with Existing System

The declarative event system can be integrated with the existing functional implementation:

```cpp
// In main.cpp, replace imperative event handlers with declarative chains
void setupEventSystem() {
    // Instead of manual subscriptions
    // g_eventBus->subscribe(handleWifiEvent, nullptr, MASK_WIFI);
    
    // Use declarative chains
    auto wifi_chain = std::make_shared<EventChain>();
    wifi_chain->do_action(EventActions::log("WiFi", createWifiLoggingHandler()))
              .do_action(EventActions::query("mdns", createMdnsQueryHandler()))
              .if_succeeded(createMqttConnectionHandler())
              .if_failed(createFallbackHandler());
    
    g_declarativeEvents.when(MASK_WIFI, wifi_chain);
}

// In the main loop, handle events through the declarative system
void app_main() {
    setupEventSystem();
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Handle events through declarative system
        Event clock_event(TOPIC_TIMER, getCurrentUptimeSeconds());
        g_declarativeEvents.handle_event(clock_event);
    }
}
```

## Advanced Patterns

### 1. Custom Actions
You can create custom actions by inheriting from `EventAction`:
```cpp
class CustomAction : public EventAction {
private:
    std::string action_name;
    std::function<EventResult(const Event&, void*)> action_func;
    
public:
    CustomAction(const std::string& name, std::function<EventResult(const Event&, void*)> func)
        : action_name(name), action_func(func) {}
    
    EventResult execute(const Event& event, void* user_data) override {
        return action_func(event, user_data);
    }
    
    std::string get_name() const override {
        return "custom(" + action_name + ")";
    }
};
```

### 2. Conditional Actions
Actions can be conditional based on event data:
```cpp
auto conditional_action = EventActions::query("conditional", [](const Event& event, void*) {
    if (event.i32 > 1000) {
        return EventResult::success_result("High value event");
    } else {
        return EventResult::failure_result("Low value event");
    }
});
```

### 3. Action Composition
Actions can be composed into more complex operations:
```cpp
auto composed_action = std::make_shared<EventChain>();
composed_action->do_action(EventActions::log("Step1", [](const Event&, void*) { return "Step 1"; }))
               .do_action(EventActions::log("Step2", [](const Event&, void*) { return "Step 2"; }))
               .do_action(EventActions::log("Step3", [](const Event&, void*) { return "Step 3"; }));
```

## Conclusion

The Declarative Event System provides a powerful way to compose event-driven applications using functional programming principles. It transforms imperative pub/sub patterns into declarative, composable event flows that are easier to understand, test, and maintain.

The system maintains the networked nature of event systems while providing a clean, functional interface for defining event flows. This makes it ideal for complex embedded systems where multiple components need to respond to events in coordinated ways.


