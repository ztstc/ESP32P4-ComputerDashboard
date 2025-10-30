idf_component_register(
    SRCS "tasks.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl__lvgl nvs_flash esp_wifi_remote esp32_p4_wifi6_touch_lcd_4b esp_lcd_st7703 esp_wifi bt bsp_extra fatfs sdmmc driver esp_hosted ui esp_http_client cJSON mbedtls
)
