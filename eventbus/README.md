# Flexible Event System + Declarative Flows for ESP32

A lightweight, zero-dependency event system with declarative flow composition for ESP32 applications.

## Features

- **Zero external dependencies** (only FreeRTOS from ESP32)
- **Fast O(1) fan-out** using 32-bit topic bitmasks
- **ISR-safe** with single dispatcher queue for ISR → task context handoff
- **Declarative flows** with composable operators: `when()`, `publish()`, `seq()`, `tee()`, `filter()`, `branch()`, `tap()`, and `async_blocking()`
- **Small footprint** - header-only implementation with configurable limits
- **Portable** - works in ESP-IDF and Arduino-ESP32

## Quick Start

### Basic Usage

```cpp
#include "eventbus/TinyEventBus.h"
#include "eventbus/EventProtocol.h"
#include "eventbus/FlowGraph.h"

// Global event bus
static TinyEventBus BUS;

extern "C" void app_main(void) {
    // Start the event bus
    BUS.begin();

    // Build declarative flows
    FlowGraph G(BUS);

    // When WiFi connects, do mDNS lookup
    G.when(TOPIC_WIFI_CONNECTED,
        G.async_blocking("mdns-query", mdns_query_worker,
            FlowGraph::publish(TOPIC_MDNS_FOUND),    // onOk
            FlowGraph::publish(TOPIC_MDNS_FAILED)   // onErr
        )
    );

    // Subscribe to events
    BUS.subscribe([](const Event& e, void*) {
        printf("mDNS found: %s\n", (char*)e.ptr);
    }, nullptr, bit(TOPIC_MDNS_FOUND));

    // Publish events
    BUS.publish(Event{TOPIC_WIFI_CONNECTED, 0, nullptr});
}
```

## Architecture

### Core Components

1. **`IEventBus`** - Interface for event bus implementations
2. **`TinyEventBus`** - Header-only implementation with O(1) fan-out
3. **`FlowGraph`** - Declarative flow composition layer
4. **`Event`** - Lightweight event structure with topic, data, and payload

### Event Structure

```cpp
struct Event {
    uint16_t type;   // Topic ID: 0..31 use mask fast-path
    int32_t  i32;    // Small scalar (e.g., pin, code)
    void*    ptr;    // Optional payload pointer (publisher-managed lifetime)
};
```

### Topic System

Topics 0-31 are reserved for fast-path bitmask filtering. Use `bit(topic)` to create masks:

```cpp
enum : uint16_t {
    TOPIC_WIFI_CONNECTED = 0,
    TOPIC_MDNS_FOUND     = 1,
    TOPIC_MQTT_CONNECTED = 3,
    // ...
    TOPIC__ASYNC_RESULT  = 31  // Reserved for async continuations
};

// Subscribe to specific topics
bus.subscribe(handler, nullptr, bit(TOPIC_WIFI_CONNECTED) | bit(TOPIC_MDNS_FOUND));
```

## Declarative Flows

### Basic Operators

```cpp
// Publish an event
FlowGraph::publish(TOPIC_MDNS_FOUND, 42, nullptr)

// Sequence two flows
FlowGraph::seq(flow1, flow2)

// Fan-out (same as seq)
FlowGraph::tee(flow1, flow2)

// Filter events
FlowGraph::filter([](const Event& e) { return e.i32 > 100; }, thenFlow)

// Branch based on condition
FlowGraph::branch(predicate, onTrueFlow, onFalseFlow)

// Observe without publishing (tap)
FlowGraph::tap([](const Event& e) { printf("Observed: %d\n", e.i32); })
```

### Async Operations

```cpp
// Run long operation in separate task
G.async_blocking("worker-name", 
    [](void** out) {
        // Do work here
        *out = result;
        return success;
    },
    FlowGraph::publish(TOPIC_SUCCESS),  // onOk
    FlowGraph::publish(TOPIC_FAILURE)   // onErr
)
```

### Complex Flows

```cpp
// When WiFi connects: do mDNS lookup, then connect to MQTT
G.when(TOPIC_WIFI_CONNECTED,
    G.async_blocking("mdns-query", mdns_query_worker,
        // Success path
        FlowGraph::seq(
            FlowGraph::tap([](const Event& e) {
                printf("Found broker: %s\n", (char*)e.ptr);
            }),
            FlowGraph::publish(TOPIC_MQTT_CONNECTED)
        ),
        // Failure path
        FlowGraph::publish(TOPIC_MDNS_FAILED)
    )
);
```

## Configuration

Configure limits via macros:

```cpp
#define EBUS_MAX_LISTENERS 8        // Max event handlers
#define EBUS_DISPATCH_QUEUE_LEN 32  // ISR queue length
```

## API Reference

### Low-level Bus API

```cpp
// Subscribe to events
ListenerHandle subscribe(EventHandler handler,
                        void* user = nullptr,
                        TopicMask mask = 0xFFFFFFFFu,
                        EventPred pred = nullptr,
                        void* predUser = nullptr);

// Unsubscribe
void unsubscribe(ListenerHandle h);

// Publish from task context
void publish(const Event& e);

// Publish from ISR
void publishFromISR(const Event& e, BaseType_t* hpw = nullptr);
```

### Declarative API

```cpp
// Create flow graph
FlowGraph G(bus);

// Declare flow for topic
G.when(topic, flow);

// Flow operators
FlowGraph::publish(topic, i32=0, ptr=nullptr)
FlowGraph::seq(a, b)
FlowGraph::tee(a, b)
FlowGraph::filter(pred, thenFlow)
FlowGraph::branch(pred, onTrue, onFalse)
FlowGraph::tap(observer)
G.async_blocking(name, workerFn, onOk, onErr)
```

## Examples

### ESP-IDF Demo

See `examples/idf_wifi_mdns_demo/main.cpp` for a complete example with WiFi and mDNS integration.

### Unit Tests

Run the comprehensive test suite:

```bash
cd eventbus/test
idf.py build
idf.py flash monitor
```

Tests cover:
- Basic event publishing
- Topic masking
- Event predicates
- ISR publishing
- Declarative flows
- Tap operator
- Flow composition
- Async blocking
- Filter and branch operators

## Integration

### ESP-IDF Component

Add to your project:

```cmake
# In your project's CMakeLists.txt
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/eventbus)

# In your component's CMakeLists.txt
idf_component_register(
    SRCS "main.cpp"
    REQUIRES eventbus freertos esp_wifi esp_event
)
```

### Arduino-ESP32

Copy the header files to your project and include them directly.

## Performance

- **O(1) fan-out** for topics 0-31 using bitmasks
- **Zero allocation** on hot path (publish in task context)
- **Single queue operation** for ISR publishing
- **Configurable limits** for memory usage

## Memory Usage

Default configuration:
- 8 listeners × 24 bytes = 192 bytes
- 32 event queue × 12 bytes = 384 bytes
- Dispatcher task: 2048 bytes stack
- **Total: ~2.6KB**

## Thread Safety

- **Task context**: Direct fan-out, no locks needed
- **ISR context**: Queue-based, safe for any ISR
- **Flow composition**: Thread-safe, can be called from any context

## Extensions

Easy to add:
- Per-listener queues for slow consumers
- Drop policy on ISR enqueue
- `debounce(ms)`, `throttle(ms)` operators
- `retry(n)` in async_blocking
- Type-tagged payload envelopes

## License

MIT License - see LICENSE file for details.

