#pragma once
#ifdef __cplusplus
extern "C" {
#endif

void app_tasks_init(void);
void tasks_on_got_ip(void);
void tasks_try_auto_connect(void);
void tasks_notify_wifi_started(void);

// 新增：电脑系统信息监控
void tasks_start_pc_monitor(const char *ip); // ip 为 NULL 或 "" 时忽略
void tasks_stop_pc_monitor(void);

// 前向声明：天气任务启动（用于 tasks_on_got_ip 中调用）
static void tasks_start_weather_if_needed(void);
// 新增：PNG 解码器初始化的前置声明，避免隐式声明
static void tasks_init_img_decoders(void);

#ifdef __cplusplus
}
#endif
