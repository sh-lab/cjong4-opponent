#ifndef CJ4_EXAMPLES_CLI_RENDER_H
#define CJ4_EXAMPLES_CLI_RENDER_H

#include <stdio.h>

#include "cjong4/core/rules.h"
#include "cjong4/core/state.h"

typedef struct
{
    uint32_t games;
    uint32_t rounds;
    uint32_t exhaustive_draw_rounds;
    uint32_t tsumo_wins[CJ4_PLAYER_COUNT];
    uint32_t ron_wins[CJ4_PLAYER_COUNT];
    uint32_t riichi_rounds[CJ4_PLAYER_COUNT];
    uint32_t deal_in_rounds[CJ4_PLAYER_COUNT];
    uint32_t noten_rounds[CJ4_PLAYER_COUNT];
} cj4_cli_stats;

typedef struct
{
    uint8_t has_exhaustive_draw_scores;
    int32_t round_end_scores[CJ4_PLAYER_COUNT];
} cj4_cli_round_snapshot;

const char *
cj4_tile_to_string(cj4_tile_id tile);

void
cj4_render_state(FILE *out, const cj4_mahjong *state, const cj4_rules *rules);

void
cj4_capture_round_snapshot(
    cj4_cli_round_snapshot *snapshot,
    const cj4_mahjong *state);

void
cj4_update_stats_for_settle(
    cj4_cli_stats *stats,
    const cj4_mahjong *state,
    const cj4_rules *rules,
    cj4_cli_round_snapshot *snapshot);

void
cj4_print_stats_summary(FILE *out, const cj4_cli_stats *stats);

#endif /* CJ4_EXAMPLES_CLI_RENDER_H */
