#ifndef CJ4_OPPONENT_TEST_STATE_HELPERS_H
#define CJ4_OPPONENT_TEST_STATE_HELPERS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cjong4/core/state.h"
#include "cjong4/manager/player_view.h"

/* Test fixtures intentionally assemble states directly. These helpers use
 * cjong4 v3.3's public packing constants without exposing mutation helpers
 * from the opponent library. */
static inline void
cj4_state_set_phase(cj4_mahjong *state, cj4_phase phase)
{
    state->progress = (uint8_t)(
        (state->progress & (uint8_t)~CJ4_STATE_PHASE_MASK) |
        ((uint8_t)phase & CJ4_STATE_PHASE_MASK));
}

static inline void
cj4_state_set_current_player(cj4_mahjong *state, cj4_player player)
{
    state->progress = (uint8_t)(
        (state->progress & (uint8_t)~CJ4_STATE_CURRENT_PLAYER_MASK) |
        (((uint8_t)player << CJ4_STATE_CURRENT_PLAYER_SHIFT) &
         CJ4_STATE_CURRENT_PLAYER_MASK));
}

static inline void
cj4_state_set_riichi(cj4_mahjong *state, cj4_player player, bool value)
{
    const uint8_t bit = (uint8_t)(CJ4_STATE_RIICHI_FLAG << player);
    state->riichi_ippatsu = value
                                ? (uint8_t)(state->riichi_ippatsu | bit)
                                : (uint8_t)(state->riichi_ippatsu &
                                            (uint8_t)~bit);
}

static inline void
cj4_state_set_double_riichi(
    cj4_mahjong *state,
    cj4_player player,
    bool value)
{
    const uint8_t bit = (uint8_t)(CJ4_STATE_RIICHI_FLAG << player);
    state->double_riichi_pending =
        value ? (uint8_t)(state->double_riichi_pending | bit)
              : (uint8_t)(state->double_riichi_pending & (uint8_t)~bit);
}

static inline void
cj4_state_clear_pending_riichi_bits(cj4_mahjong *state)
{
    state->double_riichi_pending = (uint8_t)(
        (state->double_riichi_pending & CJ4_STATE_PLAYER_FLAGS_MASK) |
        (CJ4_STATE_PENDING_RIICHI_PLAYER_NONE <<
         CJ4_STATE_PENDING_RIICHI_PLAYER_SHIFT));
}

static inline void
cj4_state_set_round_result(
    cj4_mahjong *state,
    cj4_round_end_type type,
    cj4_abortive_draw_reason reason)
{
    state->round_result = (uint8_t)(
        ((uint8_t)type & CJ4_STATE_ROUND_END_TYPE_MASK) |
        (((uint8_t)reason & CJ4_STATE_ABORTIVE_REASON_MASK) <<
         CJ4_STATE_ABORTIVE_REASON_SHIFT));
}

static inline uint8_t
cj4_location_make_hand(cj4_player player)
{
    return (uint8_t)(((uint8_t)player << CJ4_LOCATION_PLAYER_SHIFT) &
                     CJ4_LOCATION_PLAYER_MASK);
}

static inline uint8_t
cj4_location_make_discard(
    cj4_player player,
    uint8_t index,
    bool is_tsumogiri)
{
    return (uint8_t)(
        (is_tsumogiri ? CJ4_LOCATION_DISCARD_TSUMOGIRI_FLAG : 0u) |
        (((uint8_t)player << CJ4_LOCATION_PLAYER_SHIFT) &
         CJ4_LOCATION_PLAYER_MASK) |
        (index & CJ4_LOCATION_DISCARD_INDEX_MASK));
}

static inline uint8_t
cj4_location_make_discard_history(uint8_t index, bool is_riichi)
{
    return (uint8_t)(
        (is_riichi ? CJ4_LOCATION_DISCARD_RIICHI_FLAG : 0u) |
        (index & CJ4_LOCATION_DISCARD_HISTORY_INDEX_MASK));
}

static inline void
test_init_player_view(cj4_player_view *view)
{
    memset(view, 0, sizeof(*view));
    memset(view->locations, CJ4_LOCATION_NONE, sizeof(view->locations));
    view->draw_tile = CJ4_TILE_ID_INVALID;
    view->last_discard = CJ4_TILE_ID_INVALID;
    view->kan_tile = CJ4_TILE_ID_INVALID;
}

static inline void
test_view_add_hand(cj4_player_view *view, cj4_tile_id tile)
{
    view->locations[tile].placement = cj4_location_make_hand(view->player);
}

static inline void
test_view_set_discard_count(cj4_player_view *view, uint8_t count)
{
    for (uint8_t i = 0; i < count; ++i)
    {
        view->locations[i].discard =
            cj4_location_make_discard(CJ4_PLAYER_0, i, false);
        view->locations[i].discard_history =
            cj4_location_make_discard_history(i, false);
    }
}

static inline void
test_view_add_dora_indicator(cj4_player_view *view, cj4_tile_id tile)
{
    view->locations[tile].wall = 130;
}

#endif /* CJ4_OPPONENT_TEST_STATE_HELPERS_H */
