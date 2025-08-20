#ifndef MQTT_PURE_H
#define MQTT_PURE_H

#include <string>
#include <vector>
#include <functional>

// MQTT configuration
struct MqttConfig {
    std::string broker_host;
    int broker_port;
    std::string client_id;
    std::string username;
    std::string password;
    int keepalive_interval;
    int connect_timeout;
    bool clean_session;
    
    MqttConfig(const std::string& host = "localhost", int port = 1883, 
               const std::string& id = "esp32_client", const std::string& user = "", 
               const std::string& pass = "", int keepalive = 60, int timeout = 10, bool clean = true)
        : broker_host(host), broker_port(port), client_id(id), username(user), password(pass),
          keepalive_interval(keepalive), connect_timeout(timeout), clean_session(clean) {}
};

// MQTT message
struct MqttMessage {
    std::string topic;
    std::string payload;
    int qos;
    bool retained;
    
    MqttMessage(const std::string& t = "", const std::string& p = "", int q = 0, bool ret = false)
        : topic(t), payload(p), qos(q), retained(ret) {}
};

// MQTT connection result
struct MqttResult {
    bool success;
    std::string error_message;
    int error_code;
    int retry_count;
    
    MqttResult(bool s, const std::string& err = "", int code = 0, int retries = 0)
        : success(s), error_message(err), error_code(code), retry_count(retries) {}
    
    static MqttResult success_result() {
        return MqttResult(true);
    }
    
    static MqttResult failure_result(const std::string& error, int code = 0, int retries = 0) {
        return MqttResult(false, error, code, retries);
    }
};

// MQTT publish result
struct MqttPublishResult {
    bool success;
    int message_id;
    std::string error_message;
    
    MqttPublishResult(bool s, int msg_id = 0, const std::string& err = "")
        : success(s), message_id(msg_id), error_message(err) {}
    
    static MqttPublishResult success_result(int msg_id = 0) {
        return MqttPublishResult(true, msg_id);
    }
    
    static MqttPublishResult failure_result(const std::string& error) {
        return MqttPublishResult(false, 0, error);
    }
};

// MQTT subscription result
struct MqttSubscribeResult {
    bool success;
    std::string topic;
    int qos;
    std::string error_message;
    
    MqttSubscribeResult(bool s, const std::string& t = "", int q = 0, const std::string& err = "")
        : success(s), topic(t), qos(q), error_message(err) {}
    
    static MqttSubscribeResult success_result(const std::string& topic, int qos = 0) {
        return MqttSubscribeResult(true, topic, qos);
    }
    
    static MqttSubscribeResult failure_result(const std::string& topic, const std::string& error) {
        return MqttSubscribeResult(false, topic, 0, error);
    }
};

// Message callback function type
typedef std::function<void(const MqttMessage&)> MessageCallback;

// Pure function declarations
namespace MqttPure {
    
    // Initialize MQTT client
    MqttResult initialize(const MqttConfig& config);
    
    // Connect to MQTT broker
    MqttResult connect(const MqttConfig& config);
    
    // Disconnect from MQTT broker
    MqttResult disconnect();
    
    // Check if connected
    bool isConnected();
    
    // Subscribe to topic
    MqttSubscribeResult subscribe(const std::string& topic, int qos = 0);
    
    // Unsubscribe from topic
    MqttResult unsubscribe(const std::string& topic);
    
    // Publish message
    MqttPublishResult publish(const MqttMessage& message);
    
    // Publish simple message
    MqttPublishResult publish(const std::string& topic, const std::string& payload, int qos = 0, bool retained = false);
    
    // Set message callback
    void setMessageCallback(MessageCallback callback);
    
    // Get current broker info
    std::string getBrokerHost();
    int getBrokerPort();
    
    // Get client ID
    std::string getClientId();
    
    // Validate MQTT configuration
    bool validateConfig(const MqttConfig& config);
    
    // Test connection to broker
    MqttResult testConnection(const MqttConfig& config);
    
    // Cleanup MQTT resources
    void cleanup();
}

#endif // MQTT_PURE_H
