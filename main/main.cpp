#include "eventbus/TinyEventBus.h"
#include "eventbus/EventProtocol.h"
#include "eventbus/FlowGraph.h"
#include "config.h"
#include "data_structures.h"
#include "message_processor.h"
#include "device_monitor.h"
#include "custom_mqtt_client.h"
#include "system_state.h"
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <memory>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "esp_timer.h"

static const char* TAG = "MAIN";

// ===== HELPER FUNCTIONS FOR MANUAL CLEANUP =====

// Helper function for manual event cleanup
static void manual_event_cleanup(const Event& event, const char* context = "worker") {
    if (event.cleanup && event.ptr) {
        event.cleanup(event.ptr);
        ESP_LOGD("EVENT_CLEANUP", "Manually freed event payload in %s", context);
    }
}

// Helper function to create events with manual cleanup disabled
static Event create_event_with_manual_cleanup(uint16_t type, int32_t data, void* ptr = nullptr, EventCleanupFn cleanup = nullptr) {
    return Event{type, data, ptr, cleanup, false}; // auto_cleanup = false
}

// Helper function to create events with auto cleanup (for simple events)
static Event create_simple_event(uint16_t type, int32_t data, void* ptr = nullptr) {
    return Event{type, data, ptr, nullptr, true}; // auto_cleanup = true for simple events
}

// Global event bus
static TinyEventBus BUS;

// WiFi event group
static EventGroupHandle_t s_wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT = BIT1;

// ===== PURE FUNCTIONS FOR EXPLICIT DATA FLOW =====

// Pure function to get current uptime in seconds
uint64_t getCurrentUptimeSeconds() {
    return esp_timer_get_time() / 1000000;
}

// Pure function to check if device info should be published
bool shouldPublishDeviceInfo(uint64_t last_publish_time, uint64_t current_time, uint64_t interval_seconds = 60) {
    return (current_time - last_publish_time) >= interval_seconds;
}

// ===== CENTRALIZED LOGGING SYSTEM =====

// Centralized logging function that observes all events worth logging
void centralized_logging_handler(const Event& e, void* user) {
    const char* event_name = nullptr;
    const char* log_message = nullptr;
    bool is_error = false;
    
    switch (e.type) {
        case TOPIC_WIFI_CONNECTED:
            event_name = "WiFi Connected";
            log_message = "WiFi connection established successfully";
            SystemStateManager::updateWifiState(true);
            break;
            
        case TOPIC_WIFI_DISCONNECTED:
            event_name = "WiFi Disconnected";
            log_message = "WiFi connection lost";
            SystemStateManager::updateWifiState(false);
            break;
            
        case TOPIC_MDNS_FOUND:
            event_name = "mDNS Found";
            {
                // Get broker info from global state instead of event payload
                SystemState current_state = SystemStateManager::getCurrentState();
                ESP_LOGI(TAG, "mDNS found MQTT broker: %s:%d", 
                         current_state.current_broker.c_str(), current_state.current_broker_port);
                SystemStateManager::updateMdnsState(true);
            }
            return; // Already logged above
            
        case TOPIC_MDNS_FAILED:
            event_name = "mDNS Failed";
            log_message = "mDNS query failed";
            is_error = true;
            SystemStateManager::updateMdnsState(false);
            SystemStateManager::incrementErrorCount();
            break;
            
        case TOPIC_MQTT_CONNECTED:
            event_name = "MQTT Connected";
            log_message = "MQTT connection established successfully";
            SystemStateManager::updateMqttState(true);
            break;
            
        case TOPIC_MQTT_DISCONNECTED:
            event_name = "MQTT Disconnected";
            log_message = "MQTT connection lost";
            SystemStateManager::updateMqttState(false);
            break;
            
        case TOPIC_MQTT_SUBSCRIBED:
            event_name = "MQTT Subscribed";
            log_message = "Successfully subscribed to MQTT topic";
            break;
            
        case TOPIC_MQTT_MESSAGE:
            event_name = "MQTT Message";
            {
                auto* msg_data = static_cast<MqttMessageData*>(e.ptr);
                if (msg_data) {
                    ESP_LOGI(TAG, "MQTT message received - Topic: %s, Payload: %s", 
                             msg_data->topic.c_str(), msg_data->payload.c_str());
                    SystemStateManager::incrementMessageCount();
                    // Manual cleanup for dynamic message data
                    manual_event_cleanup(e, "centralized_logging_mqtt_message");
                }
            }
            return; // Already logged above
            
        case TOPIC_PIN_SET:
            event_name = "Pin Set";
            {
                ESP_LOGI(TAG, "Pin set event - Pin: %d, Value: %p", e.i32, e.ptr);
            }
            return; // Already logged above
            
        case TOPIC_PIN_READ:
            event_name = "Pin Read";
            {
                ESP_LOGI(TAG, "Pin read event - Pin: %d, Value: %p", e.i32, e.ptr);
            }
            return; // Already logged above
            
        case TOPIC_SYSTEM_ERROR:
            event_name = "System Error";
            {
                const char* error_message = nullptr;
                switch (e.i32) {
                    case 1: error_message = "WiFi connection failed"; break;
                    case 2: error_message = "MQTT client error"; break;
                    case 3: error_message = "Message processing failed"; break;
                    case 4: error_message = "Device command execution failed"; break;
                    case 5: error_message = "mDNS query failed"; break;
                    case 6: error_message = "MQTT connection failed"; break;
                    case 7: error_message = "Fallback MQTT connection failed"; break;
                    case 8: error_message = "Status publish failed"; break;
                    default: error_message = "Unknown system error"; break;
                }
                ESP_LOGE(TAG, "System error %d: %s", e.i32, error_message);
                SystemStateManager::incrementErrorCount();
            }
            return; // Already logged above
            
        case TOPIC_STATUS_PUBLISH_SUCCESS:
            event_name = "Status Published";
            log_message = "Device status published successfully";
            break;
            
        case TOPIC_TIMER:
            // Timer events are not logged by default to reduce noise
            return;
            
        default:
            event_name = "Unknown Event";
            log_message = "Unknown event type received";
            break;
    }
    
    if (log_message) {
        if (is_error) {
            ESP_LOGE(TAG, "[%s] %s", event_name, log_message);
        } else {
            ESP_LOGI(TAG, "[%s] %s", event_name, log_message);
        }
    }
}

// ===== PURE WORKER FUNCTIONS (NO LOGGING) =====

// Real mDNS query worker - pure execution only
static bool mdns_query_worker(void** out) {
    // Give mDNS some time to initialize properly after WiFi connection
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Query for MQTT service using mDNS
    mdns_result_t* results = nullptr;
    esp_err_t err = mdns_query_ptr(MDNS_SERVICE_TYPE, MDNS_PROTOCOL, MDNS_QUERY_TIMEOUT_MS, MDNS_MAX_RESULTS, &results);
    
    if (err != ESP_OK) {
        // Store fallback broker in global state
        SystemStateManager::updateBrokerInfo("10.0.0.161", MQTT_BROKER_PORT);
        SystemStateManager::updateMdnsState(false);
        *out = nullptr; // No dynamic data to return
        return true;
    }
    
    if (results == nullptr) {
        // Store fallback broker in global state
        SystemStateManager::updateBrokerInfo("10.0.0.161", MQTT_BROKER_PORT);
        SystemStateManager::updateMdnsState(false);
        *out = nullptr; // No dynamic data to return
        return true;
    }
    
    // Iterate through the linked list of results
    mdns_result_t* r = results;
    while (r) {
        // Check if this is our target service
        if (r->instance_name && strcmp(r->instance_name, MDNS_SERVICE_NAME) == 0) {
            if (r->hostname) {
                // Check if hostname is already an IP address
                if (strchr(r->hostname, '.') != nullptr && isdigit(r->hostname[0])) {
                    // It looks like an IP address, use it directly
                    std::string broker_ip(r->hostname);
                    SystemStateManager::updateBrokerInfo(broker_ip, MQTT_BROKER_PORT);
                    SystemStateManager::updateMdnsState(true);
                    
                    mdns_query_results_free(results);
                    *out = nullptr; // No dynamic data to return
                    return true;
                } else {
                    // It's a hostname, try to resolve it
                    // Try to get the IP address from the service record directly
                    if (r->addr && r->addr->addr.type == ESP_IPADDR_TYPE_V4) {
                        // Convert IP address to string
                        char ip_str[16];
                        snprintf(ip_str, sizeof(ip_str), "%lu.%lu.%lu.%lu", 
                                 r->addr->addr.u_addr.ip4.addr & 0xFF,
                                 (r->addr->addr.u_addr.ip4.addr >> 8) & 0xFF,
                                 (r->addr->addr.u_addr.ip4.addr >> 16) & 0xFF,
                                 (r->addr->addr.u_addr.ip4.addr >> 24) & 0xFF);
                        
                        // Store broker IP in global state
                        std::string broker_ip(ip_str);
                        SystemStateManager::updateBrokerInfo(broker_ip, MQTT_BROKER_PORT);
                        SystemStateManager::updateMdnsState(true);
                        
                        mdns_query_results_free(results);
                        *out = nullptr; // No dynamic data to return
                        return true;
                    } else {
                        // Try to query for the IP address of this hostname
                        esp_ip4_addr_t addr;
                        esp_err_t addr_err = mdns_query_a(r->hostname, MDNS_QUERY_TIMEOUT_MS, &addr);
                        
                        if (addr_err == ESP_OK) {
                            // Convert IP address to string
                            char ip_str[16];
                            snprintf(ip_str, sizeof(ip_str), "%lu.%lu.%lu.%lu", 
                                     addr.addr & 0xFF,
                                     (addr.addr >> 8) & 0xFF,
                                     (addr.addr >> 16) & 0xFF,
                                     (addr.addr >> 24) & 0xFF);
                            
                            // Store broker IP in global state
                            std::string broker_ip(ip_str);
                            SystemStateManager::updateBrokerInfo(broker_ip, MQTT_BROKER_PORT);
                            SystemStateManager::updateMdnsState(true);
                            
                            mdns_query_results_free(results);
                            *out = nullptr; // No dynamic data to return
                            return true;
                        } else {
                            // Fallback to hostname with .local suffix
                            std::string full_hostname = std::string(r->hostname) + ".local";
                            SystemStateManager::updateBrokerInfo(full_hostname, MQTT_BROKER_PORT);
                            SystemStateManager::updateMdnsState(true);
                            
                            mdns_query_results_free(results);
                            *out = nullptr; // No dynamic data to return
                            return true;
                        }
                    }
                }
            }
        }
        r = r->next;
    }
    
    // If no matching service found, use fallback to known broker IP
    SystemStateManager::updateBrokerInfo("10.0.0.161", MQTT_BROKER_PORT);
    SystemStateManager::updateMdnsState(false);
    mdns_query_results_free(results);
    *out = nullptr; // No dynamic data to return
    return true;
}

// MQTT connection worker that gets broker address from global state - pure execution only
static bool mqtt_connection_worker_with_event(const Event& trigger_event, void** out) {
    ESP_LOGI("MQTT_WORKER_EVENT", "Attempting MQTT connection using broker from global state");
    
    // Get broker info from global state
    SystemState current_state = SystemStateManager::getCurrentState();
    std::string broker_host = current_state.current_broker;
    int broker_port = current_state.current_broker_port;
    
    if (broker_host.empty()) {
        ESP_LOGE("MQTT_WORKER_EVENT", "No broker hostname available in global state");
        return false;
    }
    
    ESP_LOGI("MQTT_WORKER_EVENT", "Connecting to broker: %s:%d", broker_host.c_str(), broker_port);
    
    // Create MQTT connection data using broker info from global state
    MqttConnectionData connection_data(broker_host.c_str(), broker_port, get_esp32_device_id());
    
    // Connect to MQTT
    auto result = MqttClient::connect(connection_data);
    
    if (result.success) {
        // Store connection data for later use
        auto* conn_data = new MqttConnectionData(connection_data);
        *out = conn_data;
        return true;
    } else {
        ESP_LOGE("MQTT_WORKER_EVENT", "Failed to connect: %s", result.error_message.c_str());
        return false;
    }
}

// ===== WIFI EVENT HANDLERS =====

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        // Publish WiFi disconnected event with manual cleanup
        Event disconnect_event = create_simple_event(TOPIC_WIFI_DISCONNECTED, 0, nullptr);
        BUS.publish(disconnect_event);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        (void)event_data; // Unused but required by interface
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        
        // Initialize mDNS now that we have network connectivity
        ESP_LOGI("WIFI_EVENT", "Initializing mDNS after IP acquisition...");
        esp_err_t mdns_ret = mdns_init();
        ESP_LOGI("WIFI_EVENT", "mDNS init result: %s", esp_err_to_name(mdns_ret));
        
        if (mdns_ret == ESP_OK || mdns_ret == ESP_ERR_INVALID_STATE) {
            std::string hostname = get_esp32_device_id();
            esp_err_t hostname_ret = mdns_hostname_set(hostname.c_str());
            ESP_LOGI("WIFI_EVENT", "mDNS hostname set result: %s for hostname: %s", esp_err_to_name(hostname_ret), hostname.c_str());
        }
    }
}

// ===== WIFI INITIALIZATION =====

static void wifi_init_sta() {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASSWORD);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // Disable WiFi auto sleep to maintain stable connection
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    
    ESP_ERROR_CHECK(esp_wifi_start());
}

// ===== EXECUTION EVENT HANDLERS (NO LOGGING) =====

void fallback_mqtt_connection_handler(const Event& e, void* user) {
    // Get broker info from global state
    SystemState current_state = SystemStateManager::getCurrentState();
    std::string broker_host = current_state.current_broker;
    int broker_port = current_state.current_broker_port;
    
    if (broker_host.empty()) {
        // Fallback to default if no broker info available
        broker_host = MQTT_BROKER_HOST;
        broker_port = MQTT_BROKER_PORT;
    }
    
    // Create fallback connection data using global state
    MqttConnectionData fallback_data(broker_host.c_str(), broker_port, get_esp32_device_id());
    auto result = MqttClient::connect(fallback_data);
    
    if (result.success) {
        std::string control_topic = get_mqtt_control_topic();
        if (MqttClient::subscribe(control_topic, 0)) {
            // Publish success event with manual cleanup
            Event success_event = create_simple_event(TOPIC_MQTT_CONNECTED, 0, nullptr);
            BUS.publish(success_event);
        } else {
            // Publish error event with manual cleanup
            Event error_event = create_simple_event(TOPIC_SYSTEM_ERROR, 7, nullptr);
            BUS.publish(error_event);
        }
    } else {
        // Publish error event with manual cleanup
        Event error_event = create_simple_event(TOPIC_SYSTEM_ERROR, 7, nullptr);
        BUS.publish(error_event);
    }
}

// Removed: Old event handlers replaced by declarative flows below

void timer_execution_handler(const Event& e, void* user) {
    static uint64_t last_publish_time = 0;
    uint64_t current_time = getCurrentUptimeSeconds();
    
    // Update system state
    SystemStateManager::updateUptime(current_time);
    
    // Check if we should publish device info (pure function)
    if (shouldPublishDeviceInfo(last_publish_time, current_time, DEVICE_STATUS_PUBLISH_INTERVAL_MS / 1000)) {
        // Check if MQTT is connected
    if (MqttClient::isConnected()) {
            // Create device status using pure function
            std::string status_json = SystemStateManager::createDeviceStatusJson();
            
            // Publish device info
            std::string status_topic = get_mqtt_status_topic();
            if (MqttClient::publish(status_topic, status_json, 0)) {
                // Publish success event with manual cleanup
                Event success_event = create_simple_event(TOPIC_STATUS_PUBLISH_SUCCESS, 0, nullptr);
                BUS.publish(success_event);
            } else {
                // Publish error event with manual cleanup
                Event error_event = create_simple_event(TOPIC_SYSTEM_ERROR, 8, nullptr);
                BUS.publish(error_event);
            }
        }
        last_publish_time = current_time;
    }
}

// ===== MAIN APPLICATION =====

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32 EventBus System Starting ===");
    ESP_LOGI(TAG, "Device ID: %s", get_esp32_device_id().c_str());
    ESP_LOGI(TAG, "Machine ID: %s", MACHINE_ID);
    ESP_LOGI(TAG, "SSID: %s", WIFI_SSID);
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // mDNS will be initialized after WiFi connection

    // Initialize GPIO pins
    DeviceMonitor::initializePins();

    // Start the event bus
    if (!BUS.begin("event-dispatch", EVENT_BUS_TASK_STACK_SIZE, EVENT_BUS_TASK_PRIORITY)) {
        ESP_LOGE(TAG, "Failed to start event bus");
        return;
    }

    // Set event bus for MQTT client
    MqttClient::setEventBus(&BUS);

    // Build declarative flows
    FlowGraph G(BUS);

    // Flow 1: When WiFi connects, do mDNS lookup
    G.when(TOPIC_WIFI_CONNECTED,
        G.async_blocking("mdns-query", mdns_query_worker,
            // onOk → publish MDNS_FOUND (no dynamic data needed)
            [](const Event& e, IEventBus& bus) {
                // Broker info is already stored in global state by mdns_query_worker
                // Just publish the success event
                Event mdns_event = create_simple_event(TOPIC_MDNS_FOUND, 0, nullptr);
                bus.publish(mdns_event);
            },
            // onErr → publish MDNS_FAILED and system error
            FlowGraph::seq(
                FlowGraph::publish(TOPIC_MDNS_FAILED),
                FlowGraph::publish(TOPIC_SYSTEM_ERROR, 5, nullptr)
            )
        )
    );

    // Flow 2: When mDNS succeeds, connect to MQTT
    G.when(TOPIC_MDNS_FOUND,
        G.async_blocking_with_event("mqtt-connect", mqtt_connection_worker_with_event,
            // onOk → publish MQTT_CONNECTED
            FlowGraph::publish(TOPIC_MQTT_CONNECTED, 1, nullptr),
            // onErr → publish MQTT_DISCONNECTED and system error
            FlowGraph::seq(
                FlowGraph::publish(TOPIC_MQTT_DISCONNECTED, 0, nullptr),
                FlowGraph::publish(TOPIC_SYSTEM_ERROR, 6, nullptr)
            )
        )
    );

    // Flow 2.5: When MQTT connects, subscribe to control topic
    G.when(TOPIC_MQTT_CONNECTED,
        FlowGraph::tap([](const Event& e) {
            // Subscribe to control topic
            std::string control_topic = get_mqtt_control_topic();
            
            if (!MqttClient::subscribe(control_topic, 1)) {
                Event error_event = create_simple_event(TOPIC_SYSTEM_ERROR, 2, nullptr);
                BUS.publish(error_event);
            }
        })
    );

    // Flow 3: When mDNS fails, use fallback MQTT broker
    G.when(TOPIC_MDNS_FAILED,
        FlowGraph::publish(TOPIC_MDNS_FAILED)
    );

    // ===== REGISTER CENTRALIZED LOGGING HANDLER =====
    // Subscribe to all events for centralized logging
    BUS.subscribe(centralized_logging_handler, nullptr, MASK_ALL);

    // Flow 4: When MQTT message received, parse it into device commands
    G.when(TOPIC_MQTT_MESSAGE, 
        FlowGraph::tap([](const Event& e) {
            auto* msg_data = static_cast<MqttMessageData*>(e.ptr);
            if (!msg_data) {
                Event error_event = create_simple_event(TOPIC_SYSTEM_ERROR, 1, nullptr);
                BUS.publish(error_event);
                return;
            }
            
            // Parse message to device commands (pure function)
            auto device_command_result = MessageProcessor::processMessageToDeviceCommands(msg_data->payload);
            
            if (!device_command_result.success) {
                Event error_event = create_simple_event(TOPIC_SYSTEM_ERROR, 3, nullptr);
                BUS.publish(error_event);
                return;
            }
            
            // Publish specific command events for each device command
            for (const auto& device_cmd : device_command_result.device_commands) {
                switch (device_cmd.type) {
                    case PIN_SET: {
                        auto* pin_cmd_data = new PinCommandData{
                            device_cmd.pin,
                            device_cmd.value,
                            device_cmd.description
                        };
                        Event pin_event = create_event_with_manual_cleanup(TOPIC_PIN_SET, device_cmd.pin, 
                            pin_cmd_data, [](void* ptr) { delete static_cast<PinCommandData*>(ptr); });
                        BUS.publish(pin_event);
                        break;
                    }
                    case PIN_READ: {
                        auto* pin_cmd_data = new PinCommandData{
                            device_cmd.pin,
                            0, // Read doesn't have a value to set
                            device_cmd.description
                        };
                        Event pin_event = create_event_with_manual_cleanup(TOPIC_PIN_READ, device_cmd.pin, 
                            pin_cmd_data, [](void* ptr) { delete static_cast<PinCommandData*>(ptr); });
                        BUS.publish(pin_event);
                        break;
                    }
                    case PIN_MODE: {
                        auto* pin_cmd_data = new PinCommandData{
                            device_cmd.pin,
                            device_cmd.value, // Mode value
                            device_cmd.description
                        };
                        Event pin_event = create_event_with_manual_cleanup(TOPIC_PIN_MODE, device_cmd.pin, 
                            pin_cmd_data, [](void* ptr) { delete static_cast<PinCommandData*>(ptr); });
                        BUS.publish(pin_event);
                        break;
                    }
                    case DEVICE_STATUS: {
                        Event status_event = create_simple_event(TOPIC_DEVICE_STATUS, 0, nullptr);
                        BUS.publish(status_event);
                        break;
                    }
                    case DEVICE_RESET: {
                        Event reset_event = create_simple_event(TOPIC_DEVICE_RESET, 0, nullptr);
                        BUS.publish(reset_event);
                        break;
                    }
                    default: {
                        Event error_event = create_simple_event(TOPIC_SYSTEM_ERROR, 4, nullptr);
                        BUS.publish(error_event);
                        break;
                    }
                }
            }
        })
    );

    // Helper lambda for pin command execution (DRY principle)
    auto executePinCommand = [](const Event& e, DeviceCommandType cmdType) {
        auto* pin_cmd_data = static_cast<PinCommandData*>(e.ptr);
        if (!pin_cmd_data) {
            Event error_event = create_simple_event(TOPIC_SYSTEM_ERROR, 5, nullptr);
            BUS.publish(error_event);
            return;
        }
        
        DevicePinCommand device_cmd(cmdType, pin_cmd_data->pin, pin_cmd_data->value, pin_cmd_data->description);
        auto result = DeviceMonitor::executeDeviceCommand(device_cmd);
        
        if (result.success) {
            Event success_event = create_simple_event(TOPIC_PIN_SUCCESS, result.pin, reinterpret_cast<void*>(result.value));
            BUS.publish(success_event);
        } else {
            Event error_event = create_simple_event(TOPIC_SYSTEM_ERROR, 7, nullptr);
            BUS.publish(error_event);
        }
        
        // Manual cleanup of the event payload
        manual_event_cleanup(e, "executePinCommand");
    };

    // Flow 5: When pin set command received, execute it
    G.when(TOPIC_PIN_SET, FlowGraph::tap([executePinCommand](const Event& e) {
        executePinCommand(e, PIN_SET);
    }));

    // Flow 6: When pin read command received, execute it
    G.when(TOPIC_PIN_READ, FlowGraph::tap([executePinCommand](const Event& e) {
        executePinCommand(e, PIN_READ);
    }));

    // Flow 7: When pin mode command received, execute it
    G.when(TOPIC_PIN_MODE, FlowGraph::tap([executePinCommand](const Event& e) {
        executePinCommand(e, PIN_MODE);
    }));

    // ===== REGISTER LEGACY EVENT HANDLERS =====
    BUS.subscribe(fallback_mqtt_connection_handler, nullptr, bit(TOPIC_MDNS_FAILED));
    BUS.subscribe(timer_execution_handler, nullptr, bit(TOPIC_TIMER));

    // Initialize WiFi
    wifi_init_sta();

    // Main loop - wait for WiFi connection and trigger events
    EventBits_t bits;
    while (1) {
        bits = xEventGroupWaitBits(s_wifi_event_group,
                                   WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                   pdFALSE,
                                   pdFALSE,
                                   portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT) {
            Event wifi_event = create_simple_event(TOPIC_WIFI_CONNECTED, 0, nullptr);
            BUS.publish(wifi_event);
            break;
        } else if (bits & WIFI_FAIL_BIT) {
            Event error_event = create_simple_event(TOPIC_SYSTEM_ERROR, 1, nullptr);
            BUS.publish(error_event);
            break;
        }
    }

    // ===== EVENT-DRIVEN MAIN LOOP =====
    ESP_LOGI(TAG, "EventBus system running...");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY_MS));
        
        // Publish periodic timer event with current uptime
        uint64_t current_time = getCurrentUptimeSeconds();
        Event timer_event = create_simple_event(TOPIC_TIMER, static_cast<int32_t>(current_time), nullptr);
        BUS.publish(timer_event);
    }
}