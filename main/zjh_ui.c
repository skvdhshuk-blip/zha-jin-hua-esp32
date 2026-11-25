// main/zjh_ui.c
// Cartoon table interface for 扎金花: menu, 4-seat table, rules and record
// screens plus the showdown sheet. Drawn with LVGL objects on top of the
// generated art, no touch targets: every screen is driven by the three keys.
#include "zjh_ui.h"
#include "bsp_display.h"
#include "lvgl.h"
#include "generated/zjh_images.h"

#include <stdio.h>

LV_FONT_DECLARE(zjh_font_16);
LV_FONT_DECLARE(zjh_font_20);
LV_FONT_DECLARE(zjh_font_28);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_48);

#define COLOR_GOLD       0xF3B94A
#define COLOR_GOLD_DIM   0xB98A3C
#define COLOR_CREAM      0xFFF3DA
#define COLOR_INK        0x2A1710
#define COLOR_RED        0xD8402F
#define COLOR_GREEN      0x7FD08A
#define COLOR_DIM        0xC9AE83
#define COLOR_PANEL      0x0B2A20
#define COLOR_ROW        0x123A2D
#define COLOR_ROW_SEL    0x1B5240

#define CARD_W 56
#define CARD_H 80

typedef struct {
    lv_obj_t *root;
    lv_obj_t *back;
    lv_obj_t *rank;
    lv_obj_t *suit;
} card_view_t;

static lv_obj_t *s_menu_scr;
static lv_obj_t *s_table_scr;
static lv_obj_t *s_rules_scr;
static lv_obj_t *s_stats_scr;
static lv_obj_t *s_standby_scr;
static lv_obj_t *s_status_time[4];
static lv_obj_t *s_status_battery[4];
static unsigned s_status_count;
static lv_obj_t *s_standby_hour;
static lv_obj_t *s_standby_minute;
static lv_obj_t *s_standby_hint;
static lv_obj_t *s_standby_battery;
static lv_obj_t *s_standby_fill;
static int s_clock_minutes = -1;
static int s_clock_field = -1;

static lv_obj_t *s_menu_rows[ZJH_MENU_ROWS];
static lv_obj_t *s_menu_values[ZJH_MENU_ROWS];
static lv_obj_t *s_menu_chips;

static lv_obj_t *s_hud_chips;
static lv_obj_t *s_pot_label;
static lv_obj_t *s_round_label;
static lv_obj_t *s_info_label;
static lv_obj_t *s_player_label;

static lv_obj_t *s_opp_panel[ZJH_SEATS - 1];
static lv_obj_t *s_opp_chips[ZJH_SEATS - 1];
static lv_obj_t *s_opp_status[ZJH_SEATS - 1];

static card_view_t s_cards[ZJH_HAND_SIZE];

static lv_obj_t *s_actions[ZJH_ACTIONS];
static lv_obj_t *s_action_labels[ZJH_ACTIONS];

static lv_obj_t *s_toast;
static lv_obj_t *s_result;
static lv_obj_t *s_result_title;
static lv_obj_t *s_result_sub;
static lv_obj_t *s_result_payout;
static lv_obj_t *s_result_names[ZJH_SEATS];
static lv_obj_t *s_result_categories[ZJH_SEATS];
static lv_obj_t *s_result_ranks[ZJH_SEATS][ZJH_HAND_SIZE];
static lv_obj_t *s_result_suits[ZJH_SEATS][ZJH_HAND_SIZE];
static lv_obj_t *s_result_hint;

static lv_obj_t *s_rules_title;
static lv_obj_t *s_rules_body;
static lv_obj_t *s_rules_page;
static lv_obj_t *s_rules_hint;

static lv_obj_t *s_stats_values[5];

static lv_obj_t *make_panel(lv_obj_t *parent, int x, int y, int w, int h,
                            uint32_t color, uint32_t border, int border_w,
                            int radius, lv_opa_t opa)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, opa, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(obj, border_w, 0);
    return obj;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, uint32_t color,
                            int x, int y, int w, int h, lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, w, h);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, align, 0);
    return label;
}

static lv_obj_t *make_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x07130E), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    return scr;
}

static lv_obj_t *make_bg(lv_obj_t *scr, const lv_image_dsc_t *src)
{
    lv_obj_t *img = lv_image_create(scr);
    lv_image_set_src(img, src);
    lv_obj_set_pos(img, 0, 0);
    return img;
}

static void build_status(lv_obj_t *screen, int y)
{
    lv_obj_t *bar = make_panel(screen, 8, y, 224, 22, COLOR_PANEL,
                               COLOR_GOLD_DIM, 0, 7, LV_OPA_90);
    const unsigned index = s_status_count++;
    s_status_time[index] = make_label(bar, "--:--", &zjh_font_16, COLOR_CREAM,
                                      8, 1, 80, 20, LV_TEXT_ALIGN_LEFT);
    s_status_battery[index] = make_label(bar, "电量 --%", &zjh_font_16, COLOR_CREAM,
                                         120, 1, 96, 20, LV_TEXT_ALIGN_RIGHT);
}

static void anim_opa(void *obj, int32_t value)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)value, 0);
}

static void anim_y(void *obj, int32_t value)
{
    lv_obj_set_y((lv_obj_t *)obj, value);
}

static void fade_out_after(lv_obj_t *obj, uint32_t delay, uint32_t duration)
{
    lv_anim_delete(obj, anim_opa);
    lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_exec_cb(&anim, anim_opa);
    lv_anim_set_values(&anim, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&anim, duration);
    lv_anim_set_delay(&anim, delay);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in);
    lv_anim_start(&anim);
}

// ---------------------------------------------------------------------------
// Menu
// ---------------------------------------------------------------------------

static void build_menu(void)
{
    s_menu_scr = make_screen();
    make_bg(s_menu_scr, &zjh_title_bg_img);

    lv_obj_t *logo = lv_image_create(s_menu_scr);
    lv_image_set_src(logo, &zjh_logo_img);
    lv_obj_set_pos(logo, 20, 26);
    build_status(s_menu_scr, 3);

    lv_obj_t *coin = lv_image_create(s_menu_scr);
    lv_image_set_src(coin, &zjh_coin_img);
    lv_image_set_pivot(coin, 0, 0);
    lv_image_set_scale(coin, 192);
    lv_obj_set_pos(coin, 62, 102);
    s_menu_chips = make_label(s_menu_scr, "1000", &zjh_font_20, COLOR_GOLD,
                              91, 102, 100, 24, LV_TEXT_ALIGN_LEFT);

    static const char *const names[ZJH_MENU_ROWS] = {
        "开始牌局", "玩法规则", "战绩", "音效", "待机屏",
    };
    for (int i = 0; i < ZJH_MENU_ROWS; i++) {
        lv_obj_t *row = make_panel(s_menu_scr, 16, 133 + i * 33, 208, 29,
                                   COLOR_ROW, COLOR_GOLD_DIM, 1, 10, LV_OPA_90);
        s_menu_rows[i] = row;
        make_label(row, names[i], &zjh_font_20, COLOR_CREAM,
                   12, 3, 130, 26, LV_TEXT_ALIGN_LEFT);
        s_menu_values[i] = make_label(row, "", &zjh_font_16, COLOR_GOLD,
                                      140, 6, 56, 20, LV_TEXT_ALIGN_RIGHT);
    }
    make_label(s_menu_scr, "上下选择  确定进入", &zjh_font_16, COLOR_DIM,
               16, 297, 208, 20, LV_TEXT_ALIGN_CENTER);
}

void zjh_ui_show_menu(void)
{
    lv_screen_load(s_menu_scr);
}

void zjh_ui_menu(const zjh_profile_t *profile, int selected)
{
    lv_label_set_text_fmt(s_menu_chips, "%d", (int)(profile ? profile->chips : 0));
    for (int i = 0; i < ZJH_MENU_ROWS; i++) {
        const bool sel = i == selected;
        lv_obj_set_style_bg_color(s_menu_rows[i],
                                  lv_color_hex(sel ? COLOR_ROW_SEL : COLOR_ROW), 0);
        lv_obj_set_style_border_color(s_menu_rows[i],
                                      lv_color_hex(sel ? COLOR_GOLD : COLOR_GOLD_DIM), 0);
        lv_obj_set_style_border_width(s_menu_rows[i], sel ? 2 : 1, 0);
        lv_obj_set_style_bg_opa(s_menu_rows[i], sel ? LV_OPA_COVER : LV_OPA_90, 0);
    }
    if (s_menu_values[3]) {
        lv_label_set_text(s_menu_values[3],
                          (profile && profile->sound_on) ? "开" : "关");
    }
}

// ---------------------------------------------------------------------------
// Table
// ---------------------------------------------------------------------------

static void build_card(card_view_t *view, int x, int y)
{
    view->root = make_panel(s_table_scr, x, y, CARD_W, CARD_H, 0xFFFDF6,
                            COLOR_INK, 2, 7, LV_OPA_COVER);
    lv_obj_set_style_shadow_width(view->root, 6, 0);
    lv_obj_set_style_shadow_color(view->root, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(view->root, LV_OPA_40, 0);

    view->back = lv_image_create(view->root);
    lv_image_set_src(view->back, &zjh_card_back_img);
    lv_obj_set_pos(view->back, 0, 0);

    view->rank = make_label(view->root, "", &lv_font_montserrat_20, COLOR_INK,
                            5, 0, 46, 26, LV_TEXT_ALIGN_LEFT);
    view->suit = lv_image_create(view->root);
    lv_image_set_src(view->suit, &zjh_suit_spade_img);
    lv_obj_set_pos(view->suit, 15, 34);
    lv_image_set_scale(view->suit, 139);  // 48 px source -> 26 px on the card

    lv_obj_add_flag(view->back, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->rank, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->suit, LV_OBJ_FLAG_HIDDEN);
}

static void card_set(card_view_t *view, uint8_t card_id, bool face_up, bool dim)
{
    if (face_up) {
        const zjh_card_t card = zjh_card_from_id(card_id);
        const bool red = card.suit == 1 || card.suit == 2;
        lv_label_set_text(view->rank, zjh_rank_label(card.rank));
        lv_obj_set_style_text_color(view->rank,
                                    lv_color_hex(red ? COLOR_RED : COLOR_INK), 0);
        lv_image_set_src(view->suit, zjh_suit_images[card.suit]);
        lv_obj_remove_flag(view->rank, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(view->suit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view->back, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_border_color(view->root, lv_color_hex(COLOR_GOLD), 0);
    } else {
        lv_obj_add_flag(view->rank, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view->suit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(view->back, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_border_color(view->root, lv_color_hex(COLOR_INK), 0);
    }
    if (dim) lv_anim_delete(view->root, anim_opa);
    if (!lv_anim_get(view->root, anim_opa)) {
        lv_obj_set_style_opa(view->root, dim ? LV_OPA_50 : LV_OPA_COVER, 0);
    }
}

static void build_table(void)
{
    s_table_scr = make_screen();
    make_bg(s_table_scr, &zjh_bg_img);

    // HUD: coin + stack on the left, pot on the right.
    lv_obj_t *coin = lv_image_create(s_table_scr);
    lv_image_set_src(coin, &zjh_coin_img);
    lv_obj_set_pos(coin, 8, 8);
    lv_image_set_scale(coin, 205);  // 32 -> 25
    s_hud_chips = make_label(s_table_scr, "1000", &zjh_font_20, COLOR_GOLD,
                             34, 5, 96, 26, LV_TEXT_ALIGN_LEFT);

    make_label(s_table_scr, "底池", &zjh_font_16, COLOR_DIM,
               146, 9, 36, 20, LV_TEXT_ALIGN_RIGHT);
    s_pot_label = make_label(s_table_scr, "0", &zjh_font_20, COLOR_CREAM,
                             184, 5, 50, 26, LV_TEXT_ALIGN_RIGHT);

    // Opponent cards.
    static const char *const names[ZJH_SEATS - 1] = { "老陈", "阿豹", "小桃" };
    const lv_image_dsc_t *avatars[ZJH_SEATS - 1] = {
        &zjh_avatar_chen_img, &zjh_avatar_bao_img, &zjh_avatar_tao_img,
    };
    for (int i = 0; i < ZJH_SEATS - 1; i++) {
        const int x = 6 + i * 78;
        lv_obj_t *panel = make_panel(s_table_scr, x, 34, 72, 76, COLOR_PANEL,
                                     COLOR_GOLD_DIM, 1, 10, LV_OPA_80);
        s_opp_panel[i] = panel;

        lv_obj_t *frame = make_panel(panel, 16, 4, 40, 40, 0x0A1F18,
                                     COLOR_GOLD_DIM, 1, 20, LV_OPA_COVER);
        lv_obj_set_style_clip_corner(frame, true, 0);
        lv_obj_t *avatar = lv_image_create(frame);
        lv_image_set_src(avatar, avatars[i]);
        lv_obj_set_pos(avatar, -12, -12);  // 64 px portrait centre-cropped

        make_label(panel, names[i], &zjh_font_16, COLOR_CREAM,
                   0, 44, 72, 18, LV_TEXT_ALIGN_CENTER);
        s_opp_chips[i] = make_label(panel, "1000", &zjh_font_16, COLOR_GOLD,
                                    0, 60, 72, 18, LV_TEXT_ALIGN_CENTER);
        s_opp_status[i] = make_label(panel, "", &zjh_font_16, COLOR_DIM,
                                     40, 4, 24, 18, LV_TEXT_ALIGN_CENTER);
    }

    // Keep the chip beside the round text and below the action toast.
    lv_obj_t *chip = lv_image_create(s_table_scr);
    lv_image_set_src(chip, &zjh_chip_img);
    lv_image_set_pivot(chip, 0, 0);
    lv_image_set_scale(chip, 128);
    lv_obj_set_pos(chip, 25, 130);
    s_round_label = make_label(s_table_scr, "第 1 轮 · 注 10", &zjh_font_16, COLOR_DIM,
                               48, 130, 144, 20, LV_TEXT_ALIGN_CENTER);
    s_info_label = make_label(s_table_scr, "", &zjh_font_16, COLOR_CREAM,
                              10, 278, 220, 20, LV_TEXT_ALIGN_CENTER);
    build_status(s_table_scr, 298);

    // Player's hand.
    for (int i = 0; i < ZJH_HAND_SIZE; i++) {
        build_card(&s_cards[i], 30 + i * 62, 151);
    }
    s_player_label = make_label(s_table_scr, "你 · 闷牌", &zjh_font_16, COLOR_CREAM,
                                16, 233, 208, 20, LV_TEXT_ALIGN_CENTER);

    // Action bar: five two-character buttons.
    static const char *const action_names[ZJH_ACTIONS] = {
        "看牌", "跟注", "加注", "比牌", "弃牌",
    };
    for (int i = 0; i < ZJH_ACTIONS; i++) {
        lv_obj_t *btn = make_panel(s_table_scr, 4 + i * 47, 252, 44, 26,
                                   COLOR_ROW, COLOR_GOLD_DIM, 1, 8, LV_OPA_COVER);
        s_actions[i] = btn;
        s_action_labels[i] = make_label(btn, action_names[i], &zjh_font_16,
                                        COLOR_CREAM, 0, 2, 44, 20,
                                        LV_TEXT_ALIGN_CENTER);
    }

    s_toast = make_label(s_table_scr, "", &zjh_font_16, COLOR_GOLD,
                         12, 112, 216, 18, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_opa(s_toast, LV_OPA_TRANSP, 0);

    // Showdown / result sheet.
    s_result = make_panel(s_table_scr, 6, 33, 228, 258, 0x06140F,
                          COLOR_GOLD, 2, 16, LV_OPA_COVER);
    lv_obj_add_flag(s_result, LV_OBJ_FLAG_HIDDEN);
    s_result_title = make_label(s_result, "", &zjh_font_28, COLOR_GOLD,
                                6, 8, 212, 34, LV_TEXT_ALIGN_CENTER);
    s_result_sub = make_label(s_result, "", &zjh_font_16, COLOR_CREAM,
                              6, 43, 212, 20, LV_TEXT_ALIGN_CENTER);
    for (int i = 0; i < ZJH_SEATS; i++) {
        lv_obj_t *row = make_panel(s_result, 6, 70 + i * 34, 212, 30,
                                   COLOR_ROW, COLOR_GOLD_DIM, 0, 5, LV_OPA_COVER);
        s_result_names[i] = make_label(row, "", &zjh_font_16, COLOR_CREAM,
                                       3, 5, 35, 20, LV_TEXT_ALIGN_LEFT);
        s_result_categories[i] = make_label(row, "", &zjh_font_16, COLOR_DIM,
                                            168, 5, 40, 20, LV_TEXT_ALIGN_CENTER);
        for (int j = 0; j < ZJH_HAND_SIZE; j++) {
            lv_obj_t *card = make_panel(row, 40 + j * 42, 3, 39, 24,
                                        COLOR_CREAM, COLOR_INK, 0, 4, LV_OPA_COVER);
            s_result_ranks[i][j] = make_label(card, "", &zjh_font_16, COLOR_INK,
                                              1, 2, 18, 20, LV_TEXT_ALIGN_CENTER);
            lv_obj_t *suit = lv_image_create(card);
            lv_image_set_src(suit, &zjh_suit_spade_img);
            lv_image_set_pivot(suit, 0, 0);
            lv_image_set_scale(suit, 96);
            lv_obj_set_pos(suit, 19, 3);
            s_result_suits[i][j] = suit;
        }
    }
    s_result_payout = make_label(s_result, "", &zjh_font_20, COLOR_GOLD,
                                 6, 207, 212, 26, LV_TEXT_ALIGN_CENTER);
    s_result_hint = make_label(s_result, "确定 下一局  长按上 返回", &zjh_font_16,
                               COLOR_DIM, 6, 233, 212, 20, LV_TEXT_ALIGN_CENTER);
}

void zjh_ui_show_table(void)
{
    lv_screen_load(s_table_scr);
}

void zjh_ui_table(const zjh_game_t *g, int selected_action,
                  const bool enabled[ZJH_ACTIONS], bool targeting, uint8_t target)
{
    if (!g) return;

    lv_label_set_text_fmt(s_hud_chips, "%d", (int)g->seats[0].chips);
    lv_label_set_text_fmt(s_pot_label, "%d", (int)g->pot);
    if (!g->seats[0].in_hand) {
        lv_label_set_text(s_player_label, "你 · 已弃牌");
    } else {
        lv_label_set_text(s_player_label, g->seats[0].peeked ? "你 · 已看牌" : "你 · 闷牌");
    }

    // Opponents are seats 1..3 in display order 老陈, 阿豹, 小桃.
    for (int i = 0; i < ZJH_SEATS - 1; i++) {
        const zjh_seat_t *s = &g->seats[i + 1];
        lv_label_set_text_fmt(s_opp_chips[i], "%d", (int)s->chips);

        const char *status = "";
        uint32_t color = COLOR_DIM;
        if (!s->in_hand) {
            status = "弃";
            color = 0x7C6A58;
        } else if (s->peeked) {
            status = "看";
            color = COLOR_GREEN;
        } else {
            status = "闷";
            color = COLOR_GOLD;
        }
        lv_label_set_text(s_opp_status[i], status);
        lv_obj_set_style_text_color(s_opp_status[i], lv_color_hex(color), 0);

        const bool turn = (!g->over && g->turn == i + 1);
        const bool picked = targeting && target == i + 1;
        lv_obj_set_style_border_color(s_opp_panel[i],
                                      lv_color_hex(picked ? COLOR_RED
                                                          : turn ? COLOR_GOLD
                                                                 : COLOR_GOLD_DIM), 0);
        lv_obj_set_style_border_width(s_opp_panel[i], (turn || picked) ? 2 : 1, 0);
        lv_obj_set_style_opa(s_opp_panel[i], s->in_hand ? LV_OPA_COVER : LV_OPA_60, 0);
    }

    if (g->over) {
        lv_label_set_text_fmt(s_round_label, "本局结束");
    } else {
        lv_label_set_text_fmt(s_round_label, "第 %u 轮 · 注 %u",
                              (unsigned)g->round, (unsigned)g->stake);
    }

    const zjh_seat_t *me = &g->seats[0];
    for (int i = 0; i < ZJH_HAND_SIZE; i++) {
        card_set(&s_cards[i], me->cards[i], me->peeked, !me->in_hand);
    }

    // Action bar.
    for (int i = 0; i < ZJH_ACTIONS; i++) {
        const bool on = enabled[i];
        const bool sel = on && i == selected_action;
        lv_obj_set_style_bg_color(s_actions[i],
                                  lv_color_hex(sel ? COLOR_ROW_SEL : COLOR_ROW), 0);
        lv_obj_set_style_border_color(s_actions[i],
                                      lv_color_hex(sel ? COLOR_GOLD : COLOR_GOLD_DIM), 0);
        lv_obj_set_style_border_width(s_actions[i], sel ? 2 : 1, 0);
        lv_obj_set_style_text_color(s_action_labels[i],
                                    lv_color_hex(on ? (sel ? COLOR_GOLD : COLOR_CREAM)
                                                    : 0x7C6A58), 0);
        lv_obj_set_style_opa(s_actions[i], on ? LV_OPA_COVER : LV_OPA_60, 0);
    }
}

void zjh_ui_deal(void)
{
    for (int i = 0; i < ZJH_HAND_SIZE; i++) {
        lv_obj_set_y(s_cards[i].root, 120);
        lv_obj_set_style_opa(s_cards[i].root, LV_OPA_TRANSP, 0);
        lv_anim_t move;
        lv_anim_init(&move);
        lv_anim_set_var(&move, s_cards[i].root);
        lv_anim_set_exec_cb(&move, anim_y);
        lv_anim_set_values(&move, 120, 151);
        lv_anim_set_duration(&move, 260);
        lv_anim_set_delay(&move, (uint32_t)i * 90);
        lv_anim_set_path_cb(&move, lv_anim_path_ease_out);
        lv_anim_start(&move);

        lv_anim_t fade;
        lv_anim_init(&fade);
        lv_anim_set_var(&fade, s_cards[i].root);
        lv_anim_set_exec_cb(&fade, anim_opa);
        lv_anim_set_values(&fade, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_duration(&fade, 180);
        lv_anim_set_delay(&fade, (uint32_t)i * 90);
        lv_anim_start(&fade);
    }
}

void zjh_ui_info(const char *text)
{
    if (s_info_label) lv_label_set_text(s_info_label, text ? text : "");
}

void zjh_ui_toast(const char *text)
{
    if (!text || !s_toast) return;
    lv_label_set_text(s_toast, text);
    lv_anim_delete(s_toast, anim_opa);
    lv_obj_set_style_opa(s_toast, LV_OPA_COVER, 0);
    fade_out_after(s_toast, 1100, 420);
}

void zjh_ui_show_result(const char *title, const char *subtitle,
                        const char *payout, bool win)
{
    lv_label_set_text(s_result_title, title ? title : "");
    lv_obj_set_style_text_color(s_result_title,
                                lv_color_hex(win ? COLOR_GOLD : COLOR_RED), 0);
    lv_label_set_text(s_result_sub, subtitle ? subtitle : "");
    lv_label_set_text(s_result_payout, payout ? payout : "");
    lv_obj_remove_flag(s_result, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_result);

    lv_anim_delete(s_result, anim_opa);
    lv_obj_set_style_opa(s_result, LV_OPA_TRANSP, 0);
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, s_result);
    lv_anim_set_exec_cb(&anim, anim_opa);
    lv_anim_set_values(&anim, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&anim, 220);
    lv_anim_start(&anim);
}

void zjh_ui_hide_result(void)
{
    if (!s_result) return;
    lv_anim_delete(s_result, anim_opa);
    lv_obj_add_flag(s_result, LV_OBJ_FLAG_HIDDEN);
}

void zjh_ui_result_row(int index, const char *name, const zjh_seat_t *seat, bool winner)
{
    if (index < 0 || index >= ZJH_SEATS) return;
    lv_label_set_text(s_result_names[index], name);
    lv_label_set_text(s_result_categories[index], zjh_category_name(seat->hand.cat));
    const uint32_t color = winner ? COLOR_GOLD : seat->in_hand ? COLOR_CREAM : COLOR_DIM;
    lv_obj_set_style_text_color(s_result_names[index], lv_color_hex(color), 0);
    lv_obj_set_style_text_color(s_result_categories[index], lv_color_hex(color), 0);
    for (int i = 0; i < ZJH_HAND_SIZE; i++) {
        const zjh_card_t card = zjh_card_from_id(seat->cards[i]);
        const bool red = card.suit == 1 || card.suit == 2;
        lv_label_set_text(s_result_ranks[index][i], zjh_rank_label(card.rank));
        lv_obj_set_style_text_color(s_result_ranks[index][i],
                                    lv_color_hex(red ? COLOR_RED : COLOR_INK), 0);
        lv_image_set_src(s_result_suits[index][i], zjh_suit_images[card.suit]);
    }
}

// ---------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------

static const char *const k_rules[3] = {
    "牌型从大到小：豹子、顺金、金花、\n"
    "顺子、对子、单张。A23 算最小的\n"
    "顺子；同牌型逐张比大小。",
    "底注 10。闷牌跟注等于当前注额，\n"
    "看牌后跟注翻倍；加注每次 +10，\n"
    "最高到 100。\n"
    "比牌付双倍跟注，输的人弃牌，\n"
    "平局时挑战者赢。",
    "上下键切换动作，确定键执行。\n"
    "六轮下注后自动摊牌，最大牌型\n"
    "赢走底池。\n"
    "长按上键返回菜单。",
};

static void build_rules(void)
{
    s_rules_scr = make_screen();
    make_bg(s_rules_scr, &zjh_title_bg_img);
    build_status(s_rules_scr, 3);
    lv_obj_t *panel = make_panel(s_rules_scr, 10, 30, 220, 272, COLOR_PANEL,
                                 COLOR_GOLD_DIM, 1, 14, LV_OPA_90);
    s_rules_title = make_label(panel, "玩法规则", &zjh_font_28, COLOR_GOLD,
                               12, 8, 196, 34, LV_TEXT_ALIGN_CENTER);
    s_rules_body = make_label(panel, "", &zjh_font_16, COLOR_CREAM,
                              14, 52, 192, 168, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_text_line_space(s_rules_body, 6, 0);
    s_rules_page = make_label(panel, "", &zjh_font_16, COLOR_DIM,
                              12, 216, 196, 18, LV_TEXT_ALIGN_CENTER);
    s_rules_hint = make_label(panel, "确定 翻页  长按上 返回", &zjh_font_16,
                              COLOR_DIM, 12, 240, 196, 18, LV_TEXT_ALIGN_CENTER);
    (void)s_rules_hint;
}

void zjh_ui_show_rules(void)
{
    lv_screen_load(s_rules_scr);
}

void zjh_ui_rules(int page)
{
    if (page < 0) page = 0;
    if (page > 2) page = 2;
    lv_label_set_text(s_rules_body, k_rules[page]);
    lv_label_set_text_fmt(s_rules_page, "%d / 3", page + 1);
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------

static void build_stats(void)
{
    s_stats_scr = make_screen();
    make_bg(s_stats_scr, &zjh_title_bg_img);
    build_status(s_stats_scr, 3);
    lv_obj_t *panel = make_panel(s_stats_scr, 12, 46, 216, 228, COLOR_PANEL,
                                 COLOR_GOLD_DIM, 1, 14, LV_OPA_90);
    make_label(panel, "战绩", &zjh_font_28, COLOR_GOLD,
               12, 8, 192, 34, LV_TEXT_ALIGN_CENTER);

    static const char *const names[5] = {
        "对局", "胜场", "最高单局", "当前连胜", "金币",
    };
    for (int i = 0; i < 5; i++) {
        make_label(panel, names[i], &zjh_font_16, COLOR_CREAM,
                   16, 58 + i * 30, 90, 20, LV_TEXT_ALIGN_LEFT);
        s_stats_values[i] = make_label(panel, "0", &zjh_font_20, COLOR_GOLD,
                                       110, 55 + i * 30, 90, 24,
                                       LV_TEXT_ALIGN_RIGHT);
    }
    make_label(panel, "长按上键 返回", &zjh_font_16, COLOR_DIM,
               12, 198, 192, 18, LV_TEXT_ALIGN_CENTER);
}

void zjh_ui_show_stats(void)
{
    lv_screen_load(s_stats_scr);
}

void zjh_ui_stats(const zjh_profile_t *p)
{
    if (!p) return;
    lv_label_set_text_fmt(s_stats_values[0], "%u", (unsigned)p->hands);
    lv_label_set_text_fmt(s_stats_values[1], "%u", (unsigned)p->wins);
    lv_label_set_text_fmt(s_stats_values[2], "%u", (unsigned)p->best_win);
    lv_label_set_text_fmt(s_stats_values[3], "%u", (unsigned)p->streak);
    lv_label_set_text_fmt(s_stats_values[4], "%d", (int)p->chips);
}

// ---------------------------------------------------------------------------
// Standby clock
// ---------------------------------------------------------------------------

#define STANDBY_INK       0x080F20
#define STANDBY_PANEL     0x111D35
#define STANDBY_TEXT      0xEEF5FF
#define STANDBY_MUTED     0x8EA3BC
#define STANDBY_ACCENT    0x87DDD6

static void build_standby(void)
{
    s_standby_scr = make_screen();
    lv_obj_set_style_bg_color(s_standby_scr, lv_color_hex(STANDBY_INK), 0);
    lv_obj_set_style_bg_grad_color(s_standby_scr, lv_color_hex(0x172745), 0);
    lv_obj_set_style_bg_grad_dir(s_standby_scr, LV_GRAD_DIR_VER, 0);

    make_label(s_standby_scr, "当前时间", &zjh_font_16, STANDBY_MUTED,
               20, 60, 200, 20, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *clock = make_panel(s_standby_scr, 18, 87, 204, 76, STANDBY_PANEL,
                                 0x2B4160, 1, 16, LV_OPA_COVER);
    s_standby_hour = make_label(clock, "--", &lv_font_montserrat_48, STANDBY_TEXT,
                                 12, 8, 76, 58, LV_TEXT_ALIGN_CENTER);
    make_label(clock, ":", &lv_font_montserrat_48, STANDBY_MUTED,
               89, 7, 22, 58, LV_TEXT_ALIGN_CENTER);
    s_standby_minute = make_label(clock, "--", &lv_font_montserrat_48, STANDBY_TEXT,
                                   112, 8, 76, 58, LV_TEXT_ALIGN_CENTER);

    s_standby_battery = make_label(s_standby_scr, "电量 --%", &zjh_font_20, STANDBY_TEXT,
                                    20, 193, 200, 26, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *track = make_panel(s_standby_scr, 71, 227, 98, 6, 0x314561,
                                 STANDBY_PANEL, 0, 3, LV_OPA_COVER);
    s_standby_fill = make_panel(track, 0, 0, 1, 6, STANDBY_ACCENT,
                                  STANDBY_ACCENT, 0, 3, LV_OPA_COVER);
    lv_obj_add_flag(s_standby_fill, LV_OBJ_FLAG_HIDDEN);
    s_standby_hint = make_label(s_standby_scr, "确定 校时 · 长按上 返回", &zjh_font_16,
                                  STANDBY_MUTED, 8, 282, 224, 22, LV_TEXT_ALIGN_CENTER);
}

void zjh_ui_show_standby(void)
{
    lv_screen_load(s_standby_scr);
}

static void standby_digits(int minutes)
{
    if (minutes < 0) {
        lv_label_set_text(s_standby_hour, "--");
        lv_label_set_text(s_standby_minute, "--");
    } else {
        lv_label_set_text_fmt(s_standby_hour, "%02d", minutes / 60);
        lv_label_set_text_fmt(s_standby_minute, "%02d", minutes % 60);
    }
}

void zjh_ui_standby(int edit_field, int draft_minutes)
{
    s_clock_field = edit_field;
    standby_digits(edit_field < 0 ? s_clock_minutes : draft_minutes);
    lv_obj_set_style_text_color(s_standby_hour,
                                lv_color_hex(edit_field == 0 ? STANDBY_ACCENT : STANDBY_TEXT), 0);
    lv_obj_set_style_text_color(s_standby_minute,
                                lv_color_hex(edit_field == 1 ? STANDBY_ACCENT : STANDBY_TEXT), 0);
    lv_label_set_text(s_standby_hint, edit_field < 0 ? "确定 校时 · 长按上 返回"
                                     : edit_field == 0 ? "上下调小时 · 确定 下一项"
                                                       : "上下调分钟 · 确定 保存");
}

void zjh_ui_status(int minutes, int battery_percent)
{
    char time_text[6] = "--:--";
    char battery_text[24] = "电量 --%";
    if (minutes >= 0 && minutes < 24 * 60) {
        const unsigned hour = (unsigned)minutes / 60;
        const unsigned minute = (unsigned)minutes % 60;
        time_text[0] = (char)('0' + hour / 10);
        time_text[1] = (char)('0' + hour % 10);
        time_text[3] = (char)('0' + minute / 10);
        time_text[4] = (char)('0' + minute % 10);
    }
    if (battery_percent >= 0 && battery_percent <= 100) {
        snprintf(battery_text, sizeof(battery_text), "电量 %d%%", battery_percent);
    }
    for (unsigned i = 0; i < s_status_count; i++) {
        lv_label_set_text(s_status_time[i], time_text);
        lv_label_set_text(s_status_battery[i], battery_text);
    }
    s_clock_minutes = minutes;
    if (s_clock_field < 0) standby_digits(minutes);
    lv_label_set_text(s_standby_battery, battery_text);
    if (battery_percent > 0 && battery_percent <= 100) {
        lv_obj_set_width(s_standby_fill, (battery_percent * 98 + 99) / 100);
        lv_obj_set_style_bg_color(s_standby_fill,
                                  lv_color_hex(battery_percent <= 15 ? COLOR_RED : STANDBY_ACCENT), 0);
        lv_obj_remove_flag(s_standby_fill, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_standby_fill, LV_OBJ_FLAG_HIDDEN);
    }
}

void zjh_ui_init(void)
{
    build_menu();
    build_table();
    build_rules();
    build_stats();
    build_standby();
    lv_screen_load(s_menu_scr);
}
