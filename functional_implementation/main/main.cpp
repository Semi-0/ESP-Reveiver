#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <functional>
#include <memory>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

// Include our modular components
#include "config.h"
#include "event_bus_interface.h"
#include "wifi_pure.h"
#include "mdns_pure.h"
#include "mqtt_pure.h"
#include "message_processor.h"
#include "device_commands.h"
#include "device_monitor.h"

static const char* TAG = "Main";

// ===== DATA STRUCTURES FOR EXPLICIT DATA FLOW =====

// Service discovery result for explicit data passing
struct ServiceDiscoveryData {
    std::string service_name;
    std::string host;
    int port;
    bool valid;
    
    ServiceDiscoveryData() : port(0), valid(false) {}
    ServiceDiscoveryData(const std::string& name, const std::string& h, int p) 
        : service_name(name), host(h), port(p), valid(true) {}
    
    static ServiceDiscoveryData invalid() {
        return ServiceDiscoveryData();
    }
};

// MQTT connection data for explicit data passing
struct MqttConnectionData {
    std::string broker_host;
    int broker_port;
    std::string client_id;
    
    MqttConnectionData() : broker_port(0) {}
    MqttConnectionData(const std::string& host, int port, const std::string& id)
        : broker_host(host), broker_port(port), client_id(id) {}
};

// ===== CURRIED EVENT HANDLER FACTORIES =====

// Curried event handler factory - takes component name and returns event handler
auto createLoggingHandler(const char* component) {
    return [component](const Event& event, void* user_data) {
        const char* status = static_cast<const char*>(event.ptr);
        ESP_LOGI(TAG, "System event: %s - %s", component, status ? status : "unknown");
    };
}

// Curried error handler factory
auto createErrorHandler(const char* component) {
    return [component](const Event& event, void* user_data) {
        const char* message = static_cast<const char*>(event.ptr);
        int error_code = event.i32;
        ESP_LOGE(TAG, "Error in %s: %s (code: %d)", component, message ? message : "unknown error", error_code);
    };
}

// Curried WiFi logging handler
auto createWifiLoggingHandler() {
    return [](const Event& event, void* user_data) {
        bool connected = (event.i32 != 0);
        ESP_LOGI(TAG, "WiFi %s", connected ? "connected" : "disconnected");
        
        if (connected) {
            ESP_LOGI(TAG, "WiFi setup completed successfully");
        } else {
            ESP_LOGE(TAG, "WiFi setup failed");
        }
    };
}

// Curried mDNS logging handler
auto createMdnsLoggingHandler() {
    return [](const Event& event, void* user_data) {
        bool discovered = (event.i32 != 0);
        ESP_LOGI(TAG, "mDNS discovery: %s", discovered ? "success" : "failed");
        
        if (discovered) {
            ESP_LOGI(TAG, "MQTT broker discovered via mDNS");
        } else {
            ESP_LOGE(TAG, "mDNS discovery failed");
        }
    };
}

// Curried MQTT logging handler
auto createMqttLoggingHandler() {
    return [](const Event& event, void* user_data) {
        bool connected = (event.i32 != 0);
        ESP_LOGI(TAG, "MQTT %s", connected ? "connected" : "disconnected");
        
        if (connected) {
            ESP_LOGI(TAG, "MQTT connection established successfully");
        } else {
            ESP_LOGE(TAG, "MQTT connection failed");
        }
    };
}

// ===== PURE EVENT HANDLERS (NO LOGGING) =====

void handleWifiSuccessEvent(const Event& event, void* user_data) {
    // WiFi succeeded - trigger mDNS discovery
    publishSystemEvent("start_discovery", "mdns");
}

void handleMdnsSuccessEvent(const Event& event, void* user_data) {
    // Extract service discovery data from user_data
    auto* discovery_data = static_cast<ServiceDiscoveryData*>(user_data);
    
    if (discovery_data && discovery_data->valid) {
        // Create MQTT connection data from discovered service
        MqttConnectionData mqtt_data(discovery_data->host, discovery_data->port, get_esp32_device_id());
        
        // Allocate memory for the connection data and pass it to the event
        auto* mqtt_data_ptr = new MqttConnectionData(mqtt_data);
        
        // Create event with MQTT connection data
        Event mqtt_event;
        mqtt_event.type = TOPIC_SYSTEM;
        mqtt_event.ptr = mqtt_data_ptr;
        mqtt_event.i32 = 1; // Indicate start connection
        
        g_eventBus->publish(mqtt_event);
    } else {
        // No valid service discovered, use fallback
        MqttConnectionData mqtt_data(MQTT_BROKER_HOST, MQTT_BROKER_PORT, get_esp32_device_id());
        auto* mqtt_data_ptr = new MqttConnectionData(mqtt_data);
        
        Event mqtt_event;
        mqtt_event.type = TOPIC_SYSTEM;
        mqtt_event.ptr = mqtt_data_ptr;
        mqtt_event.i32 = 1; // Indicate start connection
        
        g_eventBus->publish(mqtt_event);
    }
}

void handleMqttSuccessEvent(const Event& event, void* user_data) {
    // MQTT succeeded - system is ready
    publishSystemEvent("ready", "system");
}

void handleMqttMessageEvent(const Event& event, void* user_data) {
    const char* message = static_cast<const char*>(event.ptr);
    if (!message) return;
    
    // Parse message and convert to device commands (pure function - no logging)
    auto device_command_result = MessageProcessor::processMessageToDeviceCommands(message);
    
    if (!device_command_result.success) {
        publishErrorEvent("MessageProcessor", device_command_result.error_message.c_str(), 0);
        return;
    }
    
    // Publish device command event for device monitor to handle
    if (!device_command_result.device_commands.empty()) {
        // Create device command event
        auto device_event = createDeviceCommandEvent(device_command_result.device_commands, "mqtt");
        
        // Publish device command event
        Event device_command_event;
        device_command_event.type = TOPIC_PIN;
        device_command_event.ptr = new DeviceCommandEvent(device_event);
        device_command_event.i32 = 1; // Indicate device command
        
        g_eventBus->publish(device_command_event);
    }
}

// ===== PURE FUNCTIONS (NO LOGGING, NO SIDE EFFECTS) =====

WifiResult setupWifiPure(const WifiConfig& config, int max_retries = 3) {
    for (int attempt = 1; attempt <= max_retries; attempt++) {
        // Initialize WiFi (pure function)
        auto init_result = WifiPure::initialize(config);
        if (!init_result.success) {
            continue;
        }
        
        // Connect to WiFi (pure function)
        auto connect_result = WifiPure::connect(config);
        if (connect_result.success) {
            return connect_result;
        }
        
        if (attempt < max_retries) {
            vTaskDelay(pdMS_TO_TICKS(config.retry_delay_ms));
        }
    }
    
    return WifiResult::failure_result("Max retries exceeded", max_retries);
}

ServiceDiscoveryData discoverMqttServicePure(const MdnsConfig& config, int max_retries = 3) {
    for (int attempt = 1; attempt <= max_retries; attempt++) {
        // Initialize mDNS (pure function)
        if (!MdnsPure::initialize(config)) {
            continue;
        }
        
        // Start mDNS service (pure function)
        if (!MdnsPure::start()) {
            continue;
        }
        
        // Discover MQTT services (pure function)
        auto discovery_result = MdnsPure::discoverMqttServices(config);
        if (discovery_result.success && !discovery_result.services.empty()) {
            // Validate and return first valid service
            for (const auto& service : discovery_result.services) {
                if (MdnsPure::validateService(service)) {
                    return ServiceDiscoveryData(service.service_name, service.host, service.port);
                }
            }
        }
        
        if (attempt < max_retries) {
            vTaskDelay(pdMS_TO_TICKS(config.timeout_ms));
        }
    }
    
    return ServiceDiscoveryData::invalid();
}

MqttResult setupMqttPure(const MqttConnectionData& connection_data, int max_retries = 3) {
    // Create MQTT config from connection data
    MqttConfig config;
    config.broker_host = connection_data.broker_host;
    config.broker_port = connection_data.broker_port;
    config.client_id = connection_data.client_id;
    
    for (int attempt = 1; attempt <= max_retries; attempt++) {
        // Initialize MQTT (pure function)
        auto init_result = MqttPure::initialize(config);
        if (!init_result.success) {
            continue;
        }
        
        // Connect to MQTT broker (pure function)
        auto connect_result = MqttPure::connect(config);
        if (connect_result.success) {
            // Subscribe to control topic (pure function)
            auto subscribe_result = MqttPure::subscribe(get_mqtt_control_topic(), 0);
            if (subscribe_result.success) {
                return connect_result;
            }
        }
        
        if (attempt < max_retries) {
            vTaskDelay(pdMS_TO_TICKS(config.connect_timeout * 1000));
        }
    }
    
    return MqttResult::failure_result("Max retries exceeded", 0, max_retries);
}

// ===== ORCHESTRATION EVENT HANDLERS WITH EXPLICIT DATA FLOW =====

void handleStartMdnsDiscoveryEvent(const Event& event, void* user_data) {
    // Start mDNS discovery (pure function)
    MdnsConfig mdns_config("mqtt", "_mqtt._tcp", "esp32", 1883, 5000);
    auto discovery_data = discoverMqttServicePure(mdns_config, 3);
    
    if (discovery_data.valid) {
        // Allocate memory for the discovery data
        auto* data_ptr = new ServiceDiscoveryData(discovery_data);
        
        // Publish success event with service data
        publishMdnsEvent(true, discovery_data.service_name.c_str(), 
                        discovery_data.host.c_str(), discovery_data.port);
        
        // Create event with discovery data for next step
        Event mdns_success_event;
        mdns_success_event.type = TOPIC_MDNS;
        mdns_success_event.ptr = data_ptr;
        mdns_success_event.i32 = 1; // Success
        
        g_eventBus->publish(mdns_success_event);
    } else {
        // Publish failure event
        publishMdnsEvent(false, "", "", 0);
    }
}

void handleStartMqttConnectionEvent(const Event& event, void* user_data) {
    // Extract MQTT connection data from the event
    auto* mqtt_data = static_cast<MqttConnectionData*>(event.ptr);
    
    if (!mqtt_data) {
        publishErrorEvent("Main", "No MQTT connection data provided", 0);
        return;
    }
    
    // Setup MQTT with the provided connection data (pure function)
    auto mqtt_result = setupMqttPure(*mqtt_data, 3);
    
    if (mqtt_result.success) {
        // Publish success event
        publishMqttEvent("connected", "success");
    } else {
        // Publish failure event
        publishMqttEvent("failed", mqtt_result.error_message.c_str());
    }
    
    // Clean up the allocated data
    delete mqtt_data;
}

// ===== CURRIED SUBSCRIPTION HELPER =====

// Curried subscription function that takes mask and returns a function that takes handler
auto subscribeTo(const uint32_t mask) {
    return [mask](EventHandler handler, void* user_data = nullptr) {
        return g_eventBus->subscribe(handler, user_data, mask);
    };
}

// ===== PURE FUNCTIONS FOR DEVICE INFO =====

// Pure function to create device status JSON
std::string createDeviceStatusJson(const std::string& device_id, uint64_t uptime_seconds) {
    return "{\"device_id\":\"" + device_id + 
           "\",\"status\":\"online\",\"uptime\":" + 
           std::to_string(uptime_seconds) + "}";
}

// Pure function to get current uptime in seconds
uint64_t getCurrentUptimeSeconds() {
    return esp_timer_get_time() / 1000000;
}

// Pure function to check if device info should be published
bool shouldPublishDeviceInfo(uint64_t last_publish_time, uint64_t current_time, uint64_t interval_seconds = 60) {
    return (current_time - last_publish_time) >= interval_seconds;
}

// ===== EVENT HANDLERS FOR MAIN LOOP =====

void handleMainLoopEvent(const Event& event, void* user_data) {
    // Extract current time from event
    uint64_t current_time = static_cast<uint64_t>(event.i32);
    
    // Check if MQTT is connected and we should publish device info
    if (MqttPure::isConnected()) {
        // Create device status using pure function
        std::string device_id = get_esp32_device_id();
        std::string status_json = createDeviceStatusJson(device_id, current_time);
        
        // Publish device info
        MqttPure::publish(get_mqtt_status_topic(), status_json);
    }
}

// ===== CLOCK EVENT HANDLER WITH CURRIED FUNCTIONS =====

// Curried clock handler factory that takes interval and returns a handler
auto createClockHandler(uint64_t interval_seconds) {
    static uint64_t last_publish_time = 0;
    
    return [interval_seconds](const Event& event, void* user_data) {
        uint64_t current_time = static_cast<uint64_t>(event.i32);
        
        // Check if we should publish device info (pure function)
        if (shouldPublishDeviceInfo(last_publish_time, current_time, interval_seconds)) {
            // Check if MQTT is connected
            if (MqttPure::isConnected()) {
                // Create device status using pure function
                std::string device_id = get_esp32_device_id();
                std::string status_json = createDeviceStatusJson(device_id, current_time);
                
                // Publish device info
                MqttPure::publish(get_mqtt_status_topic(), status_json);
            }
            last_publish_time = current_time;
        }
    };
}

// Clock event handler that publishes device info every 60 seconds
void handleClockEvent(const Event& event, void* user_data) {
    // Use curried clock handler with 60-second interval
    auto clock_handler = createClockHandler(60);
    clock_handler(event, user_data);
}

// Device command event handler
void handleDeviceCommandEvent(const Event& event, void* user_data) {
    auto* device_event = static_cast<DeviceCommandEvent*>(event.ptr);
    if (!device_event) return;
    
    // Execute device commands using device monitor (pure function)
    auto execution_results = DeviceMonitor::executeDeviceCommands(device_event->commands);
    
    // Publish results as events
    for (const auto& result : execution_results) {
        if (result.success) {
            publishPinEvent(result.pin, result.value, result.action_description.c_str());
        } else {
            publishErrorEvent("DeviceMonitor", result.error_message.c_str(), result.pin);
        }
    }
    
    // Clean up the allocated event data
    delete device_event;
}

// ===== PURE FUNCTIONS FOR DEVICE INFO PUBLISHING =====

// Pure function that takes clock event and MQTT publish function
auto createDeviceInfoPublisher(const std::string& device_id) {
    return [device_id](uint64_t uptime_seconds, const std::function<void(const std::string&, const std::string&)>& publish_func) {
        std::string status_json = createDeviceStatusJson(device_id, uptime_seconds);
        publish_func(get_mqtt_status_topic(), status_json);
    };
}

// Curried function that takes MQTT publish function and returns a function that takes uptime
auto createMqttDeviceInfoPublisher(const std::function<void(const std::string&, const std::string&)>& mqtt_publish_func) {
    return [mqtt_publish_func](uint64_t uptime_seconds) {
        std::string device_id = get_esp32_device_id();
        std::string status_json = createDeviceStatusJson(device_id, uptime_seconds);
        mqtt_publish_func(get_mqtt_status_topic(), status_json);
    };
}

// ===== MAIN APPLICATION ENTRY POINT =====

extern "C" void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize event bus
    initEventBus();
    if (!g_eventBus) {
        ESP_LOGE(TAG, "Failed to initialize event bus");
        return;
    }
    
    // ===== CREATE CURRIED SUBSCRIPTION FUNCTIONS =====
    auto subscribeToWifi = subscribeTo(MASK_WIFI);
    auto subscribeToMdns = subscribeTo(MASK_MDNS);
    auto subscribeToMqtt = subscribeTo(MASK_MQTT);
    auto subscribeToSystem = subscribeTo(MASK_SYSTEM);
    auto subscribeToErrors = subscribeTo(MASK_ERRORS);
    auto subscribeToTimers = subscribeTo(MASK_TIMERS);
    auto subscribeToPins = subscribeTo(MASK_PINS);
    
    // ===== REGISTER LOGGING EVENT HANDLERS USING CURRYING =====
    subscribeToWifi(createWifiLoggingHandler());
    subscribeToMdns(createMdnsLoggingHandler());
    subscribeToMqtt(createMqttLoggingHandler());
    subscribeToSystem(createLoggingHandler("system"));
    subscribeToErrors(createErrorHandler("error"));
    
    // ===== REGISTER EXECUTION EVENT HANDLERS =====
    subscribeToWifi(handleWifiSuccessEvent);
    subscribeToMdns(handleMdnsSuccessEvent);
    subscribeToMqtt(handleMqttSuccessEvent);
    subscribeToMqtt(handleMqttMessageEvent);
    
    // ===== REGISTER ORCHESTRATION EVENT HANDLERS =====
    subscribeToSystem(handleStartMdnsDiscoveryEvent);
    subscribeToSystem(handleStartMqttConnectionEvent);
    
    // ===== REGISTER MAIN LOOP EVENT HANDLER =====
    subscribeToTimers(handleMainLoopEvent);
    subscribeToTimers(handleClockEvent);
    
    // ===== REGISTER DEVICE COMMAND EVENT HANDLER =====
    subscribeToPins(handleDeviceCommandEvent);
    
    // ===== START THE EVENT-DRIVEN FLOW =====
    
    // Setup WiFi (pure function - no logging)
    WifiConfig wifi_config(WIFI_SSID, WIFI_PASSWORD, 5, 5000);
    auto wifi_result = setupWifiPure(wifi_config, 3);
    
    if (wifi_result.success) {
        // Publish WiFi success event - triggers mDNS discovery via event handler
        publishWifiEvent(true, wifi_config.ssid.c_str(), wifi_result.ip_address.c_str());
    } else {
        // Publish WiFi failure event
        publishWifiEvent(false, wifi_config.ssid.c_str(), "");
        publishErrorEvent("Main", "WiFi setup failed", 0);
        return;
    }
    
    // ===== EVENT-DRIVEN MAIN LOOP =====
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Get current uptime
        uint64_t current_time = getCurrentUptimeSeconds();
        
        // Publish clock event with current time - device info will be handled by clock handler
        Event clock_event;
        clock_event.type = TOPIC_TIMER;
        clock_event.i32 = static_cast<int32_t>(current_time);
        clock_event.ptr = nullptr;
        
        g_eventBus->publish(clock_event);
    }
}
