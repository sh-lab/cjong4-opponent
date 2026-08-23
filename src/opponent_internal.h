#ifndef CJ4M_OPPONENT_INTERNAL_H
#define CJ4M_OPPONENT_INTERNAL_H

#include <stdint.h>

#include "cjong4/core/tile.h"
#include "cjong4/core/tile_const.h"
#include "cjong4/core/state_query.h"
#include "cjong4/manager/delegate.h"

static inline cj4_hand
cj4_opponent_hand(const cj4_player_view *view)
{
    return cj4_location_collect_hand(view->locations, view->player);
}

static inline cj4_discard_list
cj4_opponent_discards(const cj4_player_view *view)
{
    return cj4_location_collect_discards(view->locations);
}

static inline cj4_meld_list
cj4_opponent_melds(const cj4_player_view *view, cj4_player player)
{
    return cj4_location_collect_melds(view->locations, player);
}

static inline void *
cj4_opponent_make_ctx(int ctx_level)
{
    switch (ctx_level)
    {
    case 0:
        return (void *)(uintptr_t)0;
    case 2:
        return (void *)(uintptr_t)2;
    default:
        return (void *)(uintptr_t)1;
    }
}

static inline uint8_t
cj4_opponent_ctx_level(void *ctx)
{
    const uintptr_t raw = (uintptr_t)ctx;
    return raw <= 2 ? (uint8_t)raw : 1;
}

static inline const cj4_action *
cj4_opponent_find_action(
    const cj4_action *actions,
    uint8_t action_count,
    cj4_action_type type)
{
    for (uint8_t i = 0; i < action_count; ++i)
    {
        if (actions[i].type == type)
            return &actions[i];
    }

    return 0;
}

static inline cj4_action
cj4_opponent_choose_win_or_pass(
    const cj4_action *actions,
    uint8_t action_count)
{
    const cj4_action *ron = cj4_opponent_find_action(actions, action_count, CJ4_ACTION_RON);
    const cj4_action *tsumo = cj4_opponent_find_action(actions, action_count, CJ4_ACTION_TSUMO);
    const cj4_action *pass = cj4_opponent_find_action(actions, action_count, CJ4_ACTION_PASS);

    if (ron)
        return *ron;
    if (tsumo)
        return *tsumo;
    if (pass)
        return *pass;
    return actions[0];
}

static inline uint8_t
cj4_opponent_count_hand_tiles(
    const cj4_player_view *view,
    cj4_tile_type type)
{
    const cj4_hand hand = cj4_opponent_hand(view);
    uint8_t count = 0;

    for (uint8_t i = 0; i < hand.count; ++i)
    {
        if (cj4_tile_get_type(hand.items[i]) == type)
            ++count;
    }

    return count;
}

static inline uint8_t
cj4_opponent_count_visible_tiles(
    const cj4_player_view *view,
    cj4_tile_type type)
{
    const cj4_dora_indicator_list dora_indicators =
        cj4_location_collect_dora_indicators(view->locations);
    const cj4_discard_list discards = cj4_opponent_discards(view);
    uint8_t count = 0;

    for (uint8_t i = 0; i < dora_indicators.count; ++i)
    {
        if (cj4_tile_get_type(dora_indicators.items[i]) == type)
            ++count;
    }

    for (uint8_t i = 0; i < discards.count; ++i)
    {
        if (cj4_tile_get_type(discards.items[i].tile) == type)
            ++count;
    }

    for (cj4_player player = CJ4_PLAYER_0; player < CJ4_PLAYER_COUNT; ++player)
    {
        const cj4_meld_list melds = cj4_opponent_melds(view, player);

        for (uint8_t i = 0; i < melds.count; ++i)
        {
            const cj4_meld *meld = &melds.items[i];

            for (uint8_t j = 0; j < meld->size; ++j)
            {
                if (cj4_tile_get_type(meld->tiles[j]) == type)
                    ++count;
            }
        }
    }

    return count;
}

static inline uint8_t
cj4_opponent_count_riichi_threats(const cj4_player_view *view)
{
    uint8_t count = 0;

    for (cj4_player player = CJ4_PLAYER_0; player < CJ4_PLAYER_COUNT; ++player)
    {
        if (player != view->player && view->is_riichi[player])
            ++count;
    }

    return count;
}

static inline uint8_t
cj4_opponent_tile_is_safe_against_player(
    const cj4_player_view *view,
    cj4_player riichi_player,
    cj4_tile_type type)
{
    const cj4_discard_list discards = cj4_opponent_discards(view);

    for (uint8_t i = 0; i < discards.count; ++i)
    {
        const cj4_discard *discard = &discards.items[i];
        if (discard->player != riichi_player)
            continue;

        if (cj4_tile_get_type(discard->tile) == type)
            return 1;
    }

    return 0;
}

static inline uint8_t
cj4_opponent_tile_safe_count(
    const cj4_player_view *view,
    cj4_tile_id tile)
{
    uint8_t safe_count = 0;
    const cj4_tile_type type = cj4_tile_get_type(tile);

    for (cj4_player player = CJ4_PLAYER_0; player < CJ4_PLAYER_COUNT; ++player)
    {
        if (player == view->player || !view->is_riichi[player])
            continue;

        if (cj4_opponent_tile_is_safe_against_player(view, player, type))
            ++safe_count;
    }

    return safe_count;
}

static inline uint8_t
cj4_opponent_tile_is_safe(
    const cj4_player_view *view,
    cj4_tile_id tile)
{
    const uint8_t threat_count = cj4_opponent_count_riichi_threats(view);
    return threat_count > 0 &&
           cj4_opponent_tile_safe_count(view, tile) == threat_count;
}

static inline cj4_tile_type
cj4_opponent_seat_wind_type(const cj4_player_view *view)
{
    return (cj4_tile_type)(CJ4_TILE_TYPE_EAST +
                           ((view->player + CJ4_PLAYER_COUNT - view->dealer) %
                            CJ4_PLAYER_COUNT));
}

static inline uint8_t
cj4_opponent_type_is_yakuhai(
    const cj4_player_view *view,
    cj4_tile_type type)
{
    return type == CJ4_TILE_TYPE_HAKU ||
           type == CJ4_TILE_TYPE_HATSU ||
           type == CJ4_TILE_TYPE_CHUN ||
           type == (cj4_tile_type)(CJ4_TILE_TYPE_EAST + view->round_wind) ||
           type == cj4_opponent_seat_wind_type(view);
}

static inline uint8_t
cj4_opponent_tile_is_yakuhai(
    const cj4_player_view *view,
    cj4_tile_id tile)
{
    return cj4_opponent_type_is_yakuhai(view, cj4_tile_get_type(tile));
}

static inline uint8_t
cj4_opponent_type_is_honor(cj4_tile_type type)
{
    return type >= CJ4_TILE_TYPE_EAST;
}

#endif /* CJ4M_OPPONENT_INTERNAL_H */
