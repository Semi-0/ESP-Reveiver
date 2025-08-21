#include "eventbus/TinyEventBus.h"
#include "eventbus/EventProtocol.h"
#include "eventbus/FlowGraph.h"
#include "config.h"
#include "data_structures.h"
#include "message_processor.h"
#include "device_monitor.h"
#include "mqtt_client_local.h"
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
#include "esp_random.h"
#include <algorithm>

static const char* TAG = "EVENTBUS_MAIN";

// Global event bus
static TinyEventBus BUS;

// WiFi event group
static EventGroupHandle_t s_wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT = BIT1;

// Recovery and Safe Mode State
static uint32_t s_backoff_ms = 1000;
static TimerHandle_t t_retry = nullptr;
static int s_subs_pending = 0;

// ===== PURE FUNCTIONS FOR EXPLICIT DATA FLOW =====

// Pure function to get current uptime in seconds
uint64_t getCurrentUptimeSeconds() {
    return esp_timer_get_time() / 1000000;
}

// ===== RECOVERY AND SAFE MODE HELPERS =====

// Backoff with jitter
static uint32_t jitter_ms(uint32_t base) {
    int32_t j = (int32_t)base / 10;
    int32_t r = (int32_t)(esp_random() % (2*j+1)) - j; // ±10%
    return (uint32_t)((int32_t)base + r);
}

static void reset_backoff() { 
    s_backoff_ms = 1000; 
}

static void retry_timer_cb(TimerHandle_t) { 
    BUS.publish(Event{TOPIC_RETRY_RESOLVE, 0, nullptr}); 
}

static void schedule_reconnect() {
    if (!t_retry) {
        t_retry = xTimerCreate("retry", pdMS_TO_TICKS(1000), pdFALSE, nullptr, retry_timer_cb);
    }
    uint32_t wait = jitter_ms(s_backoff_ms);
    s_backoff_ms = std::min<uint32_t>(s_backoff_ms * 2u, 32000u);
    xTimerChangePeriod(t_retry, pdMS_TO_TICKS(wait), 0);
    xTimerStart(t_retry, 0);
}

// Safe mode functions
static void enter_safe_mode() {
    DeviceMonitor::allOutputsSafe(); // PWM=0, GPIO LOW, stop playback
    SystemStateManager::setSafe(true);
    if (MqttClient::isConnected()) {
        MqttClient::publish(get_mqtt_safe_topic(), "{\"safe\":true}", 1, /*retain=*/true);
    }
}

static void exit_safe_mode() {
    SystemStateManager::setSafe(false);
    if (MqttClient::isConnected()) {
        MqttClient::publish(get_mqtt_safe_topic(), "{\"safe\":false}", 1, /*retain=*/true);
    }
}

// Persist broker IP in NVS
static void persist_broker_ip_if_any(const char* ip) {
    if (!ip) return;
    nvs_handle_t h;
    if (nvs_open("net", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "broker_ip", ip);
        nvs_commit(h);
        nvs_close(h);
    }
}

// Subscription management
static void begin_subscriptions() {
    std::vector<std::string> subs = { get_mqtt_control_topic() /*, ...*/ };
    s_subs_pending = (int)subs.size();
    for (auto& t : subs) {
        if (MqttClient::subscribe(t, 1)) {
            BUS.publish(Event{TOPIC_MQTT_SUBSCRIBED, 0, nullptr});
        } else {
            // Treat failure as still pending; recovery flow will retry via SYSTEM_ERROR/MDNS/backoff
            BUS.publish(Event{TOPIC_SYSTEM_ERROR, 2, nullptr});
        }
    }
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
                const char* host = static_cast<const char*>(e.ptr);
                ESP_LOGI(TAG, "mDNS found MQTT broker: %s", host ? host : "<null>");
                SystemStateManager::updateMdnsState(true);
                if (host) {
                    SystemStateManager::updateBrokerInfo(host, MQTT_BROKER_PORT);
                }
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
                }
            }
            return; // Already logged above
            
        case TOPIC_PIN_SET:
            event_name = "Pin Set";
            {
                ESP_LOGI(TAG, "Pin set event - Pin: %d, Value: %d", e.i32, static_cast<int>(e.u64));
            }
            return; // Already logged above
            
        case TOPIC_PIN_READ:
            event_name = "Pin Read";
            {
                ESP_LOGI(TAG, "Pin read event - Pin: %d, Value: %d", e.i32, static_cast<int>(e.u64));
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
        char* host = strdup("10.0.0.161");
        *out = host;
        return true;
    }
    
    if (results == nullptr) {
        char* host = strdup("10.0.0.161");
        *out = host;
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
                    char* ip_address = strdup(r->hostname);
                    *out = ip_address;
                    
                    mdns_query_results_free(results);
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
                        
                        // Allocate and copy IP address
                        char* ip_address = strdup(ip_str);
                        *out = ip_address;
                        
                        mdns_query_results_free(results);
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
                            
                            // Allocate and copy IP address
                            char* ip_address = strdup(ip_str);
                            *out = ip_address;
                            
                            mdns_query_results_free(results);
                            return true;
                        } else {
                            // Fallback to hostname with .local suffix
                            std::string full_hostname = std::string(r->hostname) + ".local";
                            char* hostname = strdup(full_hostname.c_str());
                            *out = hostname;
                            
                            mdns_query_results_free(results);
                            return true;
                        }
                    }
                }
            }
        }
        r = r->next;
    }
    
    // If no matching service found, use fallback to known broker IP
    char* host = strdup("10.0.0.161");
    mdns_query_results_free(results);
    *out = host;
    return true;
}

// MQTT connection worker that receives hostname from event - pure execution only
static bool mqtt_connection_worker_with_event(const Event& trigger_event, void** out) {
    const char* host = static_cast<const char*>(trigger_event.ptr);
    ESP_LOGI("MQTT_WORKER_EVENT", "Attempting MQTT connection to host from event: %s", host ? host : "NULL");
    
    if (!host) {
        ESP_LOGE("MQTT_WORKER_EVENT", "No hostname provided in event");
        return false;
    }
    
    // Create MQTT connection data
    MqttConnectionData connection_data(host, MQTT_BROKER_PORT, get_esp32_device_id());
    
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
    // TODO: convert ptr payloads to typed fields (avoid reinterpret_cast)
}

// ===== WIFI EVENT HANDLERS =====

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        // Publish WiFi disconnected event
        BUS.publish(Event{TOPIC_WIFI_DISCONNECTED, 0, nullptr});
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        (void)event_data; // Unused but required by interface
        ESP_LOGI("WIFI_EVENT", "Got IP address! Setting WIFI_CONNECTED_BIT");
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
        BUS.publish(Event{TOPIC_WIFI_CONNECTED, 0, nullptr});
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
    // Create fallback connection data
    MqttConnectionData fallback_data(MQTT_BROKER_HOST, MQTT_BROKER_PORT, get_esp32_device_id());
    auto result = MqttClient::connect(fallback_data);
    
    if (result.success) {
        std::string control_topic = get_mqtt_control_topic();
        if (MqttClient::subscribe(control_topic, 0)) {
            // Publish success event
            Event success_event{TOPIC_MQTT_CONNECTED, 0, nullptr};
            BUS.publish(success_event);
        } else {
            // Publish error event
            Event error_event{TOPIC_SYSTEM_ERROR, 7, nullptr};
            BUS.publish(error_event);
        }
    } else {
        // Publish error event
        Event error_event{TOPIC_SYSTEM_ERROR, 7, nullptr};
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
                // Publish success event
                Event success_event{TOPIC_STATUS_PUBLISH_SUCCESS, 0, nullptr};
                BUS.publish(success_event);
            } else {
                // Publish error event
                Event error_event{TOPIC_SYSTEM_ERROR, 8, nullptr};
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
            // onOk → publish MDNS_FOUND with hostname
            [](const Event& e, IEventBus& bus) {
                const char* hostname = static_cast<const char*>(e.ptr);
                char* hostname_copy = hostname ? strdup(hostname) : nullptr;
                bus.publish(Event{TOPIC_MDNS_FOUND, 0, hostname_copy, free});
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

    // Flow 2.5: When MQTT connects, publish online status, reset backoff, and begin subscriptions
    G.when(TOPIC_MQTT_CONNECTED,
        FlowGraph::tap([](const Event& e) {
            reset_backoff();
            MqttClient::publish(get_mqtt_status_topic(),
                               SystemStateManager::createOnlineJson(),
                               /*qos=*/1, /*retain=*/true);
            begin_subscriptions();
        })
    );

    // Flow 2.6: Each successful subscription decrements pending count; when zero => LINK_READY
    G.when(TOPIC_MQTT_SUBSCRIBED,
        FlowGraph::tap([](const Event& e) {
            if (s_subs_pending > 0 && --s_subs_pending == 0) {
                BUS.publish(Event{TOPIC_LINK_READY, 0, nullptr});
            }
        })
    );

    // Flow 2.7: Optional: auto-exit Safe Mode when link is ready
    G.when(TOPIC_LINK_READY,
        FlowGraph::tap([](const Event& e) {
#if SAFE_AUTO_EXIT_ON_CONNECT
            if (SystemStateManager::isSafe()) {
                exit_safe_mode();
                BUS.publish(Event{TOPIC_SAFE_MODE_EXIT, 0, nullptr});
            }
#endif
        })
    );

    // Flow 3: Retry driver – when asked to retry, run resolve again
    G.when(TOPIC_RETRY_RESOLVE,
        G.async_blocking("mdns-query", mdns_query_worker,
            [](const Event& e, IEventBus& bus) {
                const char* host = static_cast<const char*>(e.ptr);
                bus.publish(Event{TOPIC_MDNS_FOUND, 0, host ? strdup(host) : nullptr, free});
            },
            FlowGraph::publish(TOPIC_MDNS_FAILED)
        )
    );

    // Flow 4: mDNS success – cache IP, reset backoff, then connect
    G.when(TOPIC_MDNS_FOUND,
        FlowGraph::seq(
            FlowGraph::tap([](const Event& e) {
                const char* ip = static_cast<const char*>(e.ptr);
                persist_broker_ip_if_any(ip);
                reset_backoff();
                BUS.publish(Event{TOPIC_BROKER_PERSISTED, 0, nullptr});
            }),
            G.async_blocking_with_event("mqtt-connect", mqtt_connection_worker_with_event,
                FlowGraph::publish(TOPIC_MQTT_CONNECTED, 0, nullptr),
                FlowGraph::seq(
                    FlowGraph::publish(TOPIC_MQTT_DISCONNECTED, 0, nullptr),
                    FlowGraph::publish(TOPIC_SYSTEM_ERROR, 6, nullptr)
                )
            )
        )
    );

    // Flow 5: mDNS failed – backoff + retry
    G.when(TOPIC_MDNS_FAILED,
        FlowGraph::tap([](const Event& e) {
            schedule_reconnect();
        })
    );

    // Flow 6: Disconnected – enter safe mode and schedule retry
    G.when(TOPIC_MQTT_DISCONNECTED,
        FlowGraph::tap([](const Event& e) {
            enter_safe_mode();
            schedule_reconnect();
        })
    );

    // ===== REGISTER CENTRALIZED LOGGING HANDLER =====
    // Subscribe to all events for centralized logging
    BUS.subscribe(centralized_logging_handler, nullptr, MASK_ALL);

    // Flow 4: When MQTT message received, parse it into device commands
    G.when(TOPIC_MQTT_MESSAGE, 
        FlowGraph::tap([](const Event& e) {
            auto* msg_data = static_cast<MqttMessageData*>(e.ptr);
            if (!msg_data) {
                BUS.publish(Event{TOPIC_SYSTEM_ERROR, 1, nullptr});
                return;
            }
            
            // Parse message to device commands (pure function)
            auto device_command_result = MessageProcessor::processMessageToDeviceCommands(msg_data->payload);
            
            if (!device_command_result.success) {
                BUS.publish(Event{TOPIC_SYSTEM_ERROR, 3, nullptr});
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
                        BUS.publish(Event{TOPIC_PIN_SET, device_cmd.pin, pin_cmd_data, [](void* ptr) { delete static_cast<PinCommandData*>(ptr); }});
                        break;
                    }
                    case PIN_READ: {
                        auto* pin_cmd_data = new PinCommandData{
                            device_cmd.pin,
                            0, // Read doesn't have a value to set
                            device_cmd.description
                        };
                        BUS.publish(Event{TOPIC_PIN_READ, device_cmd.pin, pin_cmd_data, [](void* ptr) { delete static_cast<PinCommandData*>(ptr); }});
                        break;
                    }
                    case PIN_MODE: {
                        auto* pin_cmd_data = new PinCommandData{
                            device_cmd.pin,
                            device_cmd.value, // Mode value
                            device_cmd.description
                        };
                        BUS.publish(Event{TOPIC_PIN_MODE, device_cmd.pin, pin_cmd_data, [](void* ptr) { delete static_cast<PinCommandData*>(ptr); }});
                        break;
                    }
                    case DEVICE_STATUS: {
                        BUS.publish(Event{TOPIC_DEVICE_STATUS, 0, nullptr});
                        break;
                    }
                    case DEVICE_RESET: {
                        BUS.publish(Event{TOPIC_DEVICE_RESET, 0, nullptr});
                        break;
                    }
                    default: {
                        BUS.publish(Event{TOPIC_SYSTEM_ERROR, 4, nullptr});
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
            BUS.publish(Event{TOPIC_SYSTEM_ERROR, 5, nullptr});
            return;
        }
        
        DevicePinCommand device_cmd(cmdType, pin_cmd_data->pin, pin_cmd_data->value, pin_cmd_data->description);
        auto result = DeviceMonitor::executeDeviceCommand(device_cmd);
        
        if (result.success) {
            BUS.publish(Event{TOPIC_PIN_SUCCESS, result.pin, reinterpret_cast<void*>(result.value)});
        } else {
            BUS.publish(Event{TOPIC_SYSTEM_ERROR, 7, nullptr});
        }
        
        // Note: pin_cmd_data will be freed by the event bus destructor
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

    // Flow 8: Critical command paths – success exits safe mode; failure enters
    G.when(TOPIC_PIN_SUCCESS,
        FlowGraph::tap([](const Event& e) {
            if (SystemStateManager::isSafe()) {
                exit_safe_mode();
                BUS.publish(Event{TOPIC_SAFE_MODE_EXIT, 0, nullptr});
            }
        })
    );

    G.when(TOPIC_SYSTEM_ERROR,
        FlowGraph::tap([](const Event& e) {
            if (e.i32 == 4) { // device command exec failed
                enter_safe_mode();
                BUS.publish(Event{TOPIC_SAFE_MODE_ENTER, 0, nullptr});
            }
        })
    );

    // Flow 9: On-demand status publish
    G.when(TOPIC_DEVICE_STATUS,
        FlowGraph::tap([](const Event& e) {
            if (!MqttClient::isConnected()) return;
            auto js = SystemStateManager::createDeviceStatusJson();
            MqttClient::publish(get_mqtt_status_topic(), js, 1);
        })
    );

    // ===== REGISTER LEGACY EVENT HANDLERS =====
    BUS.subscribe(fallback_mqtt_connection_handler, nullptr, bit(TOPIC_MDNS_FAILED));
    BUS.subscribe(timer_execution_handler, nullptr, bit(TOPIC_TIMER));

    // Initialize WiFi
    wifi_init_sta();



    // ===== EVENT-DRIVEN MAIN LOOP =====
    ESP_LOGI(TAG, "EventBus system running...");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY_MS));
        
        // Publish periodic timer event with current uptime
        uint64_t current_time = getCurrentUptimeSeconds();
        Event timer_event{TOPIC_TIMER, 0, nullptr};
        timer_event.u64 = current_time;
        BUS.publish(timer_event);
    }
}