#include "event_bus_interface.h"
#include "tiny_event_bus.h"
#include "esp_log.h"

static const char* TAG = "EventBusGlobal";

// Global event bus instance
static TinyEventBus* g_tinyEventBus = nullptr;
IEventBus* g_eventBus = nullptr;

void initEventBus() {
    if (g_eventBus) {
        ESP_LOGW(TAG, "Event bus already initialized");
        return;
    }
    
    // Create TinyEventBus instance
    g_tinyEventBus = new TinyEventBus();
    g_eventBus = g_tinyEventBus;
    
    // Initialize the event bus
    if (!g_eventBus->initialize("event_dispatcher", 4096, 5)) {
        ESP_LOGE(TAG, "Failed to initialize event bus");
        delete g_tinyEventBus;
        g_tinyEventBus = nullptr;
        g_eventBus = nullptr;
        return;
    }
    
    ESP_LOGI(TAG, "Global event bus initialized successfully");
}

// Global convenience functions
void publishMqttEvent(const char* topic, const char* message) {
    if (g_eventBus) {
        g_eventBus->publishMqtt(topic, message);
    } else {
        ESP_LOGW(TAG, "Event bus not initialized, dropping MQTT event");
    }
}

void publishWifiEvent(bool connected, const char* ssid, const char* ip) {
    if (g_eventBus) {
        g_eventBus->publishWifi(connected, ssid, ip);
    } else {
        ESP_LOGW(TAG, "Event bus not initialized, dropping WiFi event");
    }
}

void publishMdnsEvent(bool discovered, const char* service, const char* host, int port) {
    if (g_eventBus) {
        g_eventBus->publishMdns(discovered, service, host, port);
    } else {
        ESP_LOGW(TAG, "Event bus not initialized, dropping mDNS event");
    }
}

void publishPinEvent(int pin, int value, const char* action) {
    if (g_eventBus) {
        g_eventBus->publishPin(pin, value, action);
    } else {
        ESP_LOGW(TAG, "Event bus not initialized, dropping Pin event");
    }
}

void publishSystemEvent(const char* status, const char* component) {
    if (g_eventBus) {
        g_eventBus->publishSystem(status, component);
    } else {
        ESP_LOGW(TAG, "Event bus not initialized, dropping System event");
    }
}

void publishErrorEvent(const char* component, const char* message, int error_code) {
    if (g_eventBus) {
        g_eventBus->publishError(component, message, error_code);
    } else {
        ESP_LOGW(TAG, "Event bus not initialized, dropping Error event");
    }
}
