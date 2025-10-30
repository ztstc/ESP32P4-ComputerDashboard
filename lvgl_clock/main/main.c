#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "esp_wifi_remote.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"

// 新增 SD 卡相关头文件
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "ff.h"

#include "lv_demos.h"
#include "ui.h"
#include "tasks.h"

void app_main(void)
{
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // 读取NVS中的亮度值
    int32_t brightness = 50; // 默认亮度
    nvs_handle_t nvs_handle;
    if (nvs_open("storage", NVS_READONLY, &nvs_handle) == ESP_OK) {
        nvs_get_i32(nvs_handle, "brightness", &brightness);
        nvs_close(nvs_handle);
    }
    // 读取音量值
    int32_t volume = 0; // 默认音量
    if (nvs_open("storage", NVS_READONLY, &nvs_handle) == ESP_OK) {
        nvs_get_i32(nvs_handle, "volume", &volume);
        nvs_close(nvs_handle);
    }

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false,
        }
    };

    // 初始化基础WiFi（只需执行一次）
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // 立即启动 WiFi，确保 WIFI_EVENT_STA_START 早点触发
    ESP_ERROR_CHECK(esp_wifi_start());

    // 挂载 SD 卡（SDIO slot0）
    {
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        // 可根据需要显式指定 slot（视 IDF 版本），例如：
        host.slot = SDMMC_HOST_SLOT_0; // 如果 SDK 支持，可以取消注释
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        // 使用 4 线模式（若卡/硬件支持）
        slot_config.width = 4;

        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 16 * 1024
        };

        sdmmc_card_t *card;
        esp_err_t err = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
        if (err != ESP_OK) {
            ESP_LOGE("SDCARD", "Failed to mount SD card (%s). If you want the card to be formatted, set format_if_mount_failed = true.", esp_err_to_name(err));
        } else {
            ESP_LOGI("SDCARD", "SD card mounted at '/sdcard'");
            // 打印卡信息（厂商、容量、时钟等）
            sdmmc_card_print_info(stdout, card);
        }
    }

    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();
    bsp_display_brightness_set(brightness);
    

    bsp_display_lock(0);

    //lv_demo_music();
    // lv_demo_benchmark();
    //lv_demo_widgets();
    ui_init();

    // 初始化后台任务（时间刷新等）
    app_tasks_init();  // 放在 esp_wifi_start 之后

    bsp_display_unlock();
}