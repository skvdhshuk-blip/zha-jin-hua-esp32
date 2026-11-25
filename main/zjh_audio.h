// main/zjh_audio.h
// Non-blocking sound playback for 扎金花. Button handlers only enqueue; the
// audio worker owns the codec and all PCM writes.
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

esp_err_t zjh_audio_init(uint8_t volume, bool enabled);
void zjh_audio_set_enabled(bool enabled);
void zjh_audio_play_deal(void);
void zjh_audio_play_bet(void);
void zjh_audio_play_peek(void);
void zjh_audio_play_win(void);
void zjh_audio_play_lose(void);
void zjh_audio_play_click(void);
