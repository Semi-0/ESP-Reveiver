#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

// Include our modular components
#include "config.h"
#include "wifi_manager.h"
#include "app_mqtt_client.h"
#include "mdns_service.h"
#include "message_processor.h"

// UDP socket
int udp_socket = -1;


void write_income_message(const std::string& message) {
    // Parse the message into commands (pure function)
    auto parse_result = MessageProcessor::parse_json_message(message);
    
    if (!parse_result.success) {
        ESP_LOGW(APP_TAG, "Failed to parse message: %s", parse_result.error_message.c_str());
        return;
    }
    
    // Execute the commands (pure function with explicit results)
    auto execution_results = MessageProcessor::execute_commands(parse_result.commands);
    
    // Log the results (side effect separated from logic)
    for (const auto& result : execution_results) {
        if (result.success) {
            ESP_LOGI(APP_TAG, "Command executed: %s", result.action_description.c_str());
        } else {
            ESP_LOGE(APP_TAG, "Command failed: %s", result.error_message.c_str());
        }
    }
}

// Main application entry point
extern "C" void app_main(void) {
    ESP_LOGI(APP_TAG, "=== ESP32 MQTT Receiver Starting ===");
    ESP_LOGI(APP_TAG, "Device ID: %s", get_esp32_device_id().c_str());
    ESP_LOGI(APP_TAG, "Machine ID: %s", MACHINE_ID.c_str());
    ESP_LOGI(APP_TAG, "SSID: %s", WIFI_SSID);
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    ESP_LOGI(APP_TAG, "Initializing WiFi...");
    WiFiManager::init();
    WiFiManager::start();
    
    // Wait for WiFi connection with retries
    int wifi_retry_count = 0;
    const int MAX_WIFI_RETRIES = 10;
    
    while (wifi_retry_count < MAX_WIFI_RETRIES) {
        if (WiFiManager::wait_for_connection()) {
            ESP_LOGI(APP_TAG, "WiFi connected successfully!");
            break;
        }
        
        wifi_retry_count++;
        if (wifi_retry_count < MAX_WIFI_RETRIES) {
            vTaskDelay(pdMS_TO_TICKS(5000)); // Wait 5 seconds before retry
        }
    }
    
    if (wifi_retry_count >= MAX_WIFI_RETRIES) {
        ESP_LOGE(APP_TAG, "Failed to connect to WiFi after %d attempts. Continuing anyway...", MAX_WIFI_RETRIES);
        // Don't return - continue with setup and let WiFi connect in background
    }

    // Setup mDNS
    ESP_LOGI(APP_TAG, "Setting up mDNS...");
    MDNSService::init();
    MDNSService::start();
    
    // Discover MQTT broker via mDNS (fallback to config if not found)
    std::string discovered_host; int discovered_port = 0;
    bool found = MDNSService::discover_mqtt(discovered_host, discovered_port);
    
    // Validate discovered host (must be IP address, not hostname)
    if (found) {
        // Check if discovered_host is an IP address (contains dots and no letters)
        bool is_ip = true;
        for (char c : discovered_host) {
            if (c != '.' && (c < '0' || c > '9')) {
                is_ip = false;
                break;
            }
        }
        if (!is_ip) {
            found = false;
        }
    }

    // Initialize and start MQTT client
    if (found) {
        MQTTClient::init(discovered_host, discovered_port);
    } else {
        MQTTClient::init();
    }
    MQTTClient::set_message_callback(write_income_message);
    MQTTClient::start();

    ESP_LOGI(APP_TAG, "=== Setup Complete ===");
    ESP_LOGI(APP_TAG, "Ready to receive MQTT messages on topic: %s", get_mqtt_control_topic().c_str());

        // Main loop with MQTT retry mechanism and broker memory
    static int status_counter = 0;
    static int mqtt_retry_counter = 0;
    static uint32_t last_mqtt_check = 0;
    const int MQTT_RETRY_INTERVAL = 30000; // 30 seconds
    const int MAX_MQTT_RETRIES = 5;
    
    // Broker memory system
    static std::vector<std::string> failed_brokers;
    static std::string last_working_broker;
    static int last_working_port = 0;
    
    while (1) {
        uint32_t current_time = esp_timer_get_time() / 1000; // Current time in ms
        
        // Check MQTT connection status and retry if needed
        if (!MQTTClient::is_connected() && 
            (current_time - last_mqtt_check) > MQTT_RETRY_INTERVAL) {
            
            mqtt_retry_counter++;
            last_mqtt_check = current_time;
            
            ESP_LOGW(APP_TAG, "MQTT disconnected, retry attempt %d/%d", mqtt_retry_counter, MAX_MQTT_RETRIES);
            
            if (mqtt_retry_counter <= MAX_MQTT_RETRIES) {
                // Stop current MQTT client
                MQTTClient::stop();
                
                // Get current broker info for failure tracking
                std::string current_broker = MQTTClient::get_broker_host();
                int current_port = MQTTClient::get_broker_port();
                
                // Add current broker to failed list if it's not empty
                if (!current_broker.empty()) {
                    std::string broker_key = current_broker + ":" + std::to_string(current_port);
                    if (std::find(failed_brokers.begin(), failed_brokers.end(), broker_key) == failed_brokers.end()) {
                        failed_brokers.push_back(broker_key);
                        ESP_LOGW(APP_TAG, "Added broker to failed list: %s", broker_key.c_str());
                    }
                }
                
                // Try to find a working broker
                std::string discovered_host; 
                int discovered_port = 0;
                bool found = false;
                
                // First, try mDNS discovery for new brokers
                ESP_LOGI(APP_TAG, "Rediscovering MQTT broker via mDNS...");
                found = MDNSService::discover_mqtt(discovered_host, discovered_port);
                
                if (found) {
                    std::string broker_key = discovered_host + ":" + std::to_string(discovered_port);
                    
                    // Check if this broker was already tried and failed
                    if (std::find(failed_brokers.begin(), failed_brokers.end(), broker_key) != failed_brokers.end()) {
                        ESP_LOGW(APP_TAG, "Skipping previously failed broker: %s", broker_key.c_str());
                        found = false;
                    } else {
                        ESP_LOGI(APP_TAG, "Found new broker via mDNS: %s:%d", discovered_host.c_str(), discovered_port);
                    }
                }
                
                // If mDNS failed or found a previously failed broker, try last working broker
                if (!found && !last_working_broker.empty()) {
                    ESP_LOGI(APP_TAG, "Trying last known working broker: %s:%d", last_working_broker.c_str(), last_working_port);
                    discovered_host = last_working_broker;
                    discovered_port = last_working_port;
                    found = true;
                }
                
                // If still no broker found, use configured broker as fallback
                if (!found) {
                    ESP_LOGW(APP_TAG, "No working broker found, using configured broker as fallback");
                    MQTTClient::init();
                    MQTTClient::set_message_callback(write_income_message);
                    MQTTClient::start();
                } else {
                    ESP_LOGI(APP_TAG, "Connecting to broker: %s:%d", discovered_host.c_str(), discovered_port);
                    MQTTClient::init(discovered_host, discovered_port);
                    MQTTClient::set_message_callback(write_income_message);
                    MQTTClient::start();
                }
            } else {
                ESP_LOGE(APP_TAG, "Max MQTT retries reached, clearing failed broker list and stopping retry attempts");
                failed_brokers.clear(); // Clear failed list to allow retrying all brokers
                mqtt_retry_counter = 0; // Reset for next time
            }
        }
        
        // Reset retry counter and remember working broker if MQTT is connected
        if (MQTTClient::is_connected() && mqtt_retry_counter > 0) {
            ESP_LOGI(APP_TAG, "MQTT reconnected successfully!");
            
            // Remember this working broker
            std::string working_broker = MQTTClient::get_broker_host();
            int working_port = MQTTClient::get_broker_port();
            if (!working_broker.empty()) {
                last_working_broker = working_broker;
                last_working_port = working_port;
                ESP_LOGI(APP_TAG, "Remembered working broker: %s:%d", last_working_broker.c_str(), last_working_port);
                
                // Remove from failed list if it was there
                std::string broker_key = working_broker + ":" + std::to_string(working_port);
                auto it = std::find(failed_brokers.begin(), failed_brokers.end(), broker_key);
                if (it != failed_brokers.end()) {
                    failed_brokers.erase(it);
                    ESP_LOGI(APP_TAG, "Removed broker from failed list: %s", broker_key.c_str());
                }
            }
            
            mqtt_retry_counter = 0;
        }

        // Publish status every 10 seconds (only if connected)
        if (++status_counter >= 1000) { // 10 seconds at 10ms delay
            if (MQTTClient::is_connected()) {
                MQTTClient::publish_status();
            }
            status_counter = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}