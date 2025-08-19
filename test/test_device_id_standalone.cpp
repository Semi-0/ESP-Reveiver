#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

// Mock ESP32 functions for testing
extern "C" {
    // Mock MAC address (this would be the actual MAC from ESP32)
    uint8_t mock_mac[6] = {0x24, 0x6F, 0x28, 0x58, 0xB8, 0xD8};
    
    void esp_efuse_mac_get_default(uint8_t* mac) {
        for (int i = 0; i < 6; i++) {
            mac[i] = mock_mac[i];
        }
    }
}

// Standalone device ID generation function (same logic as ESP32)
std::string get_esp32_device_id() {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    
    std::stringstream ss;
    ss << "ESP32_";
    for (int i = 3; i < 6; i++) {  // Use last 3 bytes of MAC for shorter ID
        ss << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << (int)mac[i];
    }
    
    return ss.str();
}

std::string get_mqtt_topic_prefix() {
    return get_esp32_device_id();
}

std::string get_mqtt_status_topic() {
    return get_mqtt_topic_prefix() + "/status";
}

std::string get_mqtt_control_topic() {
    return get_mqtt_topic_prefix() + "/control";
}

std::string get_mqtt_response_topic() {
    return get_mqtt_topic_prefix() + "/response";
}

void test_device_id_generation() {
    std::cout << "=== Testing ESP32 Device ID Generation ===" << std::endl;
    
    // Get the device ID
    std::string device_id = get_esp32_device_id();
    std::cout << "Generated Device ID: " << device_id << std::endl;
    
    // Show the MAC address used
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    std::cout << "MAC Address: ";
    for (int i = 0; i < 6; i++) {
        std::cout << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << (int)mac[i];
        if (i < 5) std::cout << ":";
    }
    std::cout << std::endl;
    
    // Show which bytes are used for the ID
    std::cout << "Using last 3 bytes (positions 3-5): ";
    for (int i = 3; i < 6; i++) {
        std::cout << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << (int)mac[i];
        if (i < 5) std::cout << " ";
    }
    std::cout << std::endl;
    
    // Show MQTT topics
    std::cout << "\nGenerated MQTT Topics:" << std::endl;
    std::cout << "Control Topic: " << get_mqtt_control_topic() << std::endl;
    std::cout << "Status Topic: " << get_mqtt_status_topic() << std::endl;
    std::cout << "Response Topic: " << get_mqtt_response_topic() << std::endl;
    
    // Verify the expected result
    if (device_id == "ESP32_58B8D8") {
        std::cout << "\n✓ PASSED: Device ID matches expected format" << std::endl;
    } else {
        std::cout << "\n✗ FAILED: Device ID does not match expected format" << std::endl;
        std::cout << "Expected: ESP32_58B8D8" << std::endl;
        std::cout << "Got: " << device_id << std::endl;
    }
}

void test_different_mac_addresses() {
    std::cout << "\n=== Testing Different MAC Addresses ===" << std::endl;
    
    // Test with different MAC addresses
    uint8_t test_macs[][6] = {
        {0x24, 0x6F, 0x28, 0x58, 0xB8, 0xD8},  // Original
        {0x24, 0x6F, 0x28, 0x12, 0x34, 0x56},  // Different
        {0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF},  // Another
    };
    
    for (int test = 0; test < 3; test++) {
        // Set the mock MAC
        for (int i = 0; i < 6; i++) {
            mock_mac[i] = test_macs[test][i];
        }
        
        std::string device_id = get_esp32_device_id();
        std::cout << "MAC " << (test + 1) << ": ";
        for (int i = 3; i < 6; i++) {
            std::cout << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << (int)test_macs[test][i];
            if (i < 5) std::cout << " ";
        }
        std::cout << " → Device ID: " << device_id << std::endl;
    }
}

int main() {
    test_device_id_generation();
    test_different_mac_addresses();
    
    std::cout << "\n=== Device ID Generation Test Complete ===" << std::endl;
    std::cout << "The ESP32 will automatically generate its device ID from its MAC address." << std::endl;
    std::cout << "This ensures each ESP32 has a unique identifier without manual configuration." << std::endl;
    std::cout << "\nBenefits:" << std::endl;
    std::cout << "✓ No manual configuration required" << std::endl;
    std::cout << "✓ Each ESP32 has a unique identifier" << std::endl;
    std::cout << "✓ MQTT topics are automatically generated" << std::endl;
    std::cout << "✓ Consistent across reboots and firmware updates" << std::endl;
    
    return 0;
}
