// main/zjh_hand.c
#include "zjh_hand.h"
#include <stdbool.h>

static uint8_t rank_of(zjh_card_t card)
{
    return card.rank;  // 0..12 = 2..A
}

static void sort_desc(uint8_t r[3])
{
    for (int i = 0; i < 2; i++) {
        for (int j = i + 1; j < 3; j++) {
            if (r[j] > r[i]) {
                const uint8_t t = r[i];
                r[i] = r[j];
                r[j] = t;
            }
        }
    }
}

zjh_hand_t zjh_eval(zjh_card_t cards[ZJH_HAND_SIZE])
{
    uint8_t r[3] = { rank_of(cards[0]), rank_of(cards[1]), rank_of(cards[2]) };
    sort_desc(r);
    const bool flush = cards[0].suit == cards[1].suit && cards[1].suit == cards[2].suit;

    zjh_hand_t hand = { .cat = ZJH_CAT_HIGH, .tie = { r[0], r[1], r[2] } };

    if (r[0] == r[1] && r[1] == r[2]) {
        hand.cat = ZJH_CAT_THREE;
        hand.tie[0] = r[0];
        hand.tie[1] = 0;
        hand.tie[2] = 0;
        return hand;
    }

    // A-2-3 (ranks 0,1,12 sorted descending to 12,1,0) is the lowest
    // straight, high card 3 (rank index 1).
    const bool wheel = r[0] == 12 && r[1] == 1 && r[2] == 0;
    const bool run = (r[0] == r[1] + 1u && r[1] == r[2] + 1u) || wheel;
    const uint8_t straight_high = wheel ? 1u : r[0];

    if (run && flush) {
        hand.cat = ZJH_CAT_STRAIGHT_FLUSH;
        hand.tie[0] = straight_high;
        hand.tie[1] = 0;
        hand.tie[2] = 0;
        return hand;
    }
    if (flush) {
        hand.cat = ZJH_CAT_FLUSH;
        return hand;  // tie already holds the descending ranks
    }
    if (run) {
        hand.cat = ZJH_CAT_STRAIGHT;
        hand.tie[0] = straight_high;
        hand.tie[1] = 0;
        hand.tie[2] = 0;
        return hand;
    }
    if (r[0] == r[1] || r[1] == r[2] || r[0] == r[2]) {
        // The pair is the rank that appears twice; the remaining card kicks.
        uint8_t pair = r[0];
        uint8_t kicker = r[2];
        if (r[0] == r[1]) {
            pair = r[0];
            kicker = r[2];
        } else if (r[1] == r[2]) {
            pair = r[1];
            kicker = r[0];
        } else {
            pair = r[0];
            kicker = r[1];
        }
        hand.cat = ZJH_CAT_PAIR;
        hand.tie[0] = pair;
        hand.tie[1] = kicker;
        hand.tie[2] = 0;
    }
    return hand;
}

int zjh_compare(const zjh_hand_t *a, const zjh_hand_t *b)
{
    if (a->cat != b->cat) return a->cat > b->cat ? 1 : -1;
    for (int i = 0; i < 3; i++) {
        if (a->tie[i] != b->tie[i]) return a->tie[i] > b->tie[i] ? 1 : -1;
    }
    return 0;
}

const char *zjh_category_name(uint8_t cat)
{
    static const char *const names[ZJH_CAT_COUNT] = {
        "单张", "对子", "顺子", "金花", "顺金", "豹子",
    };
    return cat < ZJH_CAT_COUNT ? names[cat] : "?";
}

uint8_t zjh_strength(const zjh_hand_t *hand)
{
    const uint8_t high = hand->tie[0];
    switch (hand->cat) {
    case ZJH_CAT_THREE:
        return (uint8_t)(94u + high);
    case ZJH_CAT_STRAIGHT_FLUSH:
        return (uint8_t)(78u + high);
    case ZJH_CAT_FLUSH:
        return (uint8_t)(64u + high);
    case ZJH_CAT_STRAIGHT:
        return (uint8_t)(50u + high);
    case ZJH_CAT_PAIR:
        return (uint8_t)(30u + high);
    case ZJH_CAT_HIGH:
    default:
        return (uint8_t)(8u + high);
    }
}
