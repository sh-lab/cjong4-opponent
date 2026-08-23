#include "cjong4/opponent/opponent_betaori.h"

#include "opponent_internal.h"

#include "cjong4/core/tile.h"

static int
cj4_opponent_betaori_discard_score(
    const cj4_player_view *view,
    cj4_tile_id tile)
{
    const uint8_t threat_count = cj4_opponent_count_riichi_threats(view);
    const uint8_t safe_count = cj4_opponent_tile_safe_count(view, tile);
    const uint8_t visible_count = cj4_opponent_count_visible_tiles(
        view,
        cj4_tile_get_type(tile));
    int score = safe_count * 100 + visible_count * 10;

    if (threat_count > 0 && safe_count == threat_count)
        score += 100;

    return score;
}

static cj4_action
cj4_opponent_betaori_decide(
    void *ctx,
    const cj4_player_view *view,
    const cj4_action *actions,
    uint8_t action_count)
{
    const uint8_t level = cj4_opponent_ctx_level(ctx);
    const uint8_t threat_count = cj4_opponent_count_riichi_threats(view);
    const cj4_action *best_discard = 0;
    int best_score = -1;

    for (uint8_t i = 0; i < action_count; ++i)
    {
        if (actions[i].type == CJ4_ACTION_RON || actions[i].type == CJ4_ACTION_TSUMO)
            return actions[i];
    }

    if (view->phase == CJ4_PHASE_DISCARD || view->phase == CJ4_PHASE_KAKAN_RESOLVE)
        return cj4_opponent_choose_win_or_pass(actions, action_count);

    if (threat_count > 0)
    {
        for (uint8_t i = 0; i < action_count; ++i)
        {
            if (actions[i].type != CJ4_ACTION_DISCARD)
                continue;

            if (level == 1 && cj4_opponent_tile_is_safe(view, actions[i].tile))
                return actions[i];
        }

        if (level == 0 && threat_count >= 2)
        {
            for (uint8_t i = 0; i < action_count; ++i)
            {
                if (actions[i].type == CJ4_ACTION_DISCARD &&
                    cj4_opponent_tile_is_safe(view, actions[i].tile))
                    return actions[i];
            }
        }

        if (level == 2)
        {
            for (uint8_t i = 0; i < action_count; ++i)
            {
                int score;

                if (actions[i].type != CJ4_ACTION_DISCARD)
                    continue;

                score = cj4_opponent_betaori_discard_score(view, actions[i].tile);
                if (!best_discard || score > best_score)
                {
                    best_discard = &actions[i];
                    best_score = score;
                }
            }

            if (best_discard)
                return *best_discard;
        }
    }

    for (uint8_t i = 0; i < action_count; ++i)
    {
        if (actions[i].type == CJ4_ACTION_DISCARD)
            return actions[i];
    }

    return actions[0];
}

cj4m_player_delegate
cj4_opponent_betaori(int ctx_level)
{
    return (cj4m_player_delegate){
        .ctx = cj4_opponent_make_ctx(ctx_level),
        .decide = cj4_opponent_betaori_decide};
}
