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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http-firmware-upgrade.h"
#include "sodium/crypto_hash_sha512.h"

#include "esp_ota_ops.h"
#include "esp_log.h"

#define OTA_BUF_SIZE 255

static const char *TAG = "http-firmware-upgrade";
static char upgrade_data_buf[OTA_BUF_SIZE + 1];

static unsigned char *read_signature(httpc_conn_t *h, int sig_len)
{
    unsigned char *sig = (unsigned char *)malloc(sizeof(unsigned char) * sig_len);
    if (sig == NULL) {
        ESP_LOGE(TAG, "Memory couldn't be allocated to signature buffer");
        goto malloc_failed;
    }

    int data_read, data_left = sig_len, index = 0;
    while (data_left != 0) {
        data_read = http_response_recv(h, (char *)sig + index, data_left);
        if (data_read == 0) {
            ESP_LOGI(TAG, "Connection closed");
            goto sig_read_failed;
        }
        if (data_read < 0) {
            ESP_LOGE(TAG, "Error: SSL data read error");
            goto sig_read_failed;
        }
        if (data_read > 0) {
            data_left -= data_read;
            index += data_read;
        }
    }
    return sig;

sig_read_failed:
    free(sig);

malloc_failed:
    http_simple_get_delete(h);
    return NULL;
}

esp_err_t  http_firmware_upgrade_with_verify(const char *url, struct esp_tls_cfg *tls_cfg,
        const unsigned char *pub_key, verify_params_t *verify_params)
{
    httpc_conn_t *h = http_simple_get_new(url, tls_cfg);
    if (!h) {
        return ESP_FAIL;
    }
    http_header_fetch(h);

    unsigned char *sig = NULL;
    void *ctx = NULL;
    if (verify_params != NULL) {
        sig = read_signature(h, verify_params->sig_len);
        if (!sig) {
            ESP_LOGE(TAG, "Signature couldn't be read");
            return ESP_FAIL;
        }
        ctx = verify_params->sign_verify_init();
    }

    esp_err_t err;
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;
    ESP_LOGI(TAG, "Starting OTA...");

    update_partition = esp_ota_get_next_update_partition(NULL);
    assert(update_partition != NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);

    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed, error=%d", err);
        http_simple_get_delete(h);
        free(sig);
        return err;
    }
    ESP_LOGI(TAG, "esp_ota_begin succeeded");

    esp_err_t ota_write_err = ESP_OK;
    int binary_file_len = 0;
    while (1) {
        int data_read = http_response_recv(h, upgrade_data_buf, OTA_BUF_SIZE);
        if (data_read == 0) {
            ESP_LOGI(TAG, "Connection closed,all data received");
            break;
        }
        if (data_read < 0) {
            ESP_LOGE(TAG, "Error: SSL data read error");
            break;
        }
        if (data_read > 0) {
            ota_write_err = esp_ota_write( update_handle, (const void *)upgrade_data_buf, data_read);
            if (ota_write_err != ESP_OK) {
                ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%d", err);
                break;
            }

            if (verify_params != NULL) {
                verify_params->sign_verify_update(ctx, (const unsigned char *)upgrade_data_buf, data_read);
            }

            binary_file_len += data_read;
            ESP_LOGD(TAG, "Written image length %d", binary_file_len);
        }
    }

    http_simple_get_delete(h);

    esp_err_t ota_end_err = esp_ota_end(update_handle);
    if (ota_write_err != ESP_OK) {
        free(sig);
        return ota_write_err;
    } else if (ota_end_err != ESP_OK) {
        free(sig);
        return ota_end_err;
    }

    if (verify_params != NULL) {
        int sign_verify_result = verify_params->sign_verify_final(ctx, sig, pub_key);
        free(sig);
        if (sign_verify_result != 0) {
            ESP_LOGE(TAG, "Signature Verification Failed");
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "Total binary data length writen: %d", binary_file_len);

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%d", err);
        return err;
    }

    return ESP_OK;
}

esp_err_t  http_firmware_upgrade(const char *url, struct esp_tls_cfg *tls_cfg)
{
    esp_err_t result = http_firmware_upgrade_with_verify(url, tls_cfg, NULL, NULL);
    return result;
}
