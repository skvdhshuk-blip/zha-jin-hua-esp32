// main/zjh_cards.h
// Deck, card ids and a small deterministic RNG for 扎金花.
// Pure C: no ESP-IDF or LVGL dependency, so host tests can drive it.
#pragma once

#include <stdint.h>

#define ZJH_DECK_SIZE 52
#define ZJH_HAND_SIZE 3

// Card ids are 0..51: id = rank * 4 + suit.
// rank: 0..12 = 2,3,...,10,J,Q,K,A ; suit: 0 spade, 1 heart, 2 diamond, 3 club.
typedef struct {
    uint8_t rank;
    uint8_t suit;
} zjh_card_t;

uint32_t zjh_rng_next(uint32_t *state);

zjh_card_t zjh_card_from_id(uint8_t id);
uint8_t zjh_card_id(zjh_card_t card);

// Fisher-Yates shuffle of 0..51 using *rng.
void zjh_deck_shuffle(uint8_t out[ZJH_DECK_SIZE], uint32_t *rng);

// "2".."10", "J", "Q", "K", "A". Never NULL for rank 0..12.
const char *zjh_rank_label(uint8_t rank);
