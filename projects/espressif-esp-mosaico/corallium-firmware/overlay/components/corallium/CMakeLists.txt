idf_component_register(SRCS "corallium_ble.c" "corallium_protocol.c" "corallium_clock.c"
    INCLUDE_DIRS "include"
    REQUIRES bt esp_timer esp_wifi esp_netif nvs_flash cjson lwip app_config wifi_manager mosaic_ui)
