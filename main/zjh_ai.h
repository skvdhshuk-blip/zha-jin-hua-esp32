// main/zjh_ai.h
// Seeded opponent policy for 扎金花. Pure C: the app feeds the game state and
// plays the returned action through zjh_apply().
#pragma once

#include "zjh_round.h"

// Choose an action for the seat whose turn it is. Never returns an action that
// zjh_action_available() rejects. *target is set for ZJH_ACT_COMPARE.
zjh_action_t zjh_ai_choose(zjh_game_t *g, uint8_t *target);
