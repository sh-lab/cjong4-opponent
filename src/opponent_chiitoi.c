#include "cjong4/opponent/opponent_chiitoi.h"

#include "opponent_internal.h"

#include "cjong4/core/tile.h"

static uint8_t
cj4_opponent_chiitoi_tile_is_pair(
    const cj4_player_view *view,
    cj4_tile_id tile)
{
    return cj4_opponent_count_hand_tiles(view, cj4_tile_get_type(tile)) >= 2;
}

static uint8_t
cj4_opponent_chiitoi_tile_is_triplet(
    const cj4_player_view *view,
    cj4_tile_id tile)
{
    return cj4_opponent_count_hand_tiles(view, cj4_tile_get_type(tile)) >= 3;
}

static uint8_t
cj4_opponent_chiitoi_tile_in_discards(
    const cj4_player_view *view,
    cj4_tile_id tile)
{
    const cj4_tile_type type = cj4_tile_get_type(tile);
    const cj4_discard_list discards = cj4_opponent_discards(view);

    for (uint8_t i = 0; i < discards.count; ++i)
    {
        if (cj4_tile_get_type(discards.items[i].tile) == type)
            return 1;
    }

    return 0;
}

static int
cj4_opponent_chiitoi_count_pair_candidates(const cj4_player_view *view)
{
    int pairs = 0;

    for (cj4_tile_type type = CJ4_TILE_TYPE_MIN; type <= CJ4_TILE_TYPE_MAX; ++type)
    {
        if (cj4_opponent_count_hand_tiles(view, type) >= 2)
            ++pairs;
    }

    return pairs;
}

static int
cj4_opponent_chiitoi_count_triplet_candidates(const cj4_player_view *view)
{
    int triplets = 0;

    for (cj4_tile_type type = CJ4_TILE_TYPE_MIN; type <= CJ4_TILE_TYPE_MAX; ++type)
    {
        if (cj4_opponent_count_hand_tiles(view, type) >= 3)
            ++triplets;
    }

    return triplets;
}

static uint8_t
cj4_opponent_chiitoi_has_yakuhai_pair(const cj4_player_view *view)
{
    for (cj4_tile_type type = CJ4_TILE_TYPE_MIN; type <= CJ4_TILE_TYPE_MAX; ++type)
    {
        if (cj4_opponent_count_hand_tiles(view, type) >= 2 &&
            cj4_opponent_type_is_yakuhai(view, type))
        {
            return 1;
        }
    }

    return 0;
}

static uint8_t
cj4_opponent_chiitoi_should_continue(
    const cj4_player_view *view,
    uint8_t level)
{
    static const uint8_t max_discards_before_give_up[3] = {6, 9, 12};
    static const uint8_t min_pairs_early[3] = {5, 4, 4};
    static const uint8_t min_pairs_late[3] = {5, 4, 3};
    const cj4_hand hand = cj4_opponent_hand(view);
    const cj4_discard_list discards = cj4_opponent_discards(view);
    const int pairs = cj4_opponent_chiitoi_count_pair_candidates(view);

    if (hand.count == 0)
        return 1;

    if (discards.count == 0)
        return 1;

    if (pairs >= 6)
        return 1;

    if (discards.count <= max_discards_before_give_up[level] &&
        pairs >= min_pairs_early[level])
    {
        return 1;
    }

    return pairs >= min_pairs_late[level];
}

static uint8_t
cj4_opponent_chiitoi_should_open_toitoi(
    const cj4_player_view *view,
    uint8_t level)
{
    static const int min_commit_score[3] = {5, 4, 3};
    const cj4_meld_list melds = cj4_opponent_melds(view, view->player);
    const int commit_score =
        (int)melds.count +
        cj4_opponent_chiitoi_count_pair_candidates(view) +
        cj4_opponent_chiitoi_count_triplet_candidates(view) +
        (cj4_opponent_chiitoi_has_yakuhai_pair(view) ? 1 : 0);

    return commit_score >= min_commit_score[level];
}

static int
cj4_opponent_chiitoi_discard_score(
    const cj4_player_view *view,
    cj4_tile_id tile)
{
    const uint8_t threat_count = cj4_opponent_count_riichi_threats(view);
    const cj4_tile_type type = cj4_tile_get_type(tile);
    const uint8_t hand_count = cj4_opponent_count_hand_tiles(view, type);
    const uint8_t visible_count = cj4_opponent_count_visible_tiles(view, type);
    const uint8_t safe_bonus =
        threat_count > 0 && cj4_opponent_tile_is_safe(view, tile) ? 80 : 0;
    const int pair_candidates = cj4_opponent_chiitoi_count_pair_candidates(view);
    int score = visible_count * 15;

    if (hand_count >= 3)
    {
        score += 260;
    }
    else if (hand_count == 2)
    {
        score -= 120;

        if (pair_candidates == 1)
            score -= 60;

        if (cj4_opponent_type_is_yakuhai(view, type))
            score -= 20;
    }
    else
    {
        score += 120;

        if (cj4_opponent_type_is_honor(type) && !cj4_opponent_type_is_yakuhai(view, type))
            score += 35;
        else if (cj4_tile_is_yaochu(tile))
            score += 20;
    }

    return score + safe_bonus;
}

static int
cj4_opponent_chiitoi_toitoi_discard_score(
    const cj4_player_view *view,
    cj4_tile_id tile,
    uint8_t has_riichi,
    uint8_t level)
{
    static const int singleton_bonus[3] = {130, 100, 70};
    static const int pair_bonus[3] = {45, 25, 10};
    static const int triplet_penalty[3] = {-80, -120, -160};
    static const int last_pair_penalty[3] = {20, 40, 60};
    static const int safe_bonus[3] = {50, 30, 15};
    const cj4_tile_type type = cj4_tile_get_type(tile);
    const uint8_t hand_count = cj4_opponent_count_hand_tiles(view, type);
    const uint8_t visible_count = cj4_opponent_count_visible_tiles(view, type);
    int score = 0;

    if (hand_count <= 1)
        score += singleton_bonus[level];
    else if (hand_count == 2)
        score += pair_bonus[level];
    else
        score += triplet_penalty[level];

    score += visible_count * 10;

    if (hand_count == 2 &&
        cj4_opponent_chiitoi_count_pair_candidates(view) == 1)
    {
        score -= last_pair_penalty[level];
    }

    if (cj4_tile_is_yaochu(tile))
        score += visible_count > 0 ? 10 : -5;

    if (has_riichi && cj4_opponent_tile_is_safe(view, tile))
        score += safe_bonus[level];

    return score;
}

static cj4_action
cj4_opponent_chiitoi_decide(
    void *ctx,
    const cj4_player_view *view,
    const cj4_action *actions,
    uint8_t action_count)
{
    const uint8_t level = cj4_opponent_ctx_level(ctx);
    const uint8_t continue_chiitoi =
        cj4_opponent_chiitoi_should_continue(view, level);

    for (uint8_t i = 0; i < action_count; ++i)
    {
        if (actions[i].type == CJ4_ACTION_RON || actions[i].type == CJ4_ACTION_TSUMO)
            return actions[i];
    }

    if (view->phase == CJ4_PHASE_DISCARD || view->phase == CJ4_PHASE_KAKAN_RESOLVE)
    {
        if (!continue_chiitoi)
        {
            const uint8_t should_open =
                cj4_opponent_chiitoi_should_open_toitoi(view, level);
            const cj4_action *minkan = cj4_opponent_find_action(
                actions,
                action_count,
                CJ4_ACTION_MINKAN);
            if (minkan && should_open)
                return *minkan;

            const cj4_action *pon = cj4_opponent_find_action(
                actions,
                action_count,
                CJ4_ACTION_PON);
            if (pon && should_open)
                return *pon;
        }

        return cj4_opponent_choose_win_or_pass(actions, action_count);
    }

    {
        const cj4_action *riichi = cj4_opponent_find_action(
            actions,
            action_count,
            CJ4_ACTION_RIICHI);
        if (riichi)
            return *riichi;
    }

    if (!continue_chiitoi)
    {
        const uint8_t should_open =
            cj4_opponent_chiitoi_should_open_toitoi(view, level);
        const cj4_action *kakan = cj4_opponent_find_action(
            actions,
            action_count,
            CJ4_ACTION_KAKAN);
        if (kakan)
            return *kakan;

        {
            const cj4_action *ankan = cj4_opponent_find_action(
                actions,
                action_count,
                CJ4_ACTION_ANKAN);
            if (ankan)
                return *ankan;
        }

        if (should_open)
        {
            const cj4_action *minkan = cj4_opponent_find_action(
                actions,
                action_count,
                CJ4_ACTION_MINKAN);
            if (minkan)
                return *minkan;
        }
    }

    {
        const cj4_action *best_discard = 0;
        const uint8_t has_riichi = cj4_opponent_count_riichi_threats(view) > 0;
        int best_score = -10000;

        for (uint8_t i = 0; i < action_count; ++i)
        {
            int score;

            if (actions[i].type != CJ4_ACTION_DISCARD)
                continue;

            if (!continue_chiitoi &&
                cj4_opponent_chiitoi_tile_is_triplet(view, actions[i].tile))
            {
                continue;
            }

            score = continue_chiitoi
                        ? cj4_opponent_chiitoi_discard_score(view, actions[i].tile)
                        : cj4_opponent_chiitoi_toitoi_discard_score(
                              view,
                              actions[i].tile,
                              has_riichi,
                              level);

            if (!continue_chiitoi &&
                cj4_opponent_chiitoi_tile_in_discards(view, actions[i].tile) &&
                !cj4_opponent_chiitoi_tile_is_pair(view, actions[i].tile))
            {
                score += 20;
            }

            if (!best_discard || score > best_score)
            {
                best_discard = &actions[i];
                best_score = score;
            }
        }

        if (best_discard)
            return *best_discard;
    }

    for (uint8_t i = 0; i < action_count; ++i)
    {
        if (actions[i].type == CJ4_ACTION_DISCARD)
            return actions[i];
    }

    return actions[0];
}

cj4m_player_delegate
cj4_opponent_chiitoi(int ctx_level)
{
    return (cj4m_player_delegate){
        .ctx = cj4_opponent_make_ctx(ctx_level),
        .decide = cj4_opponent_chiitoi_decide};
}
