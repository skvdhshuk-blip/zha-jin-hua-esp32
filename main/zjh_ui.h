// main/zjh_ui.h
// LVGL screens for 扎金花. Every function must be called with the LVGL lock
// held (bsp_lvgl_lock).
#pragma once

#include "zjh_round.h"
#include "zjh_store.h"
#include <stdbool.h>
#include <stdint.h>

#define ZJH_MENU_ROWS 5
#define ZJH_ACTIONS 5

void zjh_ui_init(void);

void zjh_ui_menu(const zjh_profile_t *profile, int selected);
void zjh_ui_show_menu(void);
void zjh_ui_status(int minutes, int battery_percent);
void zjh_ui_show_standby(void);
// Field is -1 while viewing, 0 while editing hours, and 1 for minutes.
void zjh_ui_standby(int edit_field, int draft_minutes);

void zjh_ui_show_table(void);
void zjh_ui_table(const zjh_game_t *game, int selected_action,
                  const bool enabled[ZJH_ACTIONS], bool targeting, uint8_t target);
void zjh_ui_deal(void);
void zjh_ui_info(const char *text);
void zjh_ui_result_row(int index, const char *name, const zjh_seat_t *seat, bool winner);
void zjh_ui_toast(const char *text);

void zjh_ui_show_result(const char *title, const char *subtitle,
                        const char *payout, bool win);
void zjh_ui_hide_result(void);

void zjh_ui_rules(int page);
void zjh_ui_show_rules(void);

void zjh_ui_stats(const zjh_profile_t *profile);
void zjh_ui_show_stats(void);
