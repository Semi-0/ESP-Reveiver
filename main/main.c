#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "mdns.h"
#include "mqtt_client.h"
// Simple pin register to track configured pins
std::set<int> configuredPins;

using Logger = std::function<void(const std::string&)>;

// Function to configure pin only if not already configured
void configurePinIfNeeded(int pin, int mode, Logger logger) {
    if (configuredPins.find(pin) == configuredPins.end()) {
        pinMode(pin, mode);
        configuredPins.insert(pin);
        logger("Configured pin %d as %s\n", pin, mode == OUTPUT ? "OUTPUT" : "INPUT");
        
        // Small delay after first configuration to ensure pin is ready
        delay(1);
    }
}

// WiFi settings
const char* ssid = "V2_ Lab";
const char* password = "end-of-file";
const int localPort = 12345;
const std::string myMachineId = "2";
const IPAddress MULTICAST_GROUP(239, 255, 255, 250);

// MQTT settings
const char* mqtt_server = "192.168.1.100";  // Change to your MQTT broker IP
const int mqtt_port = 1883;
const char* mqtt_username = "";  // Add if your broker requires authentication
const char* mqtt_password = "";  // Add if your broker requires authentication

// MQTT topics
const char* mqtt_status_topic = "esp32/status";
const char* mqtt_control_topic = "esp32/control";
const char* mqtt_response_topic = "esp32/response";

// mDNS settings

WiFiUDP udp;
char incomingPacket[255];

// MQTT client
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// Function declarations
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_reconnect();
void setup_mdns();
void publish_status();

// Function declarations
void processMessage(const std::string& message);
std::vector<std::string> split(const std::string& s, char delimiter);

template<typename F>
auto function_wrapper(F func, F before, F after) {
    return [=](auto... args) {
        before(args...);
        func(args...);
        after(args...);
    };
}

template<typename F>
auto logger_wrapper(Logger logger, F func, std::string before, std::string after) {
    return [=](auto... args) {
        return function_wrapper(
            func, 
        [=](auto... args) {
            logger(before);
        }, 
        [=](auto... args) {
            logger(after);
        })(args...);
    };
}


char* get_mqtt_client_id(char* machine_id) {
    return strcat("esp32-controller-", machine_id);
}

// MQTT callback function - handles incoming MQTT messages
void mqtt_callback(char* topic, byte* payload, unsigned int length) {    
    // Convert payload to string
    std::string message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    // Process the message similar to UDP messages
    processMessage(message);
}

// MQTT reconnection function
void mqtt_reconnect(PubSubClient mqtt_client) {
    while (!mqtt_client.connected()) {
        Serial.print("Attempting MQTT connection...");
        
        // Create a random client ID
        String clientId = String(mqtt_client_id);
        clientId += String(random(0xffff), HEX);
        
        // Attempt to connect
        if (mqtt_client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
            Serial.println("connected");
            
            // Subscribe to control topic
            mqtt_client.subscribe(mqtt_control_topic);
            Serial.printf("Subscribed to: %s\n", mqtt_control_topic);
            
            // Publish initial status
            publish_status();
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqtt_client.state());
            Serial.println(" retrying in 5 seconds");
            delay(5000);
        }
    }
}


char* get_mdns_name(char* machine_id) {
    return strcat("esp32-controller-", machine_id);
}
// Setup mDNS
void setup_mdns() {
    std::string mdns_name = get_mdns_name(myMachineId);
    if (MDNS.begin(mdns_name.c_str())) {
        Serial.printf("mDNS responder started: %s.local\n", mdns_name.c_str());
        
        // Add service
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("udp", "udp", localPort);
        MDNS.addService("mqtt", "tcp", mqtt_port);
    } else {
        Serial.println("Error setting up mDNS responder");
    }
}

// Publish status to MQTT
void publish_status() {
    String status = "{\"machine_id\":\"" + String(myMachineId.c_str()) + 
                   "\",\"ip\":\"" + WiFi.localIP().toString() + 
                   "\",\"rssi\":" + String(WiFi.RSSI()) + 
                   ",\"configured_pins\":" + String(configuredPins.size()) + 
                   ",\"status\":\"online\"}";
    
    mqtt_client.publish(mqtt_status_topic, status.c_str());
    Serial.printf("Status published: %s\n", status.c_str());
}

std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }

    return result;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=== ESP32 UDP Controller Starting ===");
    Serial.printf("Machine ID: %s\n", myMachineId.c_str());
    Serial.printf("SSID: %s\n", ssid);
    
    // WiFi setup
    WiFi.begin(ssid, password);
    
    // Disable WiFi sleep mode to maintain stable connection
    WiFi.setSleep(false);
    Serial.println("WiFi sleep mode disabled");
    
    Serial.print("Connecting to WiFi...");
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" connected!");
        Serial.printf("ESP32 IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("WiFi RSSI: %d dBm\n", WiFi.RSSI());
    } else {
        Serial.println(" FAILED to connect!");
        Serial.println("Please check WiFi credentials");
        Serial.println("Continuing without WiFi...");
    }
    
    // Start UDP
    if (udp.begin(localPort)) {
        Serial.printf("UDP server started on port %d\n", localPort);
    } else {
        Serial.println("Failed to start UDP server");
    }

       // Join multicast group
    if (udp.beginMulticast(MULTICAST_GROUP, localPort)) {
        Serial.println("Successfully joined multicast group 239.255.255.250");
    } else {
        Serial.println("Failed to join multicast group");
    }
    
    // Setup mDNS
    setup_mdns();
    
    // Setup MQTT
    mqtt_client.setServer(mqtt_server, mqtt_port);
    mqtt_client.setCallback(mqtt_callback);
    
    Serial.println("=== Setup Complete ===");
    Serial.println("Ready to receive UDP and MQTT messages...");
    Serial.println("Pins will be configured on-demand (only once per pin)");
}

void loop() {
    // Handle MQTT connection and messages
    if (!mqtt_client.connected()) {
        mqtt_reconnect();
    }
    mqtt_client.loop();
    
    // Add periodic status messages and MQTT status publishing
    static unsigned long lastStatus = 0;
    if (millis() - lastStatus > 10000) { // Every 10 seconds
        Serial.printf("Status: WiFi=%s, IP=%s, Configured pins: %d\n", 
                     WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected",
                     WiFi.localIP().toString().c_str(),
                     configuredPins.size());
        
        // Publish status to MQTT
        if (mqtt_client.connected()) {
            publish_status();
        }
        
        lastStatus = millis();
    }
    
    int packetSize = udp.parsePacket();
    if (packetSize) {
        int len = udp.read(incomingPacket, sizeof(incomingPacket) - 1);
        if (len > 0) {
            incomingPacket[len] = '\0';
            Serial.printf("Received %d bytes: %s\n", len, incomingPacket);
            processMessage(std::string(incomingPacket));
        }
    }

    delay(10);
}

void processMessage(const std::string& message) {
    // message: DIG|1|13|HIGH;ANA|ESP32_001|13|100; MOTOR|0|4|16|17|80
    auto messages = split(message, ';');
    for (const auto& msg : messages) {
        if (msg.empty()) continue;

        auto parts = split(msg, '|');
        if (parts.size() < 4) {
            Serial.printf("Invalid message format: %s\n", msg.c_str());
            continue;
        }

        std::string header = parts[0];
        std::string machineId = parts[1];

        if (machineId != myMachineId) {
            Serial.printf("Machine ID mismatch: expected=%s, got=%s\n", 
                         myMachineId.c_str(), machineId.c_str());
            continue;
        }

        if (header == "DIG") {
            if (parts.size() != 4) continue;
            int pin = atoi(parts[2].c_str());
            std::string valueStr = parts[3];
            
            // Configure pin on-demand (only once)
            configurePinIfNeeded(pin, OUTPUT);
            
            bool highValue = (valueStr == "HIGH" || valueStr == "high" || valueStr == "1");
            digitalWrite(pin, highValue ? HIGH : LOW);
            Serial.printf("Digital write: pin %d = %s\n", pin, highValue ? "HIGH" : "LOW");
        } else if (header == "ANA") {
            if (parts.size() != 4) continue;
            int pin = atoi(parts[2].c_str());
            int val = atoi(parts[3].c_str());
            
            // Configure pin on-demand (only once)
            configurePinIfNeeded(pin, OUTPUT);
            
            analogWrite(pin, val);
            Serial.printf("Analog write: pin %d = %d\n", pin, val);
        } else if (header == "MOTOR") {
            // Motor control: MOTOR|0|speed|in1|in2|speed_val
            if (parts.size() >= 6) {
                int speed = atoi(parts[2].c_str());
                int in1 = atoi(parts[3].c_str());
                int in2 = atoi(parts[4].c_str());
                int speedVal = atoi(parts[5].c_str());
                
                // Configure all motor pins on-demand (only once each)
                configurePinIfNeeded(speed, OUTPUT);
                configurePinIfNeeded(in1, OUTPUT);
                configurePinIfNeeded(in2, OUTPUT);
                
                // Set direction
                digitalWrite(in1, HIGH);
                digitalWrite(in2, LOW);
                
                // Set speed
                analogWrite(speed, speedVal);
                
                Serial.printf("Motor control: speed=%d, in1=%d, in2=%d, speed_val=%d\n", 
                             speed, in1, in2, speedVal);
            } else {
                Serial.printf("Invalid motor command format. Expected: MOTOR|id|speed|in1|in2|speed_val\n");
            }
        } else {
            Serial.printf("Unknown header: %s\n", header.c_str());
        }
    }
}