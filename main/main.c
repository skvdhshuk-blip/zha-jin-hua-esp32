// main/main.c -- 扎金花 entry point.
//
// The board and BSP stay untouched; this file initializes the hardware,
// forwards the three buttons to the app and keeps the deep-sleep key-wake
// guard the platform requires. All screens, rules and audio live in the app.
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "key_guard.h"
#include "zjh_app.h"
#include "zjh_store.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <stdatomic.h>

static const char *TAG = "zjh_main";

#define INPUT_QUEUE_DEPTH 8
#define INPUT_TICK_MS 20
#define BATTERY_POLL_MS 15000

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t event;
} input_event_t;

static QueueHandle_t s_input_queue;
static volatile bool s_input_ready;
static atomic_int s_battery_percent = -1;

static void battery_task(void *arg)
{
    (void)arg;
    const esp_err_t err = bsp_battery_init();
    if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "battery gauge unavailable: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }
    for (;;) {
        const int percent = bsp_battery_mv() >= 2500 ? bsp_battery_soc() : -1;
        atomic_store_explicit(&s_battery_percent, percent, memory_order_relaxed);
        vTaskDelay(pdMS_TO_TICKS(BATTERY_POLL_MS));
    }
}

static void input_task(void *arg)
{
    (void)arg;
    input_event_t input;
    for (;;) {
        if (xQueueReceive(s_input_queue, &input, pdMS_TO_TICKS(INPUT_TICK_MS)) == pdTRUE) {
            zjh_app_handle_key(input.btn, input.event);
        }
        zjh_app_tick((uint32_t)(esp_timer_get_time() / 1000),
                     atomic_load_explicit(&s_battery_percent, memory_order_relaxed));
    }
}

static esp_err_t input_dispatch_init(void)
{
    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    if (!s_input_queue) return ESP_ERR_NO_MEM;
    if (xTaskCreate(input_task, "zjh_input", 4096, NULL, 5, NULL) != pdPASS) {
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

// Waking from deep sleep by a key reboots the chip while that key is still
// held; the guard swallows that press until it comes up. Keep the guard check
// before the readiness check: a release that arrives after the post-init
// voltage sample but before input dispatch is ready still has to disarm it.
#define KEY_GUARD_MAX_US (10LL * 1000 * 1000)
static key_guard_t s_key_guard;

static void on_key(bsp_btn_t btn, bsp_btn_ev_t event, void *user)
{
    (void)user;
    const bool was_armed = s_key_guard.armed;
    if (key_guard_consume(&s_key_guard, event == BSP_BTN_RELEASE,
                          esp_timer_get_time())) {
        if (was_armed && !s_key_guard.armed) {
            ESP_LOGI(TAG, "wake key released; buttons live again");
        }
        return;
    }
    if (!s_input_ready || !s_input_queue) return;
    const input_event_t input = { .btn = btn, .event = event };
    (void)xQueueSend(s_input_queue, &input, 0);
}

void app_main(void)
{
    ESP_LOGI(TAG, "zha jin hua boot");

    const uint32_t wake_causes = esp_sleep_get_wakeup_causes();
    if (wake_causes & (1u << ESP_SLEEP_WAKEUP_GPIO)) {
        key_guard_arm(&s_key_guard, esp_timer_get_time(), KEY_GUARD_MAX_US);
        ESP_LOGI(TAG, "key wake: suppressing the held key until release");
    }

    bsp_i2c_init();
    bsp_i2c_scan();

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display/LVGL init failed; check SPI wiring "
                      "(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    if (zjh_store_init() != ESP_OK) {
        ESP_LOGW(TAG, "storage unavailable; coins will not persist");
    }
    zjh_app_init();

    esp_err_t input_err = input_dispatch_init();
    esp_err_t button_err = input_err == ESP_OK
                         ? bsp_button_init(on_key, NULL)
                         : ESP_ERR_INVALID_STATE;
    if (input_err != ESP_OK) {
        ESP_LOGE(TAG, "input task failed: %s", esp_err_to_name(input_err));
    } else if (button_err != ESP_OK) {
        ESP_LOGE(TAG, "button init failed: %s", esp_err_to_name(button_err));
    } else if (s_key_guard.armed) {
        const bool released = bsp_button_read_mv() >= BSP_BTN_MV_RELEASED_MIN;
        key_guard_sync_released(&s_key_guard, released);
        if (!s_key_guard.armed) ESP_LOGI(TAG, "wake key already released");
    }

    s_input_ready = input_err == ESP_OK && button_err == ESP_OK;
    if (xTaskCreate(battery_task, "zjh_battery", 3072, NULL, 3, NULL) != pdPASS) {
        ESP_LOGW(TAG, "battery task unavailable");
    }
    ESP_LOGI(TAG, "ready: input=%d", (int)s_input_ready);
}
