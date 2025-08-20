#include "config.h"
#include "esp_system.h"
#include "esp_log.h"
#include <sstream>
#include <iomanip>

static const char* TAG = "CONFIG";

// Generate unique device ID from MAC address
std::string get_esp32_device_id() {
    uint8_t mac[6];
    esp_err_t ret = esp_efuse_mac_get_default(mac);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get MAC address");
        return "unknown_device";
    }
    
    std::stringstream ss;
    ss << MQTT_CLIENT_ID_PREFIX;
    for (int i = 0; i < 6; i++) {
        if (i > 0) ss << "_";
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(mac[i]);
    }
    return ss.str();
}

// Get MQTT control topic with device ID
std::string get_mqtt_control_topic() {
    return get_esp32_device_id() + "/control";
}

// Get MQTT status topic with device ID
std::string get_mqtt_status_topic() {
    return get_esp32_device_id() + "/status";
}

