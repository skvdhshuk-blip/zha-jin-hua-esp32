// main/zjh_audio.c
#include "zjh_audio.h"
#include "generated/zjh_audio_assets.h"
#include "bsp_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "zjh_audio";

#define SAMPLE_RATE 16000
#define QUEUE_DEPTH 6

typedef enum {
    CMD_STOP = 0,
    CMD_DEAL,
    CMD_BET,
    CMD_PEEK,
    CMD_WIN,
    CMD_LOSE,
    CMD_CLICK,
    CMD_ENABLED,
} audio_cmd_t;

typedef struct {
    uint8_t cmd;
    uint8_t value;
} audio_msg_t;

static QueueHandle_t s_queue;
static bool s_ready;
static bool s_enabled = true;

static void write_pcm(const int16_t *pcm, size_t samples)
{
    if (!s_enabled) return;
    const size_t chunk = 256;
    size_t done = 0;
    while (done < samples) {
        size_t n = samples - done;
        if (n > chunk) n = chunk;
        if (bsp_audio_write(pcm + done, n * sizeof(int16_t)) != ESP_OK) {
            ESP_LOGW(TAG, "PCM write failed at sample %u", (unsigned)done);
            return;
        }
        done += n;
    }
}

static void audio_task(void *arg)
{
    (void)arg;
    audio_msg_t msg;
    for (;;) {
        if (xQueueReceive(s_queue, &msg, portMAX_DELAY) != pdTRUE) continue;
        switch (msg.cmd) {
        case CMD_STOP:
            vTaskDelete(NULL);
            return;
        case CMD_DEAL:
            write_pcm(zjh_deal_pcm, zjh_deal_pcm_samples);
            break;
        case CMD_BET:
            write_pcm(zjh_bet_pcm, zjh_bet_pcm_samples);
            break;
        case CMD_PEEK:
            write_pcm(zjh_peek_pcm, zjh_peek_pcm_samples);
            break;
        case CMD_WIN:
            write_pcm(zjh_win_pcm, zjh_win_pcm_samples);
            xQueueReset(s_queue);
            break;
        case CMD_LOSE:
            write_pcm(zjh_lose_pcm, zjh_lose_pcm_samples);
            xQueueReset(s_queue);
            break;
        case CMD_CLICK:
            write_pcm(zjh_click_pcm, zjh_click_pcm_samples);
            break;
        case CMD_ENABLED:
            s_enabled = msg.value != 0;
            if (!s_enabled) xQueueReset(s_queue);
            break;
        default:
            break;
        }
    }
}

static bool send(audio_msg_t msg)
{
    if (!s_queue) return false;
    return xQueueSend(s_queue, &msg, 0) == pdTRUE;
}

esp_err_t zjh_audio_init(uint8_t volume, bool enabled)
{
    if (s_ready) return ESP_OK;

    esp_err_t err = bsp_audio_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "codec init failed: %s", esp_err_to_name(err));
        return err;
    }
    err = bsp_audio_set_format(SAMPLE_RATE, 16, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S format setup failed: %s", esp_err_to_name(err));
        return err;
    }

    s_enabled = enabled;
    bsp_audio_set_volume(volume > 100 ? 100 : volume);

    s_queue = xQueueCreate(QUEUE_DEPTH, sizeof(audio_msg_t));
    if (!s_queue) return ESP_ERR_NO_MEM;
    if (xTaskCreate(audio_task, "zjh_audio", 4096, NULL, 4, NULL) != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    s_ready = true;
    return ESP_OK;
}

void zjh_audio_set_enabled(bool enabled)
{
    send((audio_msg_t){ .cmd = CMD_ENABLED, .value = enabled ? 1 : 0 });
}

void zjh_audio_play_deal(void) { send((audio_msg_t){ .cmd = CMD_DEAL }); }
void zjh_audio_play_bet(void) { send((audio_msg_t){ .cmd = CMD_BET }); }
void zjh_audio_play_peek(void) { send((audio_msg_t){ .cmd = CMD_PEEK }); }

void zjh_audio_play_win(void)
{
    if (!s_ready) return;
    xQueueReset(s_queue);
    send((audio_msg_t){ .cmd = CMD_WIN });
}

void zjh_audio_play_lose(void)
{
    if (!s_ready) return;
    xQueueReset(s_queue);
    send((audio_msg_t){ .cmd = CMD_LOSE });
}

void zjh_audio_play_click(void) { send((audio_msg_t){ .cmd = CMD_CLICK }); }
