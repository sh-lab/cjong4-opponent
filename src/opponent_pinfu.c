#include "cjong4/opponent/opponent_pinfu.h"

#include "opponent_internal.h"

#include "cjong4/core/tile.h"
#include "cjong4/core/tile_const.h"

static uint8_t
cj4_opponent_tile_is_honor(cj4_tile_id tile)
{
    return cj4_opponent_type_is_honor(cj4_tile_get_type(tile));
}

static const cj4_action *
cj4_opponent_pinfu_choose_safe_discard(
    const cj4_player_view *view,
    const cj4_action *actions,
    uint8_t action_count)
{
    for (uint8_t i = 0; i < action_count; ++i)
    {
        if (actions[i].type == CJ4_ACTION_DISCARD &&
            cj4_opponent_tile_is_safe(view, actions[i].tile))
            return &actions[i];
    }

    return 0;
}

static cj4_action
cj4_opponent_pinfu_decide(
    void *ctx,
    const cj4_player_view *view,
    const cj4_action *actions,
    uint8_t action_count)
{
    const cj4_action *riichi;
    const cj4_action *safe_discard;
    const uint8_t level = cj4_opponent_ctx_level(ctx);
    const uint8_t threat_count = cj4_opponent_count_riichi_threats(view);

    for (uint8_t i = 0; i < action_count; ++i)
    {
        if (actions[i].type == CJ4_ACTION_RON || actions[i].type == CJ4_ACTION_TSUMO)
            return actions[i];
    }

    if (view->phase == CJ4_PHASE_DISCARD || view->phase == CJ4_PHASE_KAKAN_RESOLVE)
        return cj4_opponent_choose_win_or_pass(actions, action_count);

    riichi = cj4_opponent_find_action(actions, action_count, CJ4_ACTION_RIICHI);
    safe_discard = cj4_opponent_pinfu_choose_safe_discard(
        view,
        actions,
        action_count);

    if (threat_count > 0 && safe_discard)
    {
        if (level == 0)
            return *safe_discard;

        if (level == 1 && (!riichi || threat_count >= 2))
            return *safe_discard;

        if (level == 2 && threat_count >= 2 && !riichi)
            return *safe_discard;
    }

    if (riichi)
        return *riichi;

    for (uint8_t i = 0; i < action_count; ++i)
    {
        if (actions[i].type != CJ4_ACTION_DISCARD)
            continue;

        if (cj4_opponent_tile_is_honor(actions[i].tile))
            return actions[i];
    }

    for (uint8_t i = 0; i < action_count; ++i)
    {
        if (actions[i].type == CJ4_ACTION_DISCARD)
            return actions[i];
    }

    return actions[0];
}

cj4m_player_delegate
cj4_opponent_pinfu(int ctx_level)
{
    return (cj4m_player_delegate){
        .ctx = cj4_opponent_make_ctx(ctx_level),
        .decide = cj4_opponent_pinfu_decide};
}
