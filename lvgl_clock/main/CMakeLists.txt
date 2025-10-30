idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES ui nvs_flash spi_flash esp_wifi esp_wifi_remote bt bsp_extra lvgl__lvgl libpng lvgl_libpng_glue esp_http_client cJSON
)