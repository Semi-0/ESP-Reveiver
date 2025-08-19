#include "mdns_service.h"
#include "config.h"
#include "esp_log.h"
#include "mdns.h"
#include <cstring>

void MDNSService::init() {
    // Initialize mDNS
    esp_err_t err = mdns_init();
    if (err) {
        ESP_LOGE(APP_TAG, "MDNS Init failed: %d", err);
        return;
    }
}

void MDNSService::start() {
    std::string mdns_name = "esp32-controller-" + MACHINE_ID;
    
    // Set hostname
    mdns_hostname_set(mdns_name.c_str());
    
    // Set default instance
    mdns_instance_name_set("ESP32 MQTT Controller");

    setup_services();
    
    ESP_LOGI(APP_TAG, "mDNS responder started: %s.local", mdns_name.c_str());
}

void MDNSService::stop() { mdns_free(); }

void MDNSService::setup_services() {
    // Add HTTP service
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    mdns_service_instance_name_set("_http", "_tcp", "ESP32 Web Server");

    // Add UDP service
    mdns_service_add(NULL, "_udp", "_udp", UDP_LOCAL_PORT, NULL, 0);
    mdns_service_instance_name_set("_udp", "_udp", "ESP32 UDP Server");

    // Add MQTT service
    mdns_service_add(NULL, "_mqtt", "_tcp", MQTT_PORT, NULL, 0);
    mdns_service_instance_name_set("_mqtt", "_tcp", "ESP32 MQTT Broker");

    // Add TXT records for additional information
    mdns_txt_item_t serviceTxtData[4] = {
        {"board", "esp32"},
        {"machine_id", MACHINE_ID.c_str()},
        {"version", "1.0"},
        {"status", "online"}
    };
    mdns_service_txt_set("_http", "_tcp", serviceTxtData, 4);
}

bool MDNSService::discover_mqtt(std::string& host_out, int& port_out) {
    ESP_LOGI(APP_TAG, "Starting mDNS discovery for MQTT broker...");
    mdns_result_t *results = nullptr;
    esp_err_t err = mdns_query_ptr("_mqtt", "_tcp", 1000 /* ms - reduced timeout */, 10, &results);
    ESP_LOGI(APP_TAG, "mDNS query completed with result: %d", (int)err);
    if (err != ESP_OK || results == nullptr) {
        ESP_LOGW(APP_TAG, "mDNS: no MQTT PTR results (%d)", (int)err);
        return false;
    }

    // Try all results until one succeeds
    mdns_result_t* r = results;
    int attempt = 0;
    while (r) {
        attempt++;
        ESP_LOGI(APP_TAG, "Trying MQTT broker %d: %s:%d", attempt, r->hostname ? r->hostname : "unknown", r->port);
        
        if (r->hostname && r->port) {
            // Try to resolve the IP address directly from mDNS result
            if (r->addr && r->addr->addr.type == ESP_IPADDR_TYPE_V4) {
                char ip_str[16];
                esp_ip4addr_ntoa(&r->addr->addr.u_addr.ip4, ip_str, sizeof(ip_str));
                host_out.assign(ip_str);
                port_out = r->port;
                mdns_query_results_free(results);
                ESP_LOGI(APP_TAG, "✓ mDNS discovered MQTT: %s:%d (IP: %s)", r->hostname, port_out, host_out.c_str());
                return true;
            }
            
            // Fallback: query A record for the hostname
            esp_ip4_addr_t ip_addr;
            esp_err_t a_err = mdns_query_a(r->hostname, 2000, &ip_addr);
            if (a_err == ESP_OK) {
                char ip_str[16];
                esp_ip4addr_ntoa(&ip_addr, ip_str, sizeof(ip_str));
                host_out.assign(ip_str);
                port_out = r->port;
                mdns_query_results_free(results);
                ESP_LOGI(APP_TAG, "✓ mDNS discovered MQTT: %s:%d (IP: %s via A record)", r->hostname, port_out, host_out.c_str());
                return true;
            }
            
            ESP_LOGW(APP_TAG, "✗ Could not resolve IP for hostname %s, trying next...", r->hostname);
        }
        r = r->next;
    }

    mdns_query_results_free(results);
    ESP_LOGW(APP_TAG, "mDNS: no usable MQTT SRV record found after trying %d results", attempt);
    return false;
}
