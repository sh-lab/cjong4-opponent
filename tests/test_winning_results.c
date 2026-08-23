#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "render.h"
#include "cjong4/core/state.h"
#include "cjong4/core/state_query.h"
#include "cjong4/core/state_ron.h"
#include "cjong4/core/state_settle.h"
#include "cjong4/core/state_tsumo.h"
#include "cjong4/core/tile.h"
#include "cjong4/core/tile_const.h"
#include "cjong4/core/wind.h"

#include "test_state_helpers.h"

static cj4_tile_id
tile(cj4_tile_type type, uint8_t index)
{
    return cj4_tile_make(type, index);
}

static const uint8_t test_dora_indicator_indices[CJ4_MAX_DORA_INDICATORS] = {
    130, 128, 126, 124, 122};

static const uint8_t test_ura_dora_indicator_indices[CJ4_MAX_DORA_INDICATORS] = {
    131, 129, 127, 125, 123};

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

static void
set_indicator_tile(
    cj4_mahjong *state,
    uint8_t slot,
    cj4_tile_id indicator,
    uint8_t is_ura)
{
    uint8_t position;

    assert(slot < CJ4_MAX_DORA_INDICATORS);
    position = is_ura ? test_ura_dora_indicator_indices[slot]
                      : test_dora_indicator_indices[slot];

    for (uint16_t tile_id = 0; tile_id < CJ4_TILE_ID_COUNT; ++tile_id)
    {
        if (state->locations[tile_id].wall == position)
            state->locations[tile_id].wall = CJ4_LOCATION_NONE;
    }
    state->locations[indicator].wall = position;
}

static uint8_t
contains_win_yaku(
    const cj4_win_result *result,
    cj4_win_yaku yaku)
{
    for (uint8_t i = 0; i < result->yaku_count; ++i)
    {
        if (result->yaku[i] == yaku)
            return 1;
    }

    return 0;
}

static bool
collect_winning_results(
    const cj4_mahjong *state,
    const cj4_rules *rules,
    cj4_win_result *out_results,
    uint8_t capacity,
    uint8_t *out_count)
{
    cj4_mahjong normalized;

    if (out_count)
        *out_count = 0;

    if (!state)
        return false;

    normalized = *state;
    if (cj4_state_phase(&normalized) == CJ4_PHASE_SETTLE)
        cj4_state_set_phase(&normalized, CJ4_PHASE_ROUND_END);

    if (cj4_state_phase(&normalized) != CJ4_PHASE_ROUND_END)
        return false;

    return cj4_collect_winning_results(
        &normalized,
        rules,
        out_results,
        capacity,
        out_count);
}

static void
test_collect_winning_results_from_round_end_tsumo(void)
{
    cj4_rules rules = {0};
    cj4_mahjong state = make_empty_state();
    cj4_mahjong won;
    cj4_win_result results[CJ4_PLAYER_COUNT];
    uint8_t result_count = 0;
    cj4_tile_id draw = tile(5, 0);
    const cj4_tile_id hand[] = {
        tile(0, 0), tile(1, 0), tile(2, 0),
        tile(9, 0), tile(10, 0), tile(11, 0),
        tile(18, 0), tile(19, 0), tile(20, 0),
        tile(3, 0), tile(4, 0),
        tile(15, 0), tile(15, 1),
        draw};

    set_hand(&state, CJ4_PLAYER_0, hand, (uint8_t)(sizeof(hand) / sizeof(hand[0])));
    cj4_state_set_current_player(&state, CJ4_PLAYER_0);
    state.dealer = CJ4_PLAYER_1;
    state.draw_tile = draw;
    cj4_state_set_phase(&state, CJ4_PHASE_DRAW);
    cj4_state_set_riichi(&state, CJ4_PLAYER_0, true);
    cj4_state_set_double_riichi(&state, CJ4_PLAYER_0, true);

    won = cj4_do_tsumo(state);

    assert(collect_winning_results(
        &won,
        &rules,
        results,
        CJ4_PLAYER_COUNT,
        &result_count));
    assert(result_count == 1);
    assert(results[0].player == CJ4_PLAYER_0);
    assert(results[0].han == 6);
    assert(results[0].fu == 20);
    assert(contains_win_yaku(&results[0], CJ4_WIN_YAKU_DOUBLE_RIICHI));
    assert(contains_win_yaku(&results[0], CJ4_WIN_YAKU_MENZEN_TSUMO));
    assert(contains_win_yaku(&results[0], CJ4_WIN_YAKU_PINFU));
}

static void
test_collect_winning_results_from_settle_ron(void)
{
    cj4_rules rules = {0};
    cj4_mahjong state = make_empty_state();
    cj4_mahjong won;
    cj4_mahjong settled;
    cj4_win_result results[CJ4_PLAYER_COUNT];
    uint8_t result_count = 0;
    cj4_player winners[] = {CJ4_PLAYER_2};
    const cj4_tile_id hand[] = {
        tile(1, 0), tile(2, 0), tile(3, 0),
        tile(10, 0), tile(11, 0), tile(12, 0),
        tile(19, 0), tile(20, 0), tile(21, 0),
        tile(4, 1), tile(5, 1),
        tile(14, 0), tile(14, 1)};

    rules.kuitan = 1;

    set_hand(&state, CJ4_PLAYER_2, hand, (uint8_t)(sizeof(hand) / sizeof(hand[0])));
    cj4_state_set_phase(&state, CJ4_PHASE_DISCARD);
    cj4_state_set_current_player(&state, CJ4_PLAYER_0);
    add_discard(&state, CJ4_PLAYER_0, tile(6, 0));

    won = cj4_do_ron_multi(state, winners, 1, &rules);
    settled = cj4_do_settle(won, &rules);

    assert(collect_winning_results(
        &settled,
        &rules,
        results,
        CJ4_PLAYER_COUNT,
        &result_count));
    assert(result_count == 1);
    assert(results[0].player == CJ4_PLAYER_2);
    assert(results[0].han == 4);
    assert(results[0].fu == 30);
    assert(results[0].ron_points == 7700);
    assert(contains_win_yaku(&results[0], CJ4_WIN_YAKU_TANYAO));
    assert(contains_win_yaku(&results[0], CJ4_WIN_YAKU_PINFU));
}

static void
test_collect_winning_results_from_settle_multi_ron(void)
{
    cj4_rules rules = {0};
    cj4_mahjong state = make_empty_state();
    cj4_mahjong won;
    cj4_mahjong settled;
    cj4_win_result results[CJ4_PLAYER_COUNT];
    uint8_t result_count = 0;
    cj4_player winners[] = {CJ4_PLAYER_3, CJ4_PLAYER_2};
    const cj4_tile_id ron_hand[] = {
        tile(1, 0), tile(2, 0), tile(3, 0),
        tile(10, 0), tile(11, 0), tile(12, 0),
        tile(19, 0), tile(20, 0), tile(21, 0),
        tile(4, 1), tile(5, 1),
        tile(14, 0), tile(14, 1)};
    const cj4_tile_id ron_hand_alt[] = {
        tile(1, 1), tile(2, 1), tile(3, 1),
        tile(10, 1), tile(11, 1), tile(12, 1),
        tile(19, 1), tile(20, 1), tile(21, 1),
        tile(4, 2), tile(5, 2),
        tile(14, 2), tile(14, 3)};

    rules.kuitan = 1;

    set_hand(&state, CJ4_PLAYER_2, ron_hand, (uint8_t)(sizeof(ron_hand) / sizeof(ron_hand[0])));
    set_hand(&state, CJ4_PLAYER_3, ron_hand_alt, (uint8_t)(sizeof(ron_hand_alt) / sizeof(ron_hand_alt[0])));
    cj4_state_set_phase(&state, CJ4_PHASE_DISCARD);
    cj4_state_set_current_player(&state, CJ4_PLAYER_0);
    add_discard(&state, CJ4_PLAYER_0, tile(6, 0));

    won = cj4_do_ron_multi(state, winners, 2, &rules);
    settled = cj4_do_settle(won, &rules);

    assert(collect_winning_results(
        &settled,
        &rules,
        results,
        CJ4_PLAYER_COUNT,
        &result_count));
    assert(result_count == 2);
    assert(results[0].player == CJ4_PLAYER_2);
    assert(results[1].player == CJ4_PLAYER_3);
    assert(results[0].han == 4);
    assert(results[1].han == 4);
    assert(contains_win_yaku(&results[0], CJ4_WIN_YAKU_TANYAO));
    assert(contains_win_yaku(&results[1], CJ4_WIN_YAKU_TANYAO));
}

static void
test_render_state_shows_visible_dora_indicators(void)
{
    cj4_rules rules = {0};
    cj4_mahjong state = make_empty_state();
    FILE *out;
    char buffer[4096];
    size_t size;

    state.dora_count = 2;
    set_indicator_tile(&state, 0, tile(CJ4_TILE_TYPE_8M, 0), 0);
    set_indicator_tile(&state, 1, tile(CJ4_TILE_TYPE_EAST, 0), 0);

    out = tmpfile();
    assert(out);

    cj4_render_state(out, &state, &rules);
    fflush(out);
    rewind(out);

    size = fread(buffer, 1, sizeof(buffer) - 1, out);
    buffer[size] = '\0';
    fclose(out);

    assert(strstr(buffer, "Dora Indicators: 8m E"));
}

static void
test_render_state_prints_human_readable_winning_results(void)
{
    cj4_rules rules = {0};
    cj4_mahjong state = make_empty_state();
    cj4_mahjong won;
    cj4_mahjong settled;
    FILE *out;
    char buffer[4096];
    size_t size;
    cj4_tile_id draw = tile(5, 0);
    const cj4_tile_id hand[] = {
        tile(0, 0), tile(1, 0), tile(2, 0),
        tile(9, 0), tile(10, 0), tile(11, 0),
        tile(18, 0), tile(19, 0), tile(20, 0),
        tile(3, 0), tile(4, 0),
        tile(15, 0), tile(15, 1),
        draw};

    set_hand(&state, CJ4_PLAYER_0, hand, (uint8_t)(sizeof(hand) / sizeof(hand[0])));
    cj4_state_set_current_player(&state, CJ4_PLAYER_0);
    state.dealer = CJ4_PLAYER_1;
    state.draw_tile = draw;
    cj4_state_set_phase(&state, CJ4_PHASE_DRAW);
    cj4_state_set_riichi(&state, CJ4_PLAYER_0, true);
    cj4_state_set_double_riichi(&state, CJ4_PLAYER_0, true);
    state.dora_count = 1;
    set_indicator_tile(&state, 0, tile(CJ4_TILE_TYPE_8M, 0), 0);
    set_indicator_tile(&state, 0, tile(CJ4_TILE_TYPE_8P, 0), 1);

    won = cj4_do_tsumo(state);
    settled = cj4_do_settle(won, &rules);

    out = tmpfile();
    assert(out);

    cj4_render_state(out, &settled, &rules);
    fflush(out);
    rewind(out);

    size = fread(buffer, 1, sizeof(buffer) - 1, out);
    buffer[size] = '\0';
    fclose(out);

    assert(strstr(buffer, "Winning Results:"));
    assert(strstr(buffer, "Dora Indicators: 8m"));
    assert(strstr(buffer, "Ura-Dora Indicators: 8p"));
    assert(strstr(buffer, "Player0: 6 han / 20 fu"));
    assert(strstr(buffer, "Double Riichi"));
    assert(strstr(buffer, "Menzen Tsumo"));
    assert(strstr(buffer, "Pinfu"));
    assert(strstr(buffer, "Hand: 1m 2m 3m 4m 5m 1p 2p 3p 7p 7p 1s 2s 3s / Draw: 6m"));
}

static void
test_render_state_shows_ura_dora_after_round_end(void)
{
    cj4_rules rules = {0};
    cj4_mahjong state = make_empty_state();
    cj4_mahjong won;
    FILE *out;
    char buffer[4096];
    size_t size;
    cj4_tile_id draw = tile(5, 0);
    const cj4_tile_id hand[] = {
        tile(0, 0), tile(1, 0), tile(2, 0),
        tile(9, 0), tile(10, 0), tile(11, 0),
        tile(18, 0), tile(19, 0), tile(20, 0),
        tile(3, 0), tile(4, 0),
        tile(15, 0), tile(15, 1),
        draw};

    set_hand(&state, CJ4_PLAYER_0, hand, (uint8_t)(sizeof(hand) / sizeof(hand[0])));
    cj4_state_set_current_player(&state, CJ4_PLAYER_0);
    state.dealer = CJ4_PLAYER_1;
    state.draw_tile = draw;
    cj4_state_set_phase(&state, CJ4_PHASE_DRAW);
    cj4_state_set_riichi(&state, CJ4_PLAYER_0, true);
    cj4_state_set_double_riichi(&state, CJ4_PLAYER_0, true);
    state.dora_count = 1;
    set_indicator_tile(&state, 0, tile(CJ4_TILE_TYPE_8M, 0), 0);
    set_indicator_tile(&state, 0, tile(CJ4_TILE_TYPE_8P, 0), 1);

    won = cj4_do_tsumo(state);

    out = tmpfile();
    assert(out);

    cj4_render_state(out, &won, &rules);
    fflush(out);
    rewind(out);

    size = fread(buffer, 1, sizeof(buffer) - 1, out);
    buffer[size] = '\0';
    fclose(out);

    assert(strstr(buffer, "Dora Indicators: 8m"));
    assert(strstr(buffer, "Ura-Dora Indicators: 8p"));
}

static void
test_render_state_hides_ura_dora_without_riichi(void)
{
    cj4_rules rules = {0};
    cj4_mahjong state = make_empty_state();
    cj4_mahjong won;
    cj4_mahjong settled;
    FILE *out;
    char buffer[4096];
    size_t size;
    cj4_player winners[] = {CJ4_PLAYER_2};
    const cj4_tile_id hand[] = {
        tile(1, 0), tile(2, 0), tile(3, 0),
        tile(10, 0), tile(11, 0), tile(12, 0),
        tile(19, 0), tile(20, 0), tile(21, 0),
        tile(4, 1), tile(5, 1),
        tile(14, 0), tile(14, 1)};

    rules.kuitan = 1;

    set_hand(&state, CJ4_PLAYER_2, hand, (uint8_t)(sizeof(hand) / sizeof(hand[0])));
    cj4_state_set_phase(&state, CJ4_PHASE_DISCARD);
    cj4_state_set_current_player(&state, CJ4_PLAYER_0);
    state.dora_count = 1;
    set_indicator_tile(&state, 0, tile(CJ4_TILE_TYPE_8M, 0), 0);
    set_indicator_tile(&state, 0, tile(CJ4_TILE_TYPE_8P, 0), 1);
    add_discard(&state, CJ4_PLAYER_0, tile(6, 0));

    won = cj4_do_ron_multi(state, winners, 1, &rules);
    settled = cj4_do_settle(won, &rules);

    out = tmpfile();
    assert(out);

    cj4_render_state(out, &settled, &rules);
    fflush(out);
    rewind(out);

    size = fread(buffer, 1, sizeof(buffer) - 1, out);
    buffer[size] = '\0';
    fclose(out);

    assert(strstr(buffer, "Player2: 4 han / 30 fu"));
    assert(strstr(buffer, "Dora Indicators: 8m"));
    assert(!strstr(buffer, "Ura-Dora Indicators:"));
}

static void
test_render_state_sorts_hand_but_keeps_draw_separate(void)
{
    cj4_rules rules = {0};
    cj4_mahjong state = make_empty_state();
    FILE *out;
    char buffer[4096];
    size_t size;
    const cj4_tile_id hand[] = {
        tile(CJ4_TILE_TYPE_9S, 0),
        tile(CJ4_TILE_TYPE_1P, 0),
        tile(CJ4_TILE_TYPE_EAST, 0),
        tile(CJ4_TILE_TYPE_3M, 0),
        tile(CJ4_TILE_TYPE_1M, 0),
        tile(CJ4_TILE_TYPE_2S, 0),
        tile(CJ4_TILE_TYPE_9M, 0),
        tile(CJ4_TILE_TYPE_5P, 0),
        tile(CJ4_TILE_TYPE_HATSU, 0),
        tile(CJ4_TILE_TYPE_1S, 0),
        tile(CJ4_TILE_TYPE_2M, 0),
        tile(CJ4_TILE_TYPE_4P, 0),
        tile(CJ4_TILE_TYPE_HAKU, 0),
        tile(CJ4_TILE_TYPE_7P, 0)};

    set_hand(&state, CJ4_PLAYER_0, hand, (uint8_t)(sizeof(hand) / sizeof(hand[0])));
    cj4_state_set_current_player(&state, CJ4_PLAYER_0);
    cj4_state_set_phase(&state, CJ4_PHASE_DRAW);
    state.draw_tile = tile(CJ4_TILE_TYPE_7P, 0);

    out = tmpfile();
    assert(out);

    cj4_render_state(out, &state, &rules);
    fflush(out);
    rewind(out);

    size = fread(buffer, 1, sizeof(buffer) - 1, out);
    buffer[size] = '\0';
    fclose(out);

    assert(strstr(buffer, "Hand: 1m 2m 3m 9m 1p 4p 5p 1s 2s 9s E P F / Draw: 7p"));
}

static void
test_update_stats_for_settle_counts_ron_and_deal_in(void)
{
    cj4_rules rules = {0};
    cj4_cli_stats stats = {0};
    cj4_cli_round_snapshot snapshot = {0};
    cj4_mahjong state = make_empty_state();
    cj4_mahjong won;
    cj4_mahjong settled;
    cj4_player winners[] = {CJ4_PLAYER_2};
    const cj4_tile_id hand[] = {
        tile(1, 0), tile(2, 0), tile(3, 0),
        tile(10, 0), tile(11, 0), tile(12, 0),
        tile(19, 0), tile(20, 0), tile(21, 0),
        tile(4, 1), tile(5, 1),
        tile(14, 0), tile(14, 1)};

    rules.kuitan = 1;

    set_hand(&state, CJ4_PLAYER_2, hand, (uint8_t)(sizeof(hand) / sizeof(hand[0])));
    cj4_state_set_phase(&state, CJ4_PHASE_DISCARD);
    cj4_state_set_current_player(&state, CJ4_PLAYER_0);
    cj4_state_set_riichi(&state, CJ4_PLAYER_1, true);
    add_discard(&state, CJ4_PLAYER_0, tile(6, 0));

    won = cj4_do_ron_multi(state, winners, 1, &rules);
    settled = cj4_do_settle(won, &rules);

    cj4_update_stats_for_settle(&stats, &settled, &rules, &snapshot);

    assert(stats.rounds == 1);
    assert(stats.ron_wins[CJ4_PLAYER_2] == 1);
    assert(stats.deal_in_rounds[CJ4_PLAYER_0] == 1);
    assert(stats.riichi_rounds[CJ4_PLAYER_1] == 1);
    assert(stats.exhaustive_draw_rounds == 0);
}

static void
test_update_stats_for_settle_counts_noten_from_score_delta(void)
{
    cj4_rules rules = {0};
    cj4_cli_stats stats = {0};
    cj4_cli_round_snapshot snapshot = {0};
    cj4_mahjong round_end = make_empty_state();
    cj4_mahjong settled;

    cj4_state_set_phase(&round_end, CJ4_PHASE_ROUND_END);
    cj4_state_set_round_result(
        &round_end,
        CJ4_ROUND_END_EXHAUSTIVE_DRAW,
        CJ4_ABORTIVE_DRAW_NONE);

    cj4_capture_round_snapshot(&snapshot, &round_end);

    settled = round_end;
    cj4_state_set_phase(&settled, CJ4_PHASE_SETTLE);
    settled.scores[CJ4_PLAYER_0] = 28000;
    settled.scores[CJ4_PLAYER_1] = 28000;
    settled.scores[CJ4_PLAYER_2] = 22000;
    settled.scores[CJ4_PLAYER_3] = 22000;

    cj4_update_stats_for_settle(&stats, &settled, &rules, &snapshot);

    assert(stats.rounds == 1);
    assert(stats.exhaustive_draw_rounds == 1);
    assert(stats.noten_rounds[CJ4_PLAYER_0] == 0);
    assert(stats.noten_rounds[CJ4_PLAYER_1] == 0);
    assert(stats.noten_rounds[CJ4_PLAYER_2] == 1);
    assert(stats.noten_rounds[CJ4_PLAYER_3] == 1);
}

static void
test_update_stats_for_settle_ignores_abortive_draw_for_noten(void)
{
    cj4_rules rules = {0};
    cj4_cli_stats stats = {0};
    cj4_cli_round_snapshot snapshot = {0};
    cj4_mahjong settled = make_empty_state();

    cj4_state_set_phase(&settled, CJ4_PHASE_SETTLE);
    cj4_state_set_round_result(
        &settled,
        CJ4_ROUND_END_ABORTIVE_DRAW,
        CJ4_ABORTIVE_DRAW_NONE);

    cj4_update_stats_for_settle(&stats, &settled, &rules, &snapshot);

    assert(stats.rounds == 1);
    assert(stats.exhaustive_draw_rounds == 0);
    assert(stats.noten_rounds[CJ4_PLAYER_0] == 0);
    assert(stats.noten_rounds[CJ4_PLAYER_1] == 0);
    assert(stats.noten_rounds[CJ4_PLAYER_2] == 0);
    assert(stats.noten_rounds[CJ4_PLAYER_3] == 0);
}

int
main(void)
{
    test_collect_winning_results_from_round_end_tsumo();
    test_collect_winning_results_from_settle_ron();
    test_collect_winning_results_from_settle_multi_ron();
    test_render_state_shows_visible_dora_indicators();
    test_render_state_prints_human_readable_winning_results();
    test_render_state_shows_ura_dora_after_round_end();
    test_render_state_hides_ura_dora_without_riichi();
    test_render_state_sorts_hand_but_keeps_draw_separate();
    test_update_stats_for_settle_counts_ron_and_deal_in();
    test_update_stats_for_settle_counts_noten_from_score_delta();
    test_update_stats_for_settle_ignores_abortive_draw_for_noten();
    return 0;
}
