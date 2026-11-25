// main/zjh_store.h
// NVS persistence for 扎金花: coin balance, record and audio preference.
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define ZJH_START_CHIPS 1000
#define ZJH_RELIEF_CHIPS 100

typedef struct {
    int32_t chips;
    uint32_t hands;
    uint32_t wins;
    uint32_t best_win;
    uint32_t streak;
    uint32_t best_streak;
    uint8_t volume;
    bool sound_on;
} zjh_profile_t;

esp_err_t zjh_store_init(void);

// Fills defaults for missing keys. Returns true when any key existed.
bool zjh_store_load(zjh_profile_t *profile);
esp_err_t zjh_store_save(const zjh_profile_t *profile);

void zjh_profile_defaults(zjh_profile_t *profile);
