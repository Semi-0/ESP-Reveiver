# Event-Driven Architecture - Complete Decoupling

## 🎯 **Event Flow Diagram**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              MAIN FUNCTION                                  │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ 1. Initialize Event Bus                                             │   │
│  │ 2. Register Event Handlers                                          │   │
│  │ 3. Setup WiFi (Pure Function)                                       │   │
│  │ 4. Publish WiFi Success Event                                       │   │
│  │ 5. Main Loop (Just Keep Running)                                    │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              EVENT BUS                                      │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ • Topic-based filtering (bitmasks)                                 │   │
│  │ • Asynchronous event dispatch                                       │   │
│  │ • ISR-safe publishing                                               │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           EVENT HANDLERS                                   │
│                                                                             │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐            │
│  │   LOGGING       │  │   EXECUTION     │  │  ORCHESTRATION  │            │
│  │   HANDLERS      │  │   HANDLERS      │  │   HANDLERS      │            │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘            │
│           │                     │                     │                   │
│           ▼                     ▼                     ▼                   │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐            │
│  │ • WiFi Logging  │  │ • WiFi Success  │  │ • Start mDNS    │            │
│  │ • mDNS Logging  │  │ • mDNS Success  │  │ • Start MQTT    │            │
│  │ • MQTT Logging  │  │ • MQTT Success  │  │                 │            │
│  │ • System Logging│  │ • MQTT Message  │  │                 │            │
│  │ • Error Logging │  │                 │  │                 │            │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘            │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           EVENT FLOW                                       │
│                                                                             │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐                    │
│  │   WiFi      │───▶│   mDNS      │───▶│   MQTT      │                    │
│  │  Setup      │    │ Discovery   │    │ Connection  │                    │
│  └─────────────┘    └─────────────┘    └─────────────┘                    │
│         │                   │                   │                         │
│         ▼                   ▼                   ▼                         │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐                    │
│  │ WiFi Success│───▶│ mDNS Success│───▶│ MQTT Success│                    │
│  │   Event     │    │   Event     │    │   Event     │                    │
│  └─────────────┘    └─────────────┘    └─────────────┘                    │
│         │                   │                   │                         │
│         ▼                   ▼                   ▼                         │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐                    │
│  │ Start mDNS  │    │ Start MQTT  │    │ System      │                    │
│  │ Discovery   │    │ Connection  │    │ Ready       │                    │
│  └─────────────┘    └─────────────┘    └─────────────┘                    │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           PURE FUNCTIONS                                   │
│                                                                             │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐            │
│  │   WiFi Pure     │  │   mDNS Pure     │  │   MQTT Pure     │            │
│  │   Functions     │  │   Functions     │  │   Functions     │            │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘            │
│           │                     │                     │                   │
│           ▼                     ▼                     ▼                   │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐            │
│  │ • setupWifiPure │  │ • discoverMqtt  │  │ • setupMqttPure │            │
│  │ • initialize    │  │   Pure          │  │ • connect       │            │
│  │ • connect       │  │ • initialize    │  │ • subscribe     │            │
│  │ • getStatus     │  │ • start         │  │ • publish       │            │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘            │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 🔄 **Event Flow Sequence**

### **1. Initialization Phase**
```
Main Function
    ↓
Initialize Event Bus
    ↓
Register Event Handlers (Logging + Execution + Orchestration)
    ↓
Setup WiFi (Pure Function - No Logging)
    ↓
Publish WiFi Success Event
```

### **2. Event-Driven Flow**
```
WiFi Success Event
    ↓
handleWifiSuccessEvent() → publishSystemEvent("start_discovery", "mdns")
    ↓
handleStartMdnsDiscoveryEvent() → discoverMqttPure() → publishMdnsEvent()
    ↓
handleMdnsSuccessEvent() → publishSystemEvent("start_connection", "mqtt")
    ↓
handleStartMqttConnectionEvent() → setupMqttPure() → publishMqttEvent()
    ↓
handleMqttSuccessEvent() → publishSystemEvent("ready", "system")
```

### **3. Message Processing Flow**
```
MQTT Message Received
    ↓
handleMqttMessageEvent() → parse_json_message() → execute_commands()
    ↓
publishPinEvent() or publishErrorEvent()
    ↓
Independent logging handlers log the results
```

## 🎯 **Key Benefits**

### **Complete Decoupling**
- ✅ **Logging Handlers** - Only handle logging, no execution
- ✅ **Execution Handlers** - Only handle execution, no logging
- ✅ **Orchestration Handlers** - Only handle flow control
- ✅ **Pure Functions** - No side effects, explicit inputs/outputs

### **Event-Driven Architecture**
- ✅ **Asynchronous** - Events are queued and processed asynchronously
- ✅ **Decoupled** - Components communicate only via events
- ✅ **Extensible** - Easy to add new event types and handlers
- ✅ **Testable** - Each handler can be tested independently

### **Pure Function Benefits**
- ✅ **No Side Effects** - Functions only return results, don't log
- ✅ **Explicit Inputs/Outputs** - All data flow is explicit
- ✅ **Composable** - Functions can be combined with retry logic
- ✅ **Testable** - Pure functions are easy to unit test

## 📊 **Event Types and Masks**

| Event Type | Mask | Purpose | Handlers |
|------------|------|---------|----------|
| `TOPIC_WIFI` | `MASK_WIFI` | WiFi connection events | Logging + Execution |
| `TOPIC_MDNS` | `MASK_MDNS` | mDNS discovery events | Logging + Execution |
| `TOPIC_MQTT` | `MASK_MQTT` | MQTT connection/message events | Logging + Execution |
| `TOPIC_SYSTEM` | `MASK_SYSTEM` | System orchestration events | Logging + Orchestration |
| `TOPIC_ERROR` | `MASK_ERRORS` | Error events | Logging only |
| `TOPIC_PIN` | `MASK_PINS` | Pin control events | Logging only |

## 🚀 **Usage Example**

```cpp
// Main function only orchestrates the flow
extern "C" void app_main(void) {
    // 1. Initialize event bus
    initEventBus();
    
    // 2. Register all event handlers
    registerEventHandlers();
    
    // 3. Start the flow with WiFi setup
    auto wifi_result = setupWifiPure(wifi_config);
    if (wifi_result.success) {
        publishWifiEvent(true, ssid, ip);  // Triggers mDNS discovery
    } else {
        publishWifiEvent(false, ssid, ""); // Triggers error handling
    }
    
    // 4. Main loop - just keep running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        // All communication via events
    }
}
```

This architecture provides complete separation of concerns, making the system highly maintainable, testable, and extensible!
