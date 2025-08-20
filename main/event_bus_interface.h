#ifndef EVENT_BUS_INTERFACE_H
#define EVENT_BUS_INTERFACE_H

#include "event_protocol.h"
#include <stdint.h>

// Forward declarations
struct Event;

// Event handler function type
typedef void (*EventHandler)(const Event& e, void* user_data);

// Event predicate function type
typedef bool (*EventPredicate)(const Event& e, void* user_data);

// Virtual Event Bus Interface
class IEventBus {
public:
    virtual ~IEventBus() = default;
    
    // Core event bus operations
    virtual bool initialize(const char* task_name = "event_dispatcher", 
                           uint32_t stack_size = 4096, 
                           uint32_t priority = 5) = 0;
    
    virtual int subscribe(EventHandler handler, 
                         void* user_data = nullptr,
                         uint32_t topic_mask = MASK_ALL,
                         EventPredicate predicate = nullptr,
                         void* predicate_data = nullptr) = 0;
    
    virtual void unsubscribe(int listener_id) = 0;
    
    virtual void publish(const Event& event) = 0;
    virtual void publishFromISR(const Event& event) = 0;
    
    // Convenience methods for specific event types
    virtual void publishMqtt(const char* topic, const char* message) = 0;
    virtual void publishWifi(bool connected, const char* ssid, const char* ip) = 0;
    virtual void publishMdns(bool discovered, const char* service, const char* host, int port) = 0;
    virtual void publishPin(int pin, int value, const char* action) = 0;
    virtual void publishSystem(const char* status, const char* component) = 0;
    virtual void publishError(const char* component, const char* message, int error_code) = 0;
    
    // Utility methods
    virtual bool isInitialized() const = 0;
    virtual int getListenerCount() const = 0;
};

// Event structure
struct Event {
    uint16_t type;       // topic / kind
    int32_t  i32;        // small payload slot
    void*    ptr;        // optional pointer to event data
    
    Event() : type(0), i32(0), ptr(nullptr) {}
    Event(uint16_t t, int32_t data, void* p = nullptr) : type(t), i32(data), ptr(p) {}
};

// Global event bus instance (will be set to concrete implementation)
extern IEventBus* g_eventBus;

// Global initialization function
void initEventBus();

// Global convenience functions
void publishMqttEvent(const char* topic, const char* message);
void publishWifiEvent(bool connected, const char* ssid, const char* ip);
void publishMdnsEvent(bool discovered, const char* service, const char* host, int port);
void publishPinEvent(int pin, int value, const char* action);
void publishSystemEvent(const char* status, const char* component);
void publishErrorEvent(const char* component, const char* message, int error_code);

#endif // EVENT_BUS_INTERFACE_H
