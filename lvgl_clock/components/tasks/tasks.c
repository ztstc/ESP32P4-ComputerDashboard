// ===== 天气更新：配置/状态 =====
#define WEATHER_USER_ID   "enter your user ID" // 需替换为自己的用户 ID
#define WEATHER_USER_KEY  "enter your API key" // 需替换为自己的 API Key
#define WEATHER_URL_OPTIMAL "https://api.apihz.cn/getapi.php" // 天气 API 地址 百度搜索 API 盒子 免费注册一个
#define WEATHER_REFRESH_MINUTES 30 // 天气自动刷新间隔（分钟） 时间太短会被封号

#include "tasks.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string.h>
#include "esp_sntp.h"
#include "esp_log.h"
#include "lvgl.h"
#include "bsp/display.h"
#include "ui.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "cJSON.h"
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "freertos/event_groups.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"

static const char *TAG = "app_tasks";

// UI 对象（弱符号在 ui_events.c 里已做；这里直接引用即可）
extern lv_obj_t * uic_Time9;
extern lv_obj_t * uic_Time;
extern lv_obj_t * uic_TimeNow;
extern lv_obj_t * uic_Time2;
extern lv_obj_t * uic_Year;
extern lv_obj_t * uic_MonthDay;
extern lv_obj_t * uic_DayOfWeekLabel;
extern lv_obj_t * uic_CPUName;
extern lv_obj_t * uic_GPUName;
//主页天气相关 UI 对象
extern lv_obj_t * uic_WeatherIcon; //首页天气图标
extern lv_obj_t * uic_TempNow;     //首页当前温度
extern lv_obj_t * uic_Humidity;    //首页当前湿度
//天气页面 UI 对象
extern lv_obj_t * uic_country;     //天气页面 地区：国家
extern lv_obj_t * uic_province;    //天气页面 地区：省份
extern lv_obj_t * uic_city;        //天气页面 地区：城市
extern lv_obj_t * uic_name;        //天气页面 地区：名称（全名）
extern lv_obj_t * uic_updatetime;     //天气页面 更新时间

extern lv_obj_t * uic_AlarmDate;         //天气页面 气象灾害预警 预计发生日期
extern lv_obj_t * uic_Title;             //天气页面 气象灾害预警 标题
extern lv_obj_t * uic_AlarmIcon;        //天气页面 气象灾害预警 图标
extern lv_obj_t * uic_Alarm;            //天气页面 气象灾害预警面板容器 如果无预警则隐藏

extern lv_obj_t * uic_Nowweathertemp1;  //天气页面 当前白天的温度
extern lv_obj_t * uic_Nowweatherlabel1; //天气页面 当前白天的天气描述
extern lv_obj_t * uic_Nowweather1img;   //天气页面 当前白天的天气图标
extern lv_obj_t * uic_Nowweathertemp2;  //天气页面 当前晚上的温度
extern lv_obj_t * uic_Nowweatherlabel2; //天气页面 当前晚上的天气描述
extern lv_obj_t * uic_Nowweather2img;   //天气页面 当前晚上的天气图标

extern lv_obj_t * uic_WeatherDate1;   //天气页面 未来第一天日期
extern lv_obj_t * uic_weather1temp1;  //天气页面 未来第一天白天温度
extern lv_obj_t * uic_weather1label1; //天气页面 未来第一天白天天气描述
extern lv_obj_t * uic_weather1img1;   //天气页面 未来第一天白天天气图标
extern lv_obj_t * uic_weather1temp2;  //天气页面 未来第一天晚上温度
extern lv_obj_t * uic_weather1label2; //天气页面 未来第一天晚上天气描述
extern lv_obj_t * uic_weather1img2;   //天气页面 未来第一天晚上天气图标

extern lv_obj_t * uic_WeatherDate2;   //天气页面 未来第二天日期
extern lv_obj_t * uic_weather2temp1;  //天气页面 未来第二天白天温度
extern lv_obj_t * uic_weather2label1; //天气页面 未来第二天白天天气描述
extern lv_obj_t * uic_weather2img1;   //天气页面 未来第二天白天天气图标
extern lv_obj_t * uic_weather2temp2;  //天气页面 未来第二天晚上温度
extern lv_obj_t * uic_weather2label2; //天气页面 未来第二天晚上天气描述
extern lv_obj_t * uic_weather2img2;   //天气页面 未来第二天晚上天气图标

extern lv_obj_t * uic_WeatherDate3;   //天气页面 未来第三天日期
extern lv_obj_t * uic_weather3temp1;  //天气页面 未来第三天白天温度
extern lv_obj_t * uic_weather3img1;   //天气页面 未来第三天白天天气图标
extern lv_obj_t * uic_weather3label1; //天气页面 未来第三天白天天气描述
extern lv_obj_t * uic_weather3temp2;  //天气页面 未来第三天晚上温度
extern lv_obj_t * uic_weather3img2;   //天气页面 未来第三天晚上天气图标
extern lv_obj_t * uic_weather3label2; //天气页面 未来第三天白天天气描述



bool bsp_display_lock(uint32_t timeout_ms);
void bsp_display_unlock(void);

#define TASKS_EVT_WIFI_STARTED   (1 << 0)
#define TASKS_EVT_GOT_IP         (1 << 1)
// 新增：天气立即刷新事件位
#define TASKS_EVT_WEATHER_FORCE  (1 << 2)

static EventGroupHandle_t s_evt_group = NULL;
static bool s_wifi_evt_registered = false;
static bool s_ip_evt_registered   = false;

static TaskHandle_t s_time_task_handle = NULL;
static TaskHandle_t s_wifi_auto_task_handle = NULL; // 新增: 自动连接任务
static bool s_sntp_started = false;



static TaskHandle_t s_weather_task_handle = NULL;


// ===== LVGL v8/v9 兼容层（类型/常量/接口）=====
#if LVGL_VERSION_MAJOR >= 9
    #define IMG_DSC_T   lv_image_dsc_t
    #define IMG_SET_SRC lv_image_set_src
    #define CF_RAW      LV_COLOR_FORMAT_RAW
#else
    #define IMG_DSC_T   lv_img_dsc_t
    #define IMG_SET_SRC lv_img_set_src
    #define CF_RAW      LV_IMG_CF_RAW
#endif

// PNG 数据需在设置给 LVGL 后保持存活，使用静态持有，并用 RAW 解码（需启用 PNG 解码器）
static uint8_t *s_weather_png = NULL;
static size_t s_weather_png_len = 0;
// 修正：去掉不存在的 always_zero 字段，并兼容 v8/v9 枚举名
static IMG_DSC_T s_weather_png_dsc = {
    .header = { .cf = CF_RAW, .w = 0, .h = 0 },
    .data_size = 0,
    .data = NULL
};

// 新增：多图标持久化持有者（天气页）
typedef struct {
    uint8_t *buf;
    size_t len;
    IMG_DSC_T dsc;
} img_holder_t;

static void img_holder_set(lv_obj_t *img, img_holder_t *h, uint8_t *buf, size_t len)
{
    if (!img) { if (buf) free(buf); return; }
    if (h->buf) { free(h->buf); h->buf = NULL; h->len = 0; }
    h->buf = buf;
    h->len = len;
    h->dsc.header.cf = CF_RAW;
    h->dsc.header.w = 0; // 由解码器解析
    h->dsc.header.h = 0;
    h->dsc.data = h->buf;
    h->dsc.data_size = (uint32_t)h->len;
    IMG_SET_SRC(img, &h->dsc);
}

// 天气页各图标的持久化容器（不包含首页图标，首页沿用 s_weather_png_*）
static img_holder_t s_icon_now1 = {0}; // 当前白天
static img_holder_t s_icon_now2 = {0}; // 当前夜间
static img_holder_t s_icon_w1d  = {0}; // 明天白天
static img_holder_t s_icon_w1n  = {0}; // 明天夜间
static img_holder_t s_icon_w2d  = {0}; // 后天白天
static img_holder_t s_icon_w2n  = {0}; // 后天夜间
static img_holder_t s_icon_w3d  = {0}; // 大后天白天
static img_holder_t s_icon_w3n  = {0}; // 大后天夜间

static void tasks_update_time_labels(void)
{
    if (!bsp_display_lock(50)) return;
    time_t now = time(NULL);
    struct tm tm_info = {0};
    localtime_r(&now, &tm_info);
    char buf[6];
    bool valid = (tm_info.tm_year + 1900 >= 2020);
    if (!valid) {
        snprintf(buf, sizeof(buf), "--:--");
    } else {
        snprintf(buf, sizeof(buf), "%02d:%02d", tm_info.tm_hour, tm_info.tm_min);
    }
    if (uic_Time9)   lv_label_set_text(uic_Time9, buf);
    if (uic_Time)    lv_label_set_text(uic_Time, buf);
    if (uic_Time2)   lv_label_set_text(uic_Time2, buf);
    if (uic_TimeNow) lv_label_set_text(uic_TimeNow, buf);
    if (valid) {
        int year = tm_info.tm_year + 1900;
        char year_buf[12];
        char md_buf[16];
        char dow_buf[4];
        static const char *dow_str[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        snprintf(year_buf, sizeof(year_buf), "%04d", year);
        snprintf(md_buf, sizeof(md_buf), "%d %d", tm_info.tm_mon + 1, tm_info.tm_mday);
        snprintf(dow_buf, sizeof(dow_buf), "%s", dow_str[tm_info.tm_wday % 7]);
        if (uic_Year)           lv_label_set_text(uic_Year, year_buf);
        if (uic_MonthDay)       lv_label_set_text(uic_MonthDay, md_buf);
        if (uic_DayOfWeekLabel) lv_label_set_text(uic_DayOfWeekLabel, dow_buf);
    }
    bsp_display_unlock();
}

static void sntp_sync_time_cb(struct timeval *tv)
{
    // 回调在SNTP线程上下文，避免直接操作 LVGL；使用异步调用
    lv_async_call((lv_async_cb_t)tasks_update_time_labels, NULL);
}

static void tasks_start_sntp_if_needed(void)
{
    if (s_sntp_started) return;
    s_sntp_started = true;
    setenv("TZ", "CST-8", 1);
    tzset();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_set_time_sync_notification_cb(sntp_sync_time_cb);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "cn.pool.ntp.org");
    esp_sntp_setservername(2, "ntp1.aliyun.com");
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP started");
    lv_async_call((lv_async_cb_t)tasks_update_time_labels, NULL);
}

static void time_task(void *arg)
{
    (void)arg;
    // 周期刷新，不阻塞 LVGL 任务（仅短暂加锁）
    for (;;) {
        tasks_update_time_labels();
        vTaskDelay(pdMS_TO_TICKS(10000)); // 10s 刷新一次
    }
}

void tasks_notify_wifi_started(void)
{
    if (s_evt_group) {
        xEventGroupSetBits(s_evt_group, TASKS_EVT_WIFI_STARTED);
    }
}

// 取代 ui_events.c 里的 ip_event_handler
static void tasks_ip_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        if (s_evt_group) xEventGroupSetBits(s_evt_group, TASKS_EVT_GOT_IP);
        tasks_on_got_ip();
    }
}

static void tasks_wifi_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    if (base != WIFI_EVENT) return;
    switch (id) {
        case WIFI_EVENT_STA_START:
            tasks_notify_wifi_started();
            ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            // 清除 GOT_IP，等待重连
            if (s_evt_group) xEventGroupClearBits(s_evt_group, TASKS_EVT_GOT_IP);
            break;
        default:
            break;
    }
}

static void wifi_auto_connect_task(void *arg)
{
    (void)arg;
    // 初始等待：给 hosted 传输层/协处理器充分时间
    vTaskDelay(pdMS_TO_TICKS(2000));

    for (;;) {
        // 等待 WIFI 启动
        EventBits_t bits = xEventGroupWaitBits(
            s_evt_group,
            TASKS_EVT_WIFI_STARTED,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(1000)
        );
        if (!(bits & TASKS_EVT_WIFI_STARTED)) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // 已经连接?
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // 读取 NVS
        char ssid[33] = {0};
        char pwd[65]  = {0};
        int32_t wifi_on = 0;
        nvs_handle_t nvs_handle;
        if (nvs_open("storage", NVS_READONLY, &nvs_handle) == ESP_OK) {
            size_t len = sizeof(ssid); if (nvs_get_str(nvs_handle, "wifi_ssid", ssid, &len) != ESP_OK) ssid[0] = 0;
            len = sizeof(pwd); if (nvs_get_str(nvs_handle, "wifi_pwd", pwd, &len) != ESP_OK) pwd[0] = 0;
            nvs_get_i32(nvs_handle, "wifi_on", &wifi_on);
            nvs_close(nvs_handle);
        }
        if (!wifi_on || ssid[0] == '\0') {
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        // 再次确认 STA 已启动（防止 race）
        wifi_mode_t mode;
        if (esp_wifi_get_mode(&mode) != ESP_OK || !(mode & WIFI_MODE_STA)) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        wifi_config_t cfg = {0};
        size_t s_len = strnlen(ssid, sizeof(cfg.sta.ssid));
        size_t p_len = strnlen(pwd,  sizeof(cfg.sta.password));
        memcpy(cfg.sta.ssid, ssid, s_len);
        memcpy(cfg.sta.password, pwd, p_len);
        cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        cfg.sta.pmf_cfg.capable = true;
        cfg.sta.pmf_cfg.required = false;

        esp_err_t e = esp_wifi_set_config(WIFI_IF_STA, &cfg);
        if (e != ESP_OK) {
            ESP_LOGW(TAG, "set_config failed: %s", esp_err_to_name(e));
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        ESP_LOGI(TAG, "Trying connect SSID:%s", ssid);
        e = esp_wifi_connect();
        if (e != ESP_OK) {
            ESP_LOGW(TAG, "connect failed: %s", esp_err_to_name(e));
            // 典型错误：ESP_ERR_WIFI_NOT_STARTED / BUSY -> 等待再试
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        // 等待是否拿到 IP (或超时重试)
        bool got_ip = false;
        for (int i = 0; i < 30; ++i) {
            EventBits_t b = xEventGroupGetBits(s_evt_group);
            if (b & TASKS_EVT_GOT_IP) { got_ip = true; break; }
            // 如果期间断开则跳出加速重试
            if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        if (!got_ip) {
            ESP_LOGW(TAG, "No IP yet, will retry later");
            // 断开以触发下一轮 (可选)
            // esp_wifi_disconnect();
        } else {
            ESP_LOGI(TAG, "Connected & IP acquired");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void tasks_try_auto_connect(void)
{
    if (s_wifi_auto_task_handle == NULL) {
        xTaskCreatePinnedToCore(wifi_auto_connect_task, "wifi_auto", 4096, NULL, 4, &s_wifi_auto_task_handle, 1);
    }
}

void tasks_on_got_ip(void)
{
    tasks_start_sntp_if_needed();
    // 新增：拿到 IP 后启动天气自动更新
    tasks_start_weather_if_needed(); // 有了前向声明，不再触发隐式声明
}

void app_tasks_init(void)
{
    if (!s_evt_group) {
        s_evt_group = xEventGroupCreate();
    }
    if (!s_wifi_evt_registered) {
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, tasks_wifi_event_handler, NULL);
        s_wifi_evt_registered = true;
    }
    if (!s_ip_evt_registered) {
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, tasks_ip_event_handler, NULL);
        s_ip_evt_registered = true;
    }

    if (s_time_task_handle == NULL) {
        xTaskCreatePinnedToCore(time_task, "time_task", 4096, NULL, 5, &s_time_task_handle, 1);
    }
    tasks_try_auto_connect();
    lv_async_call((lv_async_cb_t)tasks_update_time_labels, NULL);

    // 新增：初始化 PNG 解码器（只做一次）
    tasks_init_img_decoders();
}

// ====== 电脑监控任务新增开始 ======
typedef struct {
    int cpu_usage;
    int gpu_usage;
    int ram_usage;
    int disk_count;
    struct {
        char name[8];
        int usage;
    } disks[6]; // 支持6个磁盘
    float upload_mb;
    float download_mb;
    char ip[16];
    char cpu_model[48];
    char gpu_model[48];
} pc_metrics_t;

static TaskHandle_t s_pc_monitor_task = NULL;
static bool s_pc_monitor_running = false;
static char s_pc_ip[16] = {0};
static bool s_pc_first_data_sent = false;
// 新增：接收缓冲放堆上，避免大栈数组
#define PC_MONITOR_BUF_SIZE 4096
static char *s_pc_rx_buf = NULL;

extern lv_obj_t * uic_CPULoadLabel;
extern lv_obj_t * uic_CPULoad;
extern lv_obj_t * uic_GPULoadLabel;
extern lv_obj_t * uic_GPULoad;
extern lv_obj_t * uic_RAMLoad;
extern lv_obj_t * uic_RAMLoadLabel;
extern lv_obj_t * uic_Disk1Load;
extern lv_obj_t * uic_Disk2Load;
extern lv_obj_t * uic_Disk3Load;
extern lv_obj_t * uic_Disk4Load;
extern lv_obj_t * uic_Disk5Load;
extern lv_obj_t * uic_Disk6Load;
extern lv_obj_t * uic_Disk1Label;
extern lv_obj_t * uic_Disk2Label;
extern lv_obj_t * uic_Disk3Label;
extern lv_obj_t * uic_Disk4Label;
extern lv_obj_t * uic_Disk5Label;
extern lv_obj_t * uic_Disk6Label;
extern lv_obj_t * uic_UploadLabel;
extern lv_obj_t * uic_DownloadLabel;
extern lv_obj_t * uic_IPLabel;
extern lv_obj_t * uic_loading;
extern lv_obj_t * uic_UploadDownload;
// 新增：首页屏对象（用于回退）
extern lv_obj_t * ui_Home;

// 前向声明：网络速度图表更新函数
static void update_net_chart(float down_mb, float up_mb);

// ==== 平滑占用率显示新增（补齐未定义部分，需在 tasks_apply_pc_metrics 之前） ====
static lv_timer_t *s_usage_smooth_timer = NULL;
static int s_disp_cpu = -1, s_disp_gpu = -1, s_disp_ram = -1;
static int s_target_cpu = 0,  s_target_gpu = 0,  s_target_ram = 0;

static void usage_smooth_timer_cb(lv_timer_t *t)
{
    (void)t;
    bool any = false;

    struct {
        int *disp;
        int *target;
    } items[3] = {
        { &s_disp_cpu, &s_target_cpu },
        { &s_disp_gpu, &s_target_gpu },
        { &s_disp_ram, &s_target_ram }
    };

    for (int i = 0; i < 3; ++i) {
        int *d = items[i].disp;
        int  tgt = *items[i].target;
        if (*d < 0) { *d = tgt; any = true; continue; }
        if (*d == tgt) continue;
        int diff = tgt - *d;
        int ad = diff > 0 ? diff : -diff;
        int step;
        if (ad >= 20) step = 10;
        else if (ad >= 10) step = 5;
        else if (ad >= 5) step = 2;
        else step = 1;
        *d += (diff > 0 ? step : -step);
        if ((diff > 0 && *d > tgt) || (diff < 0 && *d < tgt)) *d = tgt;
        any = true;
    }

    if (!any) return;

    if (bsp_display_lock(30)) {
        if (uic_CPULoad) lv_arc_set_value(uic_CPULoad, s_disp_cpu);
        if (uic_GPULoad) lv_arc_set_value(uic_GPULoad, s_disp_gpu);
        if (uic_RAMLoad) lv_bar_set_value(uic_RAMLoad, s_disp_ram, LV_ANIM_OFF);

        if (uic_CPULoadLabel) {
            char buf[16]; snprintf(buf, sizeof(buf), "%d", s_disp_cpu);
            lv_label_set_text(uic_CPULoadLabel, buf);
        }
        if (uic_GPULoadLabel) {
            char buf[16]; snprintf(buf, sizeof(buf), "%d", s_disp_gpu);
            lv_label_set_text(uic_GPULoadLabel, buf);
        }
        if (uic_RAMLoadLabel) {
            char buf[24]; snprintf(buf, sizeof(buf), "RAM:%d%%", s_disp_ram);
            lv_label_set_text(uic_RAMLoadLabel, buf);
        }
        bsp_display_unlock();
    }
}
// ==== 平滑占用率显示新增结束 ====

// ===== 掉线检测相关新增 =====
#define PC_FAIL_THRESHOLD 3
static int s_pc_fail_count = 0;
static lv_timer_t *s_pc_back_home_timer = NULL;

// 在UI线程显示 loading 并启动10s回退计时器（一次性）
static void pc_back_home_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (bsp_display_lock(200)) {
        if (ui_Home) {
            lv_scr_load_anim(ui_Home, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
        }
        bsp_display_unlock();
    }
    if (s_pc_back_home_timer) {
        lv_timer_del(s_pc_back_home_timer);
        s_pc_back_home_timer = NULL;
    }
}

static void ui_show_loading_and_start_back(void *param)
{
    (void)param;
    if (bsp_display_lock(100)) {
        if (uic_loading) lv_obj_clear_flag(uic_loading, LV_OBJ_FLAG_HIDDEN);
        bsp_display_unlock();
    }
    if (!s_pc_back_home_timer) {
        s_pc_back_home_timer = lv_timer_create(pc_back_home_timer_cb, 10000, NULL);
        lv_timer_set_repeat_count(s_pc_back_home_timer, 1);
    }
}

static void ui_cancel_back_and_hide_loading(void *param)
{
    (void)param;
    if (s_pc_back_home_timer) {
        lv_timer_del(s_pc_back_home_timer);
        s_pc_back_home_timer = NULL;
    }
    if (bsp_display_lock(100)) {
        if (uic_loading) lv_obj_add_flag(uic_loading, LV_OBJ_FLAG_HIDDEN);
        bsp_display_unlock();
    }
}
// ===== 掉线检测相关新增结束 =====

static void tasks_apply_pc_metrics(void *param)
{
    pc_metrics_t *m = (pc_metrics_t*)param;
    if (!m) return;
    if (bsp_display_lock(100)) {
        // 型号（一次性或变更时更新）
        if (uic_CPUName && m->cpu_model[0]) lv_label_set_text(uic_CPUName, m->cpu_model);
        if (uic_GPUName && m->gpu_model[0]) lv_label_set_text(uic_GPUName, m->gpu_model);

        // 目标值更新（实际显示由平滑定时器处理）
        s_target_cpu = m->cpu_usage;
        s_target_gpu = m->gpu_usage;
        s_target_ram = m->ram_usage;
        if (s_disp_cpu < 0) {
            s_disp_cpu = s_target_cpu;
            s_disp_gpu = s_target_gpu;
            s_disp_ram = s_target_ram;
        }
        if (!s_usage_smooth_timer) {
            s_usage_smooth_timer = lv_timer_create(usage_smooth_timer_cb, 120, NULL);
        }

        // 磁盘
        lv_obj_t *diskBars[6] = {uic_Disk1Load, uic_Disk2Load, uic_Disk3Load, uic_Disk4Load, uic_Disk5Load, uic_Disk6Load};
        lv_obj_t *diskLabels[6] = {uic_Disk1Label, uic_Disk2Label, uic_Disk3Label, uic_Disk4Label, uic_Disk5Label, uic_Disk6Label};
        for (int i = 0; i < 6; ++i) {
            if (i < m->disk_count) {
                if (diskBars[i]) {
                    lv_obj_clear_flag(diskBars[i], LV_OBJ_FLAG_HIDDEN);
                    lv_bar_set_value(diskBars[i], m->disks[i].usage, LV_ANIM_OFF);
                }
                if (diskLabels[i]) {
                    char dbuf[40];
                    snprintf(dbuf, sizeof(dbuf), "%s:%d%%", m->disks[i].name, m->disks[i].usage);
                    lv_label_set_text(diskLabels[i], dbuf);
                    lv_obj_clear_flag(diskLabels[i], LV_OBJ_FLAG_HIDDEN);
                }
            } else {
                if (diskBars[i]) lv_obj_add_flag(diskBars[i], LV_OBJ_FLAG_HIDDEN);
                if (diskLabels[i]) lv_obj_add_flag(diskLabels[i], LV_OBJ_FLAG_HIDDEN);
            }
        }

        if (uic_UploadLabel) {
            char buf[48]; snprintf(buf, sizeof(buf), "Upload:%.2fMB/s", m->upload_mb);
            lv_label_set_text(uic_UploadLabel, buf);
        }
        if (uic_DownloadLabel) {
            char buf[48]; snprintf(buf, sizeof(buf), "Download:%.2fMB/s", m->download_mb);
            lv_label_set_text(uic_DownloadLabel, buf);
        }
        update_net_chart(m->download_mb, m->upload_mb);
        if (uic_IPLabel) lv_label_set_text(uic_IPLabel, m->ip);
        if (uic_loading && !s_pc_first_data_sent) {
            lv_obj_add_flag(uic_loading, LV_OBJ_FLAG_HIDDEN);
            s_pc_first_data_sent = true;
        }
        // 收到有效数据：清零失败计数并取消回退
        s_pc_fail_count = 0;
        // 若之前已显示loading/启动回退，恢复
        ui_cancel_back_and_hide_loading(NULL);

        bsp_display_unlock();
    }
    free(m);
}

// 替换：安全实现，不访问内部结构体成员
static void update_net_chart(float down_mb, float up_mb)
{
    if (!uic_UploadDownload) return;
    // 新增：缩放因子，1单位=0.01MB/s
    #define NET_CH_SCALE 100

    static bool inited = false;
    static lv_chart_series_t *s_down = NULL;
    static lv_chart_series_t *s_up   = NULL;
    enum { CAP = 10 };
    static lv_coord_t hist_down[CAP];
    static lv_coord_t hist_up[CAP];
    static int hist_count = 0;
    static lv_coord_t last_max = 0;

    if (!inited) {
        lv_chart_set_point_count(uic_UploadDownload, CAP);
        lv_chart_set_update_mode(uic_UploadDownload, LV_CHART_UPDATE_MODE_SHIFT);
        s_down = lv_chart_add_series(uic_UploadDownload, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
        s_up   = lv_chart_add_series(uic_UploadDownload, lv_palette_main(LV_PALETTE_BLUE),  LV_CHART_AXIS_PRIMARY_Y);
        for (int i = 0; i < CAP; ++i) {
            lv_chart_set_next_value(uic_UploadDownload, s_down, 0);
            lv_chart_set_next_value(uic_UploadDownload, s_up, 0);
            hist_down[i] = 0;
            hist_up[i] = 0;
        }
        // 初始 Y 轴范围：0~5MB/s -> 缩放
        lv_chart_set_range(uic_UploadDownload, LV_CHART_AXIS_PRIMARY_Y, 0, 5 * NET_CH_SCALE);
        inited = true;
    }

    // 使用缩放后的整数坐标来承载小数
    lv_coord_t d = (lv_coord_t)(down_mb * NET_CH_SCALE + 0.5f);
    lv_coord_t u = (lv_coord_t)(up_mb   * NET_CH_SCALE + 0.5f);
    if (d < 0) d = 0;
    if (u < 0) u = 0;

    // 推入图表（Shift 模式）
    lv_chart_set_next_value(uic_UploadDownload, s_down, d);
    lv_chart_set_next_value(uic_UploadDownload, s_up, u);

    // 维护本地历史窗口（缩放单位）
    if (hist_count < CAP) {
        hist_down[hist_count] = d;
        hist_up[hist_count] = u;
        hist_count++;
    } else {
        memmove(hist_down, hist_down + 1, sizeof(hist_down) - sizeof(hist_down[0]));
        memmove(hist_up,   hist_up + 1,   sizeof(hist_up)   - sizeof(hist_up[0]));
        hist_down[CAP - 1] = d;
        hist_up[CAP - 1] = u;
    }

    // 计算最大值并动态调整范围（缩放单位）
    lv_coord_t maxv = 1;
    for (int i = 0; i < hist_count; ++i) {
        if (hist_down[i] > maxv) maxv = hist_down[i];
        if (hist_up[i]   > maxv) maxv = hist_up[i];
    }
    // 增加20%余量，最小 1MB/s
    lv_coord_t new_max = maxv + maxv / 5 + 1;
    if (new_max < 1 * NET_CH_SCALE) new_max = 1 * NET_CH_SCALE;
    if (new_max != last_max) {
        lv_chart_set_range(uic_UploadDownload, LV_CHART_AXIS_PRIMARY_Y, 0, new_max);
        last_max = new_max;
    }

    lv_chart_refresh(uic_UploadDownload);
}

// 简易提取 float/ int
static float parse_first_float(const char *json, const char *key)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    char *p = strstr(json, pattern);
    if (!p) return 0;
    p += strlen(pattern);
    p = strchr(p, ':');
    if (!p) return 0;
    ++p;
    while (*p==' '||*p=='\"') ++p;
    float v=0;
    sscanf(p, "%f", &v);
    return v;
}

static int parse_cpu_usage(const char *json) { return (int)(parse_first_float(json, "usage_percent")+0.5f); } // 在 cpu 块使用前截断即可

static int parse_section_usage(const char *section, const char *json)
{
    char *s = strstr(json, section);
    if (!s) return 0;
    return parse_cpu_usage(s);
}

static void parse_disks(const char *json, pc_metrics_t *m)
{
    const char *p = strstr(json, "\"disks\"");
    if (!p) return;
    p = strchr(p, '[');
    if (!p) return;

    while (m->disk_count < 6) { // 支持6个磁盘
        // 找到下一个对象
        const char *obj = strchr(p, '{');
        if (!obj) break;

        // device
        const char *dev_key = strstr(obj, "\"device\"");
        if (!dev_key) break;
        const char *colon = strchr(dev_key, ':');
        if (!colon) break;
        const char *q1 = strchr(colon, '\"');
        if (!q1) break;
        const char *val_start = q1 + 1;
        const char *q2 = strchr(val_start, '\"');
        if (!q2) break;

        char device_buf[16] = {0};
        size_t len = q2 - val_start;
        if (len > sizeof(device_buf) - 1) len = sizeof(device_buf) - 1;
        memcpy(device_buf, val_start, len);

        // usage_percent
        const char *usage_key = strstr(dev_key, "\"usage_percent\"");
        if (!usage_key) { p = q2 + 1; continue; }
        const char *u_colon = strchr(usage_key, ':');
        if (!u_colon) { p = q2 + 1; continue; }
        float uv = 0;
        sscanf(u_colon + 1, "%f", &uv);

        // 提取盘符（形如 C:\ 或 C:）
        char name_out[8] = {0};
        if (device_buf[0]) {
            if (device_buf[1] == ':') {
                name_out[0] = device_buf[0];
                name_out[1] = '\0';
            } else {
                // 退化：取首字符直到遇到 ':' 或 '\'
                int i = 0;
                while (device_buf[i] && device_buf[i] != ':' && device_buf[i] != '\\' && i < 6) {
                    name_out[i] = device_buf[i];
                    i++;
                }
                name_out[i] = 0;
            }
        }

        if (name_out[0]) {
            strncpy(m->disks[m->disk_count].name, name_out, sizeof(m->disks[0].name) - 1);
            m->disks[m->disk_count].usage = (int)(uv + 0.5f);
            m->disk_count++;
        }

        // 前进到下一个 '}' 后继续
        const char *end_brace = strchr(q2, '}');
        if (!end_brace) break;
        p = end_brace + 1;

        // 若遇到数组结束
        if (*p == ']') break;
    }
}

static void parse_network(const char *json, pc_metrics_t *m)
{
    char *n = strstr(json, "\"network\"");
    if (!n) return;
    m->upload_mb   = parse_first_float(n, "upload_speed");
    m->download_mb = parse_first_float(n, "download_speed");
}

// 新增: 解析型号
static void parse_model(const char *json, const char *section, char *out, size_t out_sz)
{
    const char *sec = strstr(json, section);
    if (!sec) return;
    const char *model_key = strstr(sec, "\"model\"");
    if (!model_key) return;
    const char *colon = strchr(model_key, ':');
    if (!colon) return;
    const char *q1 = strchr(colon, '\"');
    if (!q1) return;
    const char *start = q1 + 1;
    const char *q2 = strchr(start, '\"');
    if (!q2) return;
    size_t len = q2 - start;
    if (len >= out_sz) len = out_sz - 1;
    memcpy(out, start, len);
    out[len] = 0;
}

static void pc_monitor_task(void *arg)
{
    (void)arg;
    const int port = 23333;
    for (;;) {
        if (!s_pc_monitor_running || s_pc_ip[0] == 0) {
            vTaskDelay(pdMS_TO_TICKS(300));
            continue;
        }
        if (!s_pc_rx_buf) {
            s_pc_rx_buf = (char*)malloc(PC_MONITOR_BUF_SIZE);
            if (!s_pc_rx_buf) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
        }

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            // 仅在已成功连通过后才计入掉线
            if (s_pc_first_data_sent && ++s_pc_fail_count >= PC_FAIL_THRESHOLD) {
                lv_async_call(ui_show_loading_and_start_back, NULL);
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(s_pc_ip);
        struct timeval tv = {.tv_sec=3, .tv_usec=0};
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
            close(sock);
            if (s_pc_first_data_sent && ++s_pc_fail_count >= PC_FAIL_THRESHOLD) {
                lv_async_call(ui_show_loading_and_start_back, NULL);
            }
            vTaskDelay(pdMS_TO_TICKS(1500));
            continue;
        }

        int total = 0;
        while (total < PC_MONITOR_BUF_SIZE - 1) {
            int r = recv(sock, s_pc_rx_buf + total, PC_MONITOR_BUF_SIZE - 1 - total, 0);
            if (r <= 0) break;
            total += r;
        }
        s_pc_rx_buf[total] = 0;
        close(sock);

        if (total > 0) {
            pc_metrics_t *m = calloc(1, sizeof(pc_metrics_t));
            if (m) {
                m->cpu_usage = parse_section_usage("\"cpu\"", s_pc_rx_buf);
                m->gpu_usage = parse_section_usage("\"gpu\"", s_pc_rx_buf);
                m->ram_usage = parse_section_usage("\"ram\"", s_pc_rx_buf);
                parse_disks(s_pc_rx_buf, m);
                parse_network(s_pc_rx_buf, m);
                parse_model(s_pc_rx_buf, "\"cpu\"", m->cpu_model, sizeof(m->cpu_model));
                parse_model(s_pc_rx_buf, "\"gpu\"", m->gpu_model, sizeof(m->gpu_model));
                strncpy(m->ip, s_pc_ip, sizeof(m->ip)-1);
                lv_async_call(tasks_apply_pc_metrics, m);
            }
        } else {
            if (s_pc_first_data_sent && ++s_pc_fail_count >= PC_FAIL_THRESHOLD) {
                lv_async_call(ui_show_loading_and_start_back, NULL);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500)); // 提升更新率
    }
}

void tasks_start_pc_monitor(const char *ip)
{
    if (!ip || !ip[0]) return;
    strncpy(s_pc_ip, ip, sizeof(s_pc_ip)-1);
    s_pc_ip[sizeof(s_pc_ip)-1] = 0;
    s_pc_monitor_running = true;
    s_pc_first_data_sent = false;
    if (!s_pc_rx_buf) {
        s_pc_rx_buf = (char*)malloc(PC_MONITOR_BUF_SIZE);
    }
    if (s_pc_monitor_task == NULL) {
        // 增大任务栈，防止后续扩展再溢出
        xTaskCreatePinnedToCore(pc_monitor_task, "pc_monitor", 8192, NULL, 4, &s_pc_monitor_task, 1);
    }
}

void tasks_stop_pc_monitor(void)
{
    s_pc_monitor_running = false;
    s_pc_ip[0] = 0;
    s_pc_first_data_sent = false;
    if (s_usage_smooth_timer) { lv_timer_del(s_usage_smooth_timer); s_usage_smooth_timer = NULL; }
    s_disp_cpu = s_disp_gpu = s_disp_ram = -1;
    s_target_cpu = s_target_gpu = s_target_ram = 0;

    // 清理掉线相关状态
    s_pc_fail_count = 0;
    if (s_pc_back_home_timer) { lv_timer_del(s_pc_back_home_timer); s_pc_back_home_timer = NULL; }
    // 恢复loading为隐藏
    ui_cancel_back_and_hide_loading(NULL);
}

// ===== 天气更新：HTTP 工具 =====
static bool http_get_text_alloc(const char *url, char **out_buf, int *out_len)
{
    if (out_buf) *out_buf = NULL;
    if (out_len) *out_len = 0;
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 7000,
    #ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
    #endif
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return false;
    }
    int content_length = esp_http_client_fetch_headers(client);
    int cap = content_length > 0 ? (content_length + 1) : 4096;
    char *buf = malloc(cap);
    if (!buf) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }
    int total = 0;
    while (1) {
        int r = esp_http_client_read(client, buf + total, cap - 1 - total);
        if (r <= 0) break;
        total += r;
        if (total >= cap - 1) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) {
                free(buf);
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return false;
            }
            buf = nb;
        }
    }
    buf[total] = 0;
    if (out_buf) *out_buf = buf; else free(buf);
    if (out_len) *out_len = total;
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return total > 0;
}

static bool http_get_bin_alloc(const char *url, uint8_t **out_buf, int *out_len)
{
    if (out_buf) *out_buf = NULL;
    if (out_len) *out_len = 0;
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 7000,
    #ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
    #endif
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return false;
    }
    int content_length = esp_http_client_fetch_headers(client);
    int cap = content_length > 0 ? content_length : 4096;
    uint8_t *buf = malloc(cap > 0 ? cap : 1);
    if (!buf) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }
    int total = 0;
    while (1) {
        int space = cap - total;
        if (space <= 0) {
            cap = cap ? (cap * 2) : 4096;
            uint8_t *nb = realloc(buf, cap);
            if (!nb) {
                free(buf);
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return false;
            }
            buf = nb;
            space = cap - total;
        }
        int r = esp_http_client_read(client, (char*)buf + total, space);
        if (r <= 0) break;
        total += r;
    }
    if (out_buf) *out_buf = buf; else free(buf);
    if (out_len) *out_len = total;
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return total > 0;
}

// ===== 天气更新：解析/构建 =====
static bool get_optimal_api_url(char *out, size_t out_sz)
{
    char *txt = NULL; int len = 0;
    if (!http_get_text_alloc(WEATHER_URL_OPTIMAL, &txt, &len)) return false;
    bool ok = false;
    cJSON *root = cJSON_ParseWithLength(txt, len);
    if (root) {
        cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
        cJSON *api  = cJSON_GetObjectItemCaseSensitive(root, "api");
        if (cJSON_IsNumber(code) && code->valueint == 200 && cJSON_IsString(api) && api->valuestring) {
            snprintf(out, out_sz, "%sapi/tianqi/tqybip.php", api->valuestring);
            ok = true;
        }
        cJSON_Delete(root);
    }
    free(txt);
    return ok;
}


typedef struct {
    int wd1, wd2;
    int humidity;
    float temp_now;
    char country[48];
    char province[48];
    char city[48];
    char name[64];
    char updatetime[48];
    uint8_t *png;
    size_t png_len;

    // 新增：当前白天/夜晚的描述与图标
    char now_w1[32];
    char now_w2[32];
    uint8_t *now_img1; size_t now_img1_len;
    uint8_t *now_img2; size_t now_img2_len;

    // 新增：未来三天（明天、后天、大后天）
    // day1 -> weatherday2, day2 -> weatherday3, day3 -> weatherday4
    char d1_date[20]; int d1_wd1, d1_wd2; char d1_w1[32], d1_w2[32];
    uint8_t *d1_img1; size_t d1_img1_len; uint8_t *d1_img2; size_t d1_img2_len;

    char d2_date[20]; int d2_wd1, d2_wd2; char d2_w1[32], d2_w2[32];
    uint8_t *d2_img1; size_t d2_img1_len; uint8_t *d2_img2; size_t d2_img2_len;

    char d3_date[20]; int d3_wd1, d3_wd2; char d3_w1[32], d3_w2[32];
    uint8_t *d3_img1; size_t d3_img1_len; uint8_t *d3_img2; size_t d3_img2_len;

    // 新增：气象预警
    bool has_alarm;
    char alarm_title[128];
    char alarm_effective[64];
    char alarm_severity[32];
} weather_ui_t;

static void tasks_apply_weather(void *param)
{
    weather_ui_t *w = (weather_ui_t*)param;
    if (!w) return;

    if (bsp_display_lock(150)) {
        // 温度范围（升序显示）
        if (uic_TempNow) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Temp: %.1f℃",w->temp_now);
            lv_label_set_text(uic_TempNow, buf);
        }
        // 湿度
        if (uic_Humidity) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Humidity: %d%%", w->humidity);
            lv_label_set_text(uic_Humidity, buf);
        }
        // 文本信息
        if (uic_country)   lv_label_set_text(uic_country,   w->country);
        if (uic_province)  lv_label_set_text(uic_province,  w->province); 
        if (uic_city)      lv_label_set_text(uic_city,      w->city);
        if (uic_updatetime)lv_label_set_text(uic_updatetime,w->updatetime);

        // 首页天气图标
        if (uic_WeatherIcon && w->png && w->png_len > 0) {
            if (s_weather_png) { free(s_weather_png); s_weather_png = NULL; s_weather_png_len = 0; }
            s_weather_png = w->png;
            s_weather_png_len = w->png_len;
            s_weather_png_dsc.header.cf = CF_RAW;
            s_weather_png_dsc.data = s_weather_png;
            s_weather_png_dsc.data_size = s_weather_png_len;
            IMG_SET_SRC(uic_WeatherIcon, &s_weather_png_dsc);
            w->png = NULL; // 转移所有权
        }

        // 天气页：当前白天/夜晚
        if (uic_Nowweathertemp1) { char buf[16]; snprintf(buf, sizeof(buf), "%d℃", w->wd1); lv_label_set_text(uic_Nowweathertemp1, buf); }
        if (uic_Nowweatherlabel1) lv_label_set_text(uic_Nowweatherlabel1, w->now_w1);
        if (w->now_img1 && w->now_img1_len > 0) img_holder_set(uic_Nowweather1img, &s_icon_now1, w->now_img1, w->now_img1_len), w->now_img1=NULL;

        if (uic_Nowweathertemp2) { char buf[16]; snprintf(buf, sizeof(buf), "%d℃", w->wd2); lv_label_set_text(uic_Nowweathertemp2, buf); }
        if (uic_Nowweatherlabel2) lv_label_set_text(uic_Nowweatherlabel2, w->now_w2);
        if (w->now_img2 && w->now_img2_len > 0) img_holder_set(uic_Nowweather2img, &s_icon_now2, w->now_img2, w->now_img2_len), w->now_img2=NULL;

        // 天气页：未来第1天（明天）
        if (uic_WeatherDate1)     lv_label_set_text(uic_WeatherDate1, w->d1_date);
        if (uic_weather1temp1)    { char buf[16]; snprintf(buf, sizeof(buf), "%d℃", w->d1_wd1); lv_label_set_text(uic_weather1temp1, buf); }
        if (uic_weather1label1)   lv_label_set_text(uic_weather1label1, w->d1_w1);
        if (w->d1_img1 && w->d1_img1_len > 0) img_holder_set(uic_weather1img1, &s_icon_w1d, w->d1_img1, w->d1_img1_len), w->d1_img1=NULL;
        if (uic_weather1temp2)    { char buf[16]; snprintf(buf, sizeof(buf), "%d℃", w->d1_wd2); lv_label_set_text(uic_weather1temp2, buf); }
        if (uic_weather1label2)   lv_label_set_text(uic_weather1label2, w->d1_w2);
        if (w->d1_img2 && w->d1_img2_len > 0) img_holder_set(uic_weather1img2, &s_icon_w1n, w->d1_img2, w->d1_img2_len), w->d1_img2=NULL;

        // 天气页：未来第2天
        if (uic_WeatherDate2)     lv_label_set_text(uic_WeatherDate2, w->d2_date);
        if (uic_weather2temp1)    { char buf[16]; snprintf(buf, sizeof(buf), "%d℃", w->d2_wd1); lv_label_set_text(uic_weather2temp1, buf); }
        if (uic_weather2label1)   lv_label_set_text(uic_weather2label1, w->d2_w1);
        if (w->d2_img1 && w->d2_img1_len > 0) img_holder_set(uic_weather2img1, &s_icon_w2d, w->d2_img1, w->d2_img1_len), w->d2_img1=NULL;
        if (uic_weather2temp2)    { char buf[16]; snprintf(buf, sizeof(buf), "%d℃", w->d2_wd2); lv_label_set_text(uic_weather2temp2, buf); }
        if (uic_weather2label2)   lv_label_set_text(uic_weather2label2, w->d2_w2);
        if (w->d2_img2 && w->d2_img2_len > 0) img_holder_set(uic_weather2img2, &s_icon_w2n, w->d2_img2, w->d2_img2_len), w->d2_img2=NULL;

        // 天气页：未来第3天
        if (uic_WeatherDate3)     lv_label_set_text(uic_WeatherDate3, w->d3_date);
        if (uic_weather3temp1)    { char buf[16]; snprintf(buf, sizeof(buf), "%d℃", w->d3_wd1); lv_label_set_text(uic_weather3temp1, buf); }
        if (uic_weather3label1)   lv_label_set_text(uic_weather3label1, w->d3_w1);
        if (w->d3_img1 && w->d3_img1_len > 0) img_holder_set(uic_weather3img1, &s_icon_w3d, w->d3_img1, w->d3_img1_len), w->d3_img1=NULL;
        if (uic_weather3temp2)    { char buf[16]; snprintf(buf, sizeof(buf), "%d℃", w->d3_wd2); lv_label_set_text(uic_weather3temp2, buf); }
        if (uic_weather3label2)   lv_label_set_text(uic_weather3label2, w->d3_w2);
        if (w->d3_img2 && w->d3_img2_len > 0) img_holder_set(uic_weather3img2, &s_icon_w3n, w->d3_img2, w->d3_img2_len), w->d3_img2=NULL;

        // 预警
        if (uic_Alarm) {
            if (w->has_alarm) {
                lv_obj_clear_flag(uic_Alarm, LV_OBJ_FLAG_HIDDEN);
                if (uic_Title)     lv_label_set_text(uic_Title, w->alarm_title);
                if (uic_AlarmDate) lv_label_set_text(uic_AlarmDate, w->alarm_effective);
                //根据effective和severity决定uic_Alarm背景颜色
                if (uic_AlarmIcon) {
                    // 简易映射：红色（严重）、橙色（较重）、黄色（一般）、蓝色（轻微）
                    if (strcmp(w->alarm_severity, "红色") == 0) {
                        lv_obj_set_style_bg_color(uic_Alarm, lv_palette_main(LV_PALETTE_RED), 0);
                    } else if (strcmp(w->alarm_severity, "橙色") == 0) {
                        lv_obj_set_style_bg_color(uic_Alarm, lv_palette_main(LV_PALETTE_ORANGE), 0);
                    } else if (strcmp(w->alarm_severity, "黄色") == 0) {
                        lv_obj_set_style_bg_color(uic_Alarm, lv_palette_main(LV_PALETTE_YELLOW), 0);
                    } else if (strcmp(w->alarm_severity, "蓝色") == 0) {
                        lv_obj_set_style_bg_color(uic_Alarm, lv_palette_main(LV_PALETTE_BLUE), 0);
                    } else {
                        lv_obj_set_style_bg_color(uic_Alarm, lv_palette_main(LV_PALETTE_GREY), 0);
                    }
                }

                // uic_AlarmIcon 若需要图标映射，可在此按 w->alarm_severity 设置
            } else {
                lv_obj_add_flag(uic_Alarm, LV_OBJ_FLAG_HIDDEN);
            }
        }

        bsp_display_unlock();
    }

    // 释放未转移的缓冲
    if (w->png) free(w->png);
    if (w->now_img1) free(w->now_img1);
    if (w->now_img2) free(w->now_img2);
    if (w->d1_img1) free(w->d1_img1);
    if (w->d1_img2) free(w->d1_img2);
    if (w->d2_img1) free(w->d2_img1);
    if (w->d2_img2) free(w->d2_img2);
    if (w->d3_img1) free(w->d3_img1);
    if (w->d3_img2) free(w->d3_img2);
    free(w);
}

static int cj_get_int(cJSON *obj, const char *key, int defv)
{
    cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(it)) return it->valueint;
    if (cJSON_IsString(it) && it->valuestring) return atoi(it->valuestring);
    return defv;
}

static const char* cj_get_strz(cJSON *obj, const char *key)
{
    cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    return (cJSON_IsString(it) && it->valuestring) ? it->valuestring : "";
}

// 新增：读取浮点类型
static float cj_get_float(cJSON *obj, const char *key, float defv)
{
    cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(it)) return (float)it->valuedouble;
    if (cJSON_IsString(it) && it->valuestring) {
        float v;
        if (sscanf(it->valuestring, "%f", &v) == 1) return v;
    }
    return defv;
}

static bool weather_fetch_once(void)
{
    char api_url[256] = {0};
    if (!get_optimal_api_url(api_url, sizeof(api_url))) {
        ESP_LOGW(TAG, "Get optimal API failed");
        return false;
    }

    // 构造请求 URL
    char url[512];
    snprintf(url, sizeof(url), "%s?id=%s&key=%s&day=%d",
                api_url, WEATHER_USER_ID, WEATHER_USER_KEY, 4);
 
    char *txt = NULL; int tlen = 0;
    if (!http_get_text_alloc(url, &txt, &tlen)) {
        ESP_LOGW(TAG, "Weather HTTP failed");
        return false;
    }

    bool ok = false;
    cJSON *root = cJSON_ParseWithLength(txt, tlen);
    if (!root) {
        ESP_LOGW(TAG, "Weather JSON parse failed");
        free(txt);
        return false;
    }

    cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
    if (!cJSON_IsNumber(code) || code->valueint != 200) {
        ESP_LOGW(TAG, "Weather JSON code invalid");
        goto L_END;
    }

    // 基本信息
    const char *guo  = cj_get_strz(root, "guo");
    const char *sheng= cj_get_strz(root, "sheng");
    const char *shi  = cj_get_strz(root, "shi");
    const char *name = cj_get_strz(root, "name");
    const char *uptime = cj_get_strz(root, "uptime");
    int Nowwd1 = cj_get_int(root, "wd1", 0);
    int Nowwd2 = cj_get_int(root, "wd2", 0);
    const char *now_w1 = cj_get_strz(root, "weather1");
    const char *now_w2 = cj_get_strz(root, "weather2");

    // 当前湿度/温度
    int humidity = 0;
    float temp_now = 0.0f;
    cJSON *nowinfo = cJSON_GetObjectItemCaseSensitive(root, "nowinfo");
    if (cJSON_IsObject(nowinfo)) {
        humidity = cj_get_int(nowinfo, "humidity", 0);
        temp_now = cj_get_float(nowinfo, "temperature", 0.0f);
    }


    // 根据时段选择首页图标
    int hour = 12;
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    hour = tm_info.tm_hour;
    const char *img_key = (hour < 12) ? "weather1img" : "weather2img";
    const char *home_img_url = cj_get_strz(root, img_key);

    // 当前白天/夜晚图标
    const char *now_img1_url = cj_get_strz(root, "weather1img");
    const char *now_img2_url = cj_get_strz(root, "weather2img");

    // 未来三天
    cJSON *day2 = cJSON_GetObjectItemCaseSensitive(root, "weatherday2");
    cJSON *day3 = cJSON_GetObjectItemCaseSensitive(root, "weatherday3");
    cJSON *day4 = cJSON_GetObjectItemCaseSensitive(root, "weatherday4");

    // 预警
    bool has_alarm = false;
    char alarm_title[128] = {0};
    char alarm_effective[64] = {0};
    char alarm_severity[32] = {0};
    cJSON *alarm = cJSON_GetObjectItemCaseSensitive(root, "alarm");
    if (cJSON_IsObject(alarm)) {
        has_alarm = true;
        snprintf(alarm_title, sizeof(alarm_title), "%s", cj_get_strz(alarm, "title"));
        snprintf(alarm_effective, sizeof(alarm_effective), "%s", cj_get_strz(alarm, "effective"));
        snprintf(alarm_severity, sizeof(alarm_severity), "%s", cj_get_strz(alarm, "severity"));
    }

    // 下载 PNG（按需）
    uint8_t *home_png = NULL; int home_png_len = 0;
    if (home_img_url && home_img_url[0]) {
        if (!http_get_bin_alloc(home_img_url, &home_png, &home_png_len)) {
            ESP_LOGW(TAG, "Weather icon download failed(home)");
        }
    }
    uint8_t *now_png1=NULL; int now_png1_len=0;
    if (now_img1_url && now_img1_url[0]) http_get_bin_alloc(now_img1_url, &now_png1, &now_png1_len);
    uint8_t *now_png2=NULL; int now_png2_len=0;
    if (now_img2_url && now_img2_url[0]) http_get_bin_alloc(now_img2_url, &now_png2, &now_png2_len);

    // 未来三天解析与下载
    char d1_date[20] = {0}, d2_date[20] = {0}, d3_date[20] = {0};
    int d1_wd1=0,d1_wd2=0,d2_wd1=0,d2_wd2=0,d3_wd1=0,d3_wd2=0;
    char d1_w1[32]={0}, d1_w2[32]={0}, d2_w1[32]={0}, d2_w2[32]={0}, d3_w1[32]={0}, d3_w2[32]={0};
    const char *d1_img1_url=NULL,*d1_img2_url=NULL,*d2_img1_url=NULL,*d2_img2_url=NULL,*d3_img1_url=NULL,*d3_img2_url=NULL;
    uint8_t *d1_img1=NULL; int d1_img1_len=0; uint8_t *d1_img2=NULL; int d1_img2_len=0;
    uint8_t *d2_img1=NULL; int d2_img1_len=0; uint8_t *d2_img2=NULL; int d2_img2_len=0;
    uint8_t *d3_img1=NULL; int d3_img1_len=0; uint8_t *d3_img2=NULL; int d3_img2_len=0;

    if (cJSON_IsObject(day2)) {
        snprintf(d1_date, sizeof(d1_date), "%s", cj_get_strz(day2, "date"));
        d1_wd1 = cj_get_int(day2, "wd1", 0);
        d1_wd2 = cj_get_int(day2, "wd2", 0);
        snprintf(d1_w1, sizeof(d1_w1), "%s", cj_get_strz(day2, "weather1"));
        snprintf(d1_w2, sizeof(d1_w2), "%s", cj_get_strz(day2, "weather2"));
        d1_img1_url = cj_get_strz(day2, "weather1img");
        d1_img2_url = cj_get_strz(day2, "weather2img");
        if (d1_img1_url && d1_img1_url[0]) http_get_bin_alloc(d1_img1_url, &d1_img1, &d1_img1_len);
        if (d1_img2_url && d1_img2_url[0]) http_get_bin_alloc(d1_img2_url, &d1_img2, &d1_img2_len);
    }
    if (cJSON_IsObject(day3)) {
        snprintf(d2_date, sizeof(d2_date), "%s", cj_get_strz(day3, "date"));
        d2_wd1 = cj_get_int(day3, "wd1", 0);
        d2_wd2 = cj_get_int(day3, "wd2", 0);
        snprintf(d2_w1, sizeof(d2_w1), "%s", cj_get_strz(day3, "weather1"));
        snprintf(d2_w2, sizeof(d2_w2), "%s", cj_get_strz(day3, "weather2"));
        d2_img1_url = cj_get_strz(day3, "weather1img");
        d2_img2_url = cj_get_strz(day3, "weather2img");
        if (d2_img1_url && d2_img1_url[0]) http_get_bin_alloc(d2_img1_url, &d2_img1, &d2_img1_len);
        if (d2_img2_url && d2_img2_url[0]) http_get_bin_alloc(d2_img2_url, &d2_img2, &d2_img2_len);
    }
    if (cJSON_IsObject(day4)) {
        snprintf(d3_date, sizeof(d3_date), "%s", cj_get_strz(day4, "date"));
        d3_wd1 = cj_get_int(day4, "wd1", 0);
        d3_wd2 = cj_get_int(day4, "wd2", 0);
        snprintf(d3_w1, sizeof(d3_w1), "%s", cj_get_strz(day4, "weather1"));
        snprintf(d3_w2, sizeof(d3_w2), "%s", cj_get_strz(day4, "weather2"));
        d3_img1_url = cj_get_strz(day4, "weather1img");
        d3_img2_url = cj_get_strz(day4, "weather2img");
        if (d3_img1_url && d3_img1_url[0]) http_get_bin_alloc(d3_img1_url, &d3_img1, &d3_img1_len);
        if (d3_img2_url && d3_img2_url[0]) http_get_bin_alloc(d3_img2_url, &d3_img2, &d3_img2_len);
    }

    // 组装 UI 数据，异步应用
    weather_ui_t *w = calloc(1, sizeof(weather_ui_t));
    if (w) {
        w->wd1 = Nowwd1;
        w->wd2 = Nowwd2;
        w->humidity = humidity;
        w->temp_now = temp_now; // 新增：写入当前温度
        snprintf(w->country,  sizeof(w->country),  "%s", guo);
        snprintf(w->province, sizeof(w->province), "%s", sheng);
        snprintf(w->city,     sizeof(w->city),     "%s", shi);
        snprintf(w->name,     sizeof(w->name),     "%s", name);
        snprintf(w->updatetime,sizeof w->updatetime, "%s", uptime);

        w->png = home_png; w->png_len = (size_t)home_png_len;

        snprintf(w->now_w1, sizeof(w->now_w1), "%s", now_w1);
        snprintf(w->now_w2, sizeof(w->now_w2), "%s", now_w2);
        w->now_img1 = now_png1; w->now_img1_len = (size_t)now_png1_len;
        w->now_img2 = now_png2; w->now_img2_len = (size_t)now_png2_len;

        snprintf(w->d1_date, sizeof(w->d1_date), "%s", d1_date);
        w->d1_wd1 = d1_wd1; w->d1_wd2 = d1_wd2;
        snprintf(w->d1_w1, sizeof(w->d1_w1), "%s", d1_w1);
        snprintf(w->d1_w2, sizeof(w->d1_w2), "%s", d1_w2);
        w->d1_img1 = d1_img1; w->d1_img1_len = (size_t)d1_img1_len;
        w->d1_img2 = d1_img2; w->d1_img2_len = (size_t)d1_img2_len;

        snprintf(w->d2_date, sizeof(w->d2_date), "%s", d2_date);
        w->d2_wd1 = d2_wd1; w->d2_wd2 = d2_wd2;
        snprintf(w->d2_w1, sizeof(w->d2_w1), "%s", d2_w1);
        snprintf(w->d2_w2, sizeof(w->d2_w2), "%s", d2_w2);
        w->d2_img1 = d2_img1; w->d2_img1_len = (size_t)d2_img1_len;
        w->d2_img2 = d2_img2; w->d2_img2_len = (size_t)d2_img2_len;

        snprintf(w->d3_date, sizeof(w->d3_date), "%s", d3_date);
        w->d3_wd1 = d3_wd1; w->d3_wd2 = d3_wd2;
        snprintf(w->d3_w1, sizeof(w->d3_w1), "%s", d3_w1);
        snprintf(w->d3_w2, sizeof(w->d3_w2), "%s", d3_w2);
        w->d3_img1 = d3_img1; w->d3_img1_len = (size_t)d3_img1_len;
        w->d3_img2 = d3_img2; w->d3_img2_len = (size_t)d3_img2_len;

        w->has_alarm = has_alarm;
        snprintf(w->alarm_title, sizeof(w->alarm_title), "%s", alarm_title);
        snprintf(w->alarm_effective, sizeof(w->alarm_effective), "%s", alarm_effective);
        snprintf(w->alarm_severity, sizeof(w->alarm_severity), "%s", alarm_severity);

        lv_async_call(tasks_apply_weather, w);
        ok = true;
    } else {
        if (home_png) free(home_png);
        if (now_png1) free(now_png1);
        if (now_png2) free(now_png2);
        if (d1_img1) free(d1_img1);
        if (d1_img2) free(d1_img2);
        if (d2_img1) free(d2_img1);
        if (d2_img2) free(d2_img2);
        if (d3_img1) free(d3_img1);
        if (d3_img2) free(d3_img2);
    }

L_END:
    cJSON_Delete(root);
    free(txt);
    return ok;
}

static void weather_task(void *arg)
{
    (void)arg;
    // 初次稍等，确保 SNTP/网络稳定
    vTaskDelay(pdMS_TO_TICKS(1000));
    for (;;) {
        // 等待拿到 IP
        EventBits_t b = xEventGroupGetBits(s_evt_group);
        if (!(b & TASKS_EVT_GOT_IP)) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        bool ok = weather_fetch_once();

        // 等待“立即刷新”或超时到期
        TickType_t wait_ticks = ok ? pdMS_TO_TICKS(WEATHER_REFRESH_MINUTES * 60 * 1000) : pdMS_TO_TICKS(5000);
        // clearOnExit = true, waitForAllBits = false
        xEventGroupWaitBits(s_evt_group, TASKS_EVT_WEATHER_FORCE, pdTRUE, pdFALSE, wait_ticks);
        // 循环后再次获取
    }
}

// 启动天气任务（只创建一次）
static void tasks_start_weather_if_needed(void)
{
    if (s_weather_task_handle == NULL) {
        xTaskCreatePinnedToCore(weather_task, "weather", 8192, NULL, 4, &s_weather_task_handle, 1);
    }
}

// 新增：对外暴露的“立即刷新”接口（UI 进入天气页时调用）
void tasks_request_weather_now(void)
{
    tasks_start_weather_if_needed();
    if (s_evt_group) {
        xEventGroupSetBits(s_evt_group, TASKS_EVT_WEATHER_FORCE);
    }
}

// 在文件顶部附近添加：PNG 解码器初始化
static void tasks_init_img_decoders(void)
{
    static bool inited = false;
    if (inited) return;
    inited = true;

    // 你提到已启用 LV_USE_LIBPNG 和 LV_USE_LODEPNG，二者都尝试初始化
    #if LV_USE_LODEPNG
    extern void lv_lodepng_init(void);
    lv_lodepng_init();
    #endif

    #if LV_USE_LIBPNG
    extern void lv_libpng_init(void);
    lv_libpng_init();
    #endif

    // 若你同时启用了内置 PNG 解码器
    #if LV_USE_PNG
    extern void lv_png_init(void);
    lv_png_init();
    #endif
}
