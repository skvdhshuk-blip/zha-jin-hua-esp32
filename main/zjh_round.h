// main/zjh_round.h
// One hand of 扎金花: deal, antes, turn order and betting.
// Pure C, no hardware dependency; the app owns timers, UI and audio.
#pragma once

#include "zjh_cards.h"
#include "zjh_hand.h"
#include <stdbool.h>
#include <stdint.h>

#define ZJH_SEATS 4
#define ZJH_ANTE 10
#define ZJH_STAKE_START 10
#define ZJH_STAKE_STEP 10
#define ZJH_STAKE_MAX 100
#define ZJH_ROUNDS_MAX 6
#define ZJH_SEAT_NONE 0xFF

typedef enum {
    ZJH_ACT_PEEK = 0,   // 看牌: free, raises your own call cost to 2x stake
    ZJH_ACT_CALL,       // 跟注
    ZJH_ACT_RAISE,      // 加注
    ZJH_ACT_COMPARE,    // 比牌
    ZJH_ACT_FOLD,       // 弃牌
} zjh_action_t;

typedef enum {
    ZJH_END_NONE = 0,
    ZJH_END_FOLD,       // everyone else folded
    ZJH_END_SHOWDOWN,   // round limit reached, best hand wins
} zjh_end_reason_t;

typedef struct {
    uint8_t cards[ZJH_HAND_SIZE];
    zjh_hand_t hand;
    int32_t chips;
    int32_t committed;   // this hand, includes the ante
    int32_t round_bet;   // this betting round
    uint8_t raises;      // raises made this hand, visible to every seat
    bool peeked;
    bool in_hand;
} zjh_seat_t;

typedef struct {
    zjh_seat_t seats[ZJH_SEATS];
    int32_t pot;
    int32_t payout;      // pot handed to the winner when the hand ended
    int32_t last_amount;
    uint16_t stake;
    uint8_t round;       // 1-based betting round
    uint8_t turn;
    uint8_t acted_mask;  // seats that acted since the last raise / round start
    uint8_t end_reason;
    uint8_t winner;
    uint8_t last_actor;
    uint8_t last_action;
    uint8_t last_target;
    uint32_t rng;
    bool over;
} zjh_game_t;

// Deal a fresh hand. Seats whose stack is below the ante sit out.
void zjh_game_start(zjh_game_t *g, const int32_t stacks[ZJH_SEATS], uint32_t seed);

int zjh_active_count(const zjh_game_t *g);
uint8_t zjh_active_mask(const zjh_game_t *g);

// Cost of a blind/peeked call for this seat right now.
uint16_t zjh_call_cost(const zjh_game_t *g, uint8_t seat);
// 比牌 costs two calls.
uint16_t zjh_compare_cost(const zjh_game_t *g, uint8_t seat);

bool zjh_action_available(const zjh_game_t *g, uint8_t seat, zjh_action_t action);

// Next seat in hand after `seat` (wrapping), or ZJH_SEAT_NONE.
uint8_t zjh_next_active(const zjh_game_t *g, uint8_t seat);

// Apply an action for the seat whose turn it is. `target` is only used by
// compare. Returns false when the action is invalid or unaffordable.
bool zjh_apply(zjh_game_t *g, zjh_action_t action, uint8_t target);
