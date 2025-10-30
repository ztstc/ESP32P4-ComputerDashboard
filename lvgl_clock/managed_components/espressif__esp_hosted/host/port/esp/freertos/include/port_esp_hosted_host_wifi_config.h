/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __PORT_ESP_HOSTED_HOST_WIFI_CONFIG_H__
#define __PORT_ESP_HOSTED_HOST_WIFI_CONFIG_H__

#include "esp_idf_version.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 1)
#error ESP-IDF version used is not supported
#endif

#if CONFIG_ESP_HOSTED_ENABLE_ITWT && CONFIG_SLAVE_SOC_WIFI_HE_SUPPORT
  #define H_WIFI_HE_SUPPORT 1
#else
  #define H_WIFI_HE_SUPPORT 0
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
  /* dual band API support available */
  #define H_WIFI_DUALBAND_SUPPORT 1
#else
  #define H_WIFI_DUALBAND_SUPPORT 0
#endif

#ifdef CONFIG_ESP_WIFI_REMOTE_EAP_ENABLED
  #define H_WIFI_ENTERPRISE_SUPPORT 1
#else
  #define H_WIFI_ENTERPRISE_SUPPORT 0
#endif

/* ESP-IDF 5.5.0 breaking change: reserved/he_reserved renamed to reserved1/reserved2 */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  #define H_WIFI_NEW_RESERVED_FIELD_NAMES 1
  #define H_PRESENT_IN_ESP_IDF_5_5_0      1
#else
  #define H_WIFI_NEW_RESERVED_FIELD_NAMES 0
  #define H_PRESENT_IN_ESP_IDF_5_5_0      0
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
  #define H_PRESENT_IN_ESP_IDF_5_4_0      1
#else
  #define H_PRESENT_IN_ESP_IDF_5_4_0      0
#endif

/* User-controllable reserved field decoding - works regardless of IDF version */
#ifdef CONFIG_ESP_HOSTED_DECODE_WIFI_RESERVED_FIELD
  #define H_DECODE_WIFI_RESERVED_FIELD 1
#else
  #define H_DECODE_WIFI_RESERVED_FIELD 0
#endif

/*
 * wifi_twt_config_t::twt_enable_keep_alive only found in
 * IDF v5.3.2 and above
 */
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 3, 1)
#define H_GOT_TWT_ENABLE_KEEP_ALIVE 1
#else
#define H_GOT_TWT_ENABLE_KEEP_ALIVE 0
#endif

/* wifi_ap_config_t::transition_disable only found in
 * IDF v5.3.3 and above, or
 * IDF v5.4.1 and above
 */
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 3)) || (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 4, 1))
#define H_GOT_AP_CONFIG_PARAM_TRANSITION_DISABLE 0
#else
#define H_GOT_AP_CONFIG_PARAM_TRANSITION_DISABLE 1
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
  #define H_PRESENT_IN_ESP_IDF_6_0_0      1
#else
  #define H_PRESENT_IN_ESP_IDF_6_0_0      0
#endif

#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 4)) || (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 4, 3)) || ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(5, 5, 0)
#define H_GOT_SET_EAP_METHODS_API 0
#else
#define H_GOT_SET_EAP_METHODS_API 1
#endif
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 4)) || (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 4, 3))
#define H_GOT_EAP_SET_DOMAIN_NAME 0
#else
#define H_GOT_EAP_SET_DOMAIN_NAME 1
#endif

#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 4, 3))
#define H_GOT_EAP_OKC_SUPPORT 0
#else
#define H_GOT_EAP_OKC_SUPPORT 1
#endif

#endif /* __ESP_HOSTED_WIFI_CONFIG_H__ */
