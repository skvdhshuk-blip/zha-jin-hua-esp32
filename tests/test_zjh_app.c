// Exercise the real application entry points with a checked LVGL lock.
#include "bsp_display.h"
#include "zjh_app.h"
#include "zjh_audio.h"
#include "zjh_clock.h"
#include "zjh_ui.h"
#include "esp_timer.h"

#include <assert.h>
#include <stdio.h>

static unsigned lock_depth;
static uint32_t time_ms;
static zjh_game_t table;
static int cursor;
static bool actions[ZJH_ACTIONS];
static bool result;
static int rules_page;
static unsigned toasts;
static unsigned saved_hands;
static unsigned revealed_seats;

static int clock_minutes = -1;
static int shown_minutes = -1;
static int shown_battery = -1;
static int clock_field;
static int clock_draft;

enum { MENU, TABLE, RULES, STATS, STANDBY };
static int visible_screen;

bool bsp_lvgl_lock(int timeout_ms)
{
    (void)timeout_ms;
    lock_depth++;
    return true;
}

void bsp_lvgl_unlock(void)
{
    assert(lock_depth > 0);
    lock_depth--;
}

static void require_lock(void) { assert(lock_depth > 0); }

void zjh_ui_init(void) { require_lock(); }
void zjh_ui_show_menu(void) { require_lock(); visible_screen = MENU; }
void zjh_ui_show_table(void) { require_lock(); visible_screen = TABLE; }
void zjh_ui_show_rules(void) { require_lock(); visible_screen = RULES; }
void zjh_ui_show_stats(void) { require_lock(); visible_screen = STATS; }
void zjh_ui_show_standby(void) { require_lock(); visible_screen = STANDBY; }

void zjh_ui_status(int minutes, int battery_percent)
{
    require_lock();
    shown_minutes = minutes;
    shown_battery = battery_percent;
}

void zjh_ui_standby(int field, int draft_minutes)
{
    require_lock();
    clock_field = field;
    clock_draft = draft_minutes;
}

int zjh_clock_get_minutes(void) { return clock_minutes; }
bool zjh_clock_set_minutes(int minutes) { clock_minutes = minutes; return true; }
void zjh_ui_deal(void) { require_lock(); }
void zjh_ui_hide_result(void) { require_lock(); result = false; }
void zjh_ui_toast(const char *text) { require_lock(); assert(*text); toasts++; }
void zjh_ui_info(const char *text) { require_lock(); (void)text; }
void zjh_ui_rules(int page) { require_lock(); rules_page = page; }
void zjh_ui_stats(const zjh_profile_t *profile) { require_lock(); (void)profile; }

void zjh_ui_menu(const zjh_profile_t *profile, int selected)
{
    require_lock();
    (void)profile;
    (void)selected;
}

void zjh_ui_table(const zjh_game_t *game, int selection,
                  const bool enabled[ZJH_ACTIONS], bool targeting, uint8_t target)
{
    require_lock();
    (void)targeting;
    (void)target;
    table = *game;
    cursor = selection;
    for (int i = 0; i < ZJH_ACTIONS; i++) actions[i] = enabled[i];
}

void zjh_ui_result_row(int index, const char *name, const zjh_seat_t *seat, bool winner)
{
    require_lock();
    assert(table.over && index >= 0 && index < ZJH_SEATS);
    assert(*name && winner == (index == table.winner));
    for (int i = 0; i < ZJH_HAND_SIZE; i++) {
        assert(seat->cards[i] == table.seats[index].cards[i]);
    }
    assert(seat->hand.cat == table.seats[index].hand.cat);
    revealed_seats |= 1u << index;
}

void zjh_ui_show_result(const char *title, const char *subtitle,
                        const char *payout, bool win)
{
    require_lock();
    (void)title;
    (void)subtitle;
    (void)payout;
    (void)win;
    result = true;
}

int64_t esp_timer_get_time(void) { return (int64_t)time_ms * 1000; }

bool zjh_store_load(zjh_profile_t *profile)
{
    *profile = (zjh_profile_t){.chips = ZJH_START_CHIPS};
    return false;
}

esp_err_t zjh_store_save(const zjh_profile_t *profile)
{
    saved_hands = profile->hands;
    return ESP_OK;
}

esp_err_t zjh_audio_init(uint8_t volume, bool enabled)
{
    (void)volume;
    (void)enabled;
    return ESP_OK;
}

void zjh_audio_set_enabled(bool enabled) { (void)enabled; }
void zjh_audio_play_deal(void) {}
void zjh_audio_play_bet(void) {}
void zjh_audio_play_peek(void) {}
void zjh_audio_play_win(void) {}
void zjh_audio_play_lose(void) {}
void zjh_audio_play_click(void) {}

static void key(bsp_btn_t button, bsp_btn_ev_t event)
{
    zjh_app_handle_key(button, event);
    assert(lock_depth == 0);
}

static void tick(void)
{
    time_ms += 1000;
    zjh_app_tick(time_ms, 68);
    assert(lock_depth == 0);
}

int main(void)
{
    zjh_app_init();
    assert(lock_depth == 0 && visible_screen == MENU);
    assert(shown_minutes == -1 && shown_battery == -1);
    tick();
    assert(shown_battery == 68);
    key(BSP_BTN_UP, BSP_BTN_CLICK);
    key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(visible_screen == STANDBY && clock_field == -1);
    key(BSP_BTN_OK, BSP_BTN_CLICK);
    key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    assert(clock_field == 0 && clock_draft == 23 * 60);
    key(BSP_BTN_OK, BSP_BTN_CLICK);
    key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    assert(clock_field == 1 && clock_draft == 23 * 60 + 59);
    key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(shown_minutes == 23 * 60 + 59 && clock_field == -1);
    clock_minutes = 0;
    tick();
    assert(shown_minutes == 0);
    key(BSP_BTN_OK, BSP_BTN_CLICK);
    key(BSP_BTN_UP, BSP_BTN_CLICK);
    key(BSP_BTN_UP, BSP_BTN_LONG);
    assert(visible_screen == STANDBY && clock_minutes == 0 && clock_field == -1);
    key(BSP_BTN_UP, BSP_BTN_LONG);
    assert(visible_screen == MENU);
    key(BSP_BTN_DOWN, BSP_BTN_CLICK); // Back to Start after returning from Standby.

    key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(visible_screen == RULES && rules_page == 0);
    key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(rules_page == 1);
    key(BSP_BTN_UP, BSP_BTN_LONG);
    assert(visible_screen == MENU);

    key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(visible_screen == STATS);
    key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(visible_screen == MENU);

    key(BSP_BTN_UP, BSP_BTN_CLICK);
    key(BSP_BTN_UP, BSP_BTN_CLICK);
    key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(visible_screen == TABLE && cursor == ZJH_ACT_PEEK && actions[ZJH_ACT_PEEK]);
    key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(table.seats[0].peeked && table.turn == 0);
    assert(cursor == ZJH_ACT_CALL && actions[ZJH_ACT_CALL] && !actions[ZJH_ACT_PEEK]);
    assert(toasts == 1);
    key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(table.turn == 1 && table.last_amount == 20 && toasts == 2);

    for (unsigned steps = 0; steps < 100 && !result; steps++) {
        if (table.turn == 0) {
            while (cursor != ZJH_ACT_FOLD) key(BSP_BTN_DOWN, BSP_BTN_CLICK);
            key(BSP_BTN_OK, BSP_BTN_CLICK);
        } else {
            tick();
        }
    }
    assert(result && saved_hands == 1);
    assert(revealed_seats == (1u << ZJH_SEATS) - 1);
    key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(visible_screen == TABLE && !result && !table.seats[0].peeked);

    puts("zjh app: PASS (rules, stats, peek, call, AI, result, next hand)");
    return 0;
}
