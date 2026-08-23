#include "cjong4/opponent/opponent_somete.h"

#include "opponent_internal.h"

#include "cjong4/core/tile.h"
#include "cjong4/core/tile_const.h"

typedef enum
{
    CJ4_OPPONENT_SOMETE_CHINITSU = 0,
    CJ4_OPPONENT_SOMETE_HONITSU = 1,
    CJ4_OPPONENT_SOMETE_TSUUIISOU = 2
} cj4_opponent_somete_route;

typedef struct
{
    cj4_opponent_somete_route route;
    uint8_t suit;
} cj4_opponent_somete_plan;

static uint8_t
cj4_opponent_somete_type_is_suited(cj4_tile_type type)
{
    return !cj4_opponent_type_is_honor(type);
}

static uint8_t
cj4_opponent_somete_type_suit(cj4_tile_type type)
{
    if (type <= CJ4_TILE_TYPE_9M)
        return 0;
    if (type <= CJ4_TILE_TYPE_9P)
        return 1;
    return 2;
}

static uint8_t
cj4_opponent_somete_type_number(cj4_tile_type type)
{
    if (type <= CJ4_TILE_TYPE_9M)
        return (uint8_t)(type - CJ4_TILE_TYPE_1M + 1);
    if (type <= CJ4_TILE_TYPE_9P)
        return (uint8_t)(type - CJ4_TILE_TYPE_1P + 1);
    return (uint8_t)(type - CJ4_TILE_TYPE_1S + 1);
}

static cj4_tile_type
cj4_opponent_somete_type_from_suit_number(uint8_t suit, uint8_t number)
{
    switch (suit)
    {
    case 0:
        return (cj4_tile_type)(CJ4_TILE_TYPE_1M + number - 1);
    case 1:
        return (cj4_tile_type)(CJ4_TILE_TYPE_1P + number - 1);
    default:
        return (cj4_tile_type)(CJ4_TILE_TYPE_1S + number - 1);
    }
}

static uint8_t
cj4_opponent_somete_count_player_discards(const cj4_player_view *view)
{
    const cj4_discard_list discards = cj4_opponent_discards(view);
    uint8_t count = 0;

    for (uint8_t i = 0; i < discards.count; ++i)
    {
        if (discards.items[i].player == view->player)
            ++count;
    }

    return count;
}

static void
cj4_opponent_somete_count_owned_tiles(
    const cj4_player_view *view,
    uint8_t suit_counts[3],
    uint8_t *honor_count)
{
    const cj4_hand hand = cj4_opponent_hand(view);
    const cj4_meld_list melds = cj4_opponent_melds(view, view->player);

    for (uint8_t i = 0; i < 3; ++i)
        suit_counts[i] = 0;
    *honor_count = 0;

    for (uint8_t i = 0; i < hand.count; ++i)
    {
        const cj4_tile_type type = cj4_tile_get_type(hand.items[i]);

        if (cj4_opponent_somete_type_is_suited(type))
            ++suit_counts[cj4_opponent_somete_type_suit(type)];
        else
            ++(*honor_count);
    }

    for (uint8_t i = 0; i < melds.count; ++i)
    {
        const cj4_meld *meld = &melds.items[i];

        for (uint8_t j = 0; j < meld->size; ++j)
        {
            const cj4_tile_type type = cj4_tile_get_type(meld->tiles[j]);

            if (cj4_opponent_somete_type_is_suited(type))
                ++suit_counts[cj4_opponent_somete_type_suit(type)];
            else
                ++(*honor_count);
        }
    }
}

static uint8_t
cj4_opponent_somete_count_owned_type(
    const cj4_player_view *view,
    cj4_tile_type type)
{
    const cj4_meld_list melds = cj4_opponent_melds(view, view->player);
    uint8_t count = cj4_opponent_count_hand_tiles(view, type);

    for (uint8_t i = 0; i < melds.count; ++i)
    {
        const cj4_meld *meld = &melds.items[i];

        for (uint8_t j = 0; j < meld->size; ++j)
        {
            if (cj4_tile_get_type(meld->tiles[j]) == type)
                ++count;
        }
    }

    return count;
}

static uint8_t
cj4_opponent_somete_count_owned_honor_pairs(const cj4_player_view *view)
{
    uint8_t pairs = 0;

    for (cj4_tile_type type = CJ4_TILE_TYPE_EAST; type <= CJ4_TILE_TYPE_CHUN; ++type)
    {
        if (cj4_opponent_somete_count_owned_type(view, type) >= 2)
            ++pairs;
    }

    return pairs;
}

static cj4_opponent_somete_plan
cj4_opponent_somete_choose_plan(const cj4_player_view *view)
{
    cj4_opponent_somete_plan plan = {
        .route = CJ4_OPPONENT_SOMETE_HONITSU,
        .suit = 0};
    uint8_t suit_counts[3];
    uint8_t honor_count = 0;
    const uint8_t player_discards = cj4_opponent_somete_count_player_discards(view);
    const cj4_meld_list melds = cj4_opponent_melds(view, view->player);
    uint8_t best_suit = 0;
    uint8_t best_count = 0;
    uint8_t suited_count = 0;
    uint8_t honor_pairs;

    cj4_opponent_somete_count_owned_tiles(view, suit_counts, &honor_count);
    honor_pairs = cj4_opponent_somete_count_owned_honor_pairs(view);

    for (uint8_t suit = 0; suit < 3; ++suit)
    {
        suited_count += suit_counts[suit];
        if (suit == 0 || suit_counts[suit] > best_count)
        {
            best_suit = suit;
            best_count = suit_counts[suit];
        }
    }

    if (((player_discards == 0 && melds.count == 0) ||
         (honor_count >= 10 && suited_count <= 2)) &&
        honor_count >= 8 &&
        suited_count <= 4 &&
        honor_pairs >= 2)
    {
        plan.route = CJ4_OPPONENT_SOMETE_TSUUIISOU;
        return plan;
    }

    plan.suit = best_suit;

    if (best_count >= 9 && honor_count <= 2)
        plan.route = CJ4_OPPONENT_SOMETE_CHINITSU;
    else if (honor_count == 0 && best_count >= 7 && suited_count - best_count <= 2)
        plan.route = CJ4_OPPONENT_SOMETE_CHINITSU;
    else
        plan.route = CJ4_OPPONENT_SOMETE_HONITSU;

    return plan;
}

static uint8_t
cj4_opponent_somete_tile_matches_plan(
    cj4_opponent_somete_plan plan,
    cj4_tile_type type)
{
    switch (plan.route)
    {
    case CJ4_OPPONENT_SOMETE_CHINITSU:
        return cj4_opponent_somete_type_is_suited(type) &&
               cj4_opponent_somete_type_suit(type) == plan.suit;
    case CJ4_OPPONENT_SOMETE_HONITSU:
        return cj4_opponent_type_is_honor(type) ||
               (cj4_opponent_somete_type_is_suited(type) &&
                cj4_opponent_somete_type_suit(type) == plan.suit);
    default:
        return cj4_opponent_type_is_honor(type);
    }
}

static uint8_t
cj4_opponent_somete_tile_is_target_suit(
    cj4_opponent_somete_plan plan,
    cj4_tile_type type)
{
    return cj4_opponent_somete_type_is_suited(type) &&
           cj4_opponent_somete_type_suit(type) == plan.suit;
}

static uint8_t
cj4_opponent_somete_count_off_route_tiles(
    const cj4_player_view *view,
    cj4_opponent_somete_plan plan)
{
    const cj4_hand hand = cj4_opponent_hand(view);
    uint8_t count = 0;

    for (uint8_t i = 0; i < hand.count; ++i)
    {
        if (!cj4_opponent_somete_tile_matches_plan(
                plan,
                cj4_tile_get_type(hand.items[i])))
        {
            ++count;
        }
    }

    return count;
}

static uint8_t
cj4_opponent_somete_count_route_tiles(
    const cj4_player_view *view,
    cj4_opponent_somete_plan plan)
{
    const cj4_hand hand = cj4_opponent_hand(view);
    uint8_t count = 0;

    for (uint8_t i = 0; i < hand.count; ++i)
    {
        if (cj4_opponent_somete_tile_matches_plan(
                plan,
                cj4_tile_get_type(hand.items[i])))
        {
            ++count;
        }
    }

    return count;
}

static uint8_t
cj4_opponent_somete_target_sequence_support(
    const cj4_player_view *view,
    cj4_tile_type type)
{
    const uint8_t suit = cj4_opponent_somete_type_suit(type);
    const uint8_t number = cj4_opponent_somete_type_number(type);
    uint8_t support = 0;

    if (number > 1)
        support += cj4_opponent_count_hand_tiles(
            view,
            cj4_opponent_somete_type_from_suit_number(suit, (uint8_t)(number - 1)));
    if (number > 2)
        support += cj4_opponent_count_hand_tiles(
            view,
            cj4_opponent_somete_type_from_suit_number(suit, (uint8_t)(number - 2)));
    if (number < 9)
        support += cj4_opponent_count_hand_tiles(
            view,
            cj4_opponent_somete_type_from_suit_number(suit, (uint8_t)(number + 1)));
    if (number < 8)
        support += cj4_opponent_count_hand_tiles(
            view,
            cj4_opponent_somete_type_from_suit_number(suit, (uint8_t)(number + 2)));

    return support;
}

static uint8_t
cj4_opponent_somete_can_open(
    const cj4_player_view *view,
    cj4_opponent_somete_plan plan,
    const cj4_action *action,
    uint8_t level)
{
    static const uint8_t max_off_route_for_open[3] = {4, 5, 6};
    const cj4_tile_type type = cj4_tile_get_type(action->tile);
    const uint8_t off_route_tiles = cj4_opponent_somete_count_off_route_tiles(view, plan);
    const uint8_t route_tiles = cj4_opponent_somete_count_route_tiles(view, plan);
    const uint8_t compatible = cj4_opponent_somete_tile_matches_plan(plan, type);
    const cj4_meld_list melds = cj4_opponent_melds(view, view->player);

    if (!compatible)
        return 0;

    if (action->type == CJ4_ACTION_CHI &&
        !cj4_opponent_somete_tile_is_target_suit(plan, type))
        return 0;

    if (melds.count > 0 && off_route_tiles <= (uint8_t)(max_off_route_for_open[level] + 1))
        return 1;

    if (route_tiles >= 8 && off_route_tiles <= max_off_route_for_open[level])
        return 1;

    if (plan.route != CJ4_OPPONENT_SOMETE_CHINITSU &&
        cj4_opponent_type_is_yakuhai(view, type) &&
        off_route_tiles <= (uint8_t)(5 + level))
        return 1;

    return off_route_tiles <= level;
}

static int
cj4_opponent_somete_discard_score(
    const cj4_player_view *view,
    cj4_opponent_somete_plan plan,
    cj4_tile_id tile)
{
    const uint8_t threat_count = cj4_opponent_count_riichi_threats(view);
    const cj4_tile_type type = cj4_tile_get_type(tile);
    const uint8_t hand_count = cj4_opponent_count_hand_tiles(view, type);
    const uint8_t visible_count = cj4_opponent_count_visible_tiles(view, type);
    const uint8_t safe_bonus = threat_count > 0 && cj4_opponent_tile_is_safe(view, tile) ? 80 : 0;
    int score = 0;

    if (!cj4_opponent_somete_tile_matches_plan(plan, type))
    {
        score += 250;
        score += visible_count * 10;
        score += hand_count == 1 ? 40 : hand_count * 15;

        if (cj4_opponent_somete_type_is_suited(type))
        {
            const uint8_t number = cj4_tile_get_number(tile);
            score += (int)(5 - (number > 5 ? (10 - number) : number)) * 10 + 40;
        }
        else if (cj4_opponent_type_is_yakuhai(view, type))
        {
            score -= 20;
        }

        return score + safe_bonus;
    }

    if (plan.route == CJ4_OPPONENT_SOMETE_TSUUIISOU)
    {
        score -= hand_count * 45;
        score -= cj4_opponent_type_is_yakuhai(view, type) ? 45 : 10;
        return score + safe_bonus;
    }

    if (cj4_opponent_type_is_honor(type))
    {
        score += cj4_opponent_type_is_yakuhai(view, type) ? -40 : 20;
        score -= hand_count * 30;
        return score + safe_bonus;
    }

    {
        const uint8_t number = cj4_tile_get_number(tile);
        const uint8_t support = cj4_opponent_somete_target_sequence_support(view, type);

        score -= hand_count * 35;
        score -= support * 25;

        if (number == 1 || number == 9)
            score += 35;
        else if (number == 2 || number == 8)
            score += 15;
        else
            score -= 10;

        score += visible_count * 5;
    }

    return score + safe_bonus;
}

static cj4_action
cj4_opponent_somete_decide(
    void *ctx,
    const cj4_player_view *view,
    const cj4_action *actions,
    uint8_t action_count)
{
    const uint8_t level = cj4_opponent_ctx_level(ctx);
    const cj4_opponent_somete_plan plan = cj4_opponent_somete_choose_plan(view);
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
        for (uint8_t i = 0; i < action_count; ++i)
        {
            if ((actions[i].type == CJ4_ACTION_MINKAN ||
                 actions[i].type == CJ4_ACTION_PON ||
                 actions[i].type == CJ4_ACTION_CHI) &&
                cj4_opponent_somete_can_open(view, plan, &actions[i], level))
            {
                return actions[i];
            }
        }

        return cj4_opponent_choose_win_or_pass(actions, action_count);
    }

    riichi = cj4_opponent_find_action(actions, action_count, CJ4_ACTION_RIICHI);
    if (riichi)
        return *riichi;

    kakan = cj4_opponent_find_action(actions, action_count, CJ4_ACTION_KAKAN);
    if (kakan &&
        cj4_opponent_somete_tile_matches_plan(plan, cj4_tile_get_type(kakan->tile)))
        return *kakan;

    ankan = cj4_opponent_find_action(actions, action_count, CJ4_ACTION_ANKAN);
    if (ankan &&
        cj4_opponent_somete_tile_matches_plan(plan, cj4_tile_get_type(ankan->tile)))
        return *ankan;

    for (uint8_t i = 0; i < action_count; ++i)
    {
        int score;

        if (actions[i].type != CJ4_ACTION_DISCARD)
            continue;

        score = cj4_opponent_somete_discard_score(view, plan, actions[i].tile);
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
cj4_opponent_somete(int ctx_level)
{
    return (cj4m_player_delegate){
        .ctx = cj4_opponent_make_ctx(ctx_level),
        .decide = cj4_opponent_somete_decide};
}
