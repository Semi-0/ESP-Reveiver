#include "eventbus/TinyEventBus.h"
#include "eventbus/EventProtocol.h"
#include "eventbus/FlowGraph.h"
#include "config.h"
#include "data_structures.h"
#include "message_processor.h"
#include "device_monitor.h"
#include "mqtt_client.h"
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

static const char* TAG = "EVENTBUS_MAIN";

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
                ESP_LOGI(TAG, "Pin set event - Pin: %d, Value: %d", e.i32, static_cast<int>(e.ptr));
            }
            return; // Already logged above
            
        case TOPIC_PIN_READ:
            event_name = "Pin Read";
            {
                ESP_LOGI(TAG, "Pin read event - Pin: %d, Value: %d", e.i32, static_cast<int>(e.ptr));
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
    // Query for MQTT service
    mdns_result_t* results = nullptr;
    esp_err_t err = mdns_query_ptr(MDNS_SERVICE_TYPE, MDNS_PROTOCOL, MDNS_QUERY_TIMEOUT_MS, MDNS_MAX_RESULTS, &results);
    
    if (err != ESP_OK || results == nullptr) {
        *out = nullptr;
        return false;
    }
    
    // Find first result with IP address
    for (int i = 0; i < results->num; i++) {
        if (results->instances[i].ip_protocol == MDNS_IP_PROTOCOL_V4) {
            char* host = strdup(results->instances[i].hostname);
            mdns_query_results_free(results);
            *out = host;
            return true;
        }
    }
    
    mdns_query_results_free(results);
    *out = nullptr;
    return false;
}

// MQTT connection worker - pure execution only
static bool mqtt_connection_worker(void** out) {
    const char* host = static_cast<const char*>(*out);
    if (!host) {
        return false;
    }
    
    // Create MQTT connection data
    MqttConnectionData connection_data(host, MQTT_BROKER_PORT, get_esp32_device_id());
    
    // Connect to MQTT
    auto result = MqttClient::connect(connection_data);
    
    if (result.success) {
        // Subscribe to control topic
        std::string control_topic = get_mqtt_control_topic();
        if (MqttClient::subscribe(control_topic, 0)) {
            // Store connection data for later use
            auto* conn_data = new MqttConnectionData(connection_data);
            *out = conn_data;
            return true;
        } else {
            return false;
        }
    } else {
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
        // Publish WiFi disconnected event
        BUS.publish(Event{TOPIC_WIFI_DISCONNECTED, 0, nullptr});
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
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

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
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

void mqtt_message_execution_handler(const Event& e, void* user) {
    auto* msg_data = static_cast<MqttMessageData*>(e.ptr);
    if (!msg_data) return;
    
    // Parse message and convert to device commands (pure function)
    auto device_command_result = MessageProcessor::processMessageToDeviceCommands(msg_data->payload);
    
    if (!device_command_result.success) {
        // Publish error event
        Event error_event{TOPIC_SYSTEM_ERROR, 3, nullptr};
        BUS.publish(error_event);
        return;
    }
    
    // Execute device commands (pure function)
    auto execution_results = DeviceMonitor::executeDeviceCommands(device_command_result.commands);
    
    // Publish results as events
    for (const auto& result : execution_results) {
        if (result.success) {
            if (result.description.find("Set") != std::string::npos) {
                Event pin_set_event{TOPIC_PIN_SET, result.pin, reinterpret_cast<void*>(result.value)};
                BUS.publish(pin_set_event);
            } else if (result.description.find("Read") != std::string::npos) {
                Event pin_read_event{TOPIC_PIN_READ, result.pin, reinterpret_cast<void*>(result.value)};
                BUS.publish(pin_read_event);
            }
        } else {
            Event error_event{TOPIC_SYSTEM_ERROR, 4, nullptr};
            BUS.publish(error_event);
        }
    }
}

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

    // Initialize mDNS
    ESP_ERROR_CHECK(mdns_init());

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
            FlowGraph::publish(TOPIC_MDNS_FOUND),
            // onErr → publish MDNS_FAILED and system error
            FlowGraph::seq(
                FlowGraph::publish(TOPIC_MDNS_FAILED),
                FlowGraph::publish(TOPIC_SYSTEM_ERROR, 5, nullptr)
            )
        )
    );

    // Flow 2: When mDNS succeeds, connect to MQTT
    G.when(TOPIC_MDNS_FOUND,
        G.async_blocking("mqtt-connect", mqtt_connection_worker,
            // onOk → publish MQTT_CONNECTED
            FlowGraph::publish(TOPIC_MQTT_CONNECTED, 1, nullptr),
            // onErr → publish MQTT_DISCONNECTED and system error
            FlowGraph::seq(
                FlowGraph::publish(TOPIC_MQTT_DISCONNECTED, 0, nullptr),
                FlowGraph::publish(TOPIC_SYSTEM_ERROR, 6, nullptr)
            )
        )
    );

    // Flow 3: When mDNS fails, use fallback MQTT broker
    G.when(TOPIC_MDNS_FAILED,
        FlowGraph::publish(TOPIC_MDNS_FAILED)
    );

    // ===== REGISTER CENTRALIZED LOGGING HANDLER =====
    // Subscribe to all events for centralized logging
    BUS.subscribe(centralized_logging_handler, nullptr, MASK_ALL);

    // ===== REGISTER EXECUTION EVENT HANDLERS =====
    BUS.subscribe(fallback_mqtt_connection_handler, nullptr, bit(TOPIC_MDNS_FAILED));
    BUS.subscribe(mqtt_message_execution_handler, nullptr, bit(TOPIC_MQTT_MESSAGE));
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
            BUS.publish(Event{TOPIC_WIFI_CONNECTED, 0, nullptr});
            break;
        } else if (bits & WIFI_FAIL_BIT) {
            BUS.publish(Event{TOPIC_SYSTEM_ERROR, 1, nullptr});
            break;
        }
    }

    // ===== EVENT-DRIVEN MAIN LOOP =====
    ESP_LOGI(TAG, "EventBus system running...");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY_MS));
        
        // Publish periodic timer event with current uptime
        uint64_t current_time = getCurrentUptimeSeconds();
        BUS.publish(Event{TOPIC_TIMER, static_cast<int32_t>(current_time), nullptr});
    }
}
