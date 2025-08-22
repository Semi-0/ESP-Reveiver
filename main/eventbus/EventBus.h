#pragma once
#include <stdint.h>
#include "freertos/FreeRTOS.h"

// Event cleanup function type
using EventCleanupFn = void(*)(void*);

// Enhanced Event structure with cleanup support
struct Event {
    uint16_t type;           // Topic ID: 0..31 use mask fast-path
    int32_t  i32;            // Small scalar (e.g., pin, code)
    void*    ptr;            // Optional payload pointer
    EventCleanupFn cleanup;  // Cleanup function for payload (nullptr if no cleanup needed)
    bool auto_cleanup;       // Whether to auto-cleanup (can be disabled for manual cleanup)
    
    Event() : type(0), i32(0), ptr(nullptr), cleanup(nullptr), auto_cleanup(true) {}
    Event(uint16_t t, int32_t data, void* p = nullptr, EventCleanupFn c = nullptr, bool auto_clean = true) 
        : type(t), i32(data), ptr(p), cleanup(c), auto_cleanup(auto_clean) {}
    
    // RAII cleanup (only if auto_cleanup is enabled)
    ~Event() {
        if (auto_cleanup && cleanup && ptr) {
            cleanup(ptr);
        }
    }
    
    // Copy constructor - transfers ownership
    Event(const Event& other) : type(other.type), i32(other.i32), ptr(other.ptr), cleanup(other.cleanup), auto_cleanup(other.auto_cleanup) {
        // Clear the source to prevent double cleanup
        const_cast<Event&>(other).ptr = nullptr;
        const_cast<Event&>(other).cleanup = nullptr;
    }
    
    // Assignment operator - transfers ownership
    Event& operator=(const Event& other) {
        if (this != &other) {
            // Clean up existing payload if auto-cleanup is enabled
            if (auto_cleanup && cleanup && ptr) {
                cleanup(ptr);
            }
            
            type = other.type;
            i32 = other.i32;
            ptr = other.ptr;
            cleanup = other.cleanup;
            auto_cleanup = other.auto_cleanup;
            
            // Clear the source to prevent double cleanup
            const_cast<Event&>(other).ptr = nullptr;
            const_cast<Event&>(other).cleanup = nullptr;
        }
        return *this;
    }
    
    // Method to disable automatic cleanup (for manual cleanup)
    void disable_auto_cleanup() {
        auto_cleanup = false;
    }
    
    // Method to enable automatic cleanup
    void enable_auto_cleanup() {
        auto_cleanup = true;
    }
    
    // Method to manually trigger cleanup
    void manual_cleanup() {
        if (cleanup && ptr) {
            cleanup(ptr);
            ptr = nullptr; // Prevent double cleanup
        }
    }
};

using EventHandler = void(*)(const Event&, void*);
using EventPred    = bool(*)(const Event&, void*);
using ListenerHandle = int;
using TopicMask      = uint32_t;

class IEventBus {
public:
    virtual ~IEventBus() {}
    virtual bool begin(const char* taskName="evt-dispatch",
                       uint32_t stack=2048,
                       UBaseType_t prio=tskIDLE_PRIORITY+1) = 0;

    virtual ListenerHandle subscribe(EventHandler handler,
                                     void* user = nullptr,
                                     TopicMask mask = 0xFFFFFFFFu,
                                     EventPred pred = nullptr,
                                     void* predUser = nullptr) = 0;

    virtual void unsubscribe(ListenerHandle h) = 0;

    // Task-context publish (direct fan-out)
    virtual void publish(const Event& e) = 0;

    // ISR-safe publish (queued to dispatcher task)
    virtual void publishFromISR(const Event& e, BaseType_t* hpw = nullptr) = 0;
};

