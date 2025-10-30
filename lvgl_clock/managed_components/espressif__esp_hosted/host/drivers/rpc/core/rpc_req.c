/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "rpc_core.h"
#include "rpc_slave_if.h"
#include "esp_hosted_rpc.h"
#include "port_esp_hosted_host_wifi_config.h"
#include "esp_hosted_transport.h"
#include "esp_hosted_bitmasks.h"
#include "esp_idf_version.h"
#include "port_esp_hosted_host_log.h"
#include "esp_hosted_os_abstraction.h"

DEFINE_LOG_TAG(rpc_req);

#define ADD_RPC_BUFF_TO_FREE_LATER(BuFf) {                                      \
	assert((app_req->n_rpc_free_buff_hdls+1)<=MAX_FREE_BUFF_HANDLES);           \
	app_req->rpc_free_buff_hdls[app_req->n_rpc_free_buff_hdls++] = BuFf;        \
}

#define RPC_ALLOC_ASSIGN(TyPe,MsG_StRuCt,InItFuNc)                            \
    TyPe *req_payload = (TyPe *)                                              \
        g_h.funcs->_h_calloc(1, sizeof(TyPe));                                \
    if (!req_payload) {                                                       \
        ESP_LOGE(TAG, "Failed to allocate memory for req->%s\n",#MsG_StRuCt);     \
        *failure_status = RPC_ERR_MEMORY_FAILURE;                              \
		return FAILURE;                                                       \
    }                                                                         \
    req->MsG_StRuCt = req_payload;                                             \
	InItFuNc(req_payload);                                                    \
    ADD_RPC_BUFF_TO_FREE_LATER((uint8_t*)req_payload);

//TODO: How this is different in slave_control.c
#define RPC_ALLOC_ELEMENT(TyPe,MsG_StRuCt,InIt_FuN) {                         \
    TyPe *NeW_AllocN = (TyPe *) g_h.funcs->_h_calloc(1, sizeof(TyPe));        \
    if (!NeW_AllocN) {                                                        \
        ESP_LOGE(TAG, "Failed to allocate memory for req->%s\n",#MsG_StRuCt);     \
        *failure_status = RPC_ERR_MEMORY_FAILURE;                              \
		return FAILURE;                                                       \
    }                                                                         \
    ADD_RPC_BUFF_TO_FREE_LATER((uint8_t*)NeW_AllocN);                         \
    MsG_StRuCt = NeW_AllocN;                                                  \
    InIt_FuN(MsG_StRuCt);                                                     \
}

/* RPC request is simple remote function invokation at slave from host
 *
 * For new RPC request, add up switch case for your message
 * If the RPC function to be invoked does not carry any arguments, just add
 * case in the top with intentional fall through
 * If any arguments are needed, you may have to add union for your message
 * in Ctrl_cmd_t in rpc_api.h and fill the request in new case
 *
 * For altogether new RPC function addition, please check
 * esp_hosted_fg/common/proto/esp_hosted_config.proto
 */
int compose_rpc_req(Rpc *req, ctrl_cmd_t *app_req, int32_t *failure_status)
{
	switch(req->msg_id) {

	case RPC_ID__Req_GetWifiMode:
	//case RPC_ID__Req_GetAPConfig:
	//case RPC_ID__Req_DisconnectAP:
	//case RPC_ID__Req_GetSoftAPConfig:
	//case RPC_ID__Req_GetSoftAPConnectedSTAList:
	//case RPC_ID__Req_StopSoftAP:
	case RPC_ID__Req_WifiGetPs:
	case RPC_ID__Req_OTABegin:
	case RPC_ID__Req_OTAEnd:
	case RPC_ID__Req_WifiDeinit:
	case RPC_ID__Req_WifiStart:
	case RPC_ID__Req_WifiStop:
	case RPC_ID__Req_WifiConnect:
	case RPC_ID__Req_WifiDisconnect:
	case RPC_ID__Req_WifiScanStop:
	case RPC_ID__Req_WifiScanGetApNum:
	case RPC_ID__Req_WifiClearApList:
	case RPC_ID__Req_WifiRestore:
	case RPC_ID__Req_WifiClearFastConnect:
	case RPC_ID__Req_WifiStaGetApInfo:
	case RPC_ID__Req_WifiGetMaxTxPower:
	case RPC_ID__Req_WifiGetChannel:
	case RPC_ID__Req_WifiGetCountryCode:
	case RPC_ID__Req_WifiGetCountry:
	case RPC_ID__Req_WifiApGetStaList:
	case RPC_ID__Req_WifiStaGetRssi:
	case RPC_ID__Req_WifiStaGetNegotiatedPhymode:
	case RPC_ID__Req_WifiStaGetAid:
	case RPC_ID__Req_WifiGetBand:
	case RPC_ID__Req_WifiGetBandMode:
#if H_WIFI_ENTERPRISE_SUPPORT
	case RPC_ID__Req_WifiStaEnterpriseEnable:
	case RPC_ID__Req_WifiStaEnterpriseDisable:
	case RPC_ID__Req_EapClearIdentity:
	case RPC_ID__Req_EapClearUsername:
	case RPC_ID__Req_EapClearPassword:
	case RPC_ID__Req_EapClearNewPassword:
	case RPC_ID__Req_EapClearCaCert:
	case RPC_ID__Req_EapClearCertificateAndKey:
#endif
	case RPC_ID__Req_WifiScanGetApRecord: {
		/* Intentional fallthrough & empty */
		break;
	} case RPC_ID__Req_GetMACAddress: {
		RPC_ALLOC_ASSIGN(RpcReqGetMacAddress, req_get_mac_address,
				rpc__req__get_mac_address__init);

		req_payload->mode = app_req->u.wifi_mac.mode;

		break;
	} case RPC_ID__Req_SetMacAddress: {
		wifi_mac_t * p = &app_req->u.wifi_mac;
		RPC_ALLOC_ASSIGN(RpcReqSetMacAddress, req_set_mac_address,
				rpc__req__set_mac_address__init);

		req_payload->mode = p->mode;
		RPC_REQ_COPY_BYTES(req_payload->mac, p->mac, BSSID_BYTES_SIZE);

		break;
	} case RPC_ID__Req_SetWifiMode: {
		hosted_mode_t * p = &app_req->u.wifi_mode;
		RPC_ALLOC_ASSIGN(RpcReqSetMode, req_set_wifi_mode,
				rpc__req__set_mode__init);

		if ((p->mode < WIFI_MODE_NULL) || (p->mode >= WIFI_MODE_MAX)) {
			ESP_LOGE(TAG, "Invalid wifi mode\n");
			*failure_status = RPC_ERR_INCORRECT_ARG;
			return FAILURE;
		}
		req_payload->mode = p->mode;
		break;
	} case RPC_ID__Req_WifiSetPs: {
		wifi_power_save_t * p = &app_req->u.wifi_ps;
		RPC_ALLOC_ASSIGN(RpcReqSetPs, req_wifi_set_ps,
				rpc__req__set_ps__init);

		req_payload->type = p->ps_mode;
		break;
	} case RPC_ID__Req_OTAWrite: {
		ota_write_t *p = & app_req->u.ota_write;
		RPC_ALLOC_ASSIGN(RpcReqOTAWrite, req_ota_write,
				rpc__req__otawrite__init);

		if (!p->ota_data || (p->ota_data_len == 0)) {
			ESP_LOGE(TAG, "Invalid parameter\n");
			*failure_status = RPC_ERR_INCORRECT_ARG;
			return FAILURE;
		}

		req_payload->ota_data.data = p->ota_data;
		req_payload->ota_data.len = p->ota_data_len;
		break;
	} case RPC_ID__Req_WifiSetMaxTxPower: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetMaxTxPower,
				req_set_wifi_max_tx_power,
				rpc__req__wifi_set_max_tx_power__init);
		req_payload->power = app_req->u.wifi_tx_power.power;
		break;
	} case RPC_ID__Req_ConfigHeartbeat: {
		RPC_ALLOC_ASSIGN(RpcReqConfigHeartbeat, req_config_heartbeat,
				rpc__req__config_heartbeat__init);
		req_payload->enable = app_req->u.e_heartbeat.enable;
		req_payload->duration = app_req->u.e_heartbeat.duration;
		if (req_payload->enable) {
			ESP_LOGW(TAG, "Enable heartbeat with duration %ld\n", (long int)req_payload->duration);
			if (CALLBACK_AVAILABLE != is_event_callback_registered(RPC_ID__Event_Heartbeat))
				ESP_LOGW(TAG, "Note: ** Subscribe heartbeat event to get notification **\n");
		} else {
			ESP_LOGI(TAG, "Disable Heartbeat\n");
		}
		break;
	} case RPC_ID__Req_WifiInit: {
		wifi_init_config_t * p_a = &app_req->u.wifi_init_config;
		RPC_ALLOC_ASSIGN(RpcReqWifiInit, req_wifi_init,
				rpc__req__wifi_init__init);
		RPC_ALLOC_ELEMENT(WifiInitConfig, req_payload->cfg, wifi_init_config__init);

		req_payload->cfg->static_rx_buf_num      = p_a->static_rx_buf_num       ;
		req_payload->cfg->dynamic_rx_buf_num     = p_a->dynamic_rx_buf_num      ;
		req_payload->cfg->tx_buf_type            = p_a->tx_buf_type             ;
		req_payload->cfg->static_tx_buf_num      = p_a->static_tx_buf_num       ;
		req_payload->cfg->dynamic_tx_buf_num     = p_a->dynamic_tx_buf_num      ;
		req_payload->cfg->rx_mgmt_buf_type       = p_a->rx_mgmt_buf_type        ;
		req_payload->cfg->rx_mgmt_buf_num        = p_a->rx_mgmt_buf_num         ;
		req_payload->cfg->cache_tx_buf_num       = p_a->cache_tx_buf_num        ;
		req_payload->cfg->csi_enable             = p_a->csi_enable              ;
		req_payload->cfg->ampdu_rx_enable        = p_a->ampdu_rx_enable         ;
		req_payload->cfg->ampdu_tx_enable        = p_a->ampdu_tx_enable         ;
		req_payload->cfg->amsdu_tx_enable        = p_a->amsdu_tx_enable         ;
		req_payload->cfg->nvs_enable             = p_a->nvs_enable              ;
		req_payload->cfg->nano_enable            = p_a->nano_enable             ;
		req_payload->cfg->rx_ba_win              = p_a->rx_ba_win               ;
		req_payload->cfg->wifi_task_core_id      = p_a->wifi_task_core_id       ;
		req_payload->cfg->beacon_max_len         = p_a->beacon_max_len          ;
		req_payload->cfg->feature_caps           = p_a->feature_caps            ;
		req_payload->cfg->mgmt_sbuf_num          = p_a->mgmt_sbuf_num           ;
		req_payload->cfg->sta_disconnected_pm    = p_a->sta_disconnected_pm     ;
		req_payload->cfg->espnow_max_encrypt_num = p_a->espnow_max_encrypt_num  ;
		req_payload->cfg->tx_hetb_queue_num      = p_a->tx_hetb_queue_num       ;
		req_payload->cfg->dump_hesigb_enable     = p_a->dump_hesigb_enable      ;
		req_payload->cfg->magic                  = p_a->magic                   ;

		/* uint64 - TODO: portable? */
		req_payload->cfg->feature_caps = p_a->feature_caps                      ;
		break;
    } case RPC_ID__Req_WifiGetConfig: {
		wifi_cfg_t * p_a = &app_req->u.wifi_config;
		RPC_ALLOC_ASSIGN(RpcReqWifiGetConfig, req_wifi_get_config,
				rpc__req__wifi_get_config__init);

		req_payload->iface = p_a->iface;
		break;
    } case RPC_ID__Req_WifiSetConfig: {
		wifi_cfg_t * p_a = &app_req->u.wifi_config;
		RPC_ALLOC_ASSIGN(RpcReqWifiSetConfig, req_wifi_set_config,
				rpc__req__wifi_set_config__init);

		req_payload->iface = p_a->iface;

		RPC_ALLOC_ELEMENT(WifiConfig, req_payload->cfg, wifi_config__init);

		switch(req_payload->iface) {

		case WIFI_IF_STA: {
			req_payload->cfg->u_case = WIFI_CONFIG__U_STA;

			wifi_sta_config_t *p_a_sta = &p_a->u.sta;
			RPC_ALLOC_ELEMENT(WifiStaConfig, req_payload->cfg->sta, wifi_sta_config__init);
			WifiStaConfig *p_c_sta = req_payload->cfg->sta;
			RPC_REQ_COPY_STR(p_c_sta->ssid, p_a_sta->ssid, SSID_LENGTH);

			RPC_REQ_COPY_STR(p_c_sta->password, p_a_sta->password, PASSWORD_LENGTH);

			p_c_sta->scan_method = p_a_sta->scan_method;
			p_c_sta->bssid_set = p_a_sta->bssid_set;

			if (p_a_sta->bssid_set)
				RPC_REQ_COPY_BYTES(p_c_sta->bssid, p_a_sta->bssid, BSSID_BYTES_SIZE);

			p_c_sta->channel = p_a_sta->channel;
			p_c_sta->listen_interval = p_a_sta->listen_interval;
			p_c_sta->sort_method = p_a_sta->sort_method;
			RPC_ALLOC_ELEMENT(WifiScanThreshold, p_c_sta->threshold, wifi_scan_threshold__init);
			p_c_sta->threshold->rssi = p_a_sta->threshold.rssi;
			p_c_sta->threshold->authmode = p_a_sta->threshold.authmode;
#if H_PRESENT_IN_ESP_IDF_5_4_0
			p_c_sta->threshold->rssi_5g_adjustment = p_a_sta->threshold.rssi_5g_adjustment;
#endif
			RPC_ALLOC_ELEMENT(WifiPmfConfig, p_c_sta->pmf_cfg, wifi_pmf_config__init);
			p_c_sta->pmf_cfg->capable = p_a_sta->pmf_cfg.capable;
			p_c_sta->pmf_cfg->required = p_a_sta->pmf_cfg.required;

			if (p_a_sta->rm_enabled)
				H_SET_BIT(WIFI_STA_CONFIG_1_rm_enabled, p_c_sta->bitmask);

			if (p_a_sta->btm_enabled)
				H_SET_BIT(WIFI_STA_CONFIG_1_btm_enabled, p_c_sta->bitmask);

			if (p_a_sta->mbo_enabled)
				H_SET_BIT(WIFI_STA_CONFIG_1_mbo_enabled, p_c_sta->bitmask);

			if (p_a_sta->ft_enabled)
				H_SET_BIT(WIFI_STA_CONFIG_1_ft_enabled, p_c_sta->bitmask);

			if (p_a_sta->owe_enabled)
				H_SET_BIT(WIFI_STA_CONFIG_1_owe_enabled, p_c_sta->bitmask);

			if (p_a_sta->transition_disable)
				H_SET_BIT(WIFI_STA_CONFIG_1_transition_disable, p_c_sta->bitmask);

#if H_DECODE_WIFI_RESERVED_FIELD
  #if H_WIFI_NEW_RESERVED_FIELD_NAMES
			WIFI_STA_CONFIG_2_SET_RESERVED_VAL(p_a_sta->reserved2, p_c_sta->he_bitmask);
  #else
			WIFI_STA_CONFIG_2_SET_RESERVED_VAL(p_a_sta->he_reserved, p_c_sta->he_bitmask);
  #endif
#endif

			p_c_sta->sae_pwe_h2e = p_a_sta->sae_pwe_h2e;
			p_c_sta->sae_pk_mode = p_a_sta->sae_pk_mode;
			p_c_sta->failure_retry_cnt = p_a_sta->failure_retry_cnt;

			if (p_a_sta->he_dcm_set)
				H_SET_BIT(WIFI_STA_CONFIG_2_he_dcm_set_BIT, p_c_sta->he_bitmask);

			// WIFI_HE_STA_CONFIG_he_dcm_max_constellation_tx is two bits wide
			if (p_a_sta->he_dcm_max_constellation_tx)
				p_c_sta->he_bitmask |= ((p_a_sta->he_dcm_max_constellation_tx & 0x03) << WIFI_STA_CONFIG_2_he_dcm_max_constellation_tx_BITS);

			// WIFI_HE_STA_CONFIG_he_dcm_max_constellation_rx is two bits wide
			if (p_a_sta->he_dcm_max_constellation_rx)
				p_c_sta->he_bitmask |= ((p_a_sta->he_dcm_max_constellation_rx & 0x03) << WIFI_STA_CONFIG_2_he_dcm_max_constellation_rx_BITS);

			if (p_a_sta->he_mcs9_enabled)
				H_SET_BIT(WIFI_STA_CONFIG_2_he_mcs9_enabled_BIT, p_c_sta->he_bitmask);

			if (p_a_sta->he_su_beamformee_disabled)
				H_SET_BIT(WIFI_STA_CONFIG_2_he_su_beamformee_disabled_BIT, p_c_sta->he_bitmask);

			if (p_a_sta->he_trig_su_bmforming_feedback_disabled)
				H_SET_BIT(WIFI_STA_CONFIG_2_he_trig_su_bmforming_feedback_disabled_BIT, p_c_sta->he_bitmask);

			if (p_a_sta->he_trig_mu_bmforming_partial_feedback_disabled)
				H_SET_BIT(WIFI_STA_CONFIG_2_he_trig_mu_bmforming_partial_feedback_disabled_BIT, p_c_sta->he_bitmask);

			if (p_a_sta->he_trig_cqi_feedback_disabled)
				H_SET_BIT(WIFI_STA_CONFIG_2_he_trig_cqi_feedback_disabled_BIT, p_c_sta->he_bitmask);

#if H_PRESENT_IN_ESP_IDF_5_5_0
			if (p_a_sta->vht_su_beamformee_disabled)
				H_SET_BIT(WIFI_STA_CONFIG_2_vht_su_beamformee_disabled, p_c_sta->he_bitmask);

			if (p_a_sta->vht_mu_beamformee_disabled)
				H_SET_BIT(WIFI_STA_CONFIG_2_vht_mu_beamformee_disabled, p_c_sta->he_bitmask);

			if (p_a_sta->vht_mcs8_enabled)
				H_SET_BIT(WIFI_STA_CONFIG_2_vht_mcs8_enabled, p_c_sta->he_bitmask);
#endif

#if H_DECODE_WIFI_RESERVED_FIELD
  #if H_WIFI_NEW_RESERVED_FIELD_NAMES
			WIFI_STA_CONFIG_2_SET_RESERVED_VAL(p_a_sta->reserved2, p_c_sta->he_bitmask);
  #else
			WIFI_STA_CONFIG_2_SET_RESERVED_VAL(p_a_sta->he_reserved, p_c_sta->he_bitmask);
  #endif
#endif

			RPC_REQ_COPY_BYTES(p_c_sta->sae_h2e_identifier, p_a_sta->sae_h2e_identifier, SAE_H2E_IDENTIFIER_LEN);
			break;
		} case WIFI_IF_AP: {
			req_payload->cfg->u_case = WIFI_CONFIG__U_AP;

			wifi_ap_config_t * p_a_ap = &p_a->u.ap;
			RPC_ALLOC_ELEMENT(WifiApConfig, req_payload->cfg->ap, wifi_ap_config__init);
			WifiApConfig * p_c_ap = req_payload->cfg->ap;

			RPC_REQ_COPY_STR(p_c_ap->ssid, p_a_ap->ssid, SSID_LENGTH);
			RPC_REQ_COPY_STR(p_c_ap->password, p_a_ap->password, PASSWORD_LENGTH);
			p_c_ap->ssid_len = p_a_ap->ssid_len;
			p_c_ap->channel = p_a_ap->channel;
			p_c_ap->authmode = p_a_ap->authmode;
			p_c_ap->ssid_hidden = p_a_ap->ssid_hidden;
			p_c_ap->max_connection = p_a_ap->max_connection;
			p_c_ap->beacon_interval = p_a_ap->beacon_interval;
			p_c_ap->csa_count = p_a_ap->csa_count;
			p_c_ap->dtim_period = p_a_ap->dtim_period;
			p_c_ap->pairwise_cipher = p_a_ap->pairwise_cipher;
			p_c_ap->ftm_responder = p_a_ap->ftm_responder;
			RPC_ALLOC_ELEMENT(WifiPmfConfig, p_c_ap->pmf_cfg, wifi_pmf_config__init);
			p_c_ap->pmf_cfg->capable = p_a_ap->pmf_cfg.capable;
			p_c_ap->pmf_cfg->required = p_a_ap->pmf_cfg.required;
			p_c_ap->sae_pwe_h2e = p_a_ap->sae_pwe_h2e;
#if H_GOT_AP_CONFIG_PARAM_TRANSITION_DISABLE
			p_c_ap->transition_disable = p_a_ap->transition_disable;
#endif
#if H_PRESENT_IN_ESP_IDF_5_5_0
			p_c_ap->sae_ext = p_a_ap->sae_ext;
			RPC_ALLOC_ELEMENT(WifiBssMaxIdleConfig, p_c_ap->bss_max_idle_cfg, wifi_bss_max_idle_config__init);
			p_c_ap->bss_max_idle_cfg->period = p_a_ap->bss_max_idle_cfg.period;
			p_c_ap->bss_max_idle_cfg->protected_keep_alive = p_a_ap->bss_max_idle_cfg.protected_keep_alive;
			p_c_ap->gtk_rekey_interval = p_a_ap->gtk_rekey_interval;
#endif
			break;
        } default: {
            ESP_LOGE(TAG, "unexpected wifi iface [%u]\n", p_a->iface);
			break;
        }

        } /* switch */
		break;

    } case RPC_ID__Req_WifiScanStart: {
		wifi_scan_config_t * p_a = &app_req->u.wifi_scan_config.cfg;

		RPC_ALLOC_ASSIGN(RpcReqWifiScanStart, req_wifi_scan_start,
				rpc__req__wifi_scan_start__init);

		req_payload->block = app_req->u.wifi_scan_config.block;
		if (app_req->u.wifi_scan_config.cfg_set) {

			RPC_ALLOC_ELEMENT(WifiScanConfig, req_payload->config, wifi_scan_config__init);

			RPC_ALLOC_ELEMENT(WifiScanTime , req_payload->config->scan_time, wifi_scan_time__init);
			RPC_ALLOC_ELEMENT(WifiActiveScanTime, req_payload->config->scan_time->active, wifi_active_scan_time__init);
			ESP_LOGD(TAG, "scan start4\n");

			WifiScanConfig *p_c = req_payload->config;
			WifiScanTime *p_c_st = NULL;
			wifi_scan_time_t *p_a_st = &p_a->scan_time;

			RPC_REQ_COPY_STR(p_c->ssid, p_a->ssid, SSID_LENGTH);
			RPC_REQ_COPY_STR(p_c->bssid, p_a->bssid, MAC_SIZE_BYTES);
			p_c->channel = p_a->channel;
			p_c->show_hidden = p_a->show_hidden;
			p_c->scan_type = p_a->scan_type;

			p_c_st = p_c->scan_time;

			p_c_st->passive = p_a_st->passive;
			p_c_st->active->min = p_a_st->active.min ;
			p_c_st->active->max = p_a_st->active.max ;

			p_c->home_chan_dwell_time = p_a->home_chan_dwell_time;

			RPC_ALLOC_ELEMENT(WifiScanChannelBitmap, p_c->channel_bitmap, wifi_scan_channel_bitmap__init);
			p_c->channel_bitmap->ghz_2_channels = p_a->channel_bitmap.ghz_2_channels;
			p_c->channel_bitmap->ghz_5_channels = p_a->channel_bitmap.ghz_5_channels;

			req_payload->config_set = 1;
		}
		ESP_LOGI(TAG, "Scan start Req\n");

		break;

	} case RPC_ID__Req_WifiScanGetApRecords: {
		RPC_ALLOC_ASSIGN(RpcReqWifiScanGetApRecords, req_wifi_scan_get_ap_records,
				rpc__req__wifi_scan_get_ap_records__init);
		req_payload->number = app_req->u.wifi_scan_ap_list.number;
		break;
	} case RPC_ID__Req_WifiDeauthSta: {
		RPC_ALLOC_ASSIGN(RpcReqWifiDeauthSta, req_wifi_deauth_sta,
				rpc__req__wifi_deauth_sta__init);
		req_payload->aid = app_req->u.wifi_deauth_sta.aid;
		break;
	} case RPC_ID__Req_WifiSetStorage: {
		wifi_storage_t * p = &app_req->u.wifi_storage;
		RPC_ALLOC_ASSIGN(RpcReqWifiSetStorage, req_wifi_set_storage,
				rpc__req__wifi_set_storage__init);
		req_payload->storage = *p;
		break;
	} case RPC_ID__Req_WifiSetBandwidth: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetBandwidth, req_wifi_set_bandwidth,
				rpc__req__wifi_set_bandwidth__init);
		req_payload->ifx = app_req->u.wifi_bandwidth.ifx;
		req_payload->bw = app_req->u.wifi_bandwidth.bw;
		break;
	} case RPC_ID__Req_WifiGetBandwidth: {
		RPC_ALLOC_ASSIGN(RpcReqWifiGetBandwidth, req_wifi_get_bandwidth,
				rpc__req__wifi_get_bandwidth__init);
		req_payload->ifx = app_req->u.wifi_bandwidth.ifx;
		break;
	} case RPC_ID__Req_WifiSetChannel: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetChannel, req_wifi_set_channel,
				rpc__req__wifi_set_channel__init);
		req_payload->primary = app_req->u.wifi_channel.primary;
		req_payload->second = app_req->u.wifi_channel.second;
		break;
	} case RPC_ID__Req_WifiSetCountryCode: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetCountryCode, req_wifi_set_country_code,
				rpc__req__wifi_set_country_code__init);
		RPC_REQ_COPY_BYTES(req_payload->country, (uint8_t *)&app_req->u.wifi_country_code.cc[0], sizeof(app_req->u.wifi_country_code.cc));
		req_payload->ieee80211d_enabled = app_req->u.wifi_country_code.ieee80211d_enabled;
		break;
	} case RPC_ID__Req_WifiSetCountry: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetCountry, req_wifi_set_country,
				rpc__req__wifi_set_country__init);

		RPC_ALLOC_ELEMENT(WifiCountry, req_payload->country, wifi_country__init);
		RPC_REQ_COPY_BYTES(req_payload->country->cc, (uint8_t *)&app_req->u.wifi_country.cc[0], sizeof(app_req->u.wifi_country.cc));
		req_payload->country->schan        = app_req->u.wifi_country.schan;
		req_payload->country->nchan        = app_req->u.wifi_country.nchan;
		req_payload->country->max_tx_power = app_req->u.wifi_country.max_tx_power;
		req_payload->country->policy       = app_req->u.wifi_country.policy;
		break;
	} case RPC_ID__Req_WifiApGetStaAid: {
		RPC_ALLOC_ASSIGN(RpcReqWifiApGetStaAid, req_wifi_ap_get_sta_aid,
				rpc__req__wifi_ap_get_sta_aid__init);

		uint8_t * p = &app_req->u.wifi_ap_get_sta_aid.mac[0];
		RPC_REQ_COPY_BYTES(req_payload->mac, p, MAC_SIZE_BYTES);
		break;
	} case RPC_ID__Req_WifiSetProtocol: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetProtocol, req_wifi_set_protocol,
				rpc__req__wifi_set_protocol__init);
		req_payload->ifx = app_req->u.wifi_protocol.ifx;
		req_payload->protocol_bitmap = app_req->u.wifi_protocol.protocol_bitmap;
		break;
	} case RPC_ID__Req_WifiGetProtocol: {
		RPC_ALLOC_ASSIGN(RpcReqWifiGetProtocol, req_wifi_get_protocol,
				rpc__req__wifi_get_protocol__init);
		req_payload->ifx = app_req->u.wifi_protocol.ifx;
		break;
	} case RPC_ID__Req_GetCoprocessorFwVersion: {
		RPC_ALLOC_ASSIGN(RpcReqGetCoprocessorFwVersion, req_get_coprocessor_fwversion,
				rpc__req__get_coprocessor_fw_version__init);
		break;
	} case RPC_ID__Req_WifiSetInactiveTime: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetInactiveTime, req_wifi_set_inactive_time,
				rpc__req__wifi_set_inactive_time__init);
		req_payload->ifx = app_req->u.wifi_inactive_time.ifx;
		req_payload->sec = app_req->u.wifi_inactive_time.sec;
		break;
	} case RPC_ID__Req_WifiGetInactiveTime: {
		RPC_ALLOC_ASSIGN(RpcReqWifiGetInactiveTime, req_wifi_get_inactive_time,
				rpc__req__wifi_get_inactive_time__init);
		req_payload->ifx = app_req->u.wifi_inactive_time.ifx;
		break;
#if H_WIFI_HE_SUPPORT
	} case RPC_ID__Req_WifiStaTwtConfig: {
		RPC_ALLOC_ASSIGN(RpcReqWifiStaTwtConfig, req_wifi_sta_twt_config,
				rpc__req__wifi_sta_twt_config__init);
		RPC_ALLOC_ELEMENT(WifiTwtConfig, req_payload->config, wifi_twt_config__init);
		req_payload->config->post_wakeup_event = app_req->u.wifi_twt_config.post_wakeup_event;
#if H_GOT_TWT_ENABLE_KEEP_ALIVE
		req_payload->config->twt_enable_keep_alive = app_req->u.wifi_twt_config.twt_enable_keep_alive;
#endif
		break;
	} case RPC_ID__Req_WifiStaItwtSetup: {
		RPC_ALLOC_ASSIGN(RpcReqWifiStaItwtSetup, req_wifi_sta_itwt_setup,
				rpc__req__wifi_sta_itwt_setup__init);
		RPC_ALLOC_ELEMENT(WifiItwtSetupConfig, req_payload->setup_config, wifi_itwt_setup_config__init);
		wifi_itwt_setup_config_t * p_a_cfg = &app_req->u.wifi_itwt_setup_config;
		WifiItwtSetupConfig * p_c_cfg = req_payload->setup_config;

		p_c_cfg->setup_cmd = p_a_cfg->setup_cmd;
		if (p_a_cfg->trigger)
			H_SET_BIT(WIFI_ITWT_CONFIG_1_trigger_BIT, p_c_cfg->bitmask_1);

		if (p_a_cfg->flow_type)
			H_SET_BIT(WIFI_ITWT_CONFIG_1_flow_type_BIT, p_c_cfg->bitmask_1);

		// WIFI_ITWT_CONFIG_1_flow_id_BIT is three bits wide
		if (p_a_cfg->flow_id)
			p_c_cfg->bitmask_1 |= ((p_a_cfg->flow_id & 0x07) << WIFI_ITWT_CONFIG_1_flow_id_BIT);

		// WIFI_ITWT_CONFIG_1_wake_invl_expn_BIT is five bits wide
		if (p_a_cfg->wake_invl_expn)
			p_c_cfg->bitmask_1 |= ((p_a_cfg->wake_invl_expn & 0x1F) << WIFI_ITWT_CONFIG_1_wake_invl_expn_BIT);

		if (p_a_cfg->wake_duration_unit)
			H_SET_BIT(WIFI_ITWT_CONFIG_1_wake_duration_unit_BIT, p_c_cfg->bitmask_1);

#if H_DECODE_WIFI_RESERVED_FIELD
		WIFI_ITWT_CONFIG_1_SET_RESERVED_VAL(p_a_cfg->reserved, p_c_cfg->bitmask_1);
#endif
		p_c_cfg->min_wake_dura = p_a_cfg->min_wake_dura;
		p_c_cfg->wake_invl_mant = p_a_cfg->wake_invl_mant;
		p_c_cfg->twt_id = p_a_cfg->twt_id;
		p_c_cfg->timeout_time_ms = p_a_cfg->timeout_time_ms;

		break;
	} case RPC_ID__Req_WifiStaItwtTeardown: {
		RPC_ALLOC_ASSIGN(RpcReqWifiStaItwtTeardown, req_wifi_sta_itwt_teardown,
				rpc__req__wifi_sta_itwt_teardown__init);

		req_payload->flow_id = app_req->u.wifi_itwt_flow_id;
		break;
	} case RPC_ID__Req_WifiStaItwtSuspend: {
		RPC_ALLOC_ASSIGN(RpcReqWifiStaItwtSuspend, req_wifi_sta_itwt_suspend,
				rpc__req__wifi_sta_itwt_suspend__init);

		req_payload->flow_id = app_req->u.wifi_itwt_suspend.flow_id;
		req_payload->suspend_time_ms = app_req->u.wifi_itwt_suspend.suspend_time_ms;
		break;
	} case RPC_ID__Req_WifiStaItwtGetFlowIdStatus: {
		RPC_ALLOC_ASSIGN(RpcReqWifiStaItwtGetFlowIdStatus, req_wifi_sta_itwt_get_flow_id_status,
				rpc__req__wifi_sta_itwt_get_flow_id_status__init);
		break;
	} case RPC_ID__Req_WifiStaItwtSendProbeReq: {
		RPC_ALLOC_ASSIGN(RpcReqWifiStaItwtSendProbeReq, req_wifi_sta_itwt_send_probe_req,
				rpc__req__wifi_sta_itwt_send_probe_req__init);
		req_payload->timeout_ms = app_req->u.wifi_itwt_probe_req_timeout_ms;
		break;
	} case RPC_ID__Req_WifiStaItwtSetTargetWakeTimeOffset: {
		RPC_ALLOC_ASSIGN(RpcReqWifiStaItwtSetTargetWakeTimeOffset, req_wifi_sta_itwt_set_target_wake_time_offset,
				rpc__req__wifi_sta_itwt_set_target_wake_time_offset__init);
		req_payload->offset_us = app_req->u.wifi_itwt_set_target_wake_time_offset_us;
		break;
#endif
#if H_WIFI_DUALBAND_SUPPORT
	} case RPC_ID__Req_WifiSetProtocols: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetProtocols, req_wifi_set_protocols,
				rpc__req__wifi_set_protocols__init);
		req_payload->ifx = app_req->u.wifi_protocols.ifx;

		RPC_ALLOC_ELEMENT(WifiProtocols, req_payload->protocols, wifi_protocols__init);
		req_payload->protocols->ghz_2g = app_req->u.wifi_protocols.ghz_2g;
		req_payload->protocols->ghz_5g = app_req->u.wifi_protocols.ghz_5g;
		break;
	} case RPC_ID__Req_WifiGetProtocols: {
		RPC_ALLOC_ASSIGN(RpcReqWifiGetProtocols, req_wifi_get_protocols,
				rpc__req__wifi_get_protocols__init);
		req_payload->ifx = app_req->u.wifi_protocols.ifx;
		break;
	} case RPC_ID__Req_WifiSetBandwidths: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetBandwidths, req_wifi_set_bandwidths,
				rpc__req__wifi_set_bandwidths__init);
		req_payload->ifx = app_req->u.wifi_bandwidths.ifx;

		RPC_ALLOC_ELEMENT(WifiBandwidths, req_payload->bandwidths, wifi_bandwidths__init);
		req_payload->bandwidths->ghz_2g = app_req->u.wifi_bandwidths.ghz_2g;
		req_payload->bandwidths->ghz_5g = app_req->u.wifi_bandwidths.ghz_5g;
		break;
	} case RPC_ID__Req_WifiGetBandwidths: {
		RPC_ALLOC_ASSIGN(RpcReqWifiGetBandwidths, req_wifi_get_bandwidths,
				rpc__req__wifi_get_bandwidths__init);
		req_payload->ifx = app_req->u.wifi_bandwidths.ifx;
		break;
	} case RPC_ID__Req_WifiSetBand: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetBand, req_wifi_set_band,
				rpc__req__wifi_set_band__init);
		req_payload->band = app_req->u.wifi_band;
		break;
	} case RPC_ID__Req_WifiSetBandMode: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetBandMode, req_wifi_set_bandmode,
				rpc__req__wifi_set_band_mode__init);
		req_payload->bandmode = app_req->u.wifi_band_mode;
		break;
#endif
	} case RPC_ID__Req_SetDhcpDnsStatus: {
		RPC_ALLOC_ASSIGN(RpcReqSetDhcpDnsStatus, req_set_dhcp_dns,
				rpc__req__set_dhcp_dns_status__init);
		RpcReqSetDhcpDnsStatus *p_c = req_payload;
		rpc_set_dhcp_dns_status_t* p_a = &app_req->u.slave_dhcp_dns_status;

		p_c->iface = p_a->iface;
		p_c->dhcp_up = p_a->dhcp_up;
		p_c->dns_up = p_a->dns_up;
		p_c->dns_type = p_a->dns_type;
		p_c->net_link_up = p_a->net_link_up;

		RPC_REQ_COPY_STR(p_c->dhcp_ip, p_a->dhcp_ip, 64);
		RPC_REQ_COPY_STR(p_c->dhcp_nm, p_a->dhcp_nm, 64);
		RPC_REQ_COPY_STR(p_c->dhcp_gw, p_a->dhcp_gw, 64);
		RPC_REQ_COPY_STR(p_c->dns_ip, p_a->dns_ip, 64);
		break;
#if H_WIFI_ENTERPRISE_SUPPORT
	} case RPC_ID__Req_EapSetIdentity: {
		RPC_ALLOC_ASSIGN(RpcReqEapSetIdentity, req_eap_set_identity,
				rpc__req__eap_set_identity__init);
		RPC_REQ_COPY_BYTES(req_payload->identity, (uint8_t *)app_req->u.eap_identity.identity, app_req->u.eap_identity.len);
		req_payload->len = app_req->u.eap_identity.len;
		break;
	} case RPC_ID__Req_EapSetUsername: {
		RPC_ALLOC_ASSIGN(RpcReqEapSetUsername, req_eap_set_username,
				rpc__req__eap_set_username__init);
		RPC_REQ_COPY_BYTES(req_payload->username, (uint8_t *)app_req->u.eap_username.username, app_req->u.eap_username.len);
		req_payload->len = app_req->u.eap_username.len;
		break;
	} case RPC_ID__Req_EapSetPassword: {
		RPC_ALLOC_ASSIGN(RpcReqEapSetPassword, req_eap_set_password,
				rpc__req__eap_set_password__init);
		RPC_REQ_COPY_BYTES(req_payload->password, (uint8_t *)app_req->u.eap_password.password, app_req->u.eap_password.len);
		req_payload->len = app_req->u.eap_password.len;
		break;
	} case RPC_ID__Req_EapSetNewPassword: {
		RPC_ALLOC_ASSIGN(RpcReqEapSetNewPassword, req_eap_set_new_password,
				rpc__req__eap_set_new_password__init);
		RPC_REQ_COPY_BYTES(req_payload->new_password, (uint8_t *)app_req->u.eap_password.password, app_req->u.eap_password.len);
		req_payload->len = app_req->u.eap_password.len;
		break;
	} case RPC_ID__Req_EapSetCaCert: {
		RPC_ALLOC_ASSIGN(RpcReqEapSetCaCert, req_eap_set_ca_cert,
				rpc__req__eap_set_ca_cert__init);
		RPC_REQ_COPY_BYTES(req_payload->ca_cert, (uint8_t *)app_req->u.eap_ca_cert.ca_cert, app_req->u.eap_ca_cert.len);
		req_payload->ca_cert_len = app_req->u.eap_ca_cert.len;
		break;
	} case RPC_ID__Req_EapSetCertificateAndKey: {
		RPC_ALLOC_ASSIGN(RpcReqEapSetCertificateAndKey, req_eap_set_certificate_and_key,
				rpc__req__eap_set_certificate_and_key__init);
		RPC_REQ_COPY_BYTES(req_payload->client_cert, (uint8_t *)app_req->u.eap_cert_key.client_cert, app_req->u.eap_cert_key.client_cert_len);
		req_payload->client_cert_len = app_req->u.eap_cert_key.client_cert_len;
		RPC_REQ_COPY_BYTES(req_payload->private_key, (uint8_t *)app_req->u.eap_cert_key.private_key, app_req->u.eap_cert_key.private_key_len);
		req_payload->private_key_len = app_req->u.eap_cert_key.private_key_len;
		RPC_REQ_COPY_BYTES(req_payload->private_key_password, (uint8_t *)app_req->u.eap_cert_key.private_key_password, app_req->u.eap_cert_key.private_key_passwd_len);
		req_payload->private_key_passwd_len = app_req->u.eap_cert_key.private_key_passwd_len;
		break;
	} case RPC_ID__Req_EapSetDisableTimeCheck: {
		RPC_ALLOC_ASSIGN(RpcReqEapSetDisableTimeCheck, req_eap_set_disable_time_check,
				rpc__req__eap_set_disable_time_check__init);
		req_payload->disable = app_req->u.eap_disable_time_check.disable;
		break;
	} case RPC_ID__Req_EapSetTtlsPhase2Method: {
		RPC_ALLOC_ASSIGN(RpcReqEapSetTtlsPhase2Method, req_eap_set_ttls_phase2_method,
				rpc__req__eap_set_ttls_phase2_method__init);
		req_payload->type = app_req->u.eap_ttls_phase2;
		break;
	} case RPC_ID__Req_EapSetSuitebCertification: {
		RPC_ALLOC_ASSIGN(RpcReqEapSetSuiteb192bitCertification, req_eap_set_suiteb_certification,
				rpc__req__eap_set_suiteb192bit_certification__init);
		req_payload->enable = app_req->u.eap_suiteb_192bit.enable;
		break;
	} case RPC_ID__Req_EapSetPacFile: {
		RPC_ALLOC_ASSIGN(RpcReqEapSetPacFile, req_eap_set_pac_file,
				rpc__req__eap_set_pac_file__init);
		RPC_REQ_COPY_BYTES(req_payload->pac_file, (uint8_t *)app_req->u.eap_pac_file.pac_file, app_req->u.eap_pac_file.len);
		req_payload->pac_file_len = app_req->u.eap_pac_file.len;
		break;
	} case RPC_ID__Req_EapSetFastParams: {
		RPC_ALLOC_ASSIGN(RpcReqEapSetFastParams, req_eap_set_fast_params,
				rpc__req__eap_set_fast_params__init);
		RPC_ALLOC_ELEMENT(EapFastConfig, req_payload->eap_fast_config, eap_fast_config__init);
		req_payload->eap_fast_config->fast_provisioning = app_req->u.eap_fast_config.fast_provisioning;
		req_payload->eap_fast_config->fast_max_pac_list_len = app_req->u.eap_fast_config.fast_max_pac_list_len;
		req_payload->eap_fast_config->fast_pac_format_binary = app_req->u.eap_fast_config.fast_pac_format_binary;
		break;
	} case RPC_ID__Req_EapUseDefaultCertBundle: {
		RPC_ALLOC_ASSIGN(RpcReqEapUseDefaultCertBundle, req_eap_use_default_cert_bundle,
				rpc__req__eap_use_default_cert_bundle__init);
		req_payload->use_default_bundle = app_req->u.eap_default_cert_bundle.use_default;
		break;
#if H_GOT_EAP_OKC_SUPPORT
	} case RPC_ID__Req_WifiSetOkcSupport: {
		RPC_ALLOC_ASSIGN(RpcReqWifiSetOkcSupport, req_wifi_set_okc_support,
				rpc__req__wifi_set_okc_support__init);
		req_payload->enable = app_req->u.wifi_okc_support.enable;
		break;
#endif
#if H_GOT_EAP_SET_DOMAIN_NAME
	} case RPC_ID__Req_EapSetDomainName: {
		RPC_ALLOC_ASSIGN(RpcReqEapSetDomainName, req_eap_set_domain_name,
				rpc__req__eap_set_domain_name__init);
		
		RPC_REQ_COPY_BYTES(req_payload->domain_name, (uint8_t *)app_req->u.eap_domain_name.domain_name, strlen(app_req->u.eap_domain_name.domain_name) + 1);
		break;
#endif
#if H_GOT_SET_EAP_METHODS_API
	} case RPC_ID__Req_EapSetEapMethods: {
		RPC_ALLOC_ASSIGN(RpcReqEapSetEapMethods, req_eap_set_eap_methods,
				rpc__req__eap_set_eap_methods__init);
		req_payload->methods = app_req->u.methods;
		break;
#endif
#endif
	} default: {
		*failure_status = RPC_ERR_UNSUPPORTED_MSG;
		ESP_LOGE(TAG, "Unsupported RPC Req[%u]",req->msg_id);
		return FAILURE;
		break;
	}

	} /* switch */
	return SUCCESS;
}
