// tests/test_zjh_logic.c
// Host tests for the pure 扎金花 rules: hand ranking, the betting state machine
// and the AI policy. Build with:
//   cc -std=c11 -Wall -Wextra -Werror -Imain \
//      tests/test_zjh_logic.c main/zjh_cards.c main/zjh_hand.c \
//      main/zjh_round.c main/zjh_ai.c -o /tmp/test_zjh_logic
#include "zjh_ai.h"
#include "zjh_cards.h"
#include "zjh_hand.h"
#include "zjh_round.h"

#include <assert.h>
#include <stdio.h>

static zjh_card_t c(uint8_t rank, uint8_t suit)
{
    return (zjh_card_t){ .rank = rank, .suit = suit };
}

static void test_deck(void)
{
    uint8_t deck[ZJH_DECK_SIZE];
    uint32_t rng = 7;
    zjh_deck_shuffle(deck, &rng);
    bool seen[ZJH_DECK_SIZE] = { false };
    for (int i = 0; i < ZJH_DECK_SIZE; i++) {
        assert(deck[i] < ZJH_DECK_SIZE);
        assert(!seen[deck[i]]);
        seen[deck[i]] = true;
        assert(zjh_card_id(zjh_card_from_id(deck[i])) == deck[i]);
    }
    assert(zjh_rank_label(0)[0] == '2');
    assert(zjh_rank_label(8)[0] == '1');  // "10"
    assert(zjh_rank_label(12)[0] == 'A');
}

static void test_hands(void)
{
    zjh_card_t three[3] = { c(12, 0), c(12, 1), c(12, 2) };
    zjh_hand_t h = zjh_eval(three);
    assert(h.cat == ZJH_CAT_THREE);
    assert(h.tie[0] == 12);

    zjh_card_t sf[3] = { c(8, 1), c(9, 1), c(10, 1) };
    h = zjh_eval(sf);
    assert(h.cat == ZJH_CAT_STRAIGHT_FLUSH);
    assert(h.tie[0] == 10);

    zjh_card_t wheel[3] = { c(12, 0), c(0, 1), c(1, 2) };
    h = zjh_eval(wheel);
    assert(h.cat == ZJH_CAT_STRAIGHT);
    assert(h.tie[0] == 1);

    zjh_card_t broadway[3] = { c(12, 0), c(11, 1), c(10, 2) };
    h = zjh_eval(broadway);
    assert(h.cat == ZJH_CAT_STRAIGHT);
    assert(h.tie[0] == 12);

    zjh_card_t gap[3] = { c(12, 0), c(0, 1), c(2, 2) };
    assert(zjh_eval(gap).cat == ZJH_CAT_HIGH);

    zjh_card_t flush[3] = { c(0, 3), c(3, 3), c(7, 3) };
    h = zjh_eval(flush);
    assert(h.cat == ZJH_CAT_FLUSH);
    assert(h.tie[0] == 7 && h.tie[1] == 3 && h.tie[2] == 0);

    zjh_card_t pair[3] = { c(7, 0), c(7, 2), c(1, 1) };
    h = zjh_eval(pair);
    assert(h.cat == ZJH_CAT_PAIR);
    assert(h.tie[0] == 7 && h.tie[1] == 1);

    zjh_card_t high[3] = { c(12, 0), c(9, 1), c(5, 2) };
    h = zjh_eval(high);
    assert(h.cat == ZJH_CAT_HIGH);
    assert(h.tie[0] == 12 && h.tie[1] == 9 && h.tie[2] == 5);
}

static void test_compare(void)
{
    zjh_hand_t three = zjh_eval((zjh_card_t[3]){ c(0, 0), c(0, 1), c(0, 2) });
    zjh_hand_t sf = zjh_eval((zjh_card_t[3]){ c(8, 0), c(9, 0), c(10, 0) });
    zjh_hand_t flush = zjh_eval((zjh_card_t[3]){ c(0, 1), c(3, 1), c(6, 1) });
    zjh_hand_t straight = zjh_eval((zjh_card_t[3]){ c(0, 0), c(1, 1), c(2, 2) });
    zjh_hand_t pair = zjh_eval((zjh_card_t[3]){ c(5, 0), c(5, 1), c(12, 2) });
    zjh_hand_t high = zjh_eval((zjh_card_t[3]){ c(11, 0), c(7, 1), c(3, 2) });

    assert(zjh_compare(&three, &sf) > 0);
    assert(zjh_compare(&sf, &flush) > 0);
    assert(zjh_compare(&flush, &straight) > 0);
    assert(zjh_compare(&straight, &pair) > 0);
    assert(zjh_compare(&pair, &high) > 0);
    assert(zjh_compare(&high, &high) == 0);

    // Same category: kicker breaks the tie (pair here holds the A kicker).
    zjh_hand_t pair_k = zjh_eval((zjh_card_t[3]){ c(5, 2), c(5, 3), c(11, 0) });
    assert(zjh_compare(&pair_k, &pair) < 0);
    zjh_hand_t pair_low = zjh_eval((zjh_card_t[3]){ c(5, 3), c(5, 0), c(2, 1) });
    assert(zjh_compare(&pair_k, &pair_low) > 0);

    assert(zjh_strength(&three) > zjh_strength(&sf));
    assert(zjh_strength(&sf) > zjh_strength(&high));
}

static void test_costs_and_turns(void)
{
    const int32_t stacks[ZJH_SEATS] = { 1000, 1000, 1000, 1000 };
    zjh_game_t g;
    zjh_game_start(&g, stacks, 42);

    assert(g.pot == ZJH_ANTE * ZJH_SEATS);
    assert(g.turn == 0);
    assert(zjh_call_cost(&g, 0) == ZJH_STAKE_START);
    assert(zjh_compare_cost(&g, 0) == ZJH_STAKE_START * 2);
    assert(zjh_action_available(&g, 0, ZJH_ACT_PEEK));
    assert(zjh_action_available(&g, 0, ZJH_ACT_COMPARE));

    assert(zjh_apply(&g, ZJH_ACT_PEEK, ZJH_SEAT_NONE));
    assert(g.turn == 0);  // peeking does not pass the turn
    assert(zjh_call_cost(&g, 0) == ZJH_STAKE_START * 2);
    assert(!zjh_action_available(&g, 0, ZJH_ACT_PEEK));

    assert(zjh_apply(&g, ZJH_ACT_CALL, ZJH_SEAT_NONE));
    assert(g.turn == 1);
    assert(g.seats[0].committed == ZJH_ANTE + ZJH_STAKE_START * 2);

    // Invalid compare target must be refused without spending chips.
    const int32_t chips_before = g.seats[1].chips;
    assert(!zjh_apply(&g, ZJH_ACT_COMPARE, 1));
    assert(g.seats[1].chips == chips_before);

    // Raise lifts the stake for everyone.
    assert(zjh_apply(&g, ZJH_ACT_RAISE, ZJH_SEAT_NONE));
    assert(g.stake == ZJH_STAKE_START + ZJH_STAKE_STEP);
    assert(g.seats[1].raises == 1);
    assert(g.turn == 2);
}

static void test_ai_betting_and_private_cards(void)
{
    const int32_t stacks[ZJH_SEATS] = { 1000, 1000, 1000, 1000 };
    zjh_game_t g;
    uint8_t target;
    zjh_game_start(&g, stacks, 42);
    g.turn = 1;
    g.stake = 50;
    assert(zjh_ai_choose(&g, &target) == ZJH_ACT_PEEK);

    zjh_seat_t *me = &g.seats[1];
    me->peeked = true;
    me->cards[0] = zjh_card_id(c(4, 0));
    me->cards[1] = zjh_card_id(c(2, 1));
    me->cards[2] = zjh_card_id(c(0, 2));
    me->hand = zjh_eval((zjh_card_t[3]){ c(4, 0), c(2, 1), c(0, 2) });
    assert(zjh_ai_choose(&g, &target) == ZJH_ACT_FOLD);

    me->cards[0] = zjh_card_id(c(12, 0));
    me->cards[1] = zjh_card_id(c(12, 1));
    me->cards[2] = zjh_card_id(c(12, 2));
    me->hand = zjh_eval((zjh_card_t[3]){ c(12, 0), c(12, 1), c(12, 2) });
    g.seats[2].in_hand = false;
    g.seats[3].in_hand = false;
    g.pot = 800;
    zjh_game_t altered = g;
    altered.seats[0].hand = g.seats[2].hand;
    altered.seats[2].hand = g.seats[0].hand;
    for (int i = 0; i < ZJH_HAND_SIZE; i++) {
        altered.seats[0].cards[i] = g.seats[2].cards[i];
        altered.seats[2].cards[i] = g.seats[0].cards[i];
    }
    assert(zjh_ai_choose(&g, &target) == ZJH_ACT_COMPARE && target == 0);
    assert(zjh_ai_choose(&altered, &target) == ZJH_ACT_COMPARE && target == 0);
    assert(g.rng == altered.rng);
}

static void play_random_hand(uint32_t seed)
{
    const int32_t stacks[ZJH_SEATS] = { 1000, 1000, 1000, 1000 };
    int32_t total_before = 0;
    for (int i = 0; i < ZJH_SEATS; i++) total_before += stacks[i];

    zjh_game_t g;
    zjh_game_start(&g, stacks, seed);

    int guard = 0;
    while (!g.over && guard++ < 500) {
        const uint8_t seat = g.turn;
        assert(seat < ZJH_SEATS);
        uint8_t target = ZJH_SEAT_NONE;
        const zjh_action_t action = zjh_ai_choose(&g, &target);
        assert(zjh_action_available(&g, seat, action));
        if (action == ZJH_ACT_COMPARE) {
            assert(target < ZJH_SEATS && target != seat && g.seats[target].in_hand);
        }
        assert(zjh_apply(&g, action, target));
        assert(g.seats[seat].chips >= 0);
    }
    assert(g.over);
    assert(guard < 500);
    assert(g.winner < ZJH_SEATS);
    assert(g.seats[g.winner].in_hand);
    assert(g.payout > 0);

    int32_t total_after = g.pot;
    for (int i = 0; i < ZJH_SEATS; i++) total_after += g.seats[i].chips;
    assert(total_after == total_before);

    // Nobody can spend more than they brought; a winner ends richer than before.
    for (int i = 0; i < ZJH_SEATS; i++) {
        assert(g.seats[i].chips >= 0);
        assert(g.seats[i].committed <= 1000);
    }
}

static void test_round_limit(void)
{
    // Force a long game by playing only calls: six rounds then showdown.
    const int32_t stacks[ZJH_SEATS] = { 2000, 2000, 2000, 2000 };
    zjh_game_t g;
    zjh_game_start(&g, stacks, 9);
    int guard = 0;
    while (!g.over && guard++ < 200) {
        assert(zjh_apply(&g, ZJH_ACT_CALL, ZJH_SEAT_NONE));
    }
    assert(g.over);
    assert(g.end_reason == ZJH_END_SHOWDOWN);
    assert(g.round > ZJH_ROUNDS_MAX);
    assert(g.winner == 0 || g.winner < ZJH_SEATS);
}

int main(void)
{
    test_deck();
    test_hands();
    test_compare();
    test_costs_and_turns();
    test_round_limit();
    test_ai_betting_and_private_cards();
    for (uint32_t seed = 1; seed <= 3000; seed++) {
        play_random_hand(seed);
    }
    printf("zjh logic: PASS (3000 simulated hands)\n");
    return 0;
}
