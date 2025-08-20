#pragma once

#include <functional>
#include <vector>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// Simple Event structure
struct SimpleEvent {
    int topic;
    int i32;
    void* ptr;
    
    SimpleEvent(int t, int i, void* p) : topic(t), i32(i), ptr(p) {}
};

// Event Protocol constants
#define TOPIC_WIFI_CONNECTED 1
#define TOPIC_MDNS_FOUND 2
#define TOPIC_MDNS_FAILED 3
#define TOPIC_MQTT_CONNECTED 4
#define TOPIC_MQTT_DISCONNECTED 5
#define TOPIC_MQTT_MESSAGE 6
#define TOPIC_SYSTEM_ERROR 7
#define TOPIC_TIMER 8

// Helper function to create bit masks
inline uint64_t bit(int topic) {
    return (1ULL << topic);
}

// Simple Event Bus for ESP32
class SimpleEventBus {
private:
    struct Subscription {
        std::function<void(const SimpleEvent&, void*)> handler;
        void* user_data;
        uint64_t topic_mask;
    };
    
    std::vector<Subscription> subscriptions;
    bool running;
    
public:
    SimpleEventBus() : running(false) {}
    ~SimpleEventBus() = default;
    
    bool begin(const char* name = "simple-eventbus", size_t stack = 4096, int priority = 5);
    void stop();
    
    void subscribe(std::function<void(const SimpleEvent&, void*)> handler, void* user_data, uint64_t topic_mask);
    void publish(const SimpleEvent& event);
    
    bool is_running() const { return running; }
};

// Simple Flow type
using SimpleFlow = std::function<void(const SimpleEvent&, SimpleEventBus&)>;

// Simple FlowGraph for ESP32
class SimpleFlowGraph {
private:
    SimpleEventBus& bus;
    
public:
    SimpleFlowGraph(SimpleEventBus& eventBus) : bus(eventBus) {}
    
    // Create a flow that publishes an event
    static SimpleFlow publish(int topic, int i32_value = 0, void* ptr = nullptr) {
        return [topic, i32_value, ptr](const SimpleEvent& trigger_event, SimpleEventBus& bus) {
            // If ptr is nullptr, use the trigger event's ptr
            void* event_ptr = (ptr != nullptr) ? ptr : trigger_event.ptr;
            SimpleEvent new_event(topic, i32_value, event_ptr);
            bus.publish(new_event);
        };
    }
    
    // Create a flow that logs an event
    static SimpleFlow tap(std::function<void(const SimpleEvent&)> logger) {
        return [logger](const SimpleEvent& event, SimpleEventBus& bus) {
            logger(event);
        };
    }
    
    // Register a flow to trigger on specific events
    void when(int topic, SimpleFlow flow) {
        bus.subscribe([flow](const SimpleEvent& event, void* user_data) {
            SimpleEventBus* bus_ptr = static_cast<SimpleEventBus*>(user_data);
            flow(event, *bus_ptr);
        }, &bus, bit(topic));
    }
};
