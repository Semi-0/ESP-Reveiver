#include "simple_event_bus.h"

static const char* TAG = "SimpleEventBus";

bool SimpleEventBus::begin(const char* name, size_t stack, int priority) {
    if (running) {
        ESP_LOGW(TAG, "Event bus already running");
        return false;
    }
    
    running = true;
    ESP_LOGI(TAG, "Simple event bus started: %s", name);
    return true;
}

void SimpleEventBus::stop() {
    if (!running) {
        return;
    }
    
    running = false;
    
    // Clear subscriptions
    subscriptions.clear();
    
    ESP_LOGI(TAG, "Simple event bus stopped");
}

void SimpleEventBus::subscribe(std::function<void(const SimpleEvent&, void*)> handler, void* user_data, uint64_t topic_mask) {
    if (!running) {
        ESP_LOGW(TAG, "Cannot subscribe to stopped event bus");
        return;
    }
    
    Subscription sub;
    sub.handler = handler;
    sub.user_data = user_data;
    sub.topic_mask = topic_mask;
    
    subscriptions.push_back(sub);
    ESP_LOGD(TAG, "Added subscription for topic mask: 0x%llx", (unsigned long long)topic_mask);
}

void SimpleEventBus::publish(const SimpleEvent& event) {
    if (!running) {
        ESP_LOGW(TAG, "Cannot publish to stopped event bus");
        return;
    }
    
    ESP_LOGD(TAG, "Publishing event - topic: %d, i32: %d", event.topic, event.i32);
    
    // Process all subscriptions for this event (synchronous)
    for (const auto& sub : subscriptions) {
        if (sub.topic_mask & bit(event.topic)) {
            sub.handler(event, sub.user_data);
        }
    }
}
