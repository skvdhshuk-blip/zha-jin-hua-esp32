// main/zjh_round.c
#include "zjh_round.h"

static bool seat_valid(uint8_t seat)
{
    return seat < ZJH_SEATS;
}

int zjh_active_count(const zjh_game_t *g)
{
    int count = 0;
    for (int i = 0; i < ZJH_SEATS; i++) {
        if (g->seats[i].in_hand) count++;
    }
    return count;
}

uint8_t zjh_active_mask(const zjh_game_t *g)
{
    uint8_t mask = 0;
    for (int i = 0; i < ZJH_SEATS; i++) {
        if (g->seats[i].in_hand) mask |= (uint8_t)(1u << i);
    }
    return mask;
}

uint8_t zjh_next_active(const zjh_game_t *g, uint8_t seat)
{
    for (uint8_t step = 1; step <= ZJH_SEATS; step++) {
        const uint8_t candidate = (uint8_t)((seat + step) % ZJH_SEATS);
        if (g->seats[candidate].in_hand) return candidate;
    }
    return ZJH_SEAT_NONE;
}

uint16_t zjh_call_cost(const zjh_game_t *g, uint8_t seat)
{
    const uint16_t base = g->stake;
    return g->seats[seat].peeked ? (uint16_t)(base * 2u) : base;
}

uint16_t zjh_compare_cost(const zjh_game_t *g, uint8_t seat)
{
    return (uint16_t)(zjh_call_cost(g, seat) * 2u);
}

bool zjh_action_available(const zjh_game_t *g, uint8_t seat, zjh_action_t action)
{
    if (!g || g->over || !seat_valid(seat)) return false;
    if (g->turn != seat || !g->seats[seat].in_hand) return false;

    const zjh_seat_t *me = &g->seats[seat];
    switch (action) {
    case ZJH_ACT_PEEK:
        return !me->peeked;
    case ZJH_ACT_CALL:
        return me->chips >= (int32_t)zjh_call_cost(g, seat);
    case ZJH_ACT_RAISE: {
        if (g->stake >= ZJH_STAKE_MAX) return false;
        const uint16_t next_stake = (uint16_t)(g->stake + ZJH_STAKE_STEP);
        const uint16_t cost = me->peeked ? (uint16_t)(next_stake * 2u) : next_stake;
        return me->chips >= (int32_t)cost;
    }
    case ZJH_ACT_COMPARE:
        if (zjh_active_count(g) < 2) return false;
        return me->chips >= (int32_t)zjh_compare_cost(g, seat) &&
               zjh_next_active(g, seat) != ZJH_SEAT_NONE;
    case ZJH_ACT_FOLD:
        return true;
    default:
        return false;
    }
}

static bool pay(zjh_game_t *g, uint8_t seat, int32_t amount)
{
    zjh_seat_t *s = &g->seats[seat];
    if (amount < 0 || s->chips < amount) return false;
    s->chips -= amount;
    s->committed += amount;
    s->round_bet += amount;
    g->pot += amount;
    return true;
}

static void finish(zjh_game_t *g, uint8_t winner, uint8_t reason)
{
    g->winner = winner;
    g->end_reason = reason;
    g->payout = g->pot;
    if (seat_valid(winner)) g->seats[winner].chips += g->pot;
    g->pot = 0;
    g->turn = ZJH_SEAT_NONE;
    g->over = true;
}

static void showdown(zjh_game_t *g)
{
    uint8_t best = ZJH_SEAT_NONE;
    for (int i = 0; i < ZJH_SEATS; i++) {
        if (!g->seats[i].in_hand) continue;
        if (best == ZJH_SEAT_NONE || zjh_compare(&g->seats[i].hand, &g->seats[best].hand) > 0) {
            best = (uint8_t)i;
        }
    }
    finish(g, best, ZJH_END_SHOWDOWN);
}

static void advance(zjh_game_t *g, uint8_t from)
{
    if (zjh_active_count(g) <= 1) {
        uint8_t winner = ZJH_SEAT_NONE;
        for (int i = 0; i < ZJH_SEATS; i++) {
            if (g->seats[i].in_hand) winner = (uint8_t)i;
        }
        finish(g, winner, ZJH_END_FOLD);
        return;
    }

    const uint8_t next = zjh_next_active(g, from);
    if (next == ZJH_SEAT_NONE) {
        showdown(g);
        return;
    }
    g->turn = next;

    if ((zjh_active_mask(g) & (uint8_t)~g->acted_mask) == 0) {
        g->round++;
        g->acted_mask = 0;
        if (g->round > ZJH_ROUNDS_MAX) showdown(g);
    }
}

static void note(zjh_game_t *g, uint8_t actor, zjh_action_t action, uint8_t target,
                 int32_t amount)
{
    g->last_actor = actor;
    g->last_action = (uint8_t)action;
    g->last_target = target;
    g->last_amount = amount;
}

void zjh_game_start(zjh_game_t *g, const int32_t stacks[ZJH_SEATS], uint32_t seed)
{
    *g = (zjh_game_t){0};
    g->rng = seed ? seed : 0x1234567u;
    g->stake = ZJH_STAKE_START;
    g->round = 1;
    g->winner = ZJH_SEAT_NONE;
    g->turn = ZJH_SEAT_NONE;
    g->last_actor = ZJH_SEAT_NONE;
    g->last_target = ZJH_SEAT_NONE;

    uint8_t deck[ZJH_DECK_SIZE];
    zjh_deck_shuffle(deck, &g->rng);

    for (int i = 0; i < ZJH_SEATS; i++) {
        zjh_seat_t *s = &g->seats[i];
        s->chips = stacks[i];
        s->in_hand = stacks[i] >= ZJH_ANTE;
        for (int c = 0; c < ZJH_HAND_SIZE; c++) {
            s->cards[c] = deck[i * ZJH_HAND_SIZE + c];
        }
        s->hand = zjh_eval((zjh_card_t[3]){
            zjh_card_from_id(s->cards[0]),
            zjh_card_from_id(s->cards[1]),
            zjh_card_from_id(s->cards[2]),
        });
        if (s->in_hand) {
            s->chips -= ZJH_ANTE;
            s->committed = ZJH_ANTE;
            s->round_bet = ZJH_ANTE;
            g->pot += ZJH_ANTE;
        }
    }

    for (int i = 0; i < ZJH_SEATS; i++) {
        if (g->seats[i].in_hand) {
            g->turn = (uint8_t)i;
            break;
        }
    }
    if (g->turn == ZJH_SEAT_NONE) {
        g->over = true;
        g->end_reason = ZJH_END_FOLD;
    }
}

bool zjh_apply(zjh_game_t *g, zjh_action_t action, uint8_t target)
{
    if (!g || g->over || g->turn == ZJH_SEAT_NONE) return false;
    const uint8_t seat = g->turn;
    if (!zjh_action_available(g, seat, action)) return false;

    zjh_seat_t *me = &g->seats[seat];

    switch (action) {
    case ZJH_ACT_PEEK:
        me->peeked = true;
        note(g, seat, action, ZJH_SEAT_NONE, 0);
        return true;  // peeking does not spend the turn

    case ZJH_ACT_CALL: {
        const int32_t cost = zjh_call_cost(g, seat);
        if (!pay(g, seat, cost)) return false;
        note(g, seat, action, ZJH_SEAT_NONE, cost);
        g->acted_mask |= (uint8_t)(1u << seat);
        advance(g, seat);
        return true;
    }

    case ZJH_ACT_RAISE: {
        const uint16_t next_stake = (uint16_t)(g->stake + ZJH_STAKE_STEP);
        const int32_t cost = me->peeked ? (int32_t)next_stake * 2 : (int32_t)next_stake;
        if (!pay(g, seat, cost)) return false;
        g->stake = next_stake;
        me->raises++;
        note(g, seat, action, ZJH_SEAT_NONE, cost);
        // Everyone else answers the new stake; only the raiser has acted.
        g->acted_mask = (uint8_t)(1u << seat);
        advance(g, seat);
        return true;
    }

    case ZJH_ACT_COMPARE: {
        if (!seat_valid(target) || target == seat || !g->seats[target].in_hand) return false;
        const int32_t cost = zjh_compare_cost(g, seat);
        if (!pay(g, seat, cost)) return false;

        const int cmp = zjh_compare(&me->hand, &g->seats[target].hand);
        if (cmp >= 0) {
            g->seats[target].in_hand = false;  // ties go to the challenger
        } else {
            me->in_hand = false;
        }
        note(g, seat, action, target, cost);
        g->acted_mask |= (uint8_t)(1u << seat);
        advance(g, seat);
        return true;
    }

    case ZJH_ACT_FOLD:
        me->in_hand = false;
        note(g, seat, action, ZJH_SEAT_NONE, 0);
        advance(g, seat);
        return true;

    default:
        return false;
    }
}
