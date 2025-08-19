#include "wifi_manager.h"
#include "config.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"

EventGroupHandle_t WiFiManager::wifi_event_group = NULL;
const int WiFiManager::WIFI_CONNECTED_BIT = BIT0;
const int WiFiManager::WIFI_FAIL_BIT = BIT1;

void WiFiManager::init() {
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Create WiFi event group
    wifi_event_group = xEventGroupCreate();

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

void WiFiManager::start() {
    // Configure WiFi
    wifi_config_t wifi_config = {};
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASSWORD);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Disable WiFi power save (auto sleep)
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_LOGI(APP_TAG, "WiFi power save disabled");
}

bool WiFiManager::wait_for_connection() {
    ESP_LOGI(APP_TAG, "Waiting for WiFi connection...");
    ESP_LOGI(APP_TAG, "Current event bits: 0x%x", xEventGroupGetBits(wifi_event_group));
    
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    ESP_LOGI(APP_TAG, "Event bits received: 0x%x", bits);
    ESP_LOGI(APP_TAG, "WIFI_CONNECTED_BIT: 0x%x", WIFI_CONNECTED_BIT);
    ESP_LOGI(APP_TAG, "WIFI_FAIL_BIT: 0x%x", WIFI_FAIL_BIT);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(APP_TAG, "Connected to WiFi");
        return true;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(APP_TAG, "Failed to connect to WiFi");
        return false;
    }
    ESP_LOGE(APP_TAG, "Unexpected event bits: 0x%x", bits);
    return false;
}

bool WiFiManager::is_connected() {
    return (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) != 0;
}

void WiFiManager::stop() {
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
}

void WiFiManager::wifi_event_handler(void* arg, esp_event_base_t event_base, 
                                    int32_t event_id, void* event_data) {
    ESP_LOGI(APP_TAG, "WiFi event: base=%d, id=%d", event_base, event_id);
    
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(APP_TAG, "WiFi station started, connecting...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(APP_TAG, "Connection to the AP failed");
        xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        ESP_LOGI(APP_TAG, "Set WIFI_FAIL_BIT: 0x%x", WIFI_FAIL_BIT);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(APP_TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(APP_TAG, "Set WIFI_CONNECTED_BIT: 0x%x", WIFI_CONNECTED_BIT);
        ESP_LOGI(APP_TAG, "Current event bits after setting: 0x%x", xEventGroupGetBits(wifi_event_group));
    }
}
