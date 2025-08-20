#include "mqtt_client.h"
#include "config.h"
#include "eventbus/EventProtocol.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_mqtt_client.h"
#include <cstring>

static const char* TAG = "MQTT_CLIENT";

// Static member initialization
IEventBus* MqttClient::event_bus_ = nullptr;
std::function<void(const MqttMessageData&)> MqttClient::message_callback_ = nullptr;
std::string MqttClient::current_broker_;
int MqttClient::current_port_ = 0;
bool MqttClient::connected_ = false;

// ESP-IDF MQTT client handle
static esp_mqtt_client_handle_t mqtt_client = nullptr;

// Pure MQTT event handler - no logging
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            MqttClient::connected_ = true;
            
            // Publish MQTT connected event
            if (MqttClient::event_bus_) {
                Event connected_event{TOPIC_MQTT_CONNECTED, 1, nullptr};
                MqttClient::event_bus_->publish(connected_event);
            }
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            MqttClient::connected_ = false;
            
            // Publish MQTT disconnected event
            if (MqttClient::event_bus_) {
                Event disconnected_event{TOPIC_MQTT_DISCONNECTED, 0, nullptr};
                MqttClient::event_bus_->publish(disconnected_event);
            }
            break;
            
        case MQTT_EVENT_DATA:
            // Create message data
            MqttMessageData message_data;
            message_data.topic = std::string(event->topic, event->topic_len);
            message_data.payload = std::string(event->data, event->data_len);
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
            
        case MQTT_EVENT_ERROR:
            MqttClient::connected_ = false;
            
            // Publish MQTT error event
            if (MqttClient::event_bus_) {
                Event error_event{TOPIC_SYSTEM_ERROR, 2, nullptr};
                MqttClient::event_bus_->publish(error_event);
            }
            break;
            
        default:
            break;
    }
}

MqttConnectionResult MqttClient::initialize(const MqttConnectionData& connection_data) {
    if (mqtt_client != nullptr) {
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = nullptr;
    }
    
    // Configure MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {};
    
    // Build URI with port
    std::string uri = "mqtt://" + connection_data.broker_host + ":" + std::to_string(connection_data.broker_port);
    mqtt_cfg.broker.address.uri = uri.c_str();
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    mqtt_cfg.credentials.client_id = connection_data.client_id.c_str();
    
    // Set connection timeout
    mqtt_cfg.session.keepalive = 60;
    mqtt_cfg.session.disable_clean_session = 0;
    mqtt_cfg.session.last_will.topic = nullptr;
    mqtt_cfg.session.last_will.msg = nullptr;
    mqtt_cfg.session.last_will.qos = 1;
    mqtt_cfg.session.last_will.retain = 0;
    
    // Create MQTT client
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == nullptr) {
        return MqttConnectionResult(false, "Failed to initialize MQTT client", -1);
    }
    
    // Register event handler
    esp_err_t err = esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, nullptr);
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
    esp_err_t err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK) {
        return MqttConnectionResult(false, "Failed to start MQTT client", err);
    }
    
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
    
    esp_err_t err = esp_mqtt_client_subscribe(mqtt_client, topic.c_str(), qos);
    if (err != ESP_OK) {
        return false;
    }
    
    return true;
}

bool MqttClient::publish(const std::string& topic, const std::string& message, int qos) {
    if (!isConnected()) {
        return false;
    }
    
    esp_err_t err = esp_mqtt_client_publish(mqtt_client, topic.c_str(), message.c_str(), message.length(), qos, 0);
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

