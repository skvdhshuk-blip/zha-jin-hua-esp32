// main/zjh_store.c
#include "zjh_store.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "zjh_store";
static const char *NS = "zjh";
static bool s_ready;

void zjh_profile_defaults(zjh_profile_t *profile)
{
    if (!profile) return;
    *profile = (zjh_profile_t){
        .chips = ZJH_START_CHIPS,
        .hands = 0,
        .wins = 0,
        .best_win = 0,
        .streak = 0,
        .best_streak = 0,
        .volume = 70,
        .sound_on = true,
    };
}

esp_err_t zjh_store_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "erasing incompatible NVS: %s", esp_err_to_name(err));
        err = nvs_flash_erase();
        if (err == ESP_OK) err = nvs_flash_init();
    }
    s_ready = err == ESP_OK;
    if (!s_ready) ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
    return err;
}

bool zjh_store_load(zjh_profile_t *profile)
{
    zjh_profile_defaults(profile);
    if (!s_ready || !profile) return false;

    nvs_handle_t handle;
    if (nvs_open(NS, NVS_READONLY, &handle) != ESP_OK) return false;

    bool found = false;
    int32_t chips = 0;
    uint32_t value = 0;
    uint8_t byte = 0;
    if (nvs_get_i32(handle, "chips", &chips) == ESP_OK) {
        profile->chips = chips;
        found = true;
    }
    if (nvs_get_u32(handle, "hands", &value) == ESP_OK) {
        profile->hands = value;
        found = true;
    }
    if (nvs_get_u32(handle, "wins", &value) == ESP_OK) {
        profile->wins = value;
        found = true;
    }
    if (nvs_get_u32(handle, "best_net", &value) == ESP_OK) {
        profile->best_win = value;
        found = true;
    }
    if (nvs_get_u32(handle, "streak", &value) == ESP_OK) {
        profile->streak = value;
        found = true;
    }
    if (nvs_get_u32(handle, "bstreak", &value) == ESP_OK) {
        profile->best_streak = value;
        found = true;
    }
    if (nvs_get_u8(handle, "volume", &byte) == ESP_OK) {
        profile->volume = byte > 100 ? 100 : byte;
        found = true;
    }
    if (nvs_get_u8(handle, "sound", &byte) == ESP_OK) {
        profile->sound_on = byte != 0;
        found = true;
    }
    nvs_close(handle);

    if (profile->chips < 0) profile->chips = 0;
    if (profile->chips > 999999) profile->chips = 999999;
    return found;
}

esp_err_t zjh_store_save(const zjh_profile_t *profile)
{
    if (!s_ready || !profile) return ESP_ERR_INVALID_STATE;
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_i32(handle, "chips", profile->chips);
    if (err == ESP_OK) err = nvs_set_u32(handle, "hands", profile->hands);
    if (err == ESP_OK) err = nvs_set_u32(handle, "wins", profile->wins);
    if (err == ESP_OK) err = nvs_set_u32(handle, "best_net", profile->best_win);
    if (err == ESP_OK) err = nvs_set_u32(handle, "streak", profile->streak);
    if (err == ESP_OK) err = nvs_set_u32(handle, "bstreak", profile->best_streak);
    if (err == ESP_OK) err = nvs_set_u8(handle, "volume", profile->volume);
    if (err == ESP_OK) err = nvs_set_u8(handle, "sound", profile->sound_on ? 1 : 0);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
    return err;
}
