#include "mqtt_client_local.h"
#include "config.h"
#include "eventbus/EventProtocol.h"
#include "esp_log.h"
#include <cstring>
#include "esp_event.h"
#include "mqtt_client.h"
#include <algorithm>

static const char* TAG = "MQTT_CLIENT";

// Static member initialization
IEventBus* MqttClient::event_bus_ = nullptr;
std::function<void(const MqttMessageData&)> MqttClient::message_callback_ = nullptr;
std::string MqttClient::current_broker_;
int MqttClient::current_port_ = 0;
bool MqttClient::connected_ = false;

// ESP-IDF MQTT client handle
static esp_mqtt_client_handle_t mqtt_client = nullptr;

// MQTT event handler with detailed logging
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    // Log event id only to avoid incorrect formatting of base
    ESP_LOGI(TAG, "MQTT Event received: event_id=%d", event_id);
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED: Successfully connected to broker");
            MqttClient::connected_ = true;
            
            // Publish MQTT connected event
            if (MqttClient::event_bus_) {
                Event connected_event{TOPIC_MQTT_CONNECTED, 1, nullptr};
                MqttClient::event_bus_->publish(connected_event);
            }
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED: Disconnected from broker");
            MqttClient::connected_ = false;
            
            // Publish MQTT disconnected event
            if (MqttClient::event_bus_) {
                Event disconnected_event{TOPIC_MQTT_DISCONNECTED, 0, nullptr};
                MqttClient::event_bus_->publish(disconnected_event);
            }
            break;
            
        case MQTT_EVENT_DATA: {
            // Safely construct topic/payload strings using explicit lengths
            std::string topic(event->topic, event->topic_len);
            std::string payload(event->data, event->data_len);

            ESP_LOGI(TAG, "MQTT_EVENT_DATA: Received message on topic: %s", topic.c_str());
            ESP_LOGI(TAG, "MQTT_EVENT_DATA: Message length: %d", event->data_len);
            ESP_LOGI(TAG, "MQTT_EVENT_DATA: Message payload: %s", payload.c_str());

            // Create message data
            MqttMessageData message_data;
            message_data.topic = topic;
            message_data.payload = payload;
            message_data.qos = event->qos;
            
            // Call message callback if set
            if (MqttClient::message_callback_) {
                MqttClient::message_callback_(message_data);
            }
            
            // Publish MQTT message event
            if (MqttClient::event_bus_) {
                auto* msg_data = new MqttMessageData(message_data);
                Event message_event{TOPIC_MQTT_MESSAGE, 0, msg_data};
                MqttClient::event_bus_->publish(message_event);
            }
            break;
        }
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR: MQTT error occurred");
            MqttClient::connected_ = false;
            
            // Publish MQTT error event
            if (MqttClient::event_bus_) {
                Event error_event{TOPIC_SYSTEM_ERROR, 2, nullptr};
                MqttClient::event_bus_->publish(error_event);
            }
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED: Successfully subscribed to topic");
            
            // Publish MQTT subscribed event
            if (MqttClient::event_bus_) {
                Event subscribed_event{TOPIC_MQTT_SUBSCRIBED, 1, nullptr};
                MqttClient::event_bus_->publish(subscribed_event);
            }
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED: Successfully unsubscribed from topic");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED: Message published successfully");
            break;
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT: About to connect to broker");
            break;
        case MQTT_EVENT_DELETED:
            ESP_LOGI(TAG, "MQTT_EVENT_DELETED: MQTT client deleted");
            break;
        case MQTT_EVENT_ANY:
            ESP_LOGI(TAG, "MQTT_EVENT_ANY: Any MQTT event");
            break;
        case MQTT_USER_EVENT:
            ESP_LOGI(TAG, "MQTT_USER_EVENT: User-defined MQTT event");
            break;
        default:
            ESP_LOGW(TAG, "MQTT Unknown event: %d", event->event_id);
            break;
    }
}

MqttConnectionResult MqttClient::initialize(const MqttConnectionData& connection_data) {
    if (mqtt_client != nullptr) {
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = nullptr;
    }
    
    // Configure MQTT client - try simpler configuration
    esp_mqtt_client_config_t mqtt_cfg = {};
    
    // Build URI with port - store it as a static string to keep it alive
    static std::string uri_storage;
    uri_storage = "mqtt://" + connection_data.broker_host + ":" + std::to_string(connection_data.broker_port);
    ESP_LOGI(TAG, "Initializing MQTT client with URI: %s", uri_storage.c_str());
    ESP_LOGI(TAG, "Client ID: %s", connection_data.client_id.c_str());
    
    // Use minimal configuration
    mqtt_cfg.broker.address.uri = uri_storage.c_str();
    mqtt_cfg.credentials.client_id = connection_data.client_id.c_str();
    
    ESP_LOGI(TAG, "Using minimal MQTT configuration");
    
    // Create MQTT client
    ESP_LOGI(TAG, "About to call esp_mqtt_client_init with config:");
    ESP_LOGI(TAG, "  URI: %s", mqtt_cfg.broker.address.uri);
    ESP_LOGI(TAG, "  Transport: %d", mqtt_cfg.broker.address.transport);
    ESP_LOGI(TAG, "  Client ID: %s", mqtt_cfg.credentials.client_id);
    ESP_LOGI(TAG, "  Keepalive: %d", mqtt_cfg.session.keepalive);
    ESP_LOGI(TAG, "  Network timeout: %d", mqtt_cfg.network.timeout_ms);
    
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == nullptr) {
        ESP_LOGE(TAG, "esp_mqtt_client_init returned NULL - this usually means memory allocation failed or invalid config");
        ESP_LOGE(TAG, "Check if URI format is correct: %s", mqtt_cfg.broker.address.uri);
        ESP_LOGE(TAG, "Check if client ID is valid: %s", mqtt_cfg.credentials.client_id);
        return MqttConnectionResult(false, "Failed to initialize MQTT client - esp_mqtt_client_init returned NULL", -1);
    }
    ESP_LOGI(TAG, "MQTT client initialized successfully, handle: %p", mqtt_client);
    
    // Register event handler
    esp_err_t err = esp_mqtt_client_register_event(mqtt_client, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), mqtt_event_handler, nullptr);
    if (err != ESP_OK) {
        return MqttConnectionResult(false, "Failed to register MQTT event handler", err);
    }
    
    // Store connection data
    current_broker_ = connection_data.broker_host;
    current_port_ = connection_data.broker_port;
    
    return MqttConnectionResult(true);
}

MqttConnectionResult MqttClient::connect(const MqttConnectionData& connection_data) {
    if (mqtt_client == nullptr) {
        auto init_result = initialize(connection_data);
        if (!init_result.success) {
            return init_result;
        }
    }
    
    // Start MQTT client
    ESP_LOGI(TAG, "Starting MQTT client with handle: %p", mqtt_client);
    esp_err_t err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_mqtt_client_start failed with error: %s (code: %d)", esp_err_to_name(err), err);
        ESP_LOGE(TAG, "This usually means the client couldn't connect to the broker");
        return MqttConnectionResult(false, "Failed to start MQTT client", err);
    }
    ESP_LOGI(TAG, "MQTT client started successfully - connection attempt initiated");
    
    return MqttConnectionResult(true);
}

void MqttClient::disconnect() {
    if (mqtt_client != nullptr) {
        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = nullptr;
    }
    connected_ = false;
}

bool MqttClient::isConnected() {
    return connected_ && mqtt_client != nullptr;
}

bool MqttClient::subscribe(const std::string& topic, int qos) {
    if (!isConnected()) {
        return false;
    }
    ESP_LOGI(TAG, "Subscribing to topic: %s", topic.c_str());
    esp_err_t err = esp_mqtt_client_subscribe(mqtt_client, topic.c_str(), qos);

    if (err != ESP_OK) {
        return false;
    }
    
    return true;
}

bool MqttClient::publish(const std::string& topic, const std::string& message, int qos, bool retain) {
    if (!isConnected()) {
        return false;
    }
    
    // Truncate noisy payload logs unless DEBUG
    std::string log_message = message;
    if (log_message.length() > MAX_PAYLOAD_LOG_LENGTH) {
        log_message = log_message.substr(0, MAX_PAYLOAD_LOG_LENGTH) + "...";
    }
    
    ESP_LOGI(TAG, "Publishing to topic: %s, message: %s, qos: %d, retain: %s", 
             topic.c_str(), log_message.c_str(), qos, retain ? "true" : "false");
    
    esp_err_t err = esp_mqtt_client_publish(mqtt_client, topic.c_str(), message.c_str(), message.length(), qos, retain ? 1 : 0);
    if (err != ESP_OK) {
        return false;
    }
    
    return true;
}

void MqttClient::setMessageCallback(std::function<void(const MqttMessageData&)> callback) {
    message_callback_ = callback;
}

std::string MqttClient::getCurrentBroker() {
    return current_broker_;
}

int MqttClient::getCurrentPort() {
    return current_port_;
}

void MqttClient::setEventBus(IEventBus* event_bus) {
    event_bus_ = event_bus;
}

