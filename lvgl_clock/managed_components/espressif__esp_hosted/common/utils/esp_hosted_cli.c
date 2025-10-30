/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include "esp_hosted_cli.h"
#include "sdkconfig.h"
#include "esp_idf_version.h"

#include "esp_console.h"

#ifdef CONFIG_ESP_HOSTED_ENABLED
#include "port_esp_hosted_host_config.h"
#include "esp_hosted_power_save.h"
#endif

#ifdef H_ESP_HOSTED_CLI_ENABLED

#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_heap_caps.h>

#ifdef CONFIG_HEAP_TRACING
 #include <esp_heap_trace.h>
#endif


#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_wifi.h>
#include "lwip/sockets.h"

/* This should be agnostic, later */
#ifdef CONFIG_ESP_HOSTED_COPROCESSOR
  #include "iperf.h"
  #include "wifi_cmd.h"
  #include "iperf_cmd.h"
  #include "ping_cmd.h"
#endif

static const char *TAG = "esp_cli";

static int task_dump_cli_handler(int argc, char *argv[])
{
	/* Just to go to the next line */
	printf("\n");
#ifndef CONFIG_FREERTOS_USE_TRACE_FACILITY
	printf("%s: To use this utility enable: Component config > FreeRTOS > Kernel > configUSE_TRACEFACILITY\n", TAG);
#else
	int num_of_tasks = uxTaskGetNumberOfTasks();
	TaskStatus_t *task_array = calloc(1, num_of_tasks * sizeof(TaskStatus_t));
	if (!task_array) {
		ESP_LOGE(TAG, "Memory not allocated for task list.");
		return 0;
	}
	num_of_tasks = uxTaskGetSystemState(task_array, num_of_tasks, NULL);
	printf("\tName\tNumber\tPriority\tStackWaterMark\n");
	for (int i = 0; i < num_of_tasks; i++) {
		printf("%16s\t%u\t%u\t%u\n",
				task_array[i].pcTaskName,
				(unsigned) task_array[i].xTaskNumber,
				(unsigned) task_array[i].uxCurrentPriority,
				(unsigned) task_array[i].usStackHighWaterMark);
	}
	free(task_array);
#endif
	return 0;
}

static int cpu_dump_cli_handler(int argc, char *argv[])
{
	/* Just to go to the next line */
	printf("\n");
#ifndef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
	printf("%s: To use this utility enable: Component config > FreeRTOS > Kernel > configGENERATE_RUN_TIME_STATS\n", TAG);
#else
	char *buf = calloc(1, 2 * 1024);
	vTaskGetRunTimeStats(buf);
	printf("%s: Run Time Stats:\n%s\n", TAG, buf);
	free(buf);
#endif
	return 0;
}

static void print_heap_stats(void)
{
	uint32_t freeSize = esp_get_free_heap_size();
	printf("The available total size of heap:%" PRIu32 "\n", freeSize);

	printf("\tDescription\tInternal\tSPIRAM\n");
	printf("Current Free Memory\t%d\t\t%d\n",
			heap_caps_get_free_size(MALLOC_CAP_8BIT) - heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
			heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
	printf("Largest Free Block\t%d\t\t%d\n",
			heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
			heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
	printf("Min. Ever Free Size\t%d\t\t%d\n",
			heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
			heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
}

static int mem_dump_cli_handler(int argc, char *argv[])
{
	/* Just to go to the next line */
	printf("\n");
	print_heap_stats();
	return 0;
}

#ifdef CONFIG_HEAP_TRACING
static int heap_trace_records;
static heap_trace_record_t *heap_trace_records_buf;
static void cli_heap_trace_start(void)
{
	/* Just to go to the next line */
	printf("\n");
	if (!heap_trace_records_buf) {
		heap_trace_records_buf = malloc(heap_trace_records * sizeof(heap_trace_record_t));
		if (!heap_trace_records_buf) {
			printf("%s: Failed to allocate records buffer\n", TAG);
			return;
		}
		if (heap_trace_init_standalone(heap_trace_records_buf, heap_trace_records) != ESP_OK) {
			printf("%s: Failed to initialise tracing\n", TAG);
			goto error1;
		}
	}
	if (heap_trace_start(HEAP_TRACE_LEAKS) != ESP_OK) {
		printf("%s: Failed to start heap trace\n", TAG);
		goto error2;
	}
	return;
error2:
	heap_trace_init_standalone(NULL, 0);
error1:
	free(heap_trace_records_buf);
	heap_trace_records_buf = NULL;
	return;
}

static void cli_heap_trace_stop(void)
{
	/* Just to go to the next line */
	printf("\n");
	if (!heap_trace_records_buf) {
		printf("%s: Tracing not started?\n", TAG);
		return;
	}
	heap_trace_stop();
	heap_trace_dump();
	heap_trace_init_standalone(NULL, 0);
	free(heap_trace_records_buf);
	heap_trace_records_buf = NULL;
}
#endif

static int heap_trace_cli_handler(int argc, char *argv[])
{
	/* Just to go to the next line */
	printf("\n");
#ifndef CONFIG_HEAP_TRACING
	printf("%s: To use this utility enable: Component config --> Heap memory debugging --> Enable heap tracing\n", TAG);
#else
	if (argc < 2) {
		printf("%s: Incorrect arguments\n", TAG);
		return 0;
	}
	if (strcmp(argv[1], "start") == 0) {
#define DEFAULT_HEAP_TRACE_RECORDS 200
		if (argc != 3) {
			heap_trace_records = DEFAULT_HEAP_TRACE_RECORDS;
		} else {
			heap_trace_records = atoi(argv[2]);
		}
		printf("%s: Using a buffer to trace %d records\n", TAG, heap_trace_records);
		cli_heap_trace_start();
	} else if (strcmp(argv[1], "stop") == 0) {
		cli_heap_trace_stop();
	} else {
		printf("%s: Invalid argument:%s:\n", TAG, argv[1]);
	}
#endif
	return 0;
}

static int reboot_cli_handler(int argc, char *argv[])
{
	/* Just to go to the next line */
	printf("\n");
	esp_unregister_shutdown_handler((shutdown_handler_t)esp_wifi_stop);
	esp_restart();
	return 0;
}

static int crash_device_handler(int argc, char** argv)
{
	printf("Crashing the device now...\n");

	// Writing at invalid address
	*(int *) (0x0) = 0;
	return ESP_OK;
}

static int sock_dump_cli_handler(int argc, char *argv[])
{
	printf("\n");
	int i, ret, used_sockets = 0;

	struct sockaddr_in local_sock, peer_sock;
	socklen_t local_sock_len = sizeof(struct sockaddr_in), peer_sock_len = sizeof(struct sockaddr_in);
	char local_ip_addr[16], peer_ip_addr[16];
	unsigned int local_port, peer_port;

	int sock_type;
	socklen_t sock_type_len;

#define TOTAL_NUM_SOCKETS MEMP_NUM_NETCONN
	printf("sock_fd\tprotocol\tlocal_addr\t\tpeer_addr\n");
	for (i = LWIP_SOCKET_OFFSET; i < LWIP_SOCKET_OFFSET + TOTAL_NUM_SOCKETS; i++) {
		memset(&local_sock, 0, sizeof(struct sockaddr_in));
		memset(&peer_sock, 0, sizeof(struct sockaddr_in));
		local_sock_len = sizeof(struct sockaddr);
		peer_sock_len = sizeof(struct sockaddr);
		memset(local_ip_addr, 0, sizeof(local_ip_addr));
		memset(peer_ip_addr, 0, sizeof(peer_ip_addr));
		local_port = 0;
		peer_port = 0;
		sock_type = 0;
		sock_type_len = sizeof(int);

		ret = getsockname(i, (struct sockaddr *)&local_sock, &local_sock_len);
		if (ret >= 0) {
			used_sockets++;
			inet_ntop(AF_INET, &local_sock.sin_addr, local_ip_addr, sizeof(local_ip_addr));
			local_port = ntohs(local_sock.sin_port);
			getsockopt(i, SOL_SOCKET, SO_TYPE, &sock_type, &sock_type_len);
			printf("%d\t%d:%s\t%16s:%d", i, sock_type, sock_type == SOCK_STREAM ? "tcp" : sock_type == SOCK_DGRAM ? "udp" : "raw", local_ip_addr, local_port);

			ret = getpeername(i, (struct sockaddr *)&peer_sock, &peer_sock_len);
			if (ret >= 0) {
				inet_ntop(AF_INET, &peer_sock.sin_addr, peer_ip_addr, sizeof(peer_ip_addr));
				peer_port = ntohs(peer_sock.sin_port);
				printf("\t%16s:%d", peer_ip_addr, peer_port);
			}
			printf("\n");
		}
	}
	printf("Remaining sockets: %d\n", TOTAL_NUM_SOCKETS - used_sockets);
	return 0;
}

#if defined(H_HOST_PS_ALLOWED)
#ifdef H_ESP_HOSTED_HOST
static int power_save_cli_handler(int argc, char *argv[])
{
	ESP_LOGI(TAG, "Putting ESP32-P4 into power save...");
	esp_hosted_power_save_start(HOSTED_POWER_SAVE_TYPE_DEEP_SLEEP);
	return 0;
}
#endif

#ifdef CONFIG_ESP_HOSTED_COPROCESSOR
static int wakeup_cli_handler(int argc, char *argv[])
{
	ESP_LOGI(TAG, "Asking host to wake-up...");
	wakeup_host(portMAX_DELAY);
	return 0;
}
#endif
#endif

static esp_console_cmd_t diag_cmds[] = {
	{
		.command = "crash",
		.help = "Crash the device writing at invalid address",
		.func = &crash_device_handler,
	},
	{
		.command = "reboot",
		.help = "Reboot the device",
		.func = reboot_cli_handler,
	},
	{
		.command = "mem-dump",
		.help = "Prints memory stats",
		.func = mem_dump_cli_handler,
	},
	{
		.command = "task-dump",
		.help = "Print task snapshots",
		.func = task_dump_cli_handler,
	},
	{
		.command = "cpu-dump",
		.help = "Print CPU consumption data at the moment",
		.func = cpu_dump_cli_handler,
	},
	{
		.command = "heap-trace",
		.help = "<start|stop> [trace-buf-size]",
		.func = heap_trace_cli_handler,
	},
	{
		.command = "sock-dump",
		.help = "",
		.func = sock_dump_cli_handler,
	},

#if defined(H_HOST_PS_ALLOWED)
#ifdef H_ESP_HOSTED_HOST
	{
		.command = "host-power-save",
		.help = "Put Host into power save",
		.func = power_save_cli_handler,
	},
#endif
#ifdef CONFIG_ESP_HOSTED_COPROCESSOR
	{
		.command = "wake-up-host",
		.help = "Wake-up host from power save",
		.func = wakeup_cli_handler,
	},
#endif
#endif
};

#ifdef CONFIG_ESP_HOSTED_COPROCESSOR

  #if CONFIG_ESP_WIFI_ENABLE_WIFI_TX_STATS || CONFIG_ESP_WIFI_ENABLE_WIFI_RX_STATS
    #include "esp_wifi_he.h"
  #endif

  #if CONFIG_ESP_WIFI_ENABLE_WIFI_TX_STATS
	extern int wifi_cmd_get_tx_statistics(int argc, char **argv);
	extern int wifi_cmd_clr_tx_statistics(int argc, char **argv);
  #endif

  #if CONFIG_ESP_WIFI_ENABLE_WIFI_RX_STATS
	extern int wifi_cmd_get_rx_statistics(int argc, char **argv);
	extern int wifi_cmd_clr_rx_statistics(int argc, char **argv);
  #endif

void iperf_hook_show_wifi_stats(iperf_traffic_type_t type, iperf_status_t status)
{
	if (status == IPERF_STARTED) {
  #if CONFIG_ESP_WIFI_ENABLE_WIFI_TX_STATS
		if (type != IPERF_UDP_SERVER) {
			wifi_cmd_clr_tx_statistics(0, NULL);
		}
  #endif
  #if CONFIG_ESP_WIFI_ENABLE_WIFI_RX_STATS
		if (type != IPERF_UDP_CLIENT) {
			wifi_cmd_clr_rx_statistics(0, NULL);
		}
  #endif
	}

	if (status == IPERF_STOPPED) {
  #if CONFIG_ESP_WIFI_ENABLE_WIFI_TX_STATS
		if (type != IPERF_UDP_SERVER) {
			wifi_cmd_get_tx_statistics(0, NULL);
		}
  #endif
  #if CONFIG_ESP_WIFI_ENABLE_WIFI_RX_STATS
		if (type != IPERF_UDP_CLIENT) {
			wifi_cmd_get_rx_statistics(0, NULL);
		}
  #endif
	}

}
#endif

int esp_cli_register_cmds(void)
{
	int cmds_num = sizeof(diag_cmds) / sizeof(esp_console_cmd_t);
	int i;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
	/* esp_console_cmd_deregister was added in ESP-IDF v5.4+ */
	const char* remove_cmd = "deep_sleep";
	ESP_LOGI(TAG, "Remove any existing deep_sleep cmd in cli");
	esp_console_cmd_deregister(remove_cmd);
#endif

	for (i = 0; i < cmds_num; i++) {
		ESP_LOGI(TAG, "Registering command: %s", diag_cmds[i].command);
		esp_console_cmd_register(&diag_cmds[i]);
	}

#ifdef CONFIG_ESP_HOSTED_COPROCESSOR

	app_register_all_wifi_commands();
	app_register_iperf_commands();
	app_register_ping_commands();
	app_register_iperf_hook_func(iperf_hook_show_wifi_stats);
#endif
	return 0;
}


#ifdef CONFIG_ESP_HOSTED_CLI_NEW_INSTANCE
int esp_hosted_cli_start(void)
{
	static int cli_started;
	if (cli_started) {
		return 0;
	}

	esp_console_repl_t *repl = NULL;
	esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
  #ifdef CONFIG_ESP_HOSTED_COPROCESSOR
	repl_config.prompt = "coprocessor> ";
  #elif defined(H_ESP_HOSTED_HOST)
	repl_config.prompt = "host> ";
  #endif
	esp_console_register_help_command();
	esp_cli_register_cmds();

  #if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
	esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

  #elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
	esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

  #elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
	esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));
  #else
    #error Unsupported console type
  #endif
	ESP_ERROR_CHECK(esp_console_start_repl(repl));
	cli_started = 1;
	return 0;
}
#else
int esp_hosted_cli_start(void)
{
	return esp_cli_register_cmds();
}
#endif

#endif /*ESP_HOSTED_CLI_ENABLED*/
