// main/zjh_ai.c
#include "zjh_ai.h"

#define EQUITY_SAMPLES 64

typedef struct {
    int aggression;
    int blind_peek_chance;
    int bluff_chance;
} temperament_t;

// Seat order: default, Lao Chen (tight), A Bao (aggressive), Xiao Tao (bluffs).
static const temperament_t temperaments[ZJH_SEATS] = {
    { 0, 40, 3 }, { -7, 65, 1 }, { 7, 25, 5 }, { 2, 45, 9 },
};

// Estimate the chance of beating one random hand using only our revealed cards.
// Unknown cards are sampled afresh per trial; actual opponents' hands are private.
static int hand_equity(const zjh_seat_t *me, uint32_t *rng)
{
    uint8_t unseen[ZJH_DECK_SIZE];
    unsigned count = 0;
    for (uint8_t card = 0; card < ZJH_DECK_SIZE; card++) {
        if (card != me->cards[0] && card != me->cards[1] && card != me->cards[2]) {
            unseen[count++] = card;
        }
    }

    unsigned points = 0;
    for (int trial = 0; trial < EQUITY_SAMPLES; trial++) {
        zjh_card_t cards[ZJH_HAND_SIZE];
        // A partial Fisher-Yates shuffle deals three distinct cards.
        for (unsigned i = 0; i < ZJH_HAND_SIZE; i++) {
            const unsigned j = i + zjh_rng_next(rng) % (count - i);
            const uint8_t card = unseen[j];
            unseen[j] = unseen[i];
            unseen[i] = card;
            cards[i] = zjh_card_from_id(card);
        }
        const zjh_hand_t opponent = zjh_eval(cards);
        const int comparison = zjh_compare(&me->hand, &opponent);
        points += comparison > 0 ? 2u : comparison == 0;
    }
    return (int)(points * 100 / (2 * EQUITY_SAMPLES));
}

zjh_action_t zjh_ai_choose(zjh_game_t *g, uint8_t *target)
{
    if (target) *target = ZJH_SEAT_NONE;
    if (!g || g->over || g->turn >= ZJH_SEATS) return ZJH_ACT_FOLD;

    const uint8_t seat = g->turn;
    const zjh_seat_t *me = &g->seats[seat];
    const temperament_t style = temperaments[seat];
    const int roll = (int)(zjh_rng_next(&g->rng) % 100u);
    const int opponents = zjh_active_count(g) - 1;
    const int cost = zjh_call_cost(g, seat);
    const int pot = g->pot > 0 ? g->pot : 1;
    const bool can_call = zjh_action_available(g, seat, ZJH_ACT_CALL);
    const bool can_raise = zjh_action_available(g, seat, ZJH_ACT_RAISE);
    if (!can_call) return ZJH_ACT_FOLD;

    if (!me->peeked) {
        // A blind player buys information when the price rises or the hand progresses.
        const int peek_chance = style.blind_peek_chance + 20 * ((int)g->round - 1);
        if (g->stake >= 3 * ZJH_STAKE_START || g->round >= 3 || roll < peek_chance) {
            return ZJH_ACT_PEEK;
        }
        if (can_raise && g->round == 1 && g->stake == ZJH_STAKE_START &&
            roll >= 100 - style.bluff_chance) {
            return ZJH_ACT_RAISE;
        }
        return ZJH_ACT_CALL;
    }

    int pressure = 0;
    int weakest_signal = 100;
    uint8_t candidate = ZJH_SEAT_NONE;
    const unsigned first = zjh_rng_next(&g->rng) % ZJH_SEATS;
    for (unsigned offset = 0; offset < ZJH_SEATS; offset++) {
        const uint8_t other = (uint8_t)((first + offset) % ZJH_SEATS);
        if (other == seat || !g->seats[other].in_hand) continue;
        const zjh_seat_t *opponent = &g->seats[other];
        const int raises = opponent->raises > 2 ? 2 : opponent->raises;
        const int signal = (opponent->peeked ? 5 : 0) + raises * 7;
        if (signal > pressure) pressure = signal;
        if (signal < weakest_signal) {
            weakest_signal = signal;
            candidate = other;
        }
    }

    const int equity = hand_equity(me, &g->rng);
    const int pot_price = cost * 100 / (pot + cost);
    const int stack_price = cost * 100 / me->chips;
    const int fold_below = 24 + 8 * opponents + pressure + pot_price + stack_price / 3
                         - style.aggression / 2;
    if (equity < fold_below && equity < 90) return ZJH_ACT_FOLD;

    const int compare_cost = zjh_compare_cost(g, seat);
    if (target && candidate != ZJH_SEAT_NONE && (opponents == 1 || g->round >= 3) &&
        zjh_action_available(g, seat, ZJH_ACT_COMPARE) && compare_cost <= pot / 3 &&
        equity >= 84 + weakest_signal / 2 - style.aggression / 2 &&
        (opponents == 1 || roll < 45 + style.aggression)) {
        *target = candidate;
        return ZJH_ACT_COMPARE;
    }

    const int raise_above = 80 + 3 * opponents + pressure / 3 - style.aggression;
    if (can_raise && equity >= raise_above && roll < 62 + style.aggression) {
        return ZJH_ACT_RAISE;
    }
    return ZJH_ACT_CALL;
}
