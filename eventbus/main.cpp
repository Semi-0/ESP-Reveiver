#include "eventbus/TinyEventBus.h"
#include "eventbus/EventProtocol.h"
#include "eventbus/FlowGraph.h"
#include "mqtt_client.h"
#include "data_structures.h"
#include "logging_handler.h"
#include <stdio.h>
#include <string.h>
#include <string>
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
#include "esp_efuse.h"

static const char* TAG = "EVENTBUS_MAIN";

// Global event bus
static TinyEventBus BUS;

// WiFi event group
static EventGroupHandle_t s_wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT = BIT1;

// WiFi configuration
#define WIFI_SSID "SD & FM"
#define WIFI_PASS "your_wifi_password"

// MQTT configuration
#define MQTT_DEFAULT_PORT 1883
#define MQTT_SUBSCRIBE_TOPIC "esp32/commands"
#define MQTT_PUBLISH_TOPIC "esp32/status"

// Function to generate unique client ID from ESP32 MAC address
std::string generate_client_id() {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    
    char client_id[32];
    snprintf(client_id, sizeof(client_id), "esp32_%02x%02x%02x%02x%02x%02x", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    return std::string(client_id);
}

// Optional logging configuration - can be enabled/disabled without affecting core logic
#define ENABLE_LOGGING 1

// Pure mDNS query worker - no logging
static bool mdns_query_worker(void** out) {
    #if ENABLE_LOGGING
    LoggingHandler::log_mdns_query_start();
    #endif
    
    // Query for MQTT service
    mdns_result_t* results = nullptr;
    esp_err_t err = mdns_query_ptr("_mqtt", "_tcp", 3000, 20, &results);
    
    if (err != ESP_OK || results == nullptr) {
        #if ENABLE_LOGGING
        LoggingHandler::log_mdns_query_failure(esp_err_to_name(err));
        #endif
        *out = nullptr;
        return false;
    }
    
    // Find first result with IP address
    for (int i = 0; i < results->num; i++) {
        if (results->instances[i].ip_protocol == MDNS_IP_PROTOCOL_V4) {
            char* host = strdup(results->instances[i].hostname);
            #if ENABLE_LOGGING
            LoggingHandler::log_mdns_query_success(host);
            #endif
            mdns_query_results_free(results);
            *out = host;
            return true;
        }
    }
    
    mdns_query_results_free(results);
    #if ENABLE_LOGGING
    LoggingHandler::log_mdns_no_broker_found();
    #endif
    *out = nullptr;
    return false;
}

// Pure MQTT connection worker - no logging
static bool mqtt_connect_worker(void** out) {
    const char* host = static_cast<const char*>(*out);
    if (!host) {
        return false;
    }
    
    #if ENABLE_LOGGING
    LoggingHandler::log_mqtt_connection_attempt(host);
    #endif
    
    // Create MQTT connection data with dynamic client ID
    std::string client_id = generate_client_id();
    MqttConnectionData connection_data(host, MQTT_DEFAULT_PORT, client_id);
    
    #if ENABLE_LOGGING
    ESP_LOGI("LOGGING", "Generated client ID: %s", client_id.c_str());
    #endif
    
    // Set event bus for MQTT client
    MqttClient::setEventBus(&BUS);
    
    // Connect to MQTT broker
    auto result = MqttClient::connect(connection_data);
    if (!result.success) {
        #if ENABLE_LOGGING
        LoggingHandler::log_mqtt_connection_failure(result.error_message);
        #endif
        return false;
    }
    
    #if ENABLE_LOGGING
    LoggingHandler::log_mqtt_connection_success();
    #endif
    
    // Subscribe to commands topic
    if (!MqttClient::subscribe(MQTT_SUBSCRIBE_TOPIC, 1)) {
        #if ENABLE_LOGGING
        LoggingHandler::log_mqtt_subscription_failure(MQTT_SUBSCRIBE_TOPIC);
        #endif
        return false;
    }
    
    #if ENABLE_LOGGING
    LoggingHandler::log_mqtt_subscription_success(MQTT_SUBSCRIBE_TOPIC);
    #endif
    
    // Set message callback to publish to event bus
    MqttClient::setMessageCallback([](const MqttMessageData& message) {
        // Create a copy of the message data for the event
        auto* msg_data = new MqttMessageData(message);
        Event message_event{TOPIC_MQTT_MESSAGE, 0, msg_data};
        BUS.publish(message_event);
    });
    
    return true;
}

// Pure WiFi event handler - no logging
static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        #if ENABLE_LOGGING
        LoggingHandler::log_wifi_disconnect();
        #endif
        esp_wifi_connect();
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        #if ENABLE_LOGGING
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        char ip_str[16];
        sprintf(ip_str, IPSTR, IP2STR(&event->ip_info.ip));
        LoggingHandler::log_wifi_got_ip(ip_str);
        #endif
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Pure WiFi initialization - no logging
static void wifi_init_sta() {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    #if ENABLE_LOGGING
    LoggingHandler::log_wifi_init_complete();
    #endif
}

// Pure event handlers - core logic only
void wifi_connected_handler(const Event& e, void* user) {
    // Core logic: WiFi is connected, ready for next step
    // No logging - side effects handled separately
}

void mdns_found_handler(const Event& e, void* user) {
    const char* host = static_cast<const char*>(e.ptr);
    // Core logic: mDNS found a broker, ready for MQTT connection
    // No logging - side effects handled separately
}

void mdns_failed_handler(const Event& e, void* user) {
    // Core logic: mDNS failed, should use fallback
    // No logging - side effects handled separately
}

void mqtt_connected_handler(const Event& e, void* user) {
    // Core logic: MQTT connected, publish initial status
    std::string status_msg = "{\"status\":\"online\",\"device\":\"esp32_eventbus\"}";
    bool success = MqttClient::publish(MQTT_PUBLISH_TOPIC, status_msg, 1);
    
    #if ENABLE_LOGGING
    if (success) {
        LoggingHandler::log_mqtt_publish_success(MQTT_PUBLISH_TOPIC);
    } else {
        LoggingHandler::log_mqtt_publish_failure(MQTT_PUBLISH_TOPIC);
    }
    #endif
}

void mqtt_message_handler(const Event& e, void* user) {
    const MqttMessageData* msg_data = static_cast<const MqttMessageData*>(e.ptr);
    if (msg_data) {
        // Core logic: Process the message and send acknowledgment
        std::string ack_msg = "{\"received\":\"" + msg_data->payload + "\"}";
        MqttClient::publish("esp32/ack", ack_msg, 1);
        
        // Clean up the message data
        delete msg_data;
    }
}

void mqtt_disconnected_handler(const Event& e, void* user) {
    // Core logic: MQTT disconnected, handle reconnection if needed
    // No logging - side effects handled separately
}

void system_error_handler(const Event& e, void* user) {
    // Core logic: System error occurred, handle recovery
    // No logging - side effects handled separately
}

// Pure status publishing function
std::string create_status_message(int uptime_seconds) {
    return "{\"uptime\":" + std::to_string(uptime_seconds) + ",\"status\":\"running\"}";
}

// Pure timer event handler
void timer_handler(const Event& e, void* user) {
    // Core logic: Timer tick, publish status if MQTT connected
    if (MqttClient::isConnected()) {
        std::string status_msg = create_status_message(e.i32 * 10);
        MqttClient::publish(MQTT_PUBLISH_TOPIC, status_msg, 1);
    }
}

// Main application
extern "C" void app_main(void) {
    #if ENABLE_LOGGING
    LoggingHandler::log_eventbus_start();
    #endif
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize mDNS
    ESP_ERROR_CHECK(mdns_init());

    // Start the event bus
    if (!BUS.begin("event-dispatch", 4096, tskIDLE_PRIORITY + 2)) {
        return;
    }

    // Build declarative flows
    FlowGraph G(BUS);

    // Flow 1: When WiFi connects, do mDNS lookup
    G.when(TOPIC_WIFI_CONNECTED,
        G.async_blocking("mdns-query", mdns_query_worker,
            FlowGraph::publish(TOPIC_MDNS_FOUND),
            FlowGraph::publish(TOPIC_MDNS_FAILED)
        )
    );

    // Flow 2: When mDNS succeeds, connect to MQTT
    G.when(TOPIC_MDNS_FOUND,
        G.async_blocking("mqtt-connect", mqtt_connect_worker,
            FlowGraph::publish(TOPIC_MQTT_CONNECTED, 1, nullptr),
            FlowGraph::publish(TOPIC_MQTT_DISCONNECTED, 0, nullptr)
        )
    );

    // Flow 3: When mDNS fails, use fallback
    G.when(TOPIC_MDNS_FAILED,
        // Fallback MQTT connection logic can be added here
    );

    // Flow 4: When MQTT connects, start device operations
    G.when(TOPIC_MQTT_CONNECTED,
        FlowGraph::seq(
            // Device operations can be added here
        )
    );

    // Subscribe to events for core logic
    BUS.subscribe(wifi_connected_handler, nullptr, bit(TOPIC_WIFI_CONNECTED));
    BUS.subscribe(mdns_found_handler, nullptr, bit(TOPIC_MDNS_FOUND));
    BUS.subscribe(mdns_failed_handler, nullptr, bit(TOPIC_MDNS_FAILED));
    BUS.subscribe(mqtt_connected_handler, nullptr, bit(TOPIC_MQTT_CONNECTED));
    BUS.subscribe(mqtt_disconnected_handler, nullptr, bit(TOPIC_MQTT_DISCONNECTED));
    BUS.subscribe(mqtt_message_handler, nullptr, bit(TOPIC_MQTT_MESSAGE));
    BUS.subscribe(system_error_handler, nullptr, bit(TOPIC_SYSTEM_ERROR));
    BUS.subscribe(timer_handler, nullptr, bit(TOPIC_TIMER));

    // Optional: Subscribe to logging handlers (only if logging is enabled)
    #if ENABLE_LOGGING
    BUS.subscribe(LoggingHandler::log_wifi_connected, nullptr, bit(TOPIC_WIFI_CONNECTED));
    BUS.subscribe(LoggingHandler::log_mdns_found, nullptr, bit(TOPIC_MDNS_FOUND));
    BUS.subscribe(LoggingHandler::log_mdns_failed, nullptr, bit(TOPIC_MDNS_FAILED));
    BUS.subscribe(LoggingHandler::log_mqtt_connected, nullptr, bit(TOPIC_MQTT_CONNECTED));
    BUS.subscribe(LoggingHandler::log_mqtt_disconnected, nullptr, bit(TOPIC_MQTT_DISCONNECTED));
    BUS.subscribe(LoggingHandler::log_mqtt_message, nullptr, bit(TOPIC_MQTT_MESSAGE));
    BUS.subscribe(LoggingHandler::log_system_error, nullptr, bit(TOPIC_SYSTEM_ERROR));
    BUS.subscribe(LoggingHandler::log_timer_tick, nullptr, bit(TOPIC_TIMER));
    #endif

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

    // Keep the system running
    #if ENABLE_LOGGING
    LoggingHandler::log_eventbus_running();
    #endif
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        
        // Publish periodic timer event
        static int timer_counter = 0;
        BUS.publish(Event{TOPIC_TIMER, timer_counter++, nullptr});
    }
}
