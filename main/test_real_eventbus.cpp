#include "eventbus/TinyEventBus.h"
#include "eventbus/EventProtocol.h"
#include "eventbus/FlowGraph.h"
#include "esp_log.h"
#include <string>

static const char* TAG = "REAL_EVENTBUS_TEST";

// Test the real event bus system
void test_real_eventbus() {
    ESP_LOGI(TAG, "Testing real event bus system...");
    
    // Create event bus
    TinyEventBus bus;
    if (!bus.begin("test-bus", 4096, 5)) {
        ESP_LOGE(TAG, "Failed to start event bus");
        return;
    }
    
    // Create flow graph
    FlowGraph G(bus);
    
    // Test basic event handler
    bus.subscribe([](const Event& e, void* user) {
        ESP_LOGI(TAG, "Received WiFi connected event");
    }, nullptr, bit(TOPIC_WIFI_CONNECTED));
    
    // Test flow composition
    G.when(TOPIC_WIFI_CONNECTED, FlowGraph::publish(TOPIC_MQTT_CONNECTED));
    
    // Test async flow
    G.when(TOPIC_WIFI_CONNECTED, 
        G.async_blocking("test-worker", 
            [](void** out) -> bool {
                ESP_LOGI(TAG, "Async worker executing...");
                *out = nullptr;
                return true;
            },
            FlowGraph::publish(TOPIC_MDNS_FOUND),
            FlowGraph::publish(TOPIC_MDNS_FAILED)
        )
    );
    
    // Publish test event
    bus.publish(Event{TOPIC_WIFI_CONNECTED, 0, nullptr});
    
    ESP_LOGI(TAG, "Real event bus test completed");
}
