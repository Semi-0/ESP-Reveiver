idf_component_register(
    SRCS "main.cpp" "config.cpp" "message_processor.cpp" "device_monitor.cpp" "mqtt_client.cpp" "system_state.cpp"
    INCLUDE_DIRS "."
    REQUIRES eventbus freertos esp_wifi esp_event esp_netif mdns nvs_flash esp_timer driver
)
