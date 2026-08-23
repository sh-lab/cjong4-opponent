#include "cjong4/opponent/opponent_tanyao.h"

#include "opponent_internal.h"

#include "cjong4/core/tile.h"

static uint8_t
cj4_opponent_tanyao_tile_matches_route(cj4_tile_id tile)
{
    return !cj4_tile_is_yaochu(tile);
}

static uint8_t
cj4_opponent_tanyao_count_off_route_tiles(const cj4_player_view *view)
{
    const cj4_hand hand = cj4_opponent_hand(view);
    uint8_t count = 0;

    for (uint8_t i = 0; i < hand.count; ++i)
    {
        if (!cj4_opponent_tanyao_tile_matches_route(hand.items[i]))
            ++count;
    }

    return count;
}

static uint8_t
cj4_opponent_tanyao_action_matches_route(const cj4_action *action)
{
    if (action->tile != CJ4_TILE_ID_INVALID &&
        !cj4_opponent_tanyao_tile_matches_route(action->tile))
    {
        return 0;
    }

    for (uint8_t i = 0; i < action->tile_count; ++i)
    {
        if (!cj4_opponent_tanyao_tile_matches_route(action->tiles[i]))
            return 0;
    }

    return 1;
}

static uint8_t
cj4_opponent_tanyao_can_open(
    const cj4_player_view *view,
    const cj4_action *action,
    uint8_t level)
{
    static const uint8_t max_off_route_for_open[3] = {1, 3, 5};
    const uint8_t off_route_tiles =
        cj4_opponent_tanyao_count_off_route_tiles(view);
    const cj4_meld_list melds = cj4_opponent_melds(view, view->player);

    if (!cj4_opponent_tanyao_action_matches_route(action))
        return 0;

    if (melds.count > 0)
    {
        return off_route_tiles <=
               (uint8_t)(max_off_route_for_open[level] + 1);
    }

    return off_route_tiles <= max_off_route_for_open[level];
}

static uint8_t
cj4_opponent_tanyao_sequence_support(
    const cj4_player_view *view,
    cj4_tile_type type)
{
    const uint8_t number = cj4_tile_type_get_number(type);
    uint8_t support = 0;

    if (number == 0)
        return 0;

    if (number >= 2)
        support += cj4_opponent_count_hand_tiles(
            view,
            (cj4_tile_type)(type - 1));
    if (number >= 3)
        support += cj4_opponent_count_hand_tiles(
            view,
            (cj4_tile_type)(type - 2));
    if (number <= 8)
        support += cj4_opponent_count_hand_tiles(
            view,
            (cj4_tile_type)(type + 1));
    if (number <= 7)
        support += cj4_opponent_count_hand_tiles(
            view,
            (cj4_tile_type)(type + 2));

    return support;
}

static int
cj4_opponent_tanyao_discard_score(
    const cj4_player_view *view,
    cj4_tile_id tile,
    uint8_t level)
{
    static const int safe_bonus[3] = {600, 400, 180};
    const cj4_tile_type type = cj4_tile_get_type(tile);
    const uint8_t hand_count = cj4_opponent_count_hand_tiles(view, type);
    const uint8_t visible_count = cj4_opponent_count_visible_tiles(view, type);
    int score = visible_count * 10;

    if (cj4_opponent_type_is_honor(type))
        score += 540;
    else if (cj4_tile_is_yaochu(tile))
        score += 500;
    else
    {
        const uint8_t number = cj4_tile_get_number(tile);
        const uint8_t distance_from_five = number > 5
                                               ? (uint8_t)(number - 5)
                                               : (uint8_t)(5 - number);

        score += distance_from_five * 25;
        score -= cj4_opponent_tanyao_sequence_support(view, type) * 12;
    }

    if (hand_count == 2)
        score -= 70;
    else if (hand_count >= 3)
        score -= 140;

    if (cj4_opponent_tile_is_safe(view, tile))
        score += safe_bonus[level];

    return score;
}

static cj4_action
cj4_opponent_tanyao_decide(
    void *ctx,
    const cj4_player_view *view,
    const cj4_action *actions,
    uint8_t action_count)
{
    const uint8_t level = cj4_opponent_ctx_level(ctx);
    const uint8_t threat_count = cj4_opponent_count_riichi_threats(view);
    const cj4_action *riichi;
    const cj4_action *best_discard = 0;
    int best_score = -10000;

    for (uint8_t i = 0; i < action_count; ++i)
    {
        if (actions[i].type == CJ4_ACTION_RON ||
            actions[i].type == CJ4_ACTION_TSUMO)
        {
            return actions[i];
        }
    }

    if (view->phase == CJ4_PHASE_KAKAN_RESOLVE)
        return cj4_opponent_choose_win_or_pass(actions, action_count);

    if (view->phase == CJ4_PHASE_DISCARD)
    {
        for (uint8_t i = 0; i < action_count; ++i)
        {
            if ((actions[i].type == CJ4_ACTION_PON ||
                 actions[i].type == CJ4_ACTION_CHI ||
                 (level == 2 && actions[i].type == CJ4_ACTION_MINKAN)) &&
                cj4_opponent_tanyao_can_open(view, &actions[i], level))
            {
                return actions[i];
            }
        }

        return cj4_opponent_choose_win_or_pass(actions, action_count);
    }

    riichi = cj4_opponent_find_action(
        actions,
        action_count,
        CJ4_ACTION_RIICHI);
    if (riichi && (threat_count == 0 || level == 2))
        return *riichi;

    if (level > 0 && threat_count == 0)
    {
        const cj4_action *kakan = cj4_opponent_find_action(
            actions,
            action_count,
            CJ4_ACTION_KAKAN);
        const cj4_action *ankan = cj4_opponent_find_action(
            actions,
            action_count,
            CJ4_ACTION_ANKAN);

        if (kakan && cj4_opponent_tanyao_action_matches_route(kakan))
            return *kakan;
        if (ankan && cj4_opponent_tanyao_action_matches_route(ankan))
            return *ankan;
    }

    for (uint8_t i = 0; i < action_count; ++i)
    {
        int score;

        if (actions[i].type != CJ4_ACTION_DISCARD)
            continue;

        score = cj4_opponent_tanyao_discard_score(
            view,
            actions[i].tile,
            level);
        if (!best_discard || score > best_score)
        {
            best_discard = &actions[i];
            best_score = score;
        }
    }

    if (best_discard)
        return *best_discard;

    if (riichi)
        return *riichi;

    return actions[0];
}

cj4m_player_delegate
cj4_opponent_tanyao(int ctx_level)
{
    return (cj4m_player_delegate){
        .ctx = cj4_opponent_make_ctx(ctx_level),
        .decide = cj4_opponent_tanyao_decide};
}
