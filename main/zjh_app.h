// main/zjh_app.h
// Application state machine for 扎金花: menu, table, rules, record and audio.
// main.c only brings up the board and forwards the three buttons.
#pragma once

#include "bsp_button.h"
#include <stdint.h>

void zjh_app_init(void);
void zjh_app_handle_key(bsp_btn_t btn, bsp_btn_ev_t event);
void zjh_app_tick(uint32_t now_ms, int battery_percent);
