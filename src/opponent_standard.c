#include "cjong4/opponent/opponent_standard.h"

#include "opponent_internal.h"
#include "cjong4/player/hand_analysis.h"

#include <stdbool.h>

typedef struct
{
    int shanten;
    int effective;
} standard_score;

static bool
score_better(standard_score a, standard_score b)
{
    return a.shanten < b.shanten ||
           (a.shanten == b.shanten && a.effective > b.effective);
}

static int
route_shanten(cj4_shanten_result result, bool kokushi)
{
    return kokushi ? result.kokushi : result.standard;
}

static bool
prefer_kokushi(const cj4_player_view *view)
{
    cj4_shanten_result result;
    return cj4p_calculate_shanten(view, &result) &&
           result.kokushi != CJ4_SHANTEN_NOT_APPLICABLE &&
           result.kokushi + 3 <= result.standard;
}

/* The public shanten API normalizes 14 tiles to a post-discard hand.
 * Check standard completion separately when counting tenpai's live waits,
 * so a chiitoitsu-only wait cannot enter the standard route's score. */
static bool
all_melds(uint8_t counts[CJ4_TILE_TYPE_COUNT])
{
    uint8_t t = 0;
    while (t < CJ4_TILE_TYPE_COUNT && counts[t] == 0)
        ++t;
    if (t == CJ4_TILE_TYPE_COUNT)
        return true;
    if (counts[t] >= 3)
    {
        counts[t] -= 3;
        const bool complete = all_melds(counts);
        counts[t] += 3;
        if (complete)
            return true;
    }
    if (t < CJ4_TILE_TYPE_EAST && t % 9 <= 6 &&
        counts[t + 1] && counts[t + 2])
    {
        --counts[t];
        --counts[t + 1];
        --counts[t + 2];
        const bool complete = all_melds(counts);
        ++counts[t];
        ++counts[t + 1];
        ++counts[t + 2];
        if (complete)
            return true;
    }
    return false;
}

static bool
standard_complete(const cj4_player_view *view)
{
    uint8_t counts[CJ4_TILE_TYPE_COUNT] = {0};
    const cj4_hand hand = cj4_opponent_hand(view);
    for (uint8_t i = 0; i < hand.count; ++i)
        ++counts[cj4_tile_get_type(hand.items[i])];
    for (uint8_t t = 0; t < CJ4_TILE_TYPE_COUNT; ++t)
    {
        if (counts[t] < 2)
            continue;
        counts[t] -= 2;
        const bool complete = all_melds(counts);
        counts[t] += 2;
        if (complete)
            return true;
    }
    return false;
}

/* view is a 13-tile-equivalent hand. Unknown physical tiles are the only
 * possible draws; known tiles (including our simulated discard) stay visible. */
static int
count_effective(const cj4_player_view *view, bool kokushi, int shanten)
{
    cj4_player_view drawn = *view;
    cj4_waiting_tile_types waits;
    int effective = 0;
    if (shanten == 0 && !cj4p_collect_waiting_tile_types(view, &waits))
        return 0;

    for (uint8_t type = 0; type < CJ4_TILE_TYPE_COUNT; ++type)
    {
        int remaining = 0;
        cj4_tile_id candidate = CJ4_TILE_ID_INVALID;
        for (uint8_t copy = 0; copy < CJ4_TILE_PER_TYPE; ++copy)
        {
            const cj4_tile_id tile = cj4_tile_make((cj4_tile_type)type, copy);
            if (cj4_location_is_unknown(&view->locations[tile]))
            {
                ++remaining;
                candidate = tile;
            }
        }
        if (!remaining || (shanten == 0 && !waits.types[type]))
            continue;
        drawn.locations[candidate].placement =
            (uint8_t)(view->player << CJ4_LOCATION_PLAYER_SHIFT);
        bool improves;
        if (shanten == 0)
            improves = kokushi || standard_complete(&drawn);
        else
        {
            cj4_shanten_result result;
            improves = cj4p_calculate_shanten(&drawn, &result) &&
                       route_shanten(result, kokushi) < shanten;
        }
        drawn.locations[candidate] = view->locations[candidate];
        if (improves)
            effective += remaining;
    }
    return effective;
}

static void
simulate_discard(cj4_player_view *view, cj4_tile_id tile)
{
    view->locations[tile].placement = CJ4_LOCATION_NONE;
    view->locations[tile].discard =
        (uint8_t)(view->player << CJ4_LOCATION_PLAYER_SHIFT);
}

/* Without rules in player_view, conservatively exclude standard kuikae
 * when projecting a call. Actual discards always come from legal actions. */
static bool
call_forbids_discard(const cj4_action *call, cj4_tile_type type)
{
    if (!call)
        return false;
    const cj4_tile_type called = cj4_tile_get_type(call->tile);
    if (type == called)
        return true;
    if (call->type != CJ4_ACTION_CHI)
        return false;
    cj4_tile_type low = called;
    for (uint8_t i = 0; i < call->tile_count; ++i)
    {
        const cj4_tile_type t = cj4_tile_get_type(call->tiles[i]);
        if (t < low)
            low = t;
    }
    return cj4_tile_type_get_suit(type) == cj4_tile_type_get_suit(called) &&
           ((called == low && (int)type == (int)low + 3) ||
            (called == low + 2 && (int)type == (int)low - 1));
}

static const cj4_action *
best_discard(const cj4_player_view *view, const cj4_action *actions,
             uint8_t count, cj4_action_type kind, bool kokushi,
             const cj4_action *call, standard_score *out)
{
    const cj4_action *best = 0;
    standard_score score = {CJ4_SHANTEN_NOT_APPLICABLE, -1};
    bool seen[CJ4_TILE_TYPE_COUNT] = {false};
    for (uint8_t i = 0; i < count; ++i)
    {
        if (actions[i].type != kind)
            continue;
        const cj4_tile_type type = cj4_tile_get_type(actions[i].tile);
        if (seen[type] || call_forbids_discard(call, type))
            continue;
        seen[type] = true;
        cj4_shanten_result result;
        if (!cj4p_calculate_shanten_after_discard(view, actions[i].tile, &result))
            continue;
        standard_score candidate = {route_shanten(result, kokushi), 0};
        if (candidate.shanten > score.shanten)
            continue;
        cj4_player_view after = *view;
        simulate_discard(&after, actions[i].tile);
        candidate.effective = count_effective(&after, kokushi, candidate.shanten);
        if (!best || score_better(candidate, score))
        {
            best = &actions[i];
            score = candidate;
        }
    }
    *out = score;
    return best;
}

static bool
score_call(const cj4_player_view *view, const cj4_action *call,
           uint8_t meld_count, standard_score *out)
{
    if (call->tile_count != 2 || meld_count >= CJ4_MAX_MELDS)
        return false;
    cj4_player_view after = *view;
    const uint8_t placement = (uint8_t)(CJ4_LOCATION_PLACEMENT_MELD_FLAG |
        (view->player << CJ4_LOCATION_PLAYER_SHIFT) |
        (meld_count << CJ4_LOCATION_MELD_GROUP_SHIFT) |
        (call->type == CJ4_ACTION_PON ? CJ4_MELD_PON : CJ4_MELD_CHI));
    after.locations[call->tile].placement = placement;
    for (uint8_t i = 0; i < call->tile_count; ++i)
        after.locations[call->tiles[i]].placement = placement;
    const cj4_hand hand = cj4_opponent_hand(&after);
    cj4_action discards[14] = {0};
    for (uint8_t i = 0; i < hand.count; ++i)
    {
        discards[i].type = CJ4_ACTION_DISCARD;
        discards[i].tile = hand.items[i];
    }
    return best_discard(&after, discards, hand.count, CJ4_ACTION_DISCARD,
                        false, call, out) != 0;
}

static cj4_action
standard_decide(void *ctx, const cj4_player_view *view,
                const cj4_action *actions, uint8_t action_count)
{
    (void)ctx;
    for (uint8_t i = 0; i < action_count; ++i)
        if (actions[i].type == CJ4_ACTION_TSUMO || actions[i].type == CJ4_ACTION_RON)
            return actions[i];

    const bool kokushi = prefer_kokushi(view);
    const cj4_action *riichi = cj4_opponent_find_action(actions, action_count, CJ4_ACTION_RIICHI);
    standard_score score;
    if (riichi)
    {
        const cj4_action *best = best_discard(view, actions, action_count,
            CJ4_ACTION_RIICHI, kokushi, 0, &score);
        return best ? *best : *riichi;
    }

    if (view->phase == CJ4_PHASE_DISCARD)
    {
        const cj4_meld_list melds = cj4_opponent_melds(view, view->player);
        uint8_t open_count = 0;
        for (uint8_t i = 0; i < melds.count; ++i)
            if (melds.items[i].type != CJ4_MELD_ANKAN)
                ++open_count;
        if (!kokushi && open_count < 3)
        {
            /* Yakuhai pon starts the open route and takes priority. */
            for (uint8_t i = 0; i < action_count; ++i)
                if (actions[i].type == CJ4_ACTION_PON &&
                    cj4_opponent_tile_is_yakuhai(view, actions[i].tile))
                    return actions[i];
            cj4_shanten_result result;
            if (open_count && cj4p_calculate_shanten(view, &result))
            {
                score.shanten = result.standard;
                score.effective = count_effective(view, false, score.shanten);
                const cj4_action *best = 0;
                for (uint8_t i = 0; i < action_count; ++i)
                {
                    if (actions[i].type != CJ4_ACTION_CHI && actions[i].type != CJ4_ACTION_PON)
                        continue;
                    standard_score candidate;
                    if (score_call(view, &actions[i], melds.count, &candidate) &&
                        score_better(candidate, score))
                    {
                        best = &actions[i];
                        score = candidate;
                    }
                }
                if (best)
                    return *best;
            }
        }
        return cj4_opponent_choose_win_or_pass(actions, action_count);
    }
    if (view->phase == CJ4_PHASE_KAKAN_RESOLVE)
        return cj4_opponent_choose_win_or_pass(actions, action_count);

    const cj4_action *best = best_discard(view, actions, action_count,
        CJ4_ACTION_DISCARD, kokushi, 0, &score);
    if (best)
        return *best;
    return cj4_opponent_choose_win_or_pass(actions, action_count);
}

cj4m_player_delegate
cj4_opponent_standard(int ctx_level)
{
    return (cj4m_player_delegate){
        .ctx = cj4_opponent_make_ctx(ctx_level),
        .decide = standard_decide};
}
