// main/zjh_hand.h
// Three-card hand evaluation for 扎金花 (Zha Jin Hua).
//
// Category order, high to low: 豹子 > 顺金(同花顺) > 金花 > 顺子 > 对子 > 单张.
// A-2-3 counts as the smallest straight. Suits never break ties; an exact tie
// is a draw and the caller decides (a 比牌 challenger wins ties).
#pragma once

#include "zjh_cards.h"
#include <stdint.h>

typedef enum {
    ZJH_CAT_HIGH = 0,           // 单张
    ZJH_CAT_PAIR,               // 对子
    ZJH_CAT_STRAIGHT,           // 顺子
    ZJH_CAT_FLUSH,              // 金花
    ZJH_CAT_STRAIGHT_FLUSH,     // 顺金
    ZJH_CAT_THREE,              // 豹子
    ZJH_CAT_COUNT,
} zjh_category_t;

typedef struct {
    uint8_t cat;     // zjh_category_t
    uint8_t tie[3];  // category tie-breakers, most significant first
} zjh_hand_t;

zjh_hand_t zjh_eval(zjh_card_t cards[ZJH_HAND_SIZE]);

// >0 when a is stronger, <0 when b is stronger, 0 on an exact tie.
int zjh_compare(const zjh_hand_t *a, const zjh_hand_t *b);

// UTF-8 display name: 单张 / 对子 / 顺子 / 金花 / 顺金 / 豹子.
const char *zjh_category_name(uint8_t cat);

// Coarse 0..100 strength for the AI: category bands plus tie-break detail.
uint8_t zjh_strength(const zjh_hand_t *hand);
