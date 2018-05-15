// Copyright 2017-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef _ESP_HTTP_FIRMWARE_UPGRADE_H_
#define _ESP_HTTP_FIRMWARE_UPGRADE_H_

#include "httpc.h"
#include "esp_tls.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct verify_params {

    void *(*sign_verify_init)();

    void (*sign_verify_update)(void *ctx, const unsigned char *data_buffer, unsigned long long data_len);

    int (*sign_verify_final)(void *ctx, unsigned char *sig, const unsigned char *pub_key);

    unsigned  int sig_len;

} verify_params_t;

esp_err_t http_firmware_upgrade(const char *url, struct esp_tls_cfg *tls_cfg);

esp_err_t http_firmware_upgrade_with_verify(const char *url, struct esp_tls_cfg *tls_cfg,
        const unsigned char *pub_key, verify_params_t *verify_params);

#ifdef __cplusplus
}
#endif

#endif

