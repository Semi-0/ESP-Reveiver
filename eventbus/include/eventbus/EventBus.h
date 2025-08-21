#pragma once
#include <stdint.h>
#include "freertos/FreeRTOS.h"

struct Event {
    uint16_t type;   // Topic ID: 0..31 use mask fast-path
    int32_t  i32;    // Small scalar (e.g., pin, code)
    void*    ptr;    // Optional payload pointer (publisher-managed lifetime)
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


