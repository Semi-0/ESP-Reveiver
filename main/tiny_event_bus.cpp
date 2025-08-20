#include "tiny_event_bus.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "TinyEventBus";

// Static buffer for string storage
char TinyEventBus::string_buffer_[MAX_STRING_LENGTH];
size_t TinyEventBus::string_buffer_pos_ = 0;

TinyEventBus::~TinyEventBus() {
    cleanup();
}

bool TinyEventBus::initialize(const char* task_name, uint32_t stack_size, uint32_t priority) {
    if (initialized_) {
        ESP_LOGW(TAG, "Event bus already initialized");
        return true;
    }
    
    // Create dispatch queue
    dispatch_queue_ = xQueueCreate(DISPATCH_QUEUE_LEN, sizeof(Event));
    if (!dispatch_queue_) {
        ESP_LOGE(TAG, "Failed to create dispatch queue");
        return false;
    }
    
    // Create dispatch task
    BaseType_t result = xTaskCreate(&TinyEventBus::dispatchTaskTrampoline, 
                                   task_name, 
                                   stack_size, 
                                   this, 
                                   priority, 
                                   &dispatch_task_handle_);
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create dispatch task");
        cleanup();
        return false;
    }
    
    // Initialize listeners
    for (int i = 0; i < MAX_LISTENERS; i++) {
        listeners_[i] = Listener{};
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "TinyEventBus initialized successfully");
    return true;
}

int TinyEventBus::subscribe(EventHandler handler, void* user_data, uint32_t topic_mask, 
                           EventPredicate predicate, void* predicate_data) {
    if (!initialized_) {
        ESP_LOGE(TAG, "Event bus not initialized");
        return -1;
    }
    
    if (!handler) {
        ESP_LOGE(TAG, "Handler cannot be null");
        return -1;
    }
    
    for (int i = 0; i < MAX_LISTENERS; i++) {
        if (!listeners_[i].in_use) {
            listeners_[i] = Listener(handler, user_data, topic_mask, predicate, predicate_data);
            ESP_LOGI(TAG, "Added listener %d with mask 0x%08X", i, topic_mask);
            return i;
        }
    }
    
    ESP_LOGE(TAG, "No free listener slots");
    return -1;
}

void TinyEventBus::unsubscribe(int listener_id) {
    if (listener_id >= 0 && listener_id < MAX_LISTENERS && listeners_[listener_id].in_use) {
        listeners_[listener_id] = Listener{};
        ESP_LOGI(TAG, "Removed listener %d", listener_id);
    }
}

void TinyEventBus::publish(const Event& event) {
    if (!initialized_) {
        ESP_LOGW(TAG, "Event bus not initialized, dropping event");
        return;
    }
    
    // Direct fan-out to listeners
    uint32_t bit = (event.type < 32) ? (1u << event.type) : 0u;
    
    for (int i = 0; i < MAX_LISTENERS; i++) {
        const auto& listener = listeners_[i];
        if (!listener.in_use || !listener.handler) continue;
        
        // Check topic mask
        if (bit && !(listener.topic_mask & bit)) continue;
        
        // Check predicate
        if (listener.predicate && !listener.predicate(event, listener.predicate_data)) continue;
        
        // Call handler
        listener.handler(event, listener.user_data);
    }
}

void TinyEventBus::publishFromISR(const Event& event) {
    if (!initialized_ || !dispatch_queue_) return;
    xQueueSendFromISR(dispatch_queue_, &event, nullptr);
}

void TinyEventBus::publishMqtt(const char* topic, const char* message) {
    if (!topic || !message) return;
    
    Event event(TOPIC_MQTT, strlen(message));
    event.ptr = const_cast<char*>(message);
    
    // Store topic in buffer for handlers that need it
    const char* stored_topic = storeString(topic);
    
    publish(event);
}

void TinyEventBus::publishWifi(bool connected, const char* ssid, const char* ip) {
    Event event(TOPIC_WIFI, connected ? 1 : 0);
    
    // Store strings in buffer
    const char* stored_ssid = ssid ? storeString(ssid) : nullptr;
    const char* stored_ip = ip ? storeString(ip) : nullptr;
    
    publish(event);
}

void TinyEventBus::publishMdns(bool discovered, const char* service, const char* host, int port) {
    Event event(TOPIC_MDNS, discovered ? 1 : 0);
    
    // Store strings in buffer
    const char* stored_service = service ? storeString(service) : nullptr;
    const char* stored_host = host ? storeString(host) : nullptr;
    
    publish(event);
}

void TinyEventBus::publishPin(int pin, int value, const char* action) {
    Event event(TOPIC_PIN, (pin << 16) | (value & 0xFFFF));
    
    // Store action string in buffer
    const char* stored_action = action ? storeString(action) : nullptr;
    
    publish(event);
}

void TinyEventBus::publishSystem(const char* status, const char* component) {
    Event event(TOPIC_SYSTEM, strlen(status));
    event.ptr = const_cast<char*>(status);
    
    // Store component string in buffer
    const char* stored_component = component ? storeString(component) : nullptr;
    
    publish(event);
}

void TinyEventBus::publishError(const char* component, const char* message, int error_code) {
    Event event(TOPIC_ERROR, error_code);
    event.ptr = const_cast<char*>(message);
    
    // Store component string in buffer
    const char* stored_component = component ? storeString(component) : nullptr;
    
    publish(event);
}

int TinyEventBus::getListenerCount() const {
    int count = 0;
    for (int i = 0; i < MAX_LISTENERS; i++) {
        if (listeners_[i].in_use) count++;
    }
    return count;
}

void TinyEventBus::dispatchTaskTrampoline(void* arg) {
    static_cast<TinyEventBus*>(arg)->dispatchTask();
}

void TinyEventBus::dispatchTask() {
    ESP_LOGI(TAG, "Dispatch task started");
    
    for (;;) {
        Event event;
        if (xQueueReceive(dispatch_queue_, &event, portMAX_DELAY) == pdTRUE) {
            publish(event); // Reuse task-context publish for fan-out
        }
    }
}

void TinyEventBus::cleanup() {
    if (dispatch_task_handle_) {
        vTaskDelete(dispatch_task_handle_);
        dispatch_task_handle_ = nullptr;
    }
    
    if (dispatch_queue_) {
        vQueueDelete(dispatch_queue_);
        dispatch_queue_ = nullptr;
    }
    
    initialized_ = false;
}

const char* TinyEventBus::storeString(const char* str) {
    if (!str) return nullptr;
    
    size_t len = strlen(str);
    if (len >= MAX_STRING_LENGTH) {
        ESP_LOGW(TAG, "String too long, truncating");
        len = MAX_STRING_LENGTH - 1;
    }
    
    // Simple circular buffer approach
    if (string_buffer_pos_ + len + 1 >= MAX_STRING_LENGTH) {
        string_buffer_pos_ = 0; // Reset to beginning
    }
    
    char* dest = &string_buffer_[string_buffer_pos_];
    strncpy(dest, str, len);
    dest[len] = '\0';
    
    string_buffer_pos_ += len + 1;
    
    return dest;
}
