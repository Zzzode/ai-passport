#include "pet_store.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "pet_decor.h"
#include "pet_save.h"

static const char *TAG = "pet_store";
static const char *NS = "pet";
static const char *KEY_A = "a";
static const char *KEY_B = "b";
static const char *KEY_CUR = "cur";
static const char *KEY_MED = "med";
static const char *KEY_VOL = "vol";
static const char *KEY_QUIET = "qt";

static nvs_handle_t s_handle;
static SemaphoreHandle_t s_lock;
static bool s_have_handle;
static int8_t s_current;   // 0/1=当前 bank；-1=尚无有效存档

static bool read_bank(const char *key, pt_state_t *out, pt_econ_t *econ,
                      pt_jobs_t *jobs, pt_decor_t *decor)
{
    uint8_t blob[PET_SAVE_BYTES];
    size_t len = sizeof(blob);
    esp_err_t err = nvs_get_blob(s_handle, key, blob, &len);
    if (err != ESP_OK || len != PET_SAVE_BYTES) {
        return false;
    }
    return pet_save_decode(out, econ, jobs, decor, blob, len);
}

static bool write_bank(const char *key, const pt_state_t *state,
                       const pt_econ_t *econ, const pt_jobs_t *jobs,
                       const pt_decor_t *decor)
{
    uint8_t blob[PET_SAVE_BYTES];
    if (pet_save_encode(state, econ, jobs, decor, blob, sizeof(blob))
        != PET_SAVE_BYTES) {
        return false;
    }
    esp_err_t err = nvs_set_blob(s_handle, key, blob, PET_SAVE_BYTES);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_blob(%s) failed: %s", key, esp_err_to_name(err));
        return false;
    }
    err = nvs_commit(s_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit(%s) failed: %s", key, esp_err_to_name(err));
        return false;
    }
    // 读回校验：CRC 不对（或写入残缺）时本次切换作废，旧 bank 不动。
    uint8_t verify[PET_SAVE_BYTES];
    size_t len = sizeof(verify);
    if (nvs_get_blob(s_handle, key, verify, &len) != ESP_OK
        || len != PET_SAVE_BYTES
        || memcmp(blob, verify, PET_SAVE_BYTES) != 0) {
        ESP_LOGE(TAG, "bank %s readback verify failed", key);
        return false;
    }
    return true;
}

void pet_store_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_current = -1;

    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        // 与官方 demo 一致：绝不主动 erase_nvs，避免误删用户数据。
        ESP_LOGW(TAG, "nvs_flash_init failed: %s", esp_err_to_name(err));
        s_have_handle = false;
        return;
    }

    err = nvs_open(NS, NVS_READWRITE, &s_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        s_have_handle = false;
        return;
    }
    s_have_handle = true;

    uint8_t cur = 0;
    if (nvs_get_u8(s_handle, KEY_CUR, &cur) == ESP_OK && cur <= 1) {
        s_current = (int8_t) cur;
    }
}

bool pet_store_load(pt_state_t *out, pt_econ_t *econ, pt_jobs_t *jobs,
                    pt_decor_t *decor, bool *med_unlocked,
                    uint8_t *volume)
{
    bool ok = false;
    if (med_unlocked != NULL) {
        *med_unlocked = false;
    }
    if (volume != NULL) {
        *volume = 2;   // 默认中档
    }
    if (!s_have_handle) {
        return false;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);

    if (med_unlocked != NULL) {
        uint8_t med = 0;
        if (nvs_get_u8(s_handle, KEY_MED, &med) == ESP_OK) {
            *med_unlocked = med != 0;
        }
    }
    if (volume != NULL) {
        uint8_t vol = 2;
        if (nvs_get_u8(s_handle, KEY_VOL, &vol) == ESP_OK && vol <= 3) {
            *volume = vol;
        }
    }

    // 先试当前 bank，损坏则回退另一份。
    const char *first = (s_current == 1) ? KEY_B : KEY_A;
    const char *second = (s_current == 1) ? KEY_A : KEY_B;
    if (read_bank(first, out, econ, jobs, decor)) {
        ok = true;
    } else if (read_bank(second, out, econ, jobs, decor)) {
        ESP_LOGW(TAG, "current bank unreadable, fell back to other bank");
        s_current = (s_current == 1) ? 0 : 1;
        ok = true;
    }

    xSemaphoreGive(s_lock);
    return ok;
}

bool pet_store_save(const pt_state_t *state, const pt_econ_t *econ,
                    const pt_jobs_t *jobs, const pt_decor_t *decor)
{
    if (!s_have_handle) {
        return false;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);

    // 写"另一个"bank；首次保存（无当前）时先写 A。
    int8_t target = (s_current < 0) ? 0 : (int8_t) (1 - s_current);
    const char *key = (target == 1) ? KEY_B : KEY_A;

    bool ok = false;
    if (write_bank(key, state, econ, jobs, decor)) {
        if (nvs_set_u8(s_handle, KEY_CUR, (uint8_t) target) == ESP_OK
            && nvs_commit(s_handle) == ESP_OK) {
            s_current = target;
            ok = true;
        }
    }

    xSemaphoreGive(s_lock);
    return ok;
}

void pet_store_set_med(bool unlocked)
{
    if (!s_have_handle) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    nvs_set_u8(s_handle, KEY_MED, unlocked ? 1 : 0);
    nvs_commit(s_handle);
    xSemaphoreGive(s_lock);
}

void pet_store_set_volume(uint8_t volume)
{
    if (!s_have_handle || volume > 3) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    nvs_set_u8(s_handle, KEY_VOL, volume);
    nvs_commit(s_handle);
    xSemaphoreGive(s_lock);
}

bool pet_store_quiet(void)
{
    if (!s_have_handle) {
        return true;   // 无 NVS 的降级环境按免打扰默认开启
    }
    uint8_t q = 1;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (nvs_get_u8(s_handle, KEY_QUIET, &q) != ESP_OK) {
        q = 1;
    }
    xSemaphoreGive(s_lock);
    return q != 0;
}

void pet_store_set_quiet(bool enabled)
{
    if (!s_have_handle) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    nvs_set_u8(s_handle, KEY_QUIET, enabled ? 1 : 0);
    nvs_commit(s_handle);
    xSemaphoreGive(s_lock);
}

// ---- P2-S3a 社交第二组双 bank ----
static const char *SKEY_A = "sa";
static const char *SKEY_B = "sb";
static const char *SKEY_CUR = "sc";
static int8_t s_soc_cur = -1;

bool pet_store_social_load(uint8_t *buf, size_t len)
{
    if (!s_have_handle) {
        return false;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_soc_cur < 0) {
        uint8_t cur = 0;
        if (nvs_get_u8(s_handle, SKEY_CUR, &cur) == ESP_OK && cur <= 1) {
            s_soc_cur = (int8_t) cur;
        }
    }
    bool ok = false;
    const char *first = (s_soc_cur == 1) ? SKEY_B : SKEY_A;
    const char *second = (s_soc_cur == 1) ? SKEY_A : SKEY_B;
    size_t rl = len;
    if (nvs_get_blob(s_handle, first, buf, &rl) == ESP_OK && rl == len) {
        ok = true;
    } else {
        rl = len;
        if (nvs_get_blob(s_handle, second, buf, &rl) == ESP_OK && rl == len) {
            s_soc_cur = (s_soc_cur == 1) ? 0 : 1;
            ok = true;
        }
    }
    xSemaphoreGive(s_lock);
    return ok;
}

bool pet_store_social_save(const uint8_t *buf, size_t len)
{
    if (!s_have_handle) {
        return false;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int8_t target = (s_soc_cur < 0) ? 0 : (int8_t) (1 - s_soc_cur);
    const char *key = (target == 1) ? SKEY_B : SKEY_A;
    bool ok = false;
    esp_err_t err = nvs_set_blob(s_handle, key, buf, len);
    if (err == ESP_OK && nvs_commit(s_handle) == ESP_OK) {
        uint8_t *verify = (uint8_t *) malloc(len);
        if (verify != NULL) {
            size_t rl = len;
            if (nvs_get_blob(s_handle, key, verify, &rl) == ESP_OK
                && rl == len && memcmp(buf, verify, len) == 0
                && nvs_set_u8(s_handle, SKEY_CUR, (uint8_t) target) == ESP_OK
                && nvs_commit(s_handle) == ESP_OK) {
                s_soc_cur = target;
                ok = true;
            }
            free(verify);
        }
    }
    if (!ok) {
        ESP_LOGE(TAG, "social bank %s write/verify failed", key);
    }
    xSemaphoreGive(s_lock);
    return ok;
}

// ---- P2-S4 图鉴第三组双 bank ----
static const char *DKEY_A = "da";
static const char *DKEY_B = "db";
static const char *DKEY_CUR = "dc";
static int8_t s_dex_cur = -1;

bool pet_store_dex_load(uint8_t *buf, size_t len)
{
    if (!s_have_handle) {
        return false;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_dex_cur < 0) {
        uint8_t cur = 0;
        if (nvs_get_u8(s_handle, DKEY_CUR, &cur) == ESP_OK && cur <= 1) {
            s_dex_cur = (int8_t) cur;
        }
    }
    bool ok = false;
    const char *first = (s_dex_cur == 1) ? DKEY_B : DKEY_A;
    const char *second = (s_dex_cur == 1) ? DKEY_A : DKEY_B;
    size_t rl = len;
    if (nvs_get_blob(s_handle, first, buf, &rl) == ESP_OK && rl == len) {
        ok = true;
    } else {
        rl = len;
        if (nvs_get_blob(s_handle, second, buf, &rl) == ESP_OK && rl == len) {
            s_dex_cur = (s_dex_cur == 1) ? 0 : 1;
            ok = true;
        }
    }
    xSemaphoreGive(s_lock);
    return ok;
}

bool pet_store_dex_save(const uint8_t *buf, size_t len)
{
    if (!s_have_handle) {
        return false;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int8_t target = (s_dex_cur < 0) ? 0 : (int8_t) (1 - s_dex_cur);
    const char *key = (target == 1) ? DKEY_B : DKEY_A;
    bool ok = false;
    esp_err_t err = nvs_set_blob(s_handle, key, buf, len);
    if (err == ESP_OK && nvs_commit(s_handle) == ESP_OK) {
        uint8_t *verify = (uint8_t *) malloc(len);
        if (verify != NULL) {
            size_t rl = len;
            if (nvs_get_blob(s_handle, key, verify, &rl) == ESP_OK
                && rl == len && memcmp(buf, verify, len) == 0
                && nvs_set_u8(s_handle, DKEY_CUR, (uint8_t) target) == ESP_OK
                && nvs_commit(s_handle) == ESP_OK) {
                s_dex_cur = target;
                ok = true;
            }
            free(verify);
        }
    }
    if (!ok) {
        ESP_LOGE(TAG, "dex bank %s write/verify failed", key);
    }
    xSemaphoreGive(s_lock);
    return ok;
}
