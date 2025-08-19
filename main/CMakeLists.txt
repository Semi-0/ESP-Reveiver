idf_component_register(SRCS "main.cpp" "config.cpp" "wifi_manager.cpp" "app_mqtt_client.cpp" "pin_controller.cpp" "mdns_service.cpp" "message_processor.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES mqtt esp_wifi esp_event esp_netif nvs_flash driver mdns json)