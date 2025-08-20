#ifndef WIFI_PURE_H
#define WIFI_PURE_H

#include <string>
#include <vector>

// WiFi configuration structure
struct WifiConfig {
    std::string ssid;
    std::string password;
    int max_retries;
    int retry_delay_ms;
    
    WifiConfig(const std::string& s, const std::string& p, int retries = 5, int delay = 5000)
        : ssid(s), password(p), max_retries(retries), retry_delay_ms(delay) {}
};

// WiFi connection result
struct WifiResult {
    bool success;
    std::string ip_address;
    std::string error_message;
    int retry_count;
    
    WifiResult(bool s, const std::string& ip = "", const std::string& err = "", int retries = 0)
        : success(s), ip_address(ip), error_message(err), retry_count(retries) {}
    
    static WifiResult success_result(const std::string& ip) {
        return WifiResult(true, ip, "", 0);
    }
    
    static WifiResult failure_result(const std::string& error, int retries = 0) {
        return WifiResult(false, "", error, retries);
    }
};

// WiFi status
struct WifiStatus {
    bool connected;
    std::string ssid;
    std::string ip_address;
    int signal_strength;
    int channel;
    
    WifiStatus(bool conn = false, const std::string& s = "", const std::string& ip = "", 
               int signal = 0, int ch = 0)
        : connected(conn), ssid(s), ip_address(ip), signal_strength(signal), channel(ch) {}
};

// Pure function declarations
namespace WifiPure {
    
    // Initialize WiFi with configuration
    WifiResult initialize(const WifiConfig& config);
    
    // Connect to WiFi with retry mechanism
    WifiResult connect(const WifiConfig& config);
    
    // Disconnect from WiFi
    WifiResult disconnect();
    
    // Get current WiFi status
    WifiStatus getStatus();
    
    // Check if WiFi is connected
    bool isConnected();
    
    // Get IP address
    std::string getIpAddress();
    
    // Get signal strength
    int getSignalStrength();
    
    // Get current SSID
    std::string getSsid();
    
    // Scan for available networks
    std::vector<std::string> scanNetworks();
    
    // Validate WiFi configuration
    bool validateConfig(const WifiConfig& config);
    
    // Cleanup WiFi resources
    void cleanup();
}

#endif // WIFI_PURE_H
