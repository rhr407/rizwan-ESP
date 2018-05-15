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
#include "http-firmware-upgrade-sign-ed25519.h"
#include "sodium/crypto_sign_ed25519.h"

crypto_hash_sha512_state state;

static void *sha512_init()
{
    crypto_hash_sha512_init(&state);
    return &state;
}

static void sha512_data_update(void *ctx, const unsigned char *data_buffer, unsigned long long data_len)
{
    crypto_hash_sha512_update(ctx, data_buffer, data_len);
}

static int sha512_final_ed25519_verify(void *ctx, unsigned char *sig, const unsigned char *pub_key)
{
    unsigned char sha512_dgst[crypto_hash_sha512_BYTES];
    crypto_hash_sha512_final(ctx, sha512_dgst);

    return crypto_sign_ed25519_verify_detached(sig, sha512_dgst, crypto_hash_sha512_BYTES, pub_key);
}

esp_err_t http_firmware_upgrade_ed25519_sign_verify(const char *url,
        struct esp_tls_cfg *tls_cfg, const unsigned char *pub_key)
{

    verify_params_t params = {
        .sig_len = crypto_sign_ed25519_BYTES,
        .sign_verify_init = sha512_init,
        .sign_verify_update = sha512_data_update,
        .sign_verify_final = sha512_final_ed25519_verify,
    };

    esp_err_t err = http_firmware_upgrade_with_verify(url, tls_cfg, pub_key, &params);
    return err;
}
