#ifndef TINY_EVENT_BUS_H
#define TINY_EVENT_BUS_H

#include "event_bus_interface.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <vector>

#ifndef MAX_LISTENERS
#define MAX_LISTENERS 16
#endif

#ifndef DISPATCH_QUEUE_LEN
#define DISPATCH_QUEUE_LEN 64
#endif

// Listener structure
struct Listener {
    EventHandler handler = nullptr;
    void* user_data = nullptr;
    uint32_t topic_mask = MASK_ALL;
    EventPredicate predicate = nullptr;
    void* predicate_data = nullptr;
    bool in_use = false;
    
    Listener() = default;
    Listener(EventHandler h, void* user, uint32_t mask, EventPredicate pred, void* pred_data)
        : handler(h), user_data(user), topic_mask(mask), predicate(pred), predicate_data(pred_data), in_use(true) {}
};

// TinyEventBus implementation
class TinyEventBus : public IEventBus {
public:
    TinyEventBus() = default;
    ~TinyEventBus() override;
    
    // IEventBus interface implementation
    bool initialize(const char* task_name = "event_dispatcher", 
                   uint32_t stack_size = 4096, 
                   uint32_t priority = 5) override;
    
    int subscribe(EventHandler handler, 
                 void* user_data = nullptr,
                 uint32_t topic_mask = MASK_ALL,
                 EventPredicate predicate = nullptr,
                 void* predicate_data = nullptr) override;
    
    void unsubscribe(int listener_id) override;
    
    void publish(const Event& event) override;
    void publishFromISR(const Event& event) override;
    
    // Convenience methods
    void publishMqtt(const char* topic, const char* message) override;
    void publishWifi(bool connected, const char* ssid, const char* ip) override;
    void publishMdns(bool discovered, const char* service, const char* host, int port) override;
    void publishPin(int pin, int value, const char* action) override;
    void publishSystem(const char* status, const char* component) override;
    void publishError(const char* component, const char* message, int error_code) override;
    
    // Utility methods
    bool isInitialized() const override { return initialized_; }
    int getListenerCount() const override;

private:
    static void dispatchTaskTrampoline(void* arg);
    void dispatchTask();
    
    bool initialized_ = false;
    QueueHandle_t dispatch_queue_ = nullptr;
    TaskHandle_t dispatch_task_handle_ = nullptr;
    Listener listeners_[MAX_LISTENERS];
    
    // Static buffers for event data (simplified approach)
    static constexpr size_t MAX_STRING_LENGTH = 256;
    static char string_buffer_[MAX_STRING_LENGTH];
    static size_t string_buffer_pos_;
    
    // Helper methods
    void cleanup();
    const char* storeString(const char* str);
};

#endif // TINY_EVENT_BUS_H
