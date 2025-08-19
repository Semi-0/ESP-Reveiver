#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"

class WiFiManager {
public:
    static void init();
    static void start();
    static bool wait_for_connection();
    static bool is_connected();
    static void stop();

private:
    static EventGroupHandle_t wifi_event_group;
    static const int WIFI_CONNECTED_BIT;
    static const int WIFI_FAIL_BIT;
    
    static void wifi_event_handler(void* arg, esp_event_base_t event_base, 
                                  int32_t event_id, void* event_data);
};

#endif // WIFI_MANAGER_H
