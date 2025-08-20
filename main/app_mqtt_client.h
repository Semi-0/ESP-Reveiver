#ifndef APP_MQTT_CLIENT_H
#define APP_MQTT_CLIENT_H

#include "mqtt_client.h"  // ESP-IDF MQTT client header
#include "esp_event.h"
#include <functional>
#include <string>

class MQTTClient {
public:
    using MessageCallback = std::function<void(const std::string&)>;
    
    static void init();                              // Use config defaults
    static void init(const std::string& host, int port); // Use discovered host/port
    static void start();
    static void stop();
    static bool is_connected();
    static bool is_connecting();
    static void publish_status();
    static void set_message_callback(MessageCallback callback);
    static std::string get_broker_host();
    static int get_broker_port();
    
    // New methods for explicit subscription management
    static bool subscribe_to_topic(const std::string& topic, int qos = 0);
    static std::string get_subscribed_topic();
    static void debug_status();

private:
    static esp_mqtt_client_handle_t client;
    static MessageCallback message_callback;
    static std::string broker_host;
    static int broker_port;
    static std::string broker_uri;
    static bool connected;
    static std::string subscribed_topic;

    static void configure_and_register();
    static void mqtt_event_handler(void *handler_args, esp_event_base_t base, 
                                  int32_t event_id, void *event_data);
};

#endif // APP_MQTT_CLIENT_H
