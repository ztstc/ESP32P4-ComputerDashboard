/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "eppp_link.h"

eppp_transport_handle_t eppp_sdio_init(struct eppp_config_sdio_s *config);
void eppp_sdio_deinit(eppp_transport_handle_t h);
