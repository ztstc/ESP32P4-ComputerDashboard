// SPDX-License-Identifier: Apache-2.0
// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "driver/sdmmc_defs.h"
#include "driver/sdmmc_host.h"

#include "port_esp_hosted_host_os.h"
#include "sdio_reg.h"
#include "port_esp_hosted_host_sdio.h"
#include "esp_hosted_transport_config.h"
#include "port_esp_hosted_host_config.h"
#include "port_esp_hosted_host_log.h"

#if H_SDIO_PWR_CTRL_LDO
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif

#include "soc/sdmmc_pins.h"
#include "hal/sdmmc_ll.h"

#include "esp_log.h"
DEFINE_LOG_TAG(sdio_wrapper);

#define CIS_BUFFER_SIZE 256
#define FUNC1_EN_MASK   (BIT(1))
#define SDIO_INIT_MAX_RETRY 10 // max number of times we try to init SDIO FN 1

#define SDIO_FAIL_IF_NULL(x) do { \
		if (!x) return ESP_FAIL;  \
	} while (0);

#define SDIO_LOCK(x) do { \
	if (x) g_h.funcs->_h_lock_mutex(sdio_bus_lock, portMAX_DELAY); \
} while (0);

#define SDIO_UNLOCK(x) do { \
	if (x) g_h.funcs->_h_unlock_mutex(sdio_bus_lock); \
} while (0);

typedef struct  {
	sdmmc_card_t *card;
	struct esp_hosted_sdio_config config;
} sdmmc_context_t;

static sdmmc_context_t context = { 0 };

static void * sdio_bus_lock;

// workarounds for known ESP-IDF SDMMC issues
static void hosted_sdio_workaround(int slot, sdmmc_slot_config_t *slot_config)
{
	if (slot == 0) {
#if !SDMMC_LL_SLOT_SUPPORT_GPIO_MATRIX(0)
		/* workaround for 1-bit mode on Slot 0 with IOMUX only pins:
		 * set gpio pins D2, D3 to pass sdmmc_host.c->sdmmc_host_init_slot() IOMUX GPIO checking
		 */
		if (slot_config->width == 1) {
			ESP_LOGW(TAG, "workaround: setting D2-D3 in 1 bit mode for slot %d", slot);
			slot_config->d2 = SDMMC_SLOT0_IOMUX_PIN_NUM_D2;
			slot_config->d3 = SDMMC_SLOT0_IOMUX_PIN_NUM_D3;
		}
#endif
	}
}

static bool hosted_sdio_enable_ldo(sdmmc_host_t *config)
{
#if H_SDIO_PWR_CTRL_LDO
	// enable LDO Power for slot, if required
	sd_pwr_ctrl_ldo_config_t ldo_config = {
		.ldo_chan_id = H_SDIO_PWR_CTRL_LDO_ID,
	};
	sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

	esp_err_t ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
		return false;
	}
	config->pwr_ctrl_handle = pwr_ctrl_handle;
#endif
	return true;
}

static esp_err_t hosted_sdio_print_cis_information(sdmmc_card_t* card)
{
	uint8_t cis_buffer[CIS_BUFFER_SIZE];
	size_t cis_data_len = 1024; //specify maximum searching range to avoid infinite loop
	esp_err_t ret = ESP_OK;

	SDIO_FAIL_IF_NULL(card);

	ret = sdmmc_io_get_cis_data(card, cis_buffer, CIS_BUFFER_SIZE, &cis_data_len);
	if (ret == ESP_ERR_INVALID_SIZE) {
		int temp_buf_size = cis_data_len;
		uint8_t* temp_buf = g_h.funcs->_h_malloc(temp_buf_size);
		assert(temp_buf);

		ESP_LOGW(TAG, "CIS data longer than expected, temporary buffer allocated.");
		ret = sdmmc_io_get_cis_data(card, temp_buf, temp_buf_size, &cis_data_len);
		if (ret != ESP_OK) {
			ESP_LOGE(TAG, "failed to get CIS data.");
			HOSTED_FREE(temp_buf);
			return ret;
		}

		sdmmc_io_print_cis_info(temp_buf, cis_data_len, NULL);

		HOSTED_FREE(temp_buf);
	} else if (ret == ESP_OK) {
		sdmmc_io_print_cis_info(cis_buffer, cis_data_len, NULL);
	} else {
		ESP_LOGE(TAG, "failed to get CIS data.");
		return ret;
	}
	return ESP_OK;
}

static esp_err_t hosted_sdio_set_blocksize(sdmmc_card_t *card, uint8_t fn, uint16_t value)
{
	size_t offset = SD_IO_FBR_START * fn;
	const uint8_t *bs_u8 = (const uint8_t *) &value;
	uint16_t bs_read = 0;
	uint8_t *bs_read_u8 = (uint8_t *) &bs_read;

	// Set and read back block size
	ESP_ERROR_CHECK(sdmmc_io_write_byte(card, SDIO_FUNC_0, offset + SD_IO_CCCR_BLKSIZEL, bs_u8[0], NULL));
	ESP_ERROR_CHECK(sdmmc_io_write_byte(card, SDIO_FUNC_0, offset + SD_IO_CCCR_BLKSIZEH, bs_u8[1], NULL));
	ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, offset + SD_IO_CCCR_BLKSIZEL, &bs_read_u8[0]));
	ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, offset + SD_IO_CCCR_BLKSIZEH, &bs_read_u8[1]));
	ESP_LOGI(TAG, "Function %d Blocksize: %d", fn, (unsigned int) bs_read);

	if (bs_read == value)
		return ESP_OK;
	else
		return ESP_FAIL;
}

static esp_err_t hosted_sdio_card_fn_init(sdmmc_card_t *card)
{
	uint8_t ioe = 0;
	uint8_t ior = 0;
	uint8_t ie = 0;
	uint8_t bus_width = 0;
	uint16_t bs = 0;
	int i = 0;

	SDIO_FAIL_IF_NULL(card);

	ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_FN_ENABLE, &ioe));

	ESP_LOGD(TAG, "IOE: 0x%02x", ioe);

	ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_FN_READY, &ior));
	ESP_LOGD(TAG, "IOR: 0x%02x", ior);

	// enable function 1
	ioe |= FUNC1_EN_MASK;
	ESP_ERROR_CHECK(sdmmc_io_write_byte(card, SDIO_FUNC_0, SD_IO_CCCR_FN_ENABLE, ioe, &ioe));
	ESP_LOGD(TAG, "IOE: 0x%02x", ioe);

	ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_FN_ENABLE, &ioe));
	ESP_LOGD(TAG, "IOE: 0x%02x", ioe);

	// wait for the card to become ready
	ior = 0;
	for (i = 0; i < SDIO_INIT_MAX_RETRY; i++) {
		ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_FN_READY, &ior));
		ESP_LOGD(TAG, "IOR: 0x%02x", ior);
		if (ior & FUNC1_EN_MASK) {
			break;
		} else {
			usleep(10 * 1000);
		}
	}
	if (i >= SDIO_INIT_MAX_RETRY) {
		// card failed to become ready
		return ESP_FAIL;
	}

	// get interrupt status
	ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_INT_ENABLE, &ie));
	ESP_LOGD(TAG, "IE: 0x%02x", ie);

	// enable interrupts for function 1 and master enable
	ie |= BIT(0) | FUNC1_EN_MASK;
	ESP_ERROR_CHECK(sdmmc_io_write_byte(card, SDIO_FUNC_0, SD_IO_CCCR_INT_ENABLE, ie, NULL));

	ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_INT_ENABLE, &ie));
	ESP_LOGD(TAG, "IE: 0x%02x", ie);

	// get bus width register
	ESP_ERROR_CHECK(sdmmc_io_read_byte(card, SDIO_FUNC_0, SD_IO_CCCR_BUS_WIDTH, &bus_width));
	ESP_LOGD(TAG, "BUS_WIDTH: 0x%02x", bus_width);

	// skip enable of continous SPI interrupts

	// set FN0 block size to 512
	bs = 512;
	ESP_ERROR_CHECK(hosted_sdio_set_blocksize(card, SDIO_FUNC_0, bs));

	// set FN1 block size to 512
	bs = 512;
	ESP_ERROR_CHECK(hosted_sdio_set_blocksize(card, SDIO_FUNC_1, bs));

	return ESP_OK;
}

static esp_err_t sdio_read_fromio(sdmmc_card_t *card, uint32_t function, uint32_t addr,
							uint8_t *data, uint16_t size)
{
	uint16_t remainder = size;
	uint16_t blocks;
	esp_err_t res;
	uint8_t *ptr = data;

	// do block mode transfer
	while (remainder >= ESP_BLOCK_SIZE) {
		blocks = H_SDIO_RX_BLOCKS_TO_TRANSFER(remainder);
		size = blocks * ESP_BLOCK_SIZE;
		res = sdmmc_io_read_blocks(card, function, addr, ptr, size);
		if (res)
			return res;

		remainder -= size;
		ptr += size;
		addr += size;
	}

	// transfer remainder using byte mode
	while (remainder > 0) {
		size = remainder;
		res = sdmmc_io_read_bytes(card, function, addr, ptr, size);
		if (res)
			return res;

		remainder -= size;
		ptr += size;
		addr += size;
	}

	return ESP_OK;
}

static esp_err_t sdio_write_toio(sdmmc_card_t *card, uint32_t function, uint32_t addr,
									uint8_t *data, uint16_t size)
{
	uint16_t remainder = size;
	uint16_t blocks;
	esp_err_t res;
	uint8_t *ptr = data;

	// do block mode transfer
	while (remainder >= ESP_BLOCK_SIZE) {
		blocks = H_SDIO_TX_BLOCKS_TO_TRANSFER(remainder);
		size = blocks * ESP_BLOCK_SIZE;
		res = sdmmc_io_write_blocks(card, function, addr, ptr, size);
		if (res)
			return res;

		remainder -= size;
		ptr += size;
		addr += size;
	}

	// transfer remainder using byte mode
	while (remainder > 0) {
		size = remainder;
		res = sdmmc_io_write_bytes(card, function, addr, ptr, size);
		if (res)
			return res;

		remainder -= size;
		ptr += size;
		addr += size;
	}

	return ESP_OK;
}

int hosted_sdio_deinit(void* ctx)
{
	sdmmc_host_t *host = (sdmmc_host_t *)ctx;
	if (host) {
		sdmmc_host_deinit();
		HOSTED_FREE(host);
	}
	return ESP_OK;
}

void * hosted_sdio_init(void)
{
	esp_err_t res;
	bool got_valid_config = false;

	sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

	if (esp_hosted_transport_is_config_valid()) {
		// copy sdio config if valid
		struct esp_hosted_transport_config *pconfig;

		if (ESP_TRANSPORT_OK == esp_hosted_transport_get_config(&pconfig)) {
			if (pconfig->transport_in_use == H_TRANSPORT_SDIO) {
				struct esp_hosted_sdio_config *psdio_config;

				if (ESP_TRANSPORT_OK == esp_hosted_sdio_get_config(&psdio_config)) {
					// copy transport config
					g_h.funcs->_h_memcpy(&context.config, psdio_config, sizeof(struct esp_hosted_sdio_config));
					got_valid_config = true;
				}
			} else {
				ESP_LOGE(TAG, "transport config is not for SDIO: ignoring");
			}
		}
	}
	if (!got_valid_config) {
		// no valid transport config: use values from esp_hosted_config.h
		context.config.clock_freq_khz = H_SDIO_CLOCK_FREQ_KHZ;
		context.config.bus_width      = H_SDIO_BUS_WIDTH;
		context.config.slot           = H_SDMMC_HOST_SLOT;
		context.config.pin_clk.pin    = H_SDIO_PIN_CLK;
		context.config.pin_cmd.pin    = H_SDIO_PIN_CMD;
		context.config.pin_d0.pin     = H_SDIO_PIN_D0;
		context.config.pin_d1.pin     = H_SDIO_PIN_D1;
		context.config.pin_d2.pin     = H_SDIO_PIN_D2;
		context.config.pin_d3.pin     = H_SDIO_PIN_D3;
	}

	// initialise SDMMC host
	res = sdmmc_host_init();
	if (res != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize SDMMC host");
		return NULL;
	}

	// configure SDIO interface and slot
	slot_config.width = context.config.bus_width;
#if defined(H_SDIO_SOC_USE_GPIO_MATRIX)
	slot_config.clk = context.config.pin_clk.pin;
	slot_config.cmd = context.config.pin_cmd.pin;
	slot_config.d0  = context.config.pin_d0.pin;
	slot_config.d1  = context.config.pin_d1.pin;
	slot_config.d2  = context.config.pin_d2.pin;
	slot_config.d3  = context.config.pin_d3.pin;
#endif

	hosted_sdio_workaround(context.config.slot, &slot_config);

	res = sdmmc_host_init_slot(context.config.slot, &slot_config);
	if (res != ESP_OK) {
		ESP_LOGE(TAG, "init SDMMC Host slot %d failed", H_SDMMC_HOST_SLOT);
		return NULL;
	}

	// initialise connected SDIO card/slave
	context.card = (sdmmc_card_t *)g_h.funcs->_h_malloc(sizeof(sdmmc_card_t));
	if (!context.card) {
		ESP_LOGE(TAG, "Failed to allocate memory for SDMMC card");
		return NULL;
	}

	// initialise mutex for bus locking
	sdio_bus_lock = g_h.funcs->_h_create_mutex();
	assert(sdio_bus_lock);

	return (void *)&context;
}

int hosted_sdio_card_init(void *ctx)
{
	SDIO_FAIL_IF_NULL(ctx);

	sdmmc_context_t *context = (sdmmc_context_t *)ctx;
	SDIO_FAIL_IF_NULL(ctx);

	struct esp_hosted_sdio_config *sdio_config = &context->config;
	SDIO_FAIL_IF_NULL(sdio_config);
	SDIO_FAIL_IF_NULL(context->card);

	sdmmc_host_t config = SDMMC_HOST_DEFAULT();
	config.slot = sdio_config->slot; // override default slot set

	if (!hosted_sdio_enable_ldo(&config)) {
		goto fail;
	}

	config.max_freq_khz = sdio_config->clock_freq_khz;
	ESP_LOGI(TAG, "SDIO master: Slot %d, Data-Lines: %d-bit Freq(KHz)[%u KHz]",
			config.slot,
			sdio_config->bus_width==4? 4:1,
			config.max_freq_khz);
	if (sdio_config->bus_width == 4) {
		ESP_LOGI(TAG, "GPIOs: CLK[%u] CMD[%u] D0[%u] D1[%u] D2[%u] D3[%u] Slave_Reset[%u]",
				sdio_config->pin_clk.pin, sdio_config->pin_cmd.pin,
				sdio_config->pin_d0.pin, sdio_config->pin_d1.pin,
				sdio_config->pin_d2.pin, sdio_config->pin_d3.pin,
				sdio_config->pin_reset.pin);
	} else {
		ESP_LOGI(TAG, "GPIOs: CLK[%u] CMD[%u] D0[%u] D1[%u] Slave_Reset[%u]",
				sdio_config->pin_clk.pin, sdio_config->pin_cmd.pin,
				sdio_config->pin_d0.pin, sdio_config->pin_d1.pin,
				sdio_config->pin_reset.pin);
	}
	ESP_LOGI(TAG, "Queues: Tx[%u] Rx[%u] SDIO-Rx-Mode[%u]",
			sdio_config->tx_queue_size, sdio_config->rx_queue_size,
			sdio_config->rx_mode);

#ifdef CONFIG_IDF_TARGET_ESP32P4
	// Set this flag to allocate aligned buffer of 512 bytes to meet
	// DMA's requirements for CMD53 byte mode. Mandatory when any
	// buffer is behind the cache, or not aligned to 4 byte boundary.
	config.flags |= SDMMC_HOST_FLAG_ALLOC_ALIGNED_BUF;
#endif

	if (sdmmc_card_init(&config, context->card) != ESP_OK) {
		ESP_LOGE(TAG, "sdmmc_card_init failed");
		goto fail;
	}

	if (esp_log_level_get(TAG) >= ESP_LOG_INFO) {
		// output CIS info from the slave
		sdmmc_card_print_info(stdout, context->card);

		if (hosted_sdio_print_cis_information(context->card) != ESP_OK) {
			ESP_LOGW(TAG, "failed to print card info");
		}
	}

	// initialise the card functions
	if (hosted_sdio_card_fn_init(context->card) != ESP_OK) {
		ESP_LOGE(TAG, "sdio_cared_fn_init failed");
		goto fail;
	}
	return ESP_OK;

fail:
	sdmmc_host_deinit();
	if (context->card) {
		HOSTED_FREE(context->card);
	}
	return ESP_FAIL;
}

int hosted_sdio_card_deinit(void *ctx)
{
	SDIO_FAIL_IF_NULL(ctx);

	sdmmc_context_t *context = (sdmmc_context_t *)ctx;
	sdmmc_card_t *card = context->card;

	if (card) {
		sdmmc_host_deinit();
		return ESP_OK;
	}
	return ESP_FAIL;
}

int hosted_sdio_read_reg(void *ctx, uint32_t reg, uint8_t *data, uint16_t size, bool lock_required)
{
	int res = 0;

	SDIO_FAIL_IF_NULL(ctx);

	sdmmc_context_t *context = (sdmmc_context_t *)ctx;
	sdmmc_card_t *card = context->card;
	SDIO_FAIL_IF_NULL(card);

	/* Need to apply address mask when reading/writing slave registers */
	reg &= ESP_ADDRESS_MASK;
	ESP_LOGV(TAG, "%s: reg[0x%" PRIx32"] size[%u]", __func__, reg, size);

	SDIO_LOCK(lock_required);
	if (size <= 1) {
		res = sdmmc_io_read_byte(card, SDIO_FUNC_1, reg, data);
	} else {
		res = sdmmc_io_read_bytes(card, SDIO_FUNC_1, reg, data, size);
	}
	SDIO_UNLOCK(lock_required);
	return res;
}

int hosted_sdio_write_reg(void *ctx, uint32_t reg, uint8_t *data, uint16_t size, bool lock_required)
{
	int res = 0;

	SDIO_FAIL_IF_NULL(ctx);

	sdmmc_context_t *context = (sdmmc_context_t *)ctx;
	sdmmc_card_t *card = context->card;

	/* Need to apply address mask when reading/writing slave registers */
	reg &= ESP_ADDRESS_MASK;
	ESP_LOGV(TAG, "%s: reg[0x%" PRIx32"] size[%u]", __func__, reg, size);

	SDIO_LOCK(lock_required);
	if (size <= 1) {
		res = sdmmc_io_write_byte(card, SDIO_FUNC_1, reg, *data, NULL);
	} else {
		res = sdmmc_io_write_bytes(card, SDIO_FUNC_1, reg, data, size);
	}
	SDIO_UNLOCK(lock_required);
	return res;
}

int hosted_sdio_read_block(void *ctx, uint32_t reg, uint8_t *data, uint16_t size, bool lock_required)
{
	int res = 0;

	SDIO_FAIL_IF_NULL(ctx);

	sdmmc_context_t *context = (sdmmc_context_t *)ctx;
	sdmmc_card_t *card = context->card;
	SDIO_FAIL_IF_NULL(card);
	ESP_LOGV(TAG, "%s: reg[0x%" PRIx32"] size[%u]", __func__, reg, size);

	SDIO_LOCK(lock_required);
	if (size <= 1) {
		res = sdmmc_io_read_byte(card, SDIO_FUNC_1, reg, data);
	} else {
		res = sdio_read_fromio(card, SDIO_FUNC_1, reg, data, H_SDIO_RX_LEN_TO_TRANSFER(size));
	}
	SDIO_UNLOCK(lock_required);
	return res;
}

int hosted_sdio_write_block(void *ctx, uint32_t reg, uint8_t *data, uint16_t size, bool lock_required)
{
	int res = 0;

	SDIO_FAIL_IF_NULL(ctx);

	sdmmc_context_t *context = (sdmmc_context_t *)ctx;
	sdmmc_card_t *card = context->card;
	SDIO_FAIL_IF_NULL(card);
	ESP_LOGV(TAG, "%s: reg[0x%" PRIx32"] size[%u]", __func__, reg, size);

	SDIO_LOCK(lock_required);
	if (size <= 1) {
		res = sdmmc_io_write_byte(card, SDIO_FUNC_1, reg, *data, NULL);
	} else {
		res = sdio_write_toio(card, SDIO_FUNC_1, reg, data, H_SDIO_TX_LEN_TO_TRANSFER(size));
	}
	SDIO_UNLOCK(lock_required);
	return res;
}

/* Blocking fn call. Returns when SDIO slave device generates a SDIO interupt */
int hosted_sdio_wait_slave_intr(void *ctx, uint32_t ticks_to_wait)
{
	SDIO_FAIL_IF_NULL(ctx);

	sdmmc_context_t *context = (sdmmc_context_t *)ctx;
	sdmmc_card_t *card = context->card;

	SDIO_FAIL_IF_NULL(card);

	return sdmmc_io_wait_int(card, ticks_to_wait);
}
