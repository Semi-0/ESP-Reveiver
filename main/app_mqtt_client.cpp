#include "app_mqtt_client.h"
#include "config.h"
#include "esp_log.h"
#include "esp_event.h"
#include "pin_controller.h"

// Static state for subscription tracking
static int g_control_sub_msg_id = -1;
static int g_test_sub_msg_id = -1;
static bool g_control_ready = false;
static bool g_test_ready = false;

esp_mqtt_client_handle_t MQTTClient::client = NULL;
MQTTClient::MessageCallback MQTTClient::message_callback = nullptr;
std::string MQTTClient::broker_host = "";
int MQTTClient::broker_port = 0;
std::string MQTTClient::broker_uri = "";
bool MQTTClient::connected = false;
std::string MQTTClient::subscribed_topic = "";

void MQTTClient::configure_and_register() {
    esp_mqtt_client_config_t mqtt_cfg = {};
    const char* host = broker_host.empty() ? MQTT_SERVER : broker_host.c_str();
    int port = broker_port == 0 ? MQTT_PORT : broker_port;
    broker_uri.clear();
    broker_uri = std::string("mqtt://") + host + ":" + std::to_string(port);
    mqtt_cfg.broker.address.uri = broker_uri.c_str();
    
    // Set client ID to the ESP32 device ID
    static std::string client_id = get_esp32_device_id();
    mqtt_cfg.credentials.client_id = client_id.c_str();
    
    // Don't set username/password if they're empty
    if (strlen(MQTT_USERNAME) > 0) {
        mqtt_cfg.credentials.username = MQTT_USERNAME;
    }
    if (strlen(MQTT_PASSWORD) > 0) {
        mqtt_cfg.credentials.authentication.password = MQTT_PASSWORD;
    }
    
    // Set additional MQTT configuration
    mqtt_cfg.session.keepalive = 60;
    mqtt_cfg.session.disable_clean_session = 0;
    mqtt_cfg.session.protocol_ver = MQTT_PROTOCOL_V_3_1_1;
    mqtt_cfg.session.last_will.topic = nullptr;
    mqtt_cfg.session.last_will.msg = nullptr;
    mqtt_cfg.session.last_will.qos = 0;
    mqtt_cfg.session.last_will.retain = 0;

    ESP_LOGI(APP_TAG, "Configuring MQTT client for: %s", broker_uri.c_str());

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(APP_TAG, "Failed to initialize MQTT client!");
        return;
    }
    
    esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
}

void MQTTClient::init() {
    broker_host.clear();
    broker_port = 0;
    configure_and_register();
}

void MQTTClient::init(const std::string& host, int port) {
    broker_host = host;
    broker_port = port;
    configure_and_register();
}

void MQTTClient::start() { 
    connected = false; 
    esp_mqtt_client_start(client);
}

void MQTTClient::stop() {
    if (client) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = NULL;
    }
}

bool MQTTClient::is_connected() { return connected; }

bool MQTTClient::is_connecting() { 
    return client != NULL && !connected; 
}

std::string MQTTClient::get_broker_host() { 
    return broker_host; 
}

int MQTTClient::get_broker_port() { 
    return broker_port; 
}

void MQTTClient::publish_status() {
    if (!is_connected()) return;
    std::string status = std::string("{") +
        "\"machine_id\":\"" + MACHINE_ID + "\"," +
        "\"device_id\":\"" + get_esp32_device_id() + "\"," +
        "\"configured_pins\":" + std::to_string(PinController::get_configured_pins_count()) + "," +
        "\"status\":\"online\"}";
    esp_mqtt_client_publish(client, get_mqtt_status_topic().c_str(), status.c_str(), status.length(), 0, 0);
}

void MQTTClient::set_message_callback(MessageCallback callback) { message_callback = callback; }

bool MQTTClient::subscribe_to_topic(const std::string& topic, int qos) {
    if (!is_connected()) {
        return false;
    }
    
    int result = esp_mqtt_client_subscribe(client, topic.c_str(), qos);
    
    if (result == -1) {
        ESP_LOGE(APP_TAG, "Failed to subscribe to topic: %s", topic.c_str());
        return false;
    } else {
        // Store the topic for tracking (but don't overwrite control topic)
        if (topic.find("/control") != std::string::npos) {
            subscribed_topic = topic; // This is the control topic
        } else if (subscribed_topic.empty()) {
            subscribed_topic = topic; // Fallback if no control topic yet
        }
        return true;
    }
}

std::string MQTTClient::get_subscribed_topic() {
    return subscribed_topic;
}

void MQTTClient::debug_status() {
    ESP_LOGI(APP_TAG, "MQTT: Connected=%s, Control=%s, Test=%s", 
             connected ? "YES" : "NO",
             g_control_ready ? "YES" : "NO", 
             g_test_ready ? "YES" : "NO");
}

void MQTTClient::mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED: {
            ESP_LOGI(APP_TAG, "MQTT connected to broker");
            connected = true;
            
            // Reset subscription state
            g_control_ready = false;
            g_test_ready = false;
            
            // Subscribe to control topic with QoS 1
            std::string control_topic = get_mqtt_control_topic();
            g_control_sub_msg_id = esp_mqtt_client_subscribe(client, control_topic.c_str(), 1);
            
            // Subscribe to test topic with QoS 1
            g_test_sub_msg_id = esp_mqtt_client_subscribe(client, "test/topic", 1);
            
            // Optional wildcard subscription for debugging
            #ifdef MQTT_DEBUG_WILDCARD_SUB
            esp_mqtt_client_subscribe(client, "#", 0);
            #endif
            
            // Publish status immediately
            publish_status();
            break;
        }
            
        case MQTT_EVENT_DATA: {
            // Handle chunked MQTT payloads properly
            static std::string acc;  // accumulates current payload
            static std::string tbuf; // topic buffer

            if (event->current_data_offset == 0) {
                acc.clear();
                tbuf.assign(event->topic, event->topic_len);
            }
            acc.append(event->data, event->data_len);

            const int end_offset = event->current_data_offset + event->data_len;
            if (end_offset == event->total_data_len) {
                ESP_LOGI(APP_TAG, "MQTT message received: %s", acc.c_str());
                if (message_callback) {
                    message_callback(acc);
                } else {
                    ESP_LOGE(APP_TAG, "No message callback set!");
                }
            }
            break;
        }
            
        case MQTT_EVENT_DISCONNECTED: {
            ESP_LOGI(APP_TAG, "MQTT disconnected from broker");
            connected = false;
            subscribed_topic.clear();
            g_control_ready = false;
            g_test_ready = false;
            break;
        }
            
        case MQTT_EVENT_SUBSCRIBED: {
            if (event->msg_id == g_control_sub_msg_id) {
                g_control_ready = true;
                ESP_LOGI(APP_TAG, "Control topic subscription confirmed");
                
                // Self-test AFTER control SUBACK
                const std::string test_message = R"({"type":"digital","pin":13,"value":1})";
                esp_mqtt_client_publish(client, get_mqtt_control_topic().c_str(),
                                       test_message.c_str(), test_message.size(), 1, 0);
            } else if (event->msg_id == g_test_sub_msg_id) {
                g_test_ready = true;
            }
            break;
        }
            
        case MQTT_EVENT_UNSUBSCRIBED: {
            break;
        }
            
        case MQTT_EVENT_PUBLISHED: {
            break;
        }
            
        case MQTT_EVENT_ERROR: {
            ESP_LOGE(APP_TAG, "MQTT error occurred");
            break;
        }
            
        default: {
            break;
        }
    }
}
