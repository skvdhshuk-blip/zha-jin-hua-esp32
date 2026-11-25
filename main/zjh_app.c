// main/zjh_app.c
#include "zjh_app.h"
#include "bsp_display.h"
#include "zjh_ai.h"
#include "zjh_audio.h"
#include "zjh_clock.h"
#include "zjh_round.h"
#include "zjh_store.h"
#include "zjh_ui.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "zjh";

#define AI_THINK_MS    650
#define AI_PEEK_MS     320
#define RULES_PAGES    3
#define MENU_START     0
#define MENU_RULES     1
#define MENU_STATS     2
#define MENU_SOUND     3
#define MENU_STANDBY   4
#define STATUS_POLL_MS 1000

typedef enum {
    CLOCK_VIEW = -1,
    CLOCK_HOURS,
    CLOCK_MINUTES,
} clock_field_t;

typedef enum {
    SCREEN_MENU = 0,
    SCREEN_TABLE,
    SCREEN_RULES,
    SCREEN_STATS,
    SCREEN_STANDBY,
} screen_t;

static zjh_profile_t s_profile;
static zjh_game_t s_game;
static screen_t s_screen = SCREEN_MENU;
static bool s_audio_ok;

static int s_menu_index;
static int s_action_index;
static int s_rules_page;
static bool s_targeting;
static uint8_t s_target;

static uint32_t s_ai_due_ms;
static int32_t s_hand_start_chips;
static bool s_result_visible;
static clock_field_t s_clock_field;
static int s_clock_draft;
static int s_battery = -1;
static int s_last_clock = -2;
static int s_last_battery = -2;
static uint32_t s_status_due;

static const char *const k_seat_names[ZJH_SEATS] = { "你", "老陈", "阿豹", "小桃" };

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static bool ui_lock(void)
{
    // The input task can wait for rendering. The button callback only queues events.
    return bsp_lvgl_lock(0);
}

static void refresh_status(void)
{
    const int minutes = zjh_clock_get_minutes();
    if (minutes == s_last_clock && s_battery == s_last_battery) return;
    if (!ui_lock()) return;
    zjh_ui_status(minutes, s_battery);
    s_last_clock = minutes;
    s_last_battery = s_battery;
    bsp_lvgl_unlock();
}

static void save_profile(void)
{
    zjh_store_save(&s_profile);
}

static void refresh_menu(void)
{
    if (!ui_lock()) return;
    zjh_ui_menu(&s_profile, s_menu_index);
    bsp_lvgl_unlock();
}

static void return_to_menu(void)
{
    if (!ui_lock()) return;
    zjh_ui_hide_result();
    zjh_ui_menu(&s_profile, s_menu_index);
    zjh_ui_show_menu();
    s_result_visible = false;
    s_screen = SCREEN_MENU;
    bsp_lvgl_unlock();
}

static void refresh_table(void)
{
    bool enabled[ZJH_ACTIONS];
    for (int i = 0; i < ZJH_ACTIONS; i++) {
        enabled[i] = !s_game.over && s_game.turn == 0 &&
                     zjh_action_available(&s_game, 0, (zjh_action_t)i);
    }
    if (s_game.turn == 0 && !s_targeting && !enabled[s_action_index]) {
        for (int i = 0; i < ZJH_ACTIONS; i++) {
            if (enabled[i]) {
                s_action_index = i;
                break;
            }
        }
    }

    char info[64] = "";
    if (s_targeting) {
        snprintf(info, sizeof(info), "%s · 确定比牌 / 长按上取消",
                 s_target < ZJH_SEATS ? k_seat_names[s_target] : "-");
    } else if (s_game.turn == 0 && !s_game.over) {
        const unsigned call = zjh_call_cost(&s_game, 0);
        switch (s_action_index) {
        case ZJH_ACT_PEEK:
            snprintf(info, sizeof(info), "免费 · 看牌后跟注翻倍");
            break;
        case ZJH_ACT_CALL:
            snprintf(info, sizeof(info), "跟注 %u 金币", call);
            break;
        case ZJH_ACT_RAISE: {
            const unsigned raise = call + ZJH_STAKE_STEP * (s_game.seats[0].peeked ? 2u : 1u);
            snprintf(info, sizeof(info), "加注 %u 金币", raise);
            break;
        }
        case ZJH_ACT_COMPARE:
            snprintf(info, sizeof(info), "比牌 %u 金币", (unsigned)zjh_compare_cost(&s_game, 0));
            break;
        case ZJH_ACT_FOLD:
            snprintf(info, sizeof(info), "弃牌后等待本局结束");
            break;
        }
    } else if (!s_game.seats[0].in_hand) {
        snprintf(info, sizeof(info), "你已弃牌，等待本局结束");
    } else if (!s_game.over) {
        snprintf(info, sizeof(info), "等待 %s 行动…",
                 k_seat_names[s_game.turn < ZJH_SEATS ? s_game.turn : 0]);
    }

    if (!ui_lock()) return;
    zjh_ui_table(&s_game, s_action_index, enabled, s_targeting, s_target);
    zjh_ui_info(info);
    bsp_lvgl_unlock();
}

// ---------------------------------------------------------------------------
// Hand flow
// ---------------------------------------------------------------------------

static void finish_hand(void);

static void start_hand(void)
{
    if (s_profile.chips < ZJH_ANTE) {
        s_profile.chips += ZJH_RELIEF_CHIPS;
        save_profile();
    }
    s_hand_start_chips = s_profile.chips;

    int32_t stacks[ZJH_SEATS] = {
        s_profile.chips, 1000, 1000, 1000,
    };
    const uint32_t seed = now_ms() ^ ((uint32_t)s_profile.chips << 8) ^
                          (uint32_t)s_profile.hands * 2654435761u;
    zjh_game_start(&s_game, stacks, seed);

    s_action_index = 0;
    s_targeting = false;
    s_target = ZJH_SEAT_NONE;
    s_result_visible = false;
    s_ai_due_ms = now_ms() + AI_THINK_MS;
    s_screen = SCREEN_TABLE;

    if (s_audio_ok) zjh_audio_play_deal();
    refresh_table();
    if (ui_lock()) {
        zjh_ui_hide_result();
        zjh_ui_show_table();
        zjh_ui_deal();
        bsp_lvgl_unlock();
    }
    if (s_profile.chips < ZJH_ANTE) {
        // Should not happen after the relief top-up, but keep the table usable.
        ESP_LOGW(TAG, "player short stack after relief");
    }
}

static void describe_action(uint8_t seat, zjh_action_t action, uint8_t target,
                            int32_t amount, char *out, size_t size)
{
    const char *seat_name = k_seat_names[seat < ZJH_SEATS ? seat : 0];
    switch (action) {
    case ZJH_ACT_PEEK:
        snprintf(out, size, "%s 看牌", seat_name);
        break;
    case ZJH_ACT_CALL:
        snprintf(out, size, "%s 跟注 %d", seat_name, (int)amount);
        break;
    case ZJH_ACT_RAISE:
        snprintf(out, size, "%s 加注 %d", seat_name, (int)amount);
        break;
    case ZJH_ACT_COMPARE:
        snprintf(out, size, "%s 比牌 %s", seat_name,
                 k_seat_names[target < ZJH_SEATS ? target : 0]);
        break;
    case ZJH_ACT_FOLD:
    default:
        snprintf(out, size, "%s 弃牌", seat_name);
        break;
    }
}

static void show_result_sheet(void)
{
    char title[24];
    char subtitle[48];
    char payout[32];

    const uint8_t winner = s_game.winner;
    const bool player_won = winner == 0;
    const bool player_folded = !s_game.seats[0].in_hand;
    const int32_t net = s_game.seats[0].chips - s_hand_start_chips;

    snprintf(title, sizeof(title), "%s", player_won ? "你赢了"
                                                    : player_folded ? "本局结束"
                                                                    : "你输了");
    if (winner < ZJH_SEATS) {
        snprintf(subtitle, sizeof(subtitle), "%s 收下底池 %d",
                 k_seat_names[winner], (int)s_game.payout);
    } else {
        snprintf(subtitle, sizeof(subtitle), "本局结束");
    }
    snprintf(payout, sizeof(payout), "本局 %+d", (int)net);

    if (ui_lock()) {
        for (int i = 0; i < ZJH_SEATS; i++) {
            zjh_ui_result_row(i, k_seat_names[i], &s_game.seats[i], i == winner);
        }
        zjh_ui_show_result(title, subtitle, payout, player_won);
        bsp_lvgl_unlock();
    }
    s_result_visible = true;

    if (s_audio_ok) {
        if (player_won) zjh_audio_play_win();
        else zjh_audio_play_lose();
    }
}

static void finish_hand(void)
{
    const bool player_won = s_game.winner == 0;
    s_profile.chips = s_game.seats[0].chips;
    s_profile.hands++;
    if (player_won) {
        s_profile.wins++;
        s_profile.streak++;
        if (s_profile.streak > s_profile.best_streak) {
            s_profile.best_streak = s_profile.streak;
        }
        const uint32_t net_win = (uint32_t)(s_game.payout - s_game.seats[0].committed);
        if (net_win > s_profile.best_win) {
            s_profile.best_win = net_win;
        }
    } else {
        s_profile.streak = 0;
    }
    save_profile();
    refresh_table();
    show_result_sheet();
    ESP_LOGI(TAG, "hand over: winner=%u reason=%u chips=%d",
             (unsigned)s_game.winner, (unsigned)s_game.end_reason,
             (int)s_profile.chips);
}

// ---------------------------------------------------------------------------
// Table input
// ---------------------------------------------------------------------------

static void cycle_action(int direction)
{
    bool enabled[ZJH_ACTIONS];
    for (int i = 0; i < ZJH_ACTIONS; i++) {
        enabled[i] = zjh_action_available(&s_game, 0, (zjh_action_t)i);
    }
    for (int step = 1; step <= ZJH_ACTIONS; step++) {
        const int idx = (s_action_index + direction * step + ZJH_ACTIONS * 2) %
                        ZJH_ACTIONS;
        if (enabled[idx]) {
            s_action_index = idx;
            break;
        }
    }
}

static void cycle_target(int direction)
{
    for (int step = 1; step <= ZJH_SEATS; step++) {
        const int idx = (s_target + direction * step + ZJH_SEATS * 2) % ZJH_SEATS;
        if (idx != 0 && s_game.seats[idx].in_hand) {
            s_target = (uint8_t)idx;
            break;
        }
    }
}

static void begin_targeting(void)
{
    int active = 0;
    for (int i = 1; i < ZJH_SEATS; i++) {
        if (s_game.seats[i].in_hand) active++;
    }
    if (active == 0) return;
    if (active == 1) {
        for (int i = 1; i < ZJH_SEATS; i++) {
            if (s_game.seats[i].in_hand) {
                s_target = (uint8_t)i;
                break;
            }
        }
        return;  // caller compares immediately
    }
    s_targeting = true;
    for (int i = 1; i < ZJH_SEATS; i++) {
        if (s_game.seats[i].in_hand) {
            s_target = (uint8_t)i;
            break;
        }
    }
    refresh_table();
}

static void show_action_toast(void)
{
    char text[40];
    describe_action(s_game.last_actor, s_game.last_action, s_game.last_target,
                    s_game.last_amount, text, sizeof(text));
    if (!ui_lock()) return;
    zjh_ui_toast(text);
    bsp_lvgl_unlock();
}

static void play_action_sound(zjh_action_t action)
{
    if (!s_audio_ok) return;
    if (action == ZJH_ACT_PEEK) zjh_audio_play_peek();
    else if (action == ZJH_ACT_FOLD) zjh_audio_play_click();
    else zjh_audio_play_bet();
}

static void player_apply(zjh_action_t action, uint8_t target)
{
    if (!zjh_apply(&s_game, action, target)) {
        refresh_table();
        return;
    }

    if (action == ZJH_ACT_PEEK) {
        // Peeking leaves the player on turn. The refresh skips unaffordable actions.
        s_action_index = ZJH_ACT_CALL;
    }
    play_action_sound(action);
    show_action_toast();
    s_ai_due_ms = now_ms() + AI_THINK_MS;
    if (s_game.over) finish_hand();
    else refresh_table();
}

static void table_key(bsp_btn_t btn, bsp_btn_ev_t event)
{
    if (s_result_visible) {
        if (event == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
            start_hand();
        } else if (event == BSP_BTN_LONG && btn == BSP_BTN_UP) {
            return_to_menu();
        }
        return;
    }

    if (s_game.over) return;
    if (s_game.turn != 0) return;  // opponents act from the tick

    if (s_targeting) {
        if (event == BSP_BTN_CLICK && btn == BSP_BTN_UP) {
            cycle_target(-1);
            refresh_table();
        } else if (event == BSP_BTN_CLICK && btn == BSP_BTN_DOWN) {
            cycle_target(1);
            refresh_table();
        } else if (event == BSP_BTN_CLICK && btn == BSP_BTN_OK &&
                   zjh_action_available(&s_game, 0, ZJH_ACT_COMPARE)) {
            const uint8_t target = s_target;
            s_targeting = false;
            player_apply(ZJH_ACT_COMPARE, target);
        } else if (event == BSP_BTN_LONG && btn == BSP_BTN_UP) {
            s_targeting = false;
            refresh_table();
        }
        return;
    }

    if (event == BSP_BTN_CLICK && btn == BSP_BTN_UP) {
        cycle_action(-1);
        refresh_table();
    } else if (event == BSP_BTN_CLICK && btn == BSP_BTN_DOWN) {
        cycle_action(1);
        refresh_table();
    } else if (event == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
        const zjh_action_t action = (zjh_action_t)s_action_index;
        if (!zjh_action_available(&s_game, 0, action)) return;
        if (action == ZJH_ACT_COMPARE) {
            begin_targeting();
            if (!s_targeting) player_apply(action, s_target);
        } else {
            player_apply(action, ZJH_SEAT_NONE);
        }
    }
}

// ---------------------------------------------------------------------------
// AI tick
// ---------------------------------------------------------------------------

static void ai_step(uint32_t now)
{
    if (s_screen != SCREEN_TABLE || s_game.over) return;
    if (s_game.turn == 0 || s_game.turn >= ZJH_SEATS) return;
    if ((int32_t)(now - s_ai_due_ms) < 0) return;

    uint8_t target = ZJH_SEAT_NONE;
    const zjh_action_t action = zjh_ai_choose(&s_game, &target);
    if (!zjh_apply(&s_game, action, target) &&
        !zjh_apply(&s_game, ZJH_ACT_FOLD, ZJH_SEAT_NONE)) {
        return;
    }

    play_action_sound(s_game.last_action);
    show_action_toast();
    s_ai_due_ms = now + (s_game.last_action == ZJH_ACT_PEEK ? AI_PEEK_MS : AI_THINK_MS);
    if (s_game.over) finish_hand();
    else refresh_table();
}

// ---------------------------------------------------------------------------
// Menu / rules / stats input
// ---------------------------------------------------------------------------

static void menu_activate(void)
{
    switch (s_menu_index) {
    case MENU_START:
        start_hand();
        break;
    case MENU_RULES:
        s_rules_page = 0;
        s_screen = SCREEN_RULES;
        if (ui_lock()) {
            zjh_ui_rules(s_rules_page);
            zjh_ui_show_rules();
            bsp_lvgl_unlock();
        }
        break;
    case MENU_STATS:
        s_screen = SCREEN_STATS;
        if (ui_lock()) {
            zjh_ui_stats(&s_profile);
            zjh_ui_show_stats();
            bsp_lvgl_unlock();
        }
        break;
    case MENU_SOUND:
        s_profile.sound_on = !s_profile.sound_on;
        if (s_audio_ok) zjh_audio_set_enabled(s_profile.sound_on);
        save_profile();
        refresh_menu();
        if (s_audio_ok && s_profile.sound_on) zjh_audio_play_click();
        break;
    case MENU_STANDBY:
        if (ui_lock()) {
            s_screen = SCREEN_STANDBY;
            s_clock_field = CLOCK_VIEW;
            zjh_ui_standby(s_clock_field, 0);
            zjh_ui_show_standby();
            bsp_lvgl_unlock();
        }
        break;
    default:
        break;
    }
}

static void menu_key(bsp_btn_t btn, bsp_btn_ev_t event)
{
    if (event != BSP_BTN_CLICK) return;
    if (btn == BSP_BTN_UP) {
        s_menu_index = (s_menu_index + ZJH_MENU_ROWS - 1) % ZJH_MENU_ROWS;
        if (s_audio_ok) zjh_audio_play_click();
        refresh_menu();
    } else if (btn == BSP_BTN_DOWN) {
        s_menu_index = (s_menu_index + 1) % ZJH_MENU_ROWS;
        if (s_audio_ok) zjh_audio_play_click();
        refresh_menu();
    } else if (btn == BSP_BTN_OK) {
        if (s_audio_ok) zjh_audio_play_click();
        menu_activate();
    }
}

static void rules_key(bsp_btn_t btn, bsp_btn_ev_t event)
{
    if (event == BSP_BTN_LONG && btn == BSP_BTN_UP) {
        return_to_menu();
        return;
    }
    if (event == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
        if (s_rules_page + 1 < RULES_PAGES) {
            if (s_audio_ok) zjh_audio_play_click();
            if (ui_lock()) {
                zjh_ui_rules(++s_rules_page);
                bsp_lvgl_unlock();
            }
        } else {
            return_to_menu();
        }
    }
}

static void stats_key(bsp_btn_t btn, bsp_btn_ev_t event)
{
    if ((event == BSP_BTN_CLICK && btn == BSP_BTN_OK) ||
        (event == BSP_BTN_LONG && btn == BSP_BTN_UP)) {
        return_to_menu();
    }
}

static void standby_key(bsp_btn_t btn, bsp_btn_ev_t event)
{
    if (btn == BSP_BTN_UP && event == BSP_BTN_LONG) {
        if (s_clock_field == CLOCK_VIEW) {
            return_to_menu();
            return;
        }
        s_clock_field = CLOCK_VIEW;
    } else if (event != BSP_BTN_CLICK) {
        return;
    } else if (btn == BSP_BTN_OK) {
        switch (s_clock_field) {
        case CLOCK_VIEW:
            s_clock_draft = zjh_clock_get_minutes();
            if (s_clock_draft < 0) s_clock_draft = 0;
            s_clock_field = CLOCK_HOURS;
            break;
        case CLOCK_HOURS:
            s_clock_field = CLOCK_MINUTES;
            break;
        case CLOCK_MINUTES:
            if (!zjh_clock_set_minutes(s_clock_draft)) {
                ESP_LOGW(TAG, "failed to set clock");
                return;
            }
            s_clock_field = CLOCK_VIEW;
            refresh_status();
            break;
        }
    } else if (s_clock_field != CLOCK_VIEW) {
        const int delta = btn == BSP_BTN_UP ? 1 : -1;
        const int hours = s_clock_draft / 60;
        const int minutes = s_clock_draft % 60;
        s_clock_draft = s_clock_field == CLOCK_HOURS
                        ? ((hours + delta + 24) % 24) * 60 + minutes
                        : hours * 60 + (minutes + delta + 60) % 60;
    } else {
        return;
    }
    if (ui_lock()) {
        zjh_ui_standby(s_clock_field, s_clock_draft);
        bsp_lvgl_unlock();
    }
}

void zjh_app_init(void)
{
    zjh_store_load(&s_profile);
    s_audio_ok = zjh_audio_init(s_profile.volume, s_profile.sound_on) == ESP_OK;
    if (!s_audio_ok) ESP_LOGW(TAG, "audio unavailable; game runs silently");

    s_screen = SCREEN_MENU;
    s_menu_index = MENU_START;
    s_rules_page = 0;
    s_targeting = false;

    if (ui_lock()) {
        zjh_ui_init();
        zjh_ui_menu(&s_profile, s_menu_index);
        zjh_ui_show_menu();
        bsp_lvgl_unlock();
    }
    refresh_status();
    ESP_LOGI(TAG, "ready: chips=%d hands=%u audio=%d",
             (int)s_profile.chips, (unsigned)s_profile.hands, (int)s_audio_ok);
}

void zjh_app_handle_key(bsp_btn_t btn, bsp_btn_ev_t event)
{
    if (event == BSP_BTN_RELEASE || event == BSP_BTN_PRESS) return;
    switch (s_screen) {
    case SCREEN_MENU:
        menu_key(btn, event);
        break;
    case SCREEN_TABLE:
        table_key(btn, event);
        break;
    case SCREEN_RULES:
        rules_key(btn, event);
        break;
    case SCREEN_STATS:
        stats_key(btn, event);
        break;
    case SCREEN_STANDBY:
        standby_key(btn, event);
        break;
    default:
        break;
    }
}

void zjh_app_tick(uint32_t now, int battery_percent)
{
    if ((int32_t)(now - s_status_due) >= 0 || battery_percent != s_battery) {
        s_status_due = now + STATUS_POLL_MS;
        s_battery = battery_percent;
        refresh_status();
    }
    if (s_screen == SCREEN_TABLE) {
        ai_step(now);
    }
}
