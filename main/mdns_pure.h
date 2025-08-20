#ifndef MDNS_PURE_H
#define MDNS_PURE_H

#include <string>
#include <vector>

// mDNS service configuration
struct MdnsConfig {
    std::string service_name;
    std::string service_type;
    std::string hostname;
    int port;
    int timeout_ms;
    
    MdnsConfig(const std::string& name = "mqtt", const std::string& type = "_mqtt._tcp", 
               const std::string& host = "esp32", int p = 1883, int timeout = 5000)
        : service_name(name), service_type(type), hostname(host), port(p), timeout_ms(timeout) {}
};

// mDNS service info
struct MdnsServiceInfo {
    std::string service_name;
    std::string service_type;
    std::string host;
    int port;
    std::string ip_address;
    bool valid;
    
    MdnsServiceInfo(const std::string& name = "", const std::string& type = "", 
                    const std::string& h = "", int p = 0, const std::string& ip = "")
        : service_name(name), service_type(type), host(h), port(p), ip_address(ip), valid(false) {}
    
    static MdnsServiceInfo invalid() {
        return MdnsServiceInfo();
    }
    
    static MdnsServiceInfo valid_service(const std::string& name, const std::string& type,
                                        const std::string& host, int port, const std::string& ip) {
        MdnsServiceInfo info(name, type, host, port, ip);
        info.valid = true;
        return info;
    }
};

// mDNS discovery result
struct MdnsResult {
    bool success;
    std::vector<MdnsServiceInfo> services;
    std::string error_message;
    int discovery_time_ms;
    
    MdnsResult(bool s, const std::vector<MdnsServiceInfo>& svcs = {}, 
               const std::string& err = "", int time = 0)
        : success(s), services(svcs), error_message(err), discovery_time_ms(time) {}
    
    static MdnsResult success_result(const std::vector<MdnsServiceInfo>& services, int time = 0) {
        return MdnsResult(true, services, "", time);
    }
    
    static MdnsResult failure_result(const std::string& error, int time = 0) {
        return MdnsResult(false, {}, error, time);
    }
};

// Pure function declarations
namespace MdnsPure {
    
    // Initialize mDNS service
    bool initialize(const MdnsConfig& config);
    
    // Start mDNS service
    bool start();
    
    // Stop mDNS service
    bool stop();
    
    // Discover MQTT services
    MdnsResult discoverMqttServices(const MdnsConfig& config);
    
    // Discover specific service
    MdnsResult discoverService(const std::string& service_type, int timeout_ms = 5000);
    
    // Get first discovered MQTT service
    MdnsServiceInfo getFirstMqttService(const MdnsConfig& config);
    
    // Validate discovered service
    bool validateService(const MdnsServiceInfo& service);
    
    // Check if service is reachable
    bool isServiceReachable(const MdnsServiceInfo& service);
    
    // Get all discovered services
    std::vector<MdnsServiceInfo> getAllServices();
    
    // Clear discovered services cache
    void clearCache();
    
    // Cleanup mDNS resources
    void cleanup();
}

#endif // MDNS_PURE_H
