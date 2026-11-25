// main/zjh_cards.c
#include "zjh_cards.h"

uint32_t zjh_rng_next(uint32_t *state)
{
    // xorshift32: deterministic, cheap, and good enough for shuffling here.
    uint32_t x = *state;
    if (x == 0) x = 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

zjh_card_t zjh_card_from_id(uint8_t id)
{
    const zjh_card_t card = { .rank = (uint8_t)(id / 4u), .suit = (uint8_t)(id % 4u) };
    return card;
}

uint8_t zjh_card_id(zjh_card_t card)
{
    return (uint8_t)(card.rank * 4u + card.suit);
}

void zjh_deck_shuffle(uint8_t out[ZJH_DECK_SIZE], uint32_t *rng)
{
    for (uint8_t i = 0; i < ZJH_DECK_SIZE; i++) out[i] = i;
    for (int i = ZJH_DECK_SIZE - 1; i > 0; i--) {
        const uint32_t r = zjh_rng_next(rng) % (uint32_t)(i + 1);
        const uint8_t tmp = out[i];
        out[i] = out[r];
        out[r] = tmp;
    }
}

const char *zjh_rank_label(uint8_t rank)
{
    static const char *const labels[13] = {
        "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A",
    };
    return rank < 13 ? labels[rank] : "?";
}
