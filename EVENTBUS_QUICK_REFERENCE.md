# EventBus Quick Reference

## Event Structure
```cpp
struct Event {
    uint16_t type = 0;   // Topic ID: 0..31 use mask fast-path
    int32_t  i32  = 0;   // Small scalar (e.g., pin, code)
    union { void* ptr = nullptr; uint64_t u64; };
    void   (*dtor)(void*) = nullptr; // if set, bus calls dtor(ptr) after dispatch
};
```

## Publishing Events

### Simple Events (No Memory Management)
```cpp
BUS.publish(Event{TOPIC_TIMER, 100, nullptr});
```

### Events with Heap Data (Requires Destructor)
```cpp
// String data
char* hostname = strdup("broker.local");
BUS.publish(Event{TOPIC_MDNS_FOUND, 0, hostname, free});

// Object data
auto* data = new PinCommandData{pin, value, description};
BUS.publish(Event{TOPIC_PIN_SET, pin, data, 
    [](void* ptr) { delete static_cast<PinCommandData*>(ptr); }});
```

## Subscribing to Events

### Subscribe to All Events
```cpp
auto handle = BUS.subscribe(my_handler, nullptr, MASK_ALL);
```

### Subscribe to Specific Topics
```cpp
auto handle = BUS.subscribe(my_handler, nullptr, bit(TOPIC_TIMER) | bit(TOPIC_WIFI_CONNECTED));
```

### Subscribe with Predicate
```cpp
auto handle = BUS.subscribe(my_handler, nullptr, MASK_ALL, 
    [](const Event& e, void* user) { return e.i32 > 5; }, nullptr);
```

## ISR-Safe Publishing
```cpp
// In ISR context
BaseType_t hpw = pdFALSE;
BUS.publishFromISR(Event{TOPIC_TIMER, 100, nullptr}, &hpw);
if (hpw == pdTRUE) {
    portYIELD_FROM_ISR();
}
```

## Mailbox for Latest-Only Topics
```cpp
TinyMailbox mailbox;

// Publish (overwrites previous)
mailbox.publish(Event{TOPIC_SETPOINT, 100, nullptr});

// Receive latest
Event latest;
if (mailbox.receive(latest)) {
    // Process latest event
}
```

## Memory Management Rules

### ✅ DO
- Always set destructor for heap-allocated data
- Use `free` for `strdup`/`malloc` data
- Use lambda destructors for `new` objects
- Let the bus handle cleanup

### ❌ DON'T
- Don't manually delete event data in handlers
- Don't store integers in `void*` (use `u64` union)
- Don't publish events that republish the same event
- Don't use `%p` for integer values in logs

## Topic Definitions
```cpp
enum : uint16_t {
    TOPIC_WIFI_CONNECTED = 0,
    TOPIC_WIFI_DISCONNECTED = 1,
    TOPIC_MDNS_FOUND = 2,
    TOPIC_MDNS_FAILED = 3,
    TOPIC_MQTT_CONNECTED = 4,
    TOPIC_MQTT_DISCONNECTED = 5,
    TOPIC_MQTT_MESSAGE = 6,
    TOPIC_PIN_SET = 7,
    TOPIC_PIN_READ = 8,
    TOPIC_PIN_MODE = 9,
    TOPIC_PIN_SUCCESS = 10,
    TOPIC_DEVICE_STATUS = 11,
    TOPIC_DEVICE_RESET = 12,
    TOPIC_SYSTEM_ERROR = 13,
    TOPIC_SYSTEM_WARNING = 14,
    TOPIC_TIMER = 15,
    TOPIC_STATUS_PUBLISH_SUCCESS = 16,
    TOPIC_MQTT_SUBSCRIBED = 17,
    TOPIC__ASYNC_RESULT = 31  // Reserved internal
};
```

## Error Handling Pattern
```cpp
void error_handler(const Event& e, void* user) {
    if (e.type == TOPIC_SYSTEM_ERROR) {
        // 1. Log the error
        ESP_LOGE(TAG, "System error: %d", e.i32);
        
        // 2. Increment error counter
        SystemStateManager::incrementErrorCount();
        
        // 3. Enter safe mode if needed
        if (e.i32 == CRITICAL_ERROR_CODE) {
            enterSafeMode();
        }
    }
}
```

## Testing
```bash
# Build and run tests
cd test
./build_eventbus_test.sh
idf.py flash monitor
```

## Configuration
```cpp
#define EBUS_MAX_LISTENERS 8
#define EBUS_DISPATCH_QUEUE_LEN 32
#define EVENT_BUS_TASK_STACK_SIZE 4096
#define EVENT_BUS_TASK_PRIORITY (tskIDLE_PRIORITY + 2)
```

## Performance Notes
- Topics 0-31 use fast bitmask filtering
- Queue overflow uses DROP_OLDEST policy
- Mailbox provides O(1) latest-only access
- Destructors called automatically after dispatch
