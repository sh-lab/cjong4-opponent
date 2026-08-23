#include "cjong4/opponent/opponent_kokushi.h"

#include "opponent_internal.h"

#include "cjong4/core/tile.h"

static int
cj4_opponent_count_unique_yaochu_types(const cj4_player_view *view)
{
    uint8_t seen[CJ4_TILE_TYPE_COUNT] = {0};
    const cj4_hand hand = cj4_opponent_hand(view);
    int count = 0;

    for (uint8_t i = 0; i < hand.count; ++i)
    {
        const cj4_tile_type type = cj4_tile_get_type(hand.items[i]);

        if (!cj4_tile_type_is_yaochu(type) || seen[type])
            continue;

        seen[type] = 1;
        ++count;
    }

    return count;
}

static int
cj4_opponent_count_yaochu_duplicates(const cj4_player_view *view)
{
    int duplicates = 0;

    for (cj4_tile_type type = CJ4_TILE_TYPE_MIN; type <= CJ4_TILE_TYPE_MAX; ++type)
    {
        const uint8_t hand_count = cj4_opponent_count_hand_tiles(view, type);

        if (!cj4_tile_type_is_yaochu(type) || hand_count <= 1)
            continue;

        duplicates += hand_count - 1;
    }

    return duplicates;
}

static uint8_t
cj4_opponent_kokushi_should_continue(
    const cj4_player_view *view,
    uint8_t level)
{
    static const uint8_t max_discards_before_give_up[3] = {8, 16, 24};
    static const uint8_t min_unique_orphans[3] = {10, 9, 8};
    const cj4_hand hand = cj4_opponent_hand(view);
    const cj4_discard_list discards = cj4_opponent_discards(view);
    const int unique_yaochu = cj4_opponent_count_unique_yaochu_types(view);
    const int duplicate_yaochu = cj4_opponent_count_yaochu_duplicates(view);

    if (hand.count == 0)
        return 1;

    if (discards.count == 0)
        return 1;

    if (unique_yaochu >= 12)
        return 1;

    if (discards.count <= max_discards_before_give_up[level] &&
        unique_yaochu + duplicate_yaochu >= 10)
        return 1;

    return unique_yaochu >= min_unique_orphans[level];
}

static int
cj4_opponent_kokushi_discard_score(
    const cj4_player_view *view,
    cj4_tile_id tile)
{
    const cj4_tile_type type = cj4_tile_get_type(tile);

    if (!cj4_tile_is_yaochu(tile))
    {
        const uint8_t number = cj4_tile_get_number(tile);
        const uint8_t distance = (uint8_t)((number <= 5) ? (number - 1) : (9 - number));

        return 1000 + distance * 10;
    }

    return (cj4_opponent_count_hand_tiles(view, type) - 1) * 100 +
           cj4_opponent_count_visible_tiles(view, type) * 10;
}

static int
cj4_opponent_kokushi_fallback_discard_score(
    const cj4_player_view *view,
    cj4_tile_id tile)
{
    const cj4_tile_type type = cj4_tile_get_type(tile);
    const uint8_t hand_count = cj4_opponent_count_hand_tiles(view, type);
    int score = cj4_opponent_count_visible_tiles(view, type) * 10;

    if (!cj4_opponent_type_is_yakuhai(view, type))
        score += cj4_tile_type_is_yaochu(type) ? 80 : 120;

    if (hand_count <= 1)
        score += 20;
    else if (hand_count == 2)
        score -= 40;
    else
        score -= 120;

    return score;
}

static cj4_action
cj4_opponent_kokushi_decide(
    void *ctx,
    const cj4_player_view *view,
    const cj4_action *actions,
    uint8_t action_count)
{
    const cj4_action *riichi;
    const cj4_action *best_discard = 0;
    const uint8_t level = cj4_opponent_ctx_level(ctx);
    const uint8_t continue_kokushi = cj4_opponent_kokushi_should_continue(
        view,
        level);
    int best_score = -1;

    for (uint8_t i = 0; i < action_count; ++i)
    {
        if (actions[i].type == CJ4_ACTION_RON || actions[i].type == CJ4_ACTION_TSUMO)
            return actions[i];
    }

    if (view->phase == CJ4_PHASE_DISCARD || view->phase == CJ4_PHASE_KAKAN_RESOLVE)
    {
        if (!continue_kokushi)
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

    for (uint8_t i = 0; i < action_count; ++i)
    {
        int score;

        if (actions[i].type != CJ4_ACTION_DISCARD)
            continue;

        score = continue_kokushi
                    ? cj4_opponent_kokushi_discard_score(view, actions[i].tile)
                    : cj4_opponent_kokushi_fallback_discard_score(view, actions[i].tile);
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
cj4_opponent_kokushi(int ctx_level)
{
    return (cj4m_player_delegate){
        .ctx = cj4_opponent_make_ctx(ctx_level),
        .decide = cj4_opponent_kokushi_decide};
}
