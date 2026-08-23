#include <assert.h>
#include <string.h>

#include "cjong4/core/action.h"
#include "cjong4/core/state.h"
#include "cjong4/core/tile.h"
#include "cjong4/core/tile_const.h"
#include "cjong4/core/wind.h"
#include "cjong4/manager/manager.h"
#include "cjong4/opponent/opponent_betaori.h"
#include "cjong4/opponent/opponent_chanta.h"
#include "cjong4/opponent/opponent_chiitoi.h"
#include "cjong4/opponent/opponent_kokushi.h"
#include "cjong4/opponent/opponent_pinfu.h"
#include "cjong4/opponent/opponent_somete.h"
#include "cjong4/opponent/opponent_tanyao.h"
#include "cjong4/opponent/opponent_toitoi.h"

#include "test_state_helpers.h"

static cj4_tile_id
tile(cj4_tile_type type, uint8_t index)
{
    return cj4_tile_make(type, index);
}

static cj4_mahjong
make_empty_state(void)
{
    cj4_mahjong state;

    memset(&state, 0, sizeof(state));
    memset(state.locations, CJ4_LOCATION_NONE, sizeof(state.locations));
    for (uint16_t i = 0; i < CJ4_TILE_ID_COUNT; ++i)
        state.locations[i].wall = (uint8_t)i;
    cj4_state_set_phase(&state, CJ4_PHASE_DRAW);
    state.dealer = CJ4_PLAYER_0;
    cj4_state_set_current_player(&state, CJ4_PLAYER_0);
    state.round_wind = CJ4_WIND_EAST;
    state.draw_tile = CJ4_TILE_ID_INVALID;
    state.pending_kakan_tile = CJ4_TILE_ID_INVALID;
    state.pending_ankan_tile = CJ4_TILE_ID_INVALID;
    for (uint8_t i = 0; i < CJ4_TILE_PER_TYPE; ++i)
        state.pending_ankan_tiles[i] = CJ4_TILE_ID_INVALID;
    cj4_state_clear_pending_riichi_bits(&state);
    state.last_discard_tile = CJ4_TILE_ID_INVALID;
    state.winning_tile = CJ4_TILE_ID_INVALID;
    cj4_state_set_round_result(
        &state,
        CJ4_ROUND_END_NONE,
        CJ4_ABORTIVE_DRAW_NONE);

    for (uint8_t i = 0; i < CJ4_PLAYER_COUNT; ++i)
        state.scores[i] = 25000;

    return state;
}

static void
set_hand(
    cj4_mahjong *state,
    cj4_player player,
    const cj4_tile_id *tiles,
    uint8_t count)
{
    for (uint8_t i = 0; i < count; ++i)
        state->locations[tiles[i]].placement = cj4_location_make_hand(player);
}

static void
add_discard(
    cj4_mahjong *state,
    cj4_player player,
    cj4_tile_id discarded)
{
    uint8_t player_discard_index = 0;

    for (uint16_t tile_id = 0; tile_id < CJ4_TILE_ID_COUNT; ++tile_id)
    {
        uint8_t encoded = state->locations[tile_id].discard;
        if (cj4_location_is_discard(encoded) &&
            cj4_location_discard_player(encoded) == player)
            ++player_discard_index;
    }

    state->locations[discarded].discard =
        cj4_location_make_discard(player, player_discard_index, false);
    state->locations[discarded].placement = CJ4_LOCATION_NONE;
    state->locations[discarded].discard_history =
        cj4_location_make_discard_history(state->discard_count, false);
    state->last_discard_tile = discarded;
    ++state->discard_count;
}

static cj4_action
make_action(
    cj4_action_type type,
    cj4_player player,
    cj4_tile_id tile_id)
{
    cj4_action action;

    memset(&action, 0, sizeof(action));
    action.type = type;
    action.player = player;
    action.tile = tile_id;
    action.tiles[0] = tile_id;
    action.tile_count = tile_id == CJ4_TILE_ID_INVALID ? 0 : 1;

    return action;
}

static void
test_chanta_discards_middle_tile_first(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_chanta(1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_1M, 0)),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_5M, 0)),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_EAST, 0))};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.dealer = CJ4_PLAYER_0;
    view.round_wind = CJ4_WIND_EAST;
    view.phase = CJ4_PHASE_DRAW;
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_1M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_5M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_EAST, 0));

    selected = delegate.decide(delegate.ctx, &view, actions, 3);

    assert(selected.type == CJ4_ACTION_DISCARD);
    assert(selected.tile == tile(CJ4_TILE_TYPE_5M, 0));
}

static void
test_chanta_ctx_controls_early_opening(void)
{
    cj4_player_view view;
    cj4m_player_delegate late = cj4_opponent_chanta(0);
    cj4m_player_delegate early = cj4_opponent_chanta(2);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_PASS, CJ4_PLAYER_0, CJ4_TILE_ID_INVALID),
        make_action(CJ4_ACTION_PON, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_1M, 0))};
    cj4_action late_selected;
    cj4_action early_selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.dealer = CJ4_PLAYER_0;
    view.round_wind = CJ4_WIND_EAST;
    view.phase = CJ4_PHASE_DISCARD;
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_1M, 1));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_1M, 2));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_2M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_8M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_5P, 0));

    late_selected = late.decide(late.ctx, &view, actions, 2);
    early_selected = early.decide(early.ctx, &view, actions, 2);

    assert(late_selected.type == CJ4_ACTION_PASS);
    assert(early_selected.type == CJ4_ACTION_PON);
}

static void
test_chiitoi_keeps_pair_over_singleton(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_chiitoi(1);
    const cj4_tile_id singleton = tile(CJ4_TILE_TYPE_4M, 0);
    const cj4_tile_id pair0 = tile(CJ4_TILE_TYPE_7P, 0);
    const cj4_tile_id pair1 = tile(CJ4_TILE_TYPE_7P, 1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, pair0),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, singleton),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, pair1)};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.dealer = CJ4_PLAYER_0;
    view.round_wind = CJ4_WIND_EAST;
    view.phase = CJ4_PHASE_DRAW;
    test_view_add_hand(&view, singleton);
    test_view_add_hand(&view, pair0);
    test_view_add_hand(&view, pair1);

    selected = delegate.decide(delegate.ctx, &view, actions, 3);

    assert(selected.type == CJ4_ACTION_DISCARD);
    assert(selected.tile == singleton);
}

static void
test_chiitoi_ctx_controls_give_up_timing(void)
{
    cj4_player_view view;
    cj4m_player_delegate early = cj4_opponent_chiitoi(0);
    cj4m_player_delegate stubborn = cj4_opponent_chiitoi(2);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_PASS, CJ4_PLAYER_1, CJ4_TILE_ID_INVALID),
        make_action(CJ4_ACTION_PON, CJ4_PLAYER_1, tile(CJ4_TILE_TYPE_HAKU, 0))};
    cj4_action early_selected;
    cj4_action stubborn_selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_1;
    view.dealer = CJ4_PLAYER_0;
    view.round_wind = CJ4_WIND_EAST;
    view.phase = CJ4_PHASE_DISCARD;
    test_view_set_discard_count(&view, 10);
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_HAKU, 1));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_HAKU, 2));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_3M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_3M, 1));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_7P, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_7P, 1));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_EAST, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_EAST, 1));

    early_selected = early.decide(early.ctx, &view, actions, 2);
    stubborn_selected = stubborn.decide(stubborn.ctx, &view, actions, 2);

    assert(early_selected.type == CJ4_ACTION_PON);
    assert(stubborn_selected.type == CJ4_ACTION_PASS);
}

static void
test_betaori_prefers_safe_discard_against_riichi(void)
{
    cj4_mahjong state = make_empty_state();
    cj4m_player_delegate delegate = cj4_opponent_betaori(1);
    const cj4_tile_id hand[] = {
        tile(0, 0), tile(1, 0), tile(2, 0), tile(3, 0)};
    cj4_player_view view;
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, hand[0]),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, hand[1]),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, hand[2])};
    cj4_action selected;

    set_hand(&state, CJ4_PLAYER_0, hand, 4);
    cj4_state_set_current_player(&state, CJ4_PLAYER_0);
    cj4_state_set_phase(&state, CJ4_PHASE_DRAW);
    state.draw_tile = hand[3];
    cj4_state_set_riichi(&state, CJ4_PLAYER_1, true);
    add_discard(&state, CJ4_PLAYER_1, tile(1, 1));

    view = cj4m_make_player_view(&state, CJ4_PLAYER_0);
    selected = delegate.decide(delegate.ctx, &view, actions, 3);

    assert(selected.type == CJ4_ACTION_DISCARD);
    assert(cj4_tile_get_type(selected.tile) == cj4_tile_get_type(hand[1]));
}

static void
test_betaori_counts_dora_indicators_as_visible_tiles(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_betaori(2);
    const cj4_tile_id first = tile(CJ4_TILE_TYPE_4M, 0);
    const cj4_tile_id visible = tile(CJ4_TILE_TYPE_5M, 0);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, first),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, visible)};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.phase = CJ4_PHASE_DRAW;
    view.is_riichi[CJ4_PLAYER_1] = 1;
    test_view_add_dora_indicator(&view, tile(CJ4_TILE_TYPE_5M, 1));

    selected = delegate.decide(delegate.ctx, &view, actions, 2);

    assert(selected.type == CJ4_ACTION_DISCARD);
    assert(selected.tile == visible);
}

static void
test_betaori_and_pinfu_decline_open_calls(void)
{
    cj4_player_view betaori_view;
    cj4_player_view pinfu_view;
    cj4m_player_delegate betaori = cj4_opponent_betaori(1);
    cj4m_player_delegate pinfu = cj4_opponent_pinfu(1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_PASS, CJ4_PLAYER_1, CJ4_TILE_ID_INVALID),
        make_action(CJ4_ACTION_PON, CJ4_PLAYER_1, tile(3, 0)),
        make_action(CJ4_ACTION_MINKAN, CJ4_PLAYER_1, tile(3, 0))};
    cj4_action betaori_selected;
    cj4_action pinfu_selected;

    memset(&betaori_view, 0, sizeof(betaori_view));
    betaori_view.player = CJ4_PLAYER_1;
    betaori_view.phase = CJ4_PHASE_DISCARD;

    memset(&pinfu_view, 0, sizeof(pinfu_view));
    pinfu_view.player = CJ4_PLAYER_1;
    pinfu_view.phase = CJ4_PHASE_DISCARD;

    betaori_selected = betaori.decide(betaori.ctx, &betaori_view, actions, 3);
    pinfu_selected = pinfu.decide(pinfu.ctx, &pinfu_view, actions, 3);

    assert(betaori_selected.type == CJ4_ACTION_PASS);
    assert(pinfu_selected.type == CJ4_ACTION_PASS);
}

static void
test_kokushi_declines_open_calls(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_kokushi(1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_PASS, CJ4_PLAYER_1, CJ4_TILE_ID_INVALID),
        make_action(CJ4_ACTION_CHI, CJ4_PLAYER_1, tile(CJ4_TILE_TYPE_1M, 0)),
        make_action(CJ4_ACTION_PON, CJ4_PLAYER_1, tile(CJ4_TILE_TYPE_EAST, 0))};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_1;
    view.phase = CJ4_PHASE_DISCARD;

    selected = delegate.decide(delegate.ctx, &view, actions, 3);

    assert(selected.type == CJ4_ACTION_PASS);
}

static void
test_kokushi_prefers_riichi_over_discard(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_kokushi(1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_5M, 0)),
        make_action(CJ4_ACTION_RIICHI, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_EAST, 0))};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.phase = CJ4_PHASE_DRAW;

    selected = delegate.decide(delegate.ctx, &view, actions, 2);

    assert(selected.type == CJ4_ACTION_RIICHI);
    assert(selected.tile == tile(CJ4_TILE_TYPE_EAST, 0));
}

static void
test_kokushi_discards_simple_tile_first(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_kokushi(1);
    const cj4_tile_id simple = tile(CJ4_TILE_TYPE_5M, 0);
    const cj4_tile_id orphan0 = tile(CJ4_TILE_TYPE_1M, 0);
    const cj4_tile_id orphan1 = tile(CJ4_TILE_TYPE_EAST, 0);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, orphan0),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, simple),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, orphan1)};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.phase = CJ4_PHASE_DRAW;
    test_view_add_hand(&view, simple);
    test_view_add_hand(&view, orphan0);
    test_view_add_hand(&view, orphan1);

    selected = delegate.decide(delegate.ctx, &view, actions, 3);

    assert(selected.type == CJ4_ACTION_DISCARD);
    assert(selected.tile == simple);
}

static void
test_kokushi_discards_duplicate_orphan_before_unique(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_kokushi(1);
    const cj4_tile_id unique = tile(CJ4_TILE_TYPE_1M, 0);
    const cj4_tile_id duplicate0 = tile(CJ4_TILE_TYPE_EAST, 0);
    const cj4_tile_id duplicate1 = tile(CJ4_TILE_TYPE_EAST, 1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, unique),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, duplicate0),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, duplicate1)};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.phase = CJ4_PHASE_DRAW;
    test_view_add_hand(&view, unique);
    test_view_add_hand(&view, duplicate0);
    test_view_add_hand(&view, duplicate1);

    selected = delegate.decide(delegate.ctx, &view, actions, 3);

    assert(selected.type == CJ4_ACTION_DISCARD);
    assert(cj4_tile_get_type(selected.tile) == CJ4_TILE_TYPE_EAST);
}

static void
test_pinfu_prefers_riichi_over_plain_discard(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_pinfu(1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(4, 0)),
        make_action(CJ4_ACTION_RIICHI, CJ4_PLAYER_0, tile(5, 0))};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.phase = CJ4_PHASE_DRAW;

    selected = delegate.decide(delegate.ctx, &view, actions, 2);

    assert(selected.type == CJ4_ACTION_RIICHI);
    assert(selected.tile == tile(5, 0));
}

static void
test_pinfu_prefers_honor_discard(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_pinfu(1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_4M, 0)),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_EAST, 0)),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_7P, 0))};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.phase = CJ4_PHASE_DRAW;

    selected = delegate.decide(delegate.ctx, &view, actions, 3);

    assert(selected.type == CJ4_ACTION_DISCARD);
    assert(cj4_tile_get_type(selected.tile) == CJ4_TILE_TYPE_EAST);
}

static void
test_somete_prefers_off_suit_discard_for_honitsu(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_somete(1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_7P, 0)),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_EAST, 0)),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_5M, 0))};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.dealer = CJ4_PLAYER_0;
    view.round_wind = CJ4_WIND_EAST;
    view.phase = CJ4_PHASE_DRAW;
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_1M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_2M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_3M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_5M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_6M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_7M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_EAST, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_EAST, 1));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_HAKU, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_7P, 0));

    selected = delegate.decide(delegate.ctx, &view, actions, 3);

    assert(selected.type == CJ4_ACTION_DISCARD);
    assert(selected.tile == tile(CJ4_TILE_TYPE_7P, 0));
}

static void
test_somete_prefers_tsuuiisou_path_from_opening_hand(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_somete(1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_1M, 0)),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_EAST, 0)),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_HAKU, 0))};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.dealer = CJ4_PLAYER_0;
    view.round_wind = CJ4_WIND_EAST;
    view.phase = CJ4_PHASE_DRAW;
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_EAST, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_EAST, 1));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_SOUTH, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_SOUTH, 1));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_WEST, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_NORTH, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_HAKU, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_HAKU, 1));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_HATSU, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_1M, 0));

    selected = delegate.decide(delegate.ctx, &view, actions, 3);

    assert(selected.type == CJ4_ACTION_DISCARD);
    assert(selected.tile == tile(CJ4_TILE_TYPE_1M, 0));
}

static void
test_somete_opens_target_pon_aggressively(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_somete(1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_PASS, CJ4_PLAYER_1, CJ4_TILE_ID_INVALID),
        make_action(CJ4_ACTION_PON, CJ4_PLAYER_1, tile(CJ4_TILE_TYPE_3M, 0))};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_1;
    view.dealer = CJ4_PLAYER_0;
    view.round_wind = CJ4_WIND_EAST;
    view.phase = CJ4_PHASE_DISCARD;
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_3M, 1));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_3M, 2));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_4M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_5M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_6M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_7M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_EAST, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_EAST, 1));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_9P, 0));

    selected = delegate.decide(delegate.ctx, &view, actions, 2);

    assert(selected.type == CJ4_ACTION_PON);
    assert(cj4_tile_get_type(selected.tile) == CJ4_TILE_TYPE_3M);
}

static void
test_somete_declines_off_route_open_call(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_somete(1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_PASS, CJ4_PLAYER_1, CJ4_TILE_ID_INVALID),
        make_action(CJ4_ACTION_PON, CJ4_PLAYER_1, tile(CJ4_TILE_TYPE_4P, 0))};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_1;
    view.dealer = CJ4_PLAYER_0;
    view.round_wind = CJ4_WIND_EAST;
    view.phase = CJ4_PHASE_DISCARD;
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_3M, 1));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_3M, 2));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_4M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_5M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_6M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_7M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_EAST, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_EAST, 1));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_4P, 1));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_4P, 2));

    selected = delegate.decide(delegate.ctx, &view, actions, 2);

    assert(selected.type == CJ4_ACTION_PASS);
}

static void
test_somete_prefers_riichi_over_discard(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_somete(1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_4M, 0)),
        make_action(CJ4_ACTION_RIICHI, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_7M, 0))};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.phase = CJ4_PHASE_DRAW;

    selected = delegate.decide(delegate.ctx, &view, actions, 2);

    assert(selected.type == CJ4_ACTION_RIICHI);
    assert(selected.tile == tile(CJ4_TILE_TYPE_7M, 0));
}

static void
test_toitoi_prefers_meld_when_available(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_toitoi(1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_PASS, CJ4_PLAYER_1, CJ4_TILE_ID_INVALID),
        make_action(CJ4_ACTION_PON, CJ4_PLAYER_1, tile(3, 0)),
        make_action(CJ4_ACTION_MINKAN, CJ4_PLAYER_1, tile(3, 0))};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_1;
    view.dealer = CJ4_PLAYER_0;
    view.round_wind = CJ4_WIND_EAST;
    view.phase = CJ4_PHASE_DRAW;
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_4M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_4M, 1));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_4M, 2));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_HAKU, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_HAKU, 1));

    selected = delegate.decide(delegate.ctx, &view, actions, 3);

    assert(selected.type == CJ4_ACTION_MINKAN);
}

static void
test_toitoi_prefers_ron_over_meld(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_toitoi(1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_PON, CJ4_PLAYER_1, tile(3, 0)),
        make_action(CJ4_ACTION_RON, CJ4_PLAYER_1, tile(3, 0))};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_1;
    view.phase = CJ4_PHASE_DRAW;

    selected = delegate.decide(delegate.ctx, &view, actions, 2);
    assert(selected.type == CJ4_ACTION_RON);
}

static void
test_toitoi_keeps_pair_over_singleton(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_toitoi(1);
    const cj4_tile_id singleton = tile(CJ4_TILE_TYPE_4M, 0);
    const cj4_tile_id pair0 = tile(CJ4_TILE_TYPE_7P, 0);
    const cj4_tile_id pair1 = tile(CJ4_TILE_TYPE_7P, 1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, pair0),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, singleton),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, pair1)};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.phase = CJ4_PHASE_DRAW;
    test_view_add_hand(&view, singleton);
    test_view_add_hand(&view, pair0);
    test_view_add_hand(&view, pair1);

    selected = delegate.decide(delegate.ctx, &view, actions, 3);

    assert(selected.type == CJ4_ACTION_DISCARD);
    assert(selected.tile == singleton);
}

static void
test_toitoi_prefers_riichi_over_discard(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_toitoi(1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_4M, 0)),
        make_action(CJ4_ACTION_RIICHI, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_7P, 0))};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.phase = CJ4_PHASE_DRAW;

    selected = delegate.decide(delegate.ctx, &view, actions, 2);

    assert(selected.type == CJ4_ACTION_RIICHI);
}

static void
test_betaori_ctx_zero_can_push_past_single_riichi(void)
{
    cj4_mahjong state = make_empty_state();
    cj4m_player_delegate cautious = cj4_opponent_betaori(2);
    cj4m_player_delegate loose = cj4_opponent_betaori(0);
    const cj4_tile_id hand[] = {
        tile(CJ4_TILE_TYPE_4M, 0), tile(CJ4_TILE_TYPE_5P, 0), tile(CJ4_TILE_TYPE_7S, 0)};
    cj4_player_view view;
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, hand[0]),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, hand[1])};
    cj4_action loose_selected;
    cj4_action cautious_selected;

    set_hand(&state, CJ4_PLAYER_0, hand, 3);
    cj4_state_set_current_player(&state, CJ4_PLAYER_0);
    cj4_state_set_phase(&state, CJ4_PHASE_DRAW);
    cj4_state_set_riichi(&state, CJ4_PLAYER_1, true);
    add_discard(&state, CJ4_PLAYER_1, tile(CJ4_TILE_TYPE_5P, 1));

    view = cj4m_make_player_view(&state, CJ4_PLAYER_0);
    loose_selected = loose.decide(loose.ctx, &view, actions, 2);
    cautious_selected = cautious.decide(cautious.ctx, &view, actions, 2);

    assert(loose_selected.tile == hand[0]);
    assert(cautious_selected.tile == hand[1]);
}

static void
test_kokushi_ctx_controls_yakuhai_fallback_calls(void)
{
    cj4_player_view view;
    cj4m_player_delegate early = cj4_opponent_kokushi(0);
    cj4m_player_delegate stubborn = cj4_opponent_kokushi(2);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_PASS, CJ4_PLAYER_1, CJ4_TILE_ID_INVALID),
        make_action(CJ4_ACTION_PON, CJ4_PLAYER_1, tile(CJ4_TILE_TYPE_HAKU, 0))};
    cj4_action early_selected;
    cj4_action stubborn_selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_1;
    view.dealer = CJ4_PLAYER_0;
    view.round_wind = CJ4_WIND_EAST;
    view.phase = CJ4_PHASE_DISCARD;
    test_view_set_discard_count(&view, 18);
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_1M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_9M, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_EAST, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_SOUTH, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_WEST, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_NORTH, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_HATSU, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_CHUN, 0));

    early_selected = early.decide(early.ctx, &view, actions, 2);
    stubborn_selected = stubborn.decide(stubborn.ctx, &view, actions, 2);

    assert(early_selected.type == CJ4_ACTION_PON);
    assert(stubborn_selected.type == CJ4_ACTION_PASS);
}

static void
test_pinfu_ctx_controls_push_fold_against_riichi(void)
{
    cj4_mahjong state = make_empty_state();
    cj4m_player_delegate folding = cj4_opponent_pinfu(0);
    cj4m_player_delegate pushing = cj4_opponent_pinfu(2);
    const cj4_tile_id hand[] = {
        tile(CJ4_TILE_TYPE_4M, 0), tile(CJ4_TILE_TYPE_5P, 0), tile(CJ4_TILE_TYPE_EAST, 0)};
    cj4_player_view view;
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, hand[0]),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, hand[1]),
        make_action(CJ4_ACTION_RIICHI, CJ4_PLAYER_0, hand[0])};
    cj4_action folding_selected;
    cj4_action pushing_selected;

    set_hand(&state, CJ4_PLAYER_0, hand, 3);
    cj4_state_set_current_player(&state, CJ4_PLAYER_0);
    cj4_state_set_phase(&state, CJ4_PHASE_DRAW);
    cj4_state_set_riichi(&state, CJ4_PLAYER_1, true);
    add_discard(&state, CJ4_PLAYER_1, tile(CJ4_TILE_TYPE_5P, 1));

    view = cj4m_make_player_view(&state, CJ4_PLAYER_0);
    folding_selected = folding.decide(folding.ctx, &view, actions, 3);
    pushing_selected = pushing.decide(pushing.ctx, &view, actions, 3);

    assert(folding_selected.type == CJ4_ACTION_DISCARD);
    assert(folding_selected.tile == hand[1]);
    assert(pushing_selected.type == CJ4_ACTION_RIICHI);
}

static void
test_tanyao_prefers_yaochu_discard(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_tanyao(1);
    const cj4_tile_id simple = tile(CJ4_TILE_TYPE_5M, 0);
    const cj4_tile_id terminal = tile(CJ4_TILE_TYPE_1P, 0);
    const cj4_tile_id honor = tile(CJ4_TILE_TYPE_EAST, 0);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, simple),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, terminal),
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, honor)};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.dealer = CJ4_PLAYER_0;
    view.round_wind = CJ4_WIND_EAST;
    view.phase = CJ4_PHASE_DRAW;
    test_view_add_hand(&view, simple);
    test_view_add_hand(&view, terminal);
    test_view_add_hand(&view, honor);

    selected = delegate.decide(delegate.ctx, &view, actions, 3);

    assert(selected.type == CJ4_ACTION_DISCARD);
    assert(selected.tile == honor);
}

static void
test_tanyao_declines_chi_with_terminal(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_tanyao(2);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_PASS, CJ4_PLAYER_0, CJ4_TILE_ID_INVALID),
        make_action(CJ4_ACTION_CHI, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_2M, 0))};
    cj4_action selected;

    actions[1].tiles[0] = tile(CJ4_TILE_TYPE_1M, 0);
    actions[1].tiles[1] = tile(CJ4_TILE_TYPE_3M, 0);
    actions[1].tile_count = 2;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.phase = CJ4_PHASE_DISCARD;
    test_view_add_hand(&view, actions[1].tiles[0]);
    test_view_add_hand(&view, actions[1].tiles[1]);

    selected = delegate.decide(delegate.ctx, &view, actions, 2);

    assert(selected.type == CJ4_ACTION_PASS);
}

static void
test_tanyao_ctx_controls_early_opening(void)
{
    cj4_player_view view;
    cj4m_player_delegate closed = cj4_opponent_tanyao(0);
    cj4m_player_delegate fast = cj4_opponent_tanyao(2);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_PASS, CJ4_PLAYER_0, CJ4_TILE_ID_INVALID),
        make_action(CJ4_ACTION_CHI, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_4M, 0))};
    cj4_action closed_selected;
    cj4_action fast_selected;

    actions[1].tiles[0] = tile(CJ4_TILE_TYPE_3M, 0);
    actions[1].tiles[1] = tile(CJ4_TILE_TYPE_5M, 0);
    actions[1].tile_count = 2;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.phase = CJ4_PHASE_DISCARD;
    test_view_add_hand(&view, actions[1].tiles[0]);
    test_view_add_hand(&view, actions[1].tiles[1]);
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_6P, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_7P, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_1S, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_9S, 0));

    closed_selected = closed.decide(closed.ctx, &view, actions, 2);
    fast_selected = fast.decide(fast.ctx, &view, actions, 2);

    assert(closed_selected.type == CJ4_ACTION_PASS);
    assert(fast_selected.type == CJ4_ACTION_CHI);
}

static void
test_tanyao_prefers_win_over_open_call(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_tanyao(2);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_PON, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_5P, 0)),
        make_action(CJ4_ACTION_RON, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_5P, 0))};
    cj4_action selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.phase = CJ4_PHASE_DISCARD;

    selected = delegate.decide(delegate.ctx, &view, actions, 2);

    assert(selected.type == CJ4_ACTION_RON);
}

static void
test_tanyao_accepts_simple_ankan(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_tanyao(1);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_8S, 0)),
        make_action(CJ4_ACTION_ANKAN, CJ4_PLAYER_0, CJ4_TILE_ID_INVALID)};
    cj4_action selected;

    for (uint8_t i = 0; i < CJ4_TILE_PER_TYPE; ++i)
        actions[1].tiles[i] = tile(CJ4_TILE_TYPE_5S, i);
    actions[1].tile_count = CJ4_TILE_PER_TYPE;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.phase = CJ4_PHASE_DRAW;

    selected = delegate.decide(delegate.ctx, &view, actions, 2);

    assert(selected.type == CJ4_ACTION_ANKAN);
}

static void
test_toitoi_ctx_controls_early_opening(void)
{
    cj4_player_view view;
    cj4m_player_delegate late = cj4_opponent_toitoi(0);
    cj4m_player_delegate early = cj4_opponent_toitoi(2);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_PASS, CJ4_PLAYER_0, CJ4_TILE_ID_INVALID),
        make_action(CJ4_ACTION_PON, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_3M, 0))};
    cj4_action late_selected;
    cj4_action early_selected;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.dealer = CJ4_PLAYER_0;
    view.round_wind = CJ4_WIND_EAST;
    view.phase = CJ4_PHASE_DISCARD;
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_3M, 1));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_3M, 2));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_HAKU, 0));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_HAKU, 1));
    test_view_add_hand(&view, tile(CJ4_TILE_TYPE_7P, 0));

    late_selected = late.decide(late.ctx, &view, actions, 2);
    early_selected = early.decide(early.ctx, &view, actions, 2);

    assert(late_selected.type == CJ4_ACTION_PASS);
    assert(early_selected.type == CJ4_ACTION_PON);
}

static void
test_ctx_one_matches_current_factory_baseline(void)
{
    cj4_player_view view;
    cj4m_player_delegate delegate = cj4_opponent_pinfu(99);
    cj4_action actions[] = {
        make_action(CJ4_ACTION_DISCARD, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_4M, 0)),
        make_action(CJ4_ACTION_RIICHI, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_7P, 0))};
    cj4_action first;
    cj4_action second;

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.phase = CJ4_PHASE_DRAW;

    first = delegate.decide(delegate.ctx, &view, actions, 2);
    second = delegate.decide(delegate.ctx, &view, actions, 2);

    assert(first.type == CJ4_ACTION_RIICHI);
    assert(second.type == CJ4_ACTION_RIICHI);
    assert(first.tile == second.tile);
}

static void
test_all_opponents_choose_rinshan_tsumo_before_kan_dora_reveal(void)
{
    cj4m_player_delegate (*factories[])(int) = {
        cj4_opponent_betaori,
        cj4_opponent_chanta,
        cj4_opponent_chiitoi,
        cj4_opponent_kokushi,
        cj4_opponent_pinfu,
        cj4_opponent_somete,
        cj4_opponent_tanyao,
        cj4_opponent_toitoi};
    cj4_player_view view;
    cj4_action actions[] = {
        make_action(CJ4_ACTION_PASS, CJ4_PLAYER_0, CJ4_TILE_ID_INVALID),
        make_action(CJ4_ACTION_TSUMO, CJ4_PLAYER_0, tile(CJ4_TILE_TYPE_5M, 0))};

    test_init_player_view(&view);
    view.player = CJ4_PLAYER_0;
    view.phase = CJ4_PHASE_DRAW;
    test_view_add_dora_indicator(&view, tile(CJ4_TILE_TYPE_EAST, 0));

    for (uint8_t i = 0;
         i < (uint8_t)(sizeof(factories) / sizeof(factories[0]));
         ++i)
    {
        cj4m_player_delegate delegate = factories[i](1);
        cj4_action selected = delegate.decide(delegate.ctx, &view, actions, 2);

        assert(selected.type == CJ4_ACTION_TSUMO);
    }
}

int
main(void)
{
    test_chanta_discards_middle_tile_first();
    test_chanta_ctx_controls_early_opening();
    test_chiitoi_keeps_pair_over_singleton();
    test_chiitoi_ctx_controls_give_up_timing();
    test_betaori_prefers_safe_discard_against_riichi();
    test_betaori_counts_dora_indicators_as_visible_tiles();
    test_betaori_ctx_zero_can_push_past_single_riichi();
    test_betaori_and_pinfu_decline_open_calls();
    test_kokushi_declines_open_calls();
    test_kokushi_prefers_riichi_over_discard();
    test_kokushi_discards_simple_tile_first();
    test_kokushi_discards_duplicate_orphan_before_unique();
    test_kokushi_ctx_controls_yakuhai_fallback_calls();
    test_pinfu_prefers_riichi_over_plain_discard();
    test_pinfu_prefers_honor_discard();
    test_somete_prefers_off_suit_discard_for_honitsu();
    test_somete_prefers_tsuuiisou_path_from_opening_hand();
    test_somete_opens_target_pon_aggressively();
    test_somete_declines_off_route_open_call();
    test_somete_prefers_riichi_over_discard();
    test_pinfu_ctx_controls_push_fold_against_riichi();
    test_tanyao_prefers_yaochu_discard();
    test_tanyao_declines_chi_with_terminal();
    test_tanyao_ctx_controls_early_opening();
    test_tanyao_prefers_win_over_open_call();
    test_tanyao_accepts_simple_ankan();
    test_toitoi_prefers_meld_when_available();
    test_toitoi_prefers_ron_over_meld();
    test_toitoi_keeps_pair_over_singleton();
    test_toitoi_prefers_riichi_over_discard();
    test_toitoi_ctx_controls_early_opening();
    test_ctx_one_matches_current_factory_baseline();
    test_all_opponents_choose_rinshan_tsumo_before_kan_dora_reveal();
    return 0;
}
