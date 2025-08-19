#ifndef MDNS_SERVICE_H
#define MDNS_SERVICE_H

#include <string>

class MDNSService {
public:
    static void init();
    static void start();
    static void stop();

    // Discover first MQTT service on LAN; returns true on success
    static bool discover_mqtt(std::string& host_out, int& port_out);

private:
    static void setup_services();
};

#endif // MDNS_SERVICE_H
