// tests/wasm_runner.c
// The same rules checks as tests/test_zjh_logic.c, but freestanding and
// compiled to WebAssembly so it can run on a machine that refuses freshly
// built native binaries. Returns 0 on success or the failing check's line.
#include "zjh_ai.h"
#include "zjh_cards.h"
#include "zjh_hand.h"
#include "zjh_round.h"

#define CHECK(cond) do { if (!(cond)) return __LINE__; } while (0)

// clang may lower struct zeroing to these; wasm32 has no libc here.
typedef __SIZE_TYPE__ wasm_size_t;
void *memset(void *dst, int value, wasm_size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    for (wasm_size_t i = 0; i < n; i++) d[i] = (unsigned char)value;
    return dst;
}
void *memcpy(void *dst, const void *src, wasm_size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (wasm_size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

static zjh_card_t card(uint8_t rank, uint8_t suit)
{
    zjh_card_t c = { rank, suit };
    return c;
}

static int check_deck(void)
{
    uint8_t deck[ZJH_DECK_SIZE];
    uint32_t rng = 7;
    zjh_deck_shuffle(deck, &rng);
    uint8_t seen[ZJH_DECK_SIZE];
    memset(seen, 0, sizeof(seen));
    for (int i = 0; i < ZJH_DECK_SIZE; i++) {
        CHECK(deck[i] < ZJH_DECK_SIZE);
        CHECK(!seen[deck[i]]);
        seen[deck[i]] = 1;
        CHECK(zjh_card_id(zjh_card_from_id(deck[i])) == deck[i]);
    }
    CHECK(zjh_rank_label(0)[0] == '2');
    CHECK(zjh_rank_label(12)[0] == 'A');
    return 0;
}

static int check_hands(void)
{
    zjh_card_t three[3] = { card(12, 0), card(12, 1), card(12, 2) };
    zjh_hand_t h = zjh_eval(three);
    CHECK(h.cat == ZJH_CAT_THREE);
    CHECK(h.tie[0] == 12);

    zjh_card_t sf[3] = { card(8, 1), card(9, 1), card(10, 1) };
    h = zjh_eval(sf);
    CHECK(h.cat == ZJH_CAT_STRAIGHT_FLUSH);
    CHECK(h.tie[0] == 10);

    zjh_card_t wheel[3] = { card(12, 0), card(0, 1), card(1, 2) };
    h = zjh_eval(wheel);
    CHECK(h.cat == ZJH_CAT_STRAIGHT);
    CHECK(h.tie[0] == 1);

    zjh_card_t gap[3] = { card(12, 0), card(0, 1), card(2, 2) };
    CHECK(zjh_eval(gap).cat == ZJH_CAT_HIGH);

    zjh_card_t flush[3] = { card(0, 3), card(3, 3), card(7, 3) };
    h = zjh_eval(flush);
    CHECK(h.cat == ZJH_CAT_FLUSH);
    CHECK(h.tie[0] == 7 && h.tie[1] == 3 && h.tie[2] == 0);

    zjh_card_t pair[3] = { card(7, 0), card(7, 2), card(1, 1) };
    h = zjh_eval(pair);
    CHECK(h.cat == ZJH_CAT_PAIR);
    CHECK(h.tie[0] == 7 && h.tie[1] == 1);
    return 0;
}

static int check_compare(void)
{
    zjh_card_t t[3] = { card(0, 0), card(0, 1), card(0, 2) };
    zjh_card_t s[3] = { card(8, 0), card(9, 0), card(10, 0) };
    zjh_card_t f[3] = { card(0, 1), card(3, 1), card(6, 1) };
    zjh_card_t st[3] = { card(0, 0), card(1, 1), card(2, 2) };
    zjh_card_t pr[3] = { card(5, 0), card(5, 1), card(12, 2) };
    zjh_card_t hi[3] = { card(11, 0), card(7, 1), card(3, 2) };
    zjh_hand_t ht = zjh_eval(t), hs = zjh_eval(s), hf = zjh_eval(f);
    zjh_hand_t hst = zjh_eval(st), hp = zjh_eval(pr), hh = zjh_eval(hi);
    CHECK(zjh_compare(&ht, &hs) > 0);
    CHECK(zjh_compare(&hs, &hf) > 0);
    CHECK(zjh_compare(&hf, &hst) > 0);
    CHECK(zjh_compare(&hst, &hp) > 0);
    CHECK(zjh_compare(&hp, &hh) > 0);
    CHECK(zjh_compare(&hh, &hh) == 0);
    return 0;
}

static int check_flow(void)
{
    const int32_t stacks[ZJH_SEATS] = { 1000, 1000, 1000, 1000 };
    zjh_game_t g;
    zjh_game_start(&g, stacks, 42);
    CHECK(g.pot == ZJH_ANTE * ZJH_SEATS);
    CHECK(g.turn == 0);
    CHECK(zjh_call_cost(&g, 0) == ZJH_STAKE_START);
    CHECK(zjh_compare_cost(&g, 0) == ZJH_STAKE_START * 2);

    CHECK(zjh_apply(&g, ZJH_ACT_PEEK, ZJH_SEAT_NONE));
    CHECK(g.turn == 0);
    CHECK(zjh_call_cost(&g, 0) == ZJH_STAKE_START * 2);
    CHECK(zjh_apply(&g, ZJH_ACT_CALL, ZJH_SEAT_NONE));
    CHECK(g.turn == 1);

    const int32_t chips_before = g.seats[1].chips;
    CHECK(!zjh_apply(&g, ZJH_ACT_COMPARE, 1));
    CHECK(g.seats[1].chips == chips_before);

    CHECK(zjh_apply(&g, ZJH_ACT_RAISE, ZJH_SEAT_NONE));
    CHECK(g.stake == ZJH_STAKE_START + ZJH_STAKE_STEP);
    CHECK(g.turn == 2);
    return 0;
}

static int check_random_hands(void)
{
    const int32_t stacks[ZJH_SEATS] = { 1000, 1000, 1000, 1000 };
    for (uint32_t seed = 1; seed <= 3000; seed++) {
        zjh_game_t g;
        zjh_game_start(&g, stacks, seed);
        int guard = 0;
        while (!g.over && guard++ < 500) {
            const uint8_t seat = g.turn;
            CHECK(seat < ZJH_SEATS);
            uint8_t target = ZJH_SEAT_NONE;
            const zjh_action_t action = zjh_ai_choose(&g, &target);
            CHECK(zjh_action_available(&g, seat, action));
            if (action == ZJH_ACT_COMPARE) {
                CHECK(target < ZJH_SEATS && target != seat && g.seats[target].in_hand);
            }
            CHECK(zjh_apply(&g, action, target));
            CHECK(g.seats[seat].chips >= 0);
        }
        CHECK(g.over);
        CHECK(guard < 500);
        CHECK(g.winner < ZJH_SEATS);
        CHECK(g.seats[g.winner].in_hand);
        CHECK(g.payout > 0);

        int32_t total = g.pot;
        for (int i = 0; i < ZJH_SEATS; i++) total += g.seats[i].chips;
        CHECK(total == 1000 * ZJH_SEATS);
    }
    return 0;
}

static int check_round_limit(void)
{
    const int32_t stacks[ZJH_SEATS] = { 2000, 2000, 2000, 2000 };
    zjh_game_t g;
    zjh_game_start(&g, stacks, 9);
    int guard = 0;
    while (!g.over && guard++ < 200) {
        CHECK(zjh_apply(&g, ZJH_ACT_CALL, ZJH_SEAT_NONE));
    }
    CHECK(g.over);
    CHECK(g.end_reason == ZJH_END_SHOWDOWN);
    CHECK(g.round > ZJH_ROUNDS_MAX);
    return 0;
}

__attribute__((export_name("run"))) int run(void)
{
    int rc = check_deck();
    if (rc) return rc;
    rc = check_hands();
    if (rc) return rc;
    rc = check_compare();
    if (rc) return rc;
    rc = check_flow();
    if (rc) return rc;
    rc = check_round_limit();
    if (rc) return rc;
    return check_random_hands();
}
