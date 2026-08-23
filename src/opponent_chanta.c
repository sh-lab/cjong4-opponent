#include "cjong4/opponent/opponent_chanta.h"

#include "opponent_internal.h"

#include "cjong4/core/tile.h"
#include "cjong4/core/tile_const.h"

static uint8_t
cj4_opponent_chanta_type_is_suited(cj4_tile_type type)
{
    return !cj4_opponent_type_is_honor(type);
}

static uint8_t
cj4_opponent_chanta_count_middle_tiles(const cj4_player_view *view)
{
    const cj4_hand hand = cj4_opponent_hand(view);
    uint8_t count = 0;

    for (uint8_t i = 0; i < hand.count; ++i)
    {
        const cj4_tile_id tile = hand.items[i];

        if (!cj4_tile_is_yaochu(tile))
        {
            const uint8_t number = cj4_tile_get_number(tile);
            if (number >= 4 && number <= 6)
                ++count;
        }
    }

    return count;
}

static uint8_t
cj4_opponent_chanta_count_terminal_or_honor_tiles(const cj4_player_view *view)
{
    const cj4_hand hand = cj4_opponent_hand(view);
    uint8_t count = 0;

    for (uint8_t i = 0; i < hand.count; ++i)
    {
        if (cj4_tile_is_yaochu(hand.items[i]))
            ++count;
    }

    return count;
}

static uint8_t
cj4_opponent_chanta_edge_support(
    const cj4_player_view *view,
    cj4_tile_type type)
{
    const uint8_t number = cj4_opponent_type_is_honor(type)
                               ? 0
                               : cj4_tile_get_number(cj4_tile_make(type, 0));
    uint8_t support = 0;

    if (!cj4_opponent_chanta_type_is_suited(type))
        return 0;

    switch (number)
    {
    case 1:
        support += cj4_opponent_count_hand_tiles(view, (cj4_tile_type)(type + 1));
        support += cj4_opponent_count_hand_tiles(view, (cj4_tile_type)(type + 2));
        break;
    case 2:
        support += cj4_opponent_count_hand_tiles(view, (cj4_tile_type)(type - 1));
        support += cj4_opponent_count_hand_tiles(view, (cj4_tile_type)(type + 1));
        break;
    case 3:
        support += cj4_opponent_count_hand_tiles(view, (cj4_tile_type)(type - 2));
        support += cj4_opponent_count_hand_tiles(view, (cj4_tile_type)(type - 1));
        break;
    case 7:
        support += cj4_opponent_count_hand_tiles(view, (cj4_tile_type)(type + 1));
        support += cj4_opponent_count_hand_tiles(view, (cj4_tile_type)(type + 2));
        break;
    case 8:
        support += cj4_opponent_count_hand_tiles(view, (cj4_tile_type)(type - 1));
        support += cj4_opponent_count_hand_tiles(view, (cj4_tile_type)(type + 1));
        break;
    case 9:
        support += cj4_opponent_count_hand_tiles(view, (cj4_tile_type)(type - 2));
        support += cj4_opponent_count_hand_tiles(view, (cj4_tile_type)(type - 1));
        break;
    default:
        break;
    }

    return support;
}

static uint8_t
cj4_opponent_chanta_should_continue(
    const cj4_player_view *view,
    uint8_t level)
{
    static const uint8_t early_discard_limit[3] = {6, 9, 12};
    static const uint8_t late_middle_limit[3] = {2, 3, 4};
    static const uint8_t early_middle_limit[3] = {4, 5, 6};
    const uint8_t middle_tiles = cj4_opponent_chanta_count_middle_tiles(view);
    const uint8_t terminal_or_honor_tiles =
        cj4_opponent_chanta_count_terminal_or_honor_tiles(view);
    const cj4_hand hand = cj4_opponent_hand(view);
    const cj4_discard_list discards = cj4_opponent_discards(view);

    if (hand.count == 0)
        return 1;

    if (discards.count <= 1)
        return 1;

    if (terminal_or_honor_tiles >= 8)
        return 1;

    if (discards.count <= early_discard_limit[level] &&
        middle_tiles <= early_middle_limit[level])
    {
        return 1;
    }

    return middle_tiles <= late_middle_limit[level];
}

static uint8_t
cj4_opponent_chanta_tile_matches_route(cj4_tile_id tile)
{
    if (cj4_tile_is_yaochu(tile))
        return 1;

    switch (cj4_tile_get_number(tile))
    {
    case 2:
    case 3:
    case 7:
    case 8:
        return 1;
    default:
        return 0;
    }
}

static uint8_t
cj4_opponent_chanta_count_off_route_tiles(const cj4_player_view *view)
{
    const cj4_hand hand = cj4_opponent_hand(view);
    uint8_t count = 0;

    for (uint8_t i = 0; i < hand.count; ++i)
    {
        if (!cj4_opponent_chanta_tile_matches_route(hand.items[i]))
            ++count;
    }

    return count;
}

static uint8_t
cj4_opponent_chanta_count_route_tiles(const cj4_player_view *view)
{
    const cj4_hand hand = cj4_opponent_hand(view);
    uint8_t count = 0;

    for (uint8_t i = 0; i < hand.count; ++i)
    {
        if (cj4_opponent_chanta_tile_matches_route(hand.items[i]))
            ++count;
    }

    return count;
}

static uint8_t
cj4_opponent_chanta_can_open(
    const cj4_player_view *view,
    const cj4_action *action,
    uint8_t level)
{
    static const uint8_t max_off_route_for_open[3] = {3, 4, 5};
    const uint8_t off_route_tiles = cj4_opponent_chanta_count_off_route_tiles(view);
    const uint8_t route_tiles = cj4_opponent_chanta_count_route_tiles(view);
    const cj4_meld_list melds = cj4_opponent_melds(view, view->player);

    if (!cj4_opponent_chanta_tile_matches_route(action->tile))
        return 0;

    if (melds.count > 0 &&
        off_route_tiles <= (uint8_t)(max_off_route_for_open[level] + 1))
    {
        return 1;
    }

    if (route_tiles >= 8 && off_route_tiles <= max_off_route_for_open[level])
        return 1;

    if (cj4_opponent_tile_is_yakuhai(view, action->tile) &&
        off_route_tiles <= (uint8_t)(4 + level))
    {
        return 1;
    }

    return off_route_tiles <= level;
}

static int
cj4_opponent_chanta_discard_score(
    const cj4_player_view *view,
    cj4_tile_id tile)
{
    const uint8_t threat_count = cj4_opponent_count_riichi_threats(view);
    const cj4_tile_type type = cj4_tile_get_type(tile);
    const uint8_t hand_count = cj4_opponent_count_hand_tiles(view, type);
    const uint8_t visible_count = cj4_opponent_count_visible_tiles(view, type);
    const uint8_t safe_bonus =
        threat_count > 0 && cj4_opponent_tile_is_safe(view, tile) ? 80 : 0;
    int score = 0;

    if (cj4_opponent_type_is_honor(type))
    {
        score += cj4_opponent_type_is_yakuhai(view, type) ? -40 : 35;
        score += visible_count * 15;
        score -= hand_count * 30;
        return score + safe_bonus;
    }

    {
        const uint8_t number = cj4_tile_get_number(tile);
        const uint8_t support = cj4_opponent_chanta_edge_support(view, type);

        score += visible_count * 10;
        score -= hand_count * 20;

        switch (number)
        {
        case 1:
        case 9:
            score += 10;
            score -= support * 25;
            break;
        case 2:
        case 8:
            score += 85;
            score -= support * 30;
            break;
        case 3:
        case 7:
            score += 135;
            score -= support * 20;
            break;
        case 4:
        case 6:
            score += 240;
            break;
        default:
            score += 280;
            break;
        }
    }

    return score + safe_bonus;
}

static int
cj4_opponent_chanta_fallback_discard_score(
    const cj4_player_view *view,
    cj4_tile_id tile)
{
    const cj4_tile_type type = cj4_tile_get_type(tile);
    const uint8_t hand_count = cj4_opponent_count_hand_tiles(view, type);
    const uint8_t visible_count = cj4_opponent_count_visible_tiles(view, type);
    int score = visible_count * 10;

    if (cj4_opponent_type_is_yakuhai(view, type))
        score -= 60;
    else if (cj4_opponent_type_is_honor(type))
        score += 40;
    else if (cj4_tile_is_yaochu(tile))
        score += 60;
    else
        score += 120;

    if (hand_count <= 1)
        score += 20;
    else if (hand_count == 2)
        score -= 40;
    else
        score -= 120;

    return score;
}

static cj4_action
cj4_opponent_chanta_decide(
    void *ctx,
    const cj4_player_view *view,
    const cj4_action *actions,
    uint8_t action_count)
{
    const uint8_t level = cj4_opponent_ctx_level(ctx);
    const uint8_t continue_chanta =
        cj4_opponent_chanta_should_continue(view, level);
    const cj4_action *riichi;
    const cj4_action *kakan;
    const cj4_action *ankan;
    const cj4_action *best_discard = 0;
    int best_score = -10000;

    for (uint8_t i = 0; i < action_count; ++i)
    {
        if (actions[i].type == CJ4_ACTION_RON || actions[i].type == CJ4_ACTION_TSUMO)
            return actions[i];
    }

    if (view->phase == CJ4_PHASE_DISCARD || view->phase == CJ4_PHASE_KAKAN_RESOLVE)
    {
        if (continue_chanta)
        {
            for (uint8_t i = 0; i < action_count; ++i)
            {
                if ((actions[i].type == CJ4_ACTION_MINKAN ||
                     actions[i].type == CJ4_ACTION_PON ||
                     actions[i].type == CJ4_ACTION_CHI) &&
                    cj4_opponent_chanta_can_open(view, &actions[i], level))
                {
                    return actions[i];
                }
            }
        }
        else
        {
            const cj4_action *minkan = cj4_opponent_find_action(
                actions,
                action_count,
                CJ4_ACTION_MINKAN);
            if (minkan && cj4_opponent_tile_is_yakuhai(view, minkan->tile))
                return *minkan;

            const cj4_action *pon = cj4_opponent_find_action(
                actions,
                action_count,
                CJ4_ACTION_PON);
            if (pon && cj4_opponent_tile_is_yakuhai(view, pon->tile))
                return *pon;
        }

        return cj4_opponent_choose_win_or_pass(actions, action_count);
    }

    riichi = cj4_opponent_find_action(actions, action_count, CJ4_ACTION_RIICHI);
    if (riichi)
        return *riichi;

    if (continue_chanta)
    {
        kakan = cj4_opponent_find_action(actions, action_count, CJ4_ACTION_KAKAN);
        if (kakan && cj4_opponent_chanta_tile_matches_route(kakan->tile))
            return *kakan;

        ankan = cj4_opponent_find_action(actions, action_count, CJ4_ACTION_ANKAN);
        if (ankan && cj4_opponent_chanta_tile_matches_route(ankan->tile))
            return *ankan;
    }

    for (uint8_t i = 0; i < action_count; ++i)
    {
        int score;

        if (actions[i].type != CJ4_ACTION_DISCARD)
            continue;

        score = continue_chanta
                    ? cj4_opponent_chanta_discard_score(view, actions[i].tile)
                    : cj4_opponent_chanta_fallback_discard_score(view, actions[i].tile);
        if (!best_discard || score > best_score)
        {
            best_discard = &actions[i];
            best_score = score;
        }
    }

    if (best_discard)
        return *best_discard;

    return actions[0];
}

cj4m_player_delegate
cj4_opponent_chanta(int ctx_level)
{
    return (cj4m_player_delegate){
        .ctx = cj4_opponent_make_ctx(ctx_level),
        .decide = cj4_opponent_chanta_decide};
}
