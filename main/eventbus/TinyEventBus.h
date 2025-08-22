#pragma once
#include "eventbus/EventBus.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#ifndef EBUS_MAX_LISTENERS
#define EBUS_MAX_LISTENERS 8
#endif
#ifndef EBUS_DISPATCH_QUEUE_LEN
#define EBUS_DISPATCH_QUEUE_LEN 32
#endif

class TinyEventBus final : public IEventBus {
public:
    bool begin(const char* taskName="evt-dispatch", uint32_t stack=2048, UBaseType_t prio=tskIDLE_PRIORITY+1) override {
        q_ = xQueueCreate(EBUS_DISPATCH_QUEUE_LEN, sizeof(Event));
        if (!q_) return false;
        return xTaskCreate(&TinyEventBus::dispatchTramp, taskName, stack, this, prio, &task_) == pdPASS;
    }

    ListenerHandle subscribe(EventHandler handler,
                             void* user = nullptr,
                             TopicMask mask = 0xFFFFFFFFu,
                             EventPred pred = nullptr,
                             void* predUser = nullptr) override {
        for (int i=0;i<EBUS_MAX_LISTENERS;i++) {
            if (!ls_[i].inUse) {
                ls_[i] = {true, handler, user, mask, pred, predUser};
                return i;
            }
        }
        return -1;
    }

    void unsubscribe(ListenerHandle h) override {
        if (h>=0 && h<EBUS_MAX_LISTENERS) ls_[h] = {};
    }

    void publish(const Event& e) override {
        const uint32_t b = (e.type < 32) ? (1u << e.type) : 0u;
        for (int i=0;i<EBUS_MAX_LISTENERS;i++) {
            auto& n = ls_[i];
            if (!n.inUse || !n.h) continue;
            if (b && !(n.mask & b)) continue;
            if (n.pred && !n.pred(e, n.predUser)) continue;
            n.h(e, n.user);
        }
    }

    // Publish event to queue with drop oldest behavior (task context)
    void publishToQueue(const Event& e) {
        if (!q_) return;
        
        // Try to send the event normally first
        BaseType_t result = xQueueSend(q_, &e, 0); // Non-blocking
        
        // If queue is full, drop the oldest event and try again
        if (result == errQUEUE_FULL) {
            dropOldestEvent();
            // Try to send the new event again
            xQueueSend(q_, &e, 0);
        }
    }

    void publishFromISR(const Event& e, BaseType_t* hpw=nullptr) override {
        if (!q_) return;
        
        // Try to send the event normally first
        BaseType_t result = xQueueSendFromISR(q_, &e, hpw);
        
        // If queue is full, drop the oldest event and try again
        if (result == errQUEUE_FULL) {
            dropOldestEventFromISR();
            // Try to send the new event again
            xQueueSendFromISR(q_, &e, hpw);
        }
    }

private:
    struct Node {
        bool inUse=false;
        EventHandler h=nullptr;
        void* user=nullptr;
        TopicMask mask=0xFFFFFFFFu;
        EventPred pred=nullptr;
        void* predUser=nullptr;
    };

    static void dispatchTramp(void* arg) { static_cast<TinyEventBus*>(arg)->dispatch(); }
    void dispatch() {
        for (;;) {
            Event e;
            if (xQueueReceive(q_, &e, portMAX_DELAY) == pdTRUE) {
                publish(e);
                // Event destructor will handle cleanup automatically
            }
        }
    }

    // Drop the oldest event from the queue (ISR-safe) with proper cleanup
    void dropOldestEventFromISR() {
        if (!q_) return;
        
        Event oldest_event;
        // Remove the oldest event without blocking
        if (xQueueReceiveFromISR(q_, &oldest_event, nullptr) == pdTRUE) {
            // Event destructor will automatically clean up the payload
            // This is safe because the Event destructor handles cleanup
        }
    }

    // Drop the oldest event from the queue (task context) with proper cleanup
    void dropOldestEvent() {
        if (!q_) return;
        
        Event oldest_event;
        // Remove the oldest event without blocking
        if (xQueueReceive(q_, &oldest_event, 0) == pdTRUE) {
            // Event destructor will automatically clean up the payload
            // This is safe because the Event destructor handles cleanup
        }
    }

    // Get queue statistics for monitoring
    struct QueueStats {
        UBaseType_t messages_waiting;
        UBaseType_t spaces_available;
        UBaseType_t total_spaces;
    };
    
    QueueStats getQueueStats() const {
        QueueStats stats = {0, 0, 0};
        if (q_) {
            stats.messages_waiting = uxQueueMessagesWaiting(q_);
            stats.spaces_available = uxQueueSpacesAvailable(q_);
            stats.total_spaces = EBUS_DISPATCH_QUEUE_LEN;
        }
        return stats;
    }

    QueueHandle_t q_ = nullptr;
    TaskHandle_t  task_ = nullptr;
    Node ls_[EBUS_MAX_LISTENERS];
};

