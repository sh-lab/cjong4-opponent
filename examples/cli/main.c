#include "render.h"

#include "cjong4/core/rules.h"
#include "cjong4/core/state_init.h"
#include "cjong4/core/state_query.h"
#include "cjong4/core/state_round.h"
#include "cjong4/core/tile.h"
#include "cjong4/manager/manager.h"
#include "cjong4/manager/player_view.h"
#include "cjong4/opponent/opponent_betaori.h"
#include "cjong4/opponent/opponent_kokushi.h"
#include "cjong4/opponent/opponent_pinfu.h"
#include "cjong4/opponent/opponent_somete.h"
#include "cjong4/opponent/opponent_toitoi.h"
#include "cjong4/opponent/opponent_chiitoi.h"
#include "cjong4/opponent/opponent_chanta.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *const cj4_honor_tile_names[] = {
    "E", "S", "W", "N", "P", "F", "C"};
static const uint8_t cj4_dora_indicator_indices[] = {
    130, 128, 126, 124, 122};

#ifndef CJ4_EXAMPLES_CLI_NO_MAIN
static void
fill_wall(cj4_tile_id wall[CJ4_TILE_ID_COUNT])
{
    for (uint8_t i = 0; i < CJ4_TILE_ID_COUNT; ++i)
        wall[i] = i;

    for (int i = CJ4_TILE_ID_COUNT - 1; i > 0; --i)
    {
        int j = rand() % (i + 1);
        cj4_tile_id tmp = wall[i];
        wall[i] = wall[j];
        wall[j] = tmp;
    }
}

static cj4_rules
make_rules(void)
{
    cj4_rules rules = {0};

    rules.initial_score = 25000;
    rules.target_score = 30000;
    rules.game_type = CJ4_GAME_TONPUU;
    rules.tobi_end = 1;
    rules.kuitan = 1;
    rules.ippatsu = 1;
    rules.max_ron_players = 3;
    rules.noten_penalty = 1;
    rules.noten_penalty_points = 3000;

    return rules;
}

static int
should_render_step(const cj4_mahjong *state, int show_all_steps)
{
    return show_all_steps || cj4_state_phase(state) == CJ4_PHASE_SETTLE;
}

static void
init_default_delegates(cj4m_player_delegate delegates[CJ4_PLAYER_COUNT])
{
    delegates[0] = cj4_opponent_chiitoi(1);
    delegates[1] = cj4_opponent_toitoi(1);
    delegates[2] = cj4_opponent_kokushi(1);
    delegates[3] = cj4_opponent_pinfu(1);
}
#endif

static double
rate_percent(uint32_t numerator, uint32_t denominator)
{
    if (denominator == 0)
        return 0.0;

    return (100.0 * (double)numerator) / (double)denominator;
}

static bool
prepare_winning_results_state(
    const cj4_mahjong *state,
    cj4_mahjong *normalized)
{
    if (!state || !normalized)
        return false;

    *normalized = *state;

    if (cj4_state_phase(normalized) == CJ4_PHASE_SETTLE)
    {
        normalized->progress =
            (uint8_t)((normalized->progress &
                       (uint8_t)~CJ4_STATE_PHASE_MASK) |
                      ((uint8_t)CJ4_PHASE_ROUND_END &
                       CJ4_STATE_PHASE_MASK));
    }

    return cj4_state_phase(normalized) == CJ4_PHASE_ROUND_END;
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

    if (!prepare_winning_results_state(state, &normalized))
        return false;

    return cj4_collect_winning_results(
        &normalized,
        rules,
        out_results,
        capacity,
        out_count);
}

static const char *
win_yaku_to_string(cj4_win_yaku yaku)
{
    switch (yaku)
    {
    case CJ4_WIN_YAKU_RIICHI:
        return "Riichi";
    case CJ4_WIN_YAKU_DOUBLE_RIICHI:
        return "Double Riichi";
    case CJ4_WIN_YAKU_IPPATSU:
        return "Ippatsu";
    case CJ4_WIN_YAKU_MENZEN_TSUMO:
        return "Menzen Tsumo";
    case CJ4_WIN_YAKU_TANYAO:
        return "Tanyao";
    case CJ4_WIN_YAKU_YAKUHAI_HAKU:
        return "Yakuhai (Haku)";
    case CJ4_WIN_YAKU_YAKUHAI_HATSU:
        return "Yakuhai (Hatsu)";
    case CJ4_WIN_YAKU_YAKUHAI_CHUN:
        return "Yakuhai (Chun)";
    case CJ4_WIN_YAKU_YAKUHAI_SEAT_WIND:
        return "Yakuhai (Seat Wind)";
    case CJ4_WIN_YAKU_YAKUHAI_ROUND_WIND:
        return "Yakuhai (Round Wind)";
    case CJ4_WIN_YAKU_CHIITOI:
        return "Chiitoitsu";
    case CJ4_WIN_YAKU_KOKUSHI:
        return "Kokushi Musou";
    case CJ4_WIN_YAKU_KOKUSHI_13_WAIT:
        return "Kokushi Musou 13-Wait";
    case CJ4_WIN_YAKU_TOITOI:
        return "Toitoi";
    case CJ4_WIN_YAKU_HONROUTOU:
        return "Honroutou";
    case CJ4_WIN_YAKU_HONITSU:
        return "Honitsu";
    case CJ4_WIN_YAKU_CHINITSU:
        return "Chinitsu";
    case CJ4_WIN_YAKU_PINFU:
        return "Pinfu";
    case CJ4_WIN_YAKU_IIPEIKOU:
        return "Iipeikou";
    case CJ4_WIN_YAKU_RYANPEIKOU:
        return "Ryanpeikou";
    case CJ4_WIN_YAKU_SANSHOKU_DOUJUN:
        return "Sanshoku Doujun";
    case CJ4_WIN_YAKU_ITTSUU:
        return "Ittsuu";
    case CJ4_WIN_YAKU_CHANTA:
        return "Chanta";
    case CJ4_WIN_YAKU_JUNCHAN:
        return "Junchan";
    case CJ4_WIN_YAKU_SANANKOU:
        return "Sanankou";
    case CJ4_WIN_YAKU_SHOUSANGEN:
        return "Shousangen";
    case CJ4_WIN_YAKU_DAISANGEN:
        return "Daisangen";
    case CJ4_WIN_YAKU_SHOUSUUSHII:
        return "Shousuushii";
    case CJ4_WIN_YAKU_DAISUUSHII:
        return "Daisuushii";
    case CJ4_WIN_YAKU_TSUUIISOU:
        return "Tsuuiisou";
    case CJ4_WIN_YAKU_RYUUIISOU:
        return "Ryuuiisou";
    case CJ4_WIN_YAKU_CHINROUTOU:
        return "Chinroutou";
    case CJ4_WIN_YAKU_SANKANTSU:
        return "Sankantsu";
    case CJ4_WIN_YAKU_SUUKANTSU:
        return "Suukantsu";
    case CJ4_WIN_YAKU_SANSHOKU_DOUKOU:
        return "Sanshoku Doukou";
    case CJ4_WIN_YAKU_SUUANKOU:
        return "Suuankou";
    case CJ4_WIN_YAKU_SUUANKOU_TANKI:
        return "Suuankou Tanki";
    case CJ4_WIN_YAKU_CHUUREN:
        return "Chuuren Poutou";
    case CJ4_WIN_YAKU_JUNSEI_CHUUREN:
        return "Junsei Chuuren Poutou";
    case CJ4_WIN_YAKU_RINSHAN:
        return "Rinshan Kaihou";
    case CJ4_WIN_YAKU_HAITEI:
        return "Haitei Raoyue";
    case CJ4_WIN_YAKU_HOUTEI:
        return "Houtei Raoyui";
    case CJ4_WIN_YAKU_CHANKAN:
        return "Chankan";
    case CJ4_WIN_YAKU_TENHOU:
        return "Tenhou";
    case CJ4_WIN_YAKU_CHIIHOU:
        return "Chiihou";
    default:
        return "Unknown yaku";
    }
}

static const char *
cj4_phase_to_string(cj4_phase phase)
{
    switch (phase)
    {
    case CJ4_PHASE_DRAW:
        return "DRAW";
    case CJ4_PHASE_KAKAN_RESOLVE:
        return "KAKAN_RESOLVE";
    case CJ4_PHASE_ANKAN_RESOLVE:
        return "ANKAN_RESOLVE";
    case CJ4_PHASE_AFTER_CALL:
        return "AFTER_CALL";
    case CJ4_PHASE_DISCARD:
        return "DISCARD";
    case CJ4_PHASE_ROUND_END:
        return "ROUND_END";
    case CJ4_PHASE_SETTLE:
        return "SETTLE";
    case CJ4_PHASE_GAME_END:
        return "GAME_END";
    default:
        return "UNKNOWN";
    }
}

static const char *
cj4_wind_to_string(cj4_wind wind)
{
    switch (wind)
    {
    case CJ4_WIND_EAST:
        return "East";
    case CJ4_WIND_SOUTH:
        return "South";
    case CJ4_WIND_WEST:
        return "West";
    case CJ4_WIND_NORTH:
        return "North";
    default:
        return "?";
    }
}

static const char *
cj4_meld_type_to_string(cj4_meld_type type)
{
    switch (type)
    {
    case CJ4_MELD_CHI:
        return "CHI";
    case CJ4_MELD_PON:
        return "PON";
    case CJ4_MELD_MINKAN:
        return "MINKAN";
    case CJ4_MELD_ANKAN:
        return "ANKAN";
    case CJ4_MELD_KAKAN:
        return "KAKAN";
    default:
        return "?";
    }
}

static void
cj4_print_tile_list(
    FILE *out,
    const cj4_tile_id *tiles,
    uint8_t tile_count)
{
    if (tile_count == 0)
    {
        fputs("-", out);
        return;
    }

    for (uint8_t i = 0; i < tile_count; ++i)
    {
        if (i > 0)
            fputc(' ', out);
        fputs(cj4_tile_to_string(tiles[i]), out);
    }
}

static uint8_t
cj4_collect_visible_dora_indicators(
    const cj4_mahjong *state,
    cj4_tile_id *out_tiles,
    uint8_t capacity)
{
    uint8_t count = state->dora_count;

    if (count > capacity)
        count = capacity;

    for (uint8_t i = 0; i < count; ++i)
        out_tiles[i] =
            cj4_get_wall_tile(state, cj4_dora_indicator_indices[i]);

    return count;
}

static uint8_t
cj4_collect_visible_ura_dora_indicators(
    const cj4_mahjong *state,
    const cj4_rules *rules,
    cj4_tile_id *out_tiles,
    uint8_t capacity)
{
    cj4_win_result results[CJ4_PLAYER_COUNT];
    uint8_t result_count = 0;

    if (!rules || !out_tiles || capacity == 0)
        return 0;

    if (!collect_winning_results(
            state,
            rules,
            results,
            CJ4_PLAYER_COUNT,
            &result_count))
        return 0;

    for (uint8_t i = 0; i < result_count; ++i)
    {
        uint8_t count = results[i].ura_dora_indicators_count;

        if (count == 0)
            continue;

        if (count > capacity)
            count = capacity;

        for (uint8_t j = 0; j < count; ++j)
            out_tiles[j] = results[i].ura_dora_indicators[j];

        return count;
    }

    return 0;
}

static void
cj4_sort_hand_tiles(
    cj4_tile_id *tiles,
    uint8_t tile_count)
{
    for (uint8_t i = 1; i < tile_count; ++i)
    {
        cj4_tile_id current = tiles[i];
        cj4_tile_type current_type = cj4_tile_get_type(current);
        uint8_t j = i;

        while (j > 0)
        {
            cj4_tile_id previous = tiles[j - 1];
            cj4_tile_type previous_type = cj4_tile_get_type(previous);

            if (previous_type < current_type)
                break;
            if (previous_type == current_type && previous < current)
                break;

            tiles[j] = previous;
            --j;
        }

        tiles[j] = current;
    }
}

static void
cj4_print_winning_results(
    FILE *out,
    const cj4_mahjong *state,
    const cj4_rules *rules)
{
    cj4_win_result results[CJ4_PLAYER_COUNT];
    uint8_t result_count = 0;

    if (cj4_state_phase(state) != CJ4_PHASE_ROUND_END &&
        cj4_state_phase(state) != CJ4_PHASE_SETTLE)
        return;

    if (!collect_winning_results(
            state,
            rules,
            results,
            CJ4_PLAYER_COUNT,
            &result_count) ||
        result_count == 0)
        return;

    fputs("\nWinning Results:\n", out);

    for (uint8_t i = 0; i < result_count; ++i)
    {
        const cj4_win_result *result = &results[i];

        fprintf(out,
                "Player%u: %u han / %u fu",
                (unsigned)result->player,
                (unsigned)result->han,
                (unsigned)result->fu);

        if (result->yakuman_count > 0)
            fprintf(out, " / %u yakuman", (unsigned)result->yakuman_count);

        if (result->ron_points > 0)
        {
            fprintf(out, " / Ron: %" PRId32, result->ron_points);
        }
        else if (result->tsumo_dealer_payment > 0 ||
                 result->tsumo_non_dealer_payment > 0)
        {
            fprintf(out,
                    " / Tsumo: dealer pays %" PRId32 ", non-dealer pays %" PRId32,
                    result->tsumo_dealer_payment,
                    result->tsumo_non_dealer_payment);
        }

        fputs(" / Yaku: ", out);

        if (result->yaku_count == 0)
        {
            fputs("-", out);
        }
        else
        {
            for (uint8_t j = 0; j < result->yaku_count; ++j)
            {
                if (j > 0)
                    fputs(", ", out);

                fputs(win_yaku_to_string(result->yaku[j]), out);
            }
        }

        fputs(" / Dora Indicators: ", out);
        cj4_print_tile_list(
            out,
            result->dora_indicators,
            result->dora_indicators_count);

        if (cj4_state_phase(state) == CJ4_PHASE_SETTLE &&
            result->ura_dora_indicators_count > 0)
        {
            fputs(" / Ura-Dora Indicators: ", out);
            cj4_print_tile_list(
                out,
                result->ura_dora_indicators,
                result->ura_dora_indicators_count);
        }

        fputc('\n', out);
    }
}

static void
cj4_print_player_hand(
    FILE *out,
    const cj4_mahjong *state,
    cj4_player player)
{
    cj4_tile_id hand[CJ4_MAX_HAND_TILES];
    uint8_t hand_count = 0;
    cj4_tile_id draw_tile = CJ4_TILE_ID_INVALID;

    if (state->draw_tile != CJ4_TILE_ID_INVALID)
    {
        const cj4_location draw_location =
            cj4_location_get(state->locations, state->draw_tile);

        if (cj4_location_is_hand(draw_location.placement) &&
            cj4_location_placement_player(draw_location.placement) == player)
        {
            draw_tile = state->draw_tile;
        }
    }

    for (cj4_tile_id tile = CJ4_TILE_ID_MIN; tile <= CJ4_TILE_ID_MAX; ++tile)
    {
        const cj4_location location =
            cj4_location_get(state->locations, tile);
        if (!cj4_location_is_hand(location.placement) ||
            cj4_location_placement_player(location.placement) != player)
            continue;
        if (tile == draw_tile)
            continue;

        if (hand_count < CJ4_MAX_HAND_TILES)
            hand[hand_count++] = tile;
    }

    cj4_sort_hand_tiles(hand, hand_count);

    fprintf(out, "Hand: ");
    cj4_print_tile_list(out, hand, hand_count);
    if (draw_tile != CJ4_TILE_ID_INVALID)
        fprintf(out, " / Draw: %s", cj4_tile_to_string(draw_tile));
    fputc('\n', out);
}

static void
cj4_print_player_discards(
    FILE *out,
    const cj4_mahjong *state,
    cj4_player player)
{
    const cj4_discard_list discards =
        cj4_location_collect_discards(state->locations);
    uint8_t printed = 0;

    fprintf(out, "Discards: ");
    for (uint8_t i = 0; i < discards.count; ++i)
    {
        const cj4_discard *discard = &discards.items[i];
        if (discard->player != player)
            continue;

        if (printed > 0)
            fputc(' ', out);
        fputs(cj4_tile_to_string(discard->tile), out);
        ++printed;
    }

    if (printed == 0)
        fputc('-', out);

    fputc('\n', out);
}

static void
cj4_print_player_melds(
    FILE *out,
    const cj4_mahjong *state,
    cj4_player player)
{
    const cj4_meld_list melds =
        cj4_location_collect_melds(state->locations, player);

    if (melds.count == 0)
    {
        fputs("Melds: -\n", out);
        return;
    }

    fprintf(out, "Melds: ");
    for (uint8_t i = 0; i < melds.count; ++i)
    {
        const cj4_meld *meld = &melds.items[i];
        if (i > 0)
            fputs(" | ", out);

        fprintf(out, "%s(", cj4_meld_type_to_string(meld->type));
        for (uint8_t j = 0; j < meld->size; ++j)
        {
            if (j > 0)
                fputc(' ', out);
            fputs(cj4_tile_to_string(meld->tiles[j]), out);
        }
        fputc(')', out);
    }
    fputc('\n', out);
}

const char *
cj4_tile_to_string(cj4_tile_id tile)
{
    static const char *const suit_suffixes[] = {"m", "p", "s"};
    static char buffers[8][4];
    static uint8_t next_buffer = 0;
    cj4_tile_type type;
    uint8_t number;
    char *buffer;

    if (tile == CJ4_TILE_ID_INVALID)
        return "-";

    type = cj4_tile_get_type(tile);
    if (type >= CJ4_TILE_TYPE_HONOR_MIN)
        return cj4_honor_tile_names[type - CJ4_TILE_TYPE_HONOR_MIN];

    number = cj4_tile_type_get_number(type);
    buffer = buffers[next_buffer];
    next_buffer = (uint8_t)((next_buffer + 1) % 8);

    assert(cj4_tile_get_suit(tile) <= CJ4_TILE_SUIT_SOUZU);
    buffer[0] = (char)('0' + number);
    buffer[1] = suit_suffixes[cj4_tile_get_suit(tile)][0];
    buffer[2] = '\0';

    return buffer;
}

void
cj4_render_state(FILE *out, const cj4_mahjong *state, const cj4_rules *rules)
{
    cj4_tile_id dora_indicators[sizeof(cj4_dora_indicator_indices) /
                                sizeof(cj4_dora_indicator_indices[0])];
    cj4_tile_id ura_dora_indicators[CJ4_MAX_WIN_RESULT_DORA_INDICATORS];
    uint8_t dora_indicator_count = cj4_collect_visible_dora_indicators(
        state,
        dora_indicators,
        (uint8_t)(sizeof(dora_indicators) / sizeof(dora_indicators[0])));
    uint8_t ura_dora_indicator_count = cj4_collect_visible_ura_dora_indicators(
        state,
        rules,
        ura_dora_indicators,
        (uint8_t)(sizeof(ura_dora_indicators) / sizeof(ura_dora_indicators[0])));

    fprintf(out,
            "Round: %s / Dealer: Player%u / Current: Player%u / Phase: %s\n",
            cj4_wind_to_string(state->round_wind),
            (unsigned)state->dealer,
            (unsigned)cj4_state_current_player(state),
            cj4_phase_to_string(cj4_state_phase(state)));
    fprintf(out,
            "Honba: %u / Riichi sticks: %u / Wall pos: %u\n",
            (unsigned)state->honba,
            (unsigned)state->riichi_sticks,
            (unsigned)state->wall_pos);
    fputs("Dora Indicators: ", out);
    cj4_print_tile_list(out, dora_indicators, dora_indicator_count);
    fputc('\n', out);
    if (ura_dora_indicator_count > 0)
    {
        fputs("Ura-Dora Indicators: ", out);
        cj4_print_tile_list(out, ura_dora_indicators, ura_dora_indicator_count);
        fputc('\n', out);
    }

    for (cj4_player player = CJ4_PLAYER_0; player < CJ4_PLAYER_COUNT; ++player)
    {
        fprintf(out, "\nPlayer%u (score: %" PRId32, (unsigned)player, state->scores[player]);
        if (cj4_state_is_riichi(state, player))
            fputs(", riichi", out);
        fputs(")\n", out);
        cj4_print_player_hand(out, state, player);
        cj4_print_player_discards(out, state, player);
        cj4_print_player_melds(out, state, player);
    }

    cj4_print_winning_results(out, state, rules);
}

#ifndef CJ4_EXAMPLES_CLI_NO_MAIN
static uint8_t
seat_order_from_starting_dealer(cj4_player player, cj4_player starting_dealer)
{
    return (uint8_t)((player + CJ4_PLAYER_COUNT - starting_dealer) % CJ4_PLAYER_COUNT);
}

static void
print_final_ranking(
    FILE *out,
    const cj4_mahjong *state,
    cj4_player starting_dealer)
{
    cj4_player ranking[CJ4_PLAYER_COUNT];

    for (cj4_player player = CJ4_PLAYER_0; player < CJ4_PLAYER_COUNT; ++player)
        ranking[player] = player;

    for (uint8_t i = 0; i < CJ4_PLAYER_COUNT; ++i)
    {
        uint8_t best = i;

        for (uint8_t j = (uint8_t)(i + 1); j < CJ4_PLAYER_COUNT; ++j)
        {
            cj4_player lhs = ranking[j];
            cj4_player rhs = ranking[best];

            if (state->scores[lhs] > state->scores[rhs])
            {
                best = j;
                continue;
            }

            if (state->scores[lhs] == state->scores[rhs] &&
                seat_order_from_starting_dealer(lhs, starting_dealer) <
                    seat_order_from_starting_dealer(rhs, starting_dealer))
            {
                best = j;
            }
        }

        if (best != i)
        {
            cj4_player tmp = ranking[i];
            ranking[i] = ranking[best];
            ranking[best] = tmp;
        }
    }

    fputs("\n=== Final Ranking ===\n", out);
    for (uint8_t i = 0; i < CJ4_PLAYER_COUNT; ++i)
    {
        cj4_player player = ranking[i];
        fprintf(out,
                "%u. Player%u (%" PRId32 ")\n",
                (unsigned)(i + 1),
                (unsigned)player,
                state->scores[player]);
    }
}
#endif

void
cj4_capture_round_snapshot(
    cj4_cli_round_snapshot *snapshot,
    const cj4_mahjong *state)
{
    if (!snapshot || !state)
        return;

    snapshot->has_exhaustive_draw_scores = 0;

    if (cj4_state_phase(state) != CJ4_PHASE_ROUND_END ||
        cj4_state_round_end_type(state) != CJ4_ROUND_END_EXHAUSTIVE_DRAW)
        return;

    snapshot->has_exhaustive_draw_scores = 1;
    memcpy(snapshot->round_end_scores, state->scores, sizeof(snapshot->round_end_scores));
}

void
cj4_update_stats_for_settle(
    cj4_cli_stats *stats,
    const cj4_mahjong *state,
    const cj4_rules *rules,
    cj4_cli_round_snapshot *snapshot)
{
    cj4_win_result results[CJ4_PLAYER_COUNT];
    uint8_t result_count = 0;
    uint8_t had_ron = 0;

    if (!stats || !state || !rules)
        return;

    if (cj4_state_phase(state) != CJ4_PHASE_SETTLE)
        return;

    stats->rounds++;

    for (cj4_player player = CJ4_PLAYER_0; player < CJ4_PLAYER_COUNT; ++player)
    {
        if (cj4_state_is_riichi(state, player))
            stats->riichi_rounds[player]++;
    }

    if (collect_winning_results(
            state,
            rules,
            results,
            CJ4_PLAYER_COUNT,
            &result_count) &&
        result_count > 0)
    {
        for (uint8_t i = 0; i < result_count; ++i)
        {
            const cj4_win_result *result = &results[i];

            if (result->ron_points > 0)
            {
                stats->ron_wins[result->player]++;
                had_ron = 1;
            }
            else if (result->tsumo_dealer_payment > 0 ||
                     result->tsumo_non_dealer_payment > 0)
            {
                stats->tsumo_wins[result->player]++;
            }
        }

        if (had_ron)
            stats->deal_in_rounds[cj4_state_current_player(state)]++;

        if (snapshot)
            snapshot->has_exhaustive_draw_scores = 0;
        return;
    }

    if (cj4_state_round_end_type(state) != CJ4_ROUND_END_EXHAUSTIVE_DRAW)
    {
        if (snapshot)
            snapshot->has_exhaustive_draw_scores = 0;
        return;
    }

    stats->exhaustive_draw_rounds++;

    if (!snapshot || !snapshot->has_exhaustive_draw_scores)
        return;

    for (cj4_player player = CJ4_PLAYER_0; player < CJ4_PLAYER_COUNT; ++player)
    {
        if (state->scores[player] < snapshot->round_end_scores[player])
            stats->noten_rounds[player]++;
    }

    snapshot->has_exhaustive_draw_scores = 0;
}

void
cj4_print_stats_summary(FILE *out, const cj4_cli_stats *stats)
{
    if (!out || !stats)
        return;

    fprintf(out, "\n=== Statistics Summary ===\n");
    fprintf(out, "Games: %u\n", (unsigned)stats->games);
    fprintf(out, "Rounds: %u\n", (unsigned)stats->rounds);
    fprintf(out,
            "Exhaustive Draw Rounds: %u\n\n",
            (unsigned)stats->exhaustive_draw_rounds);

    for (cj4_player player = CJ4_PLAYER_0; player < CJ4_PLAYER_COUNT; ++player)
    {
        fprintf(out, "Player%u\n", (unsigned)player);
        fprintf(out,
                "  Tsumo Win Rate : %u / %u (%.2f%%)\n",
                (unsigned)stats->tsumo_wins[player],
                (unsigned)stats->rounds,
                rate_percent(stats->tsumo_wins[player], stats->rounds));
        fprintf(out,
                "  Ron Rate       : %u / %u (%.2f%%)\n",
                (unsigned)stats->ron_wins[player],
                (unsigned)stats->rounds,
                rate_percent(stats->ron_wins[player], stats->rounds));
        fprintf(out,
                "  Riichi Rate    : %u / %u (%.2f%%)\n",
                (unsigned)stats->riichi_rounds[player],
                (unsigned)stats->rounds,
                rate_percent(stats->riichi_rounds[player], stats->rounds));
        fprintf(out,
                "  Deal-in Rate   : %u / %u (%.2f%%)\n",
                (unsigned)stats->deal_in_rounds[player],
                (unsigned)stats->rounds,
                rate_percent(stats->deal_in_rounds[player], stats->rounds));
        fprintf(out,
                "  Noten Rate     : %u / %u (%.2f%%)\n\n",
                (unsigned)stats->noten_rounds[player],
                (unsigned)stats->exhaustive_draw_rounds,
                rate_percent(
                    stats->noten_rounds[player],
                    stats->exhaustive_draw_rounds));
    }
}

#ifndef CJ4_EXAMPLES_CLI_NO_MAIN
static int
run_stats_mode(uint32_t game_count)
{
    cj4_rules rules = make_rules();
    cj4m_player_delegate delegates[CJ4_PLAYER_COUNT];
    cj4_cli_stats stats = {0};

    init_default_delegates(delegates);

    for (uint32_t game = 0; game < game_count; ++game)
    {
        cj4_tile_id wall[CJ4_TILE_ID_COUNT];
        cj4_mahjong state;
        cj4_cli_round_snapshot snapshot = {0};

        fill_wall(wall);
        state = cj4_create_initial_state(wall, &rules);

        while (cj4_state_phase(&state) != CJ4_PHASE_GAME_END)
        {
            if (cj4_state_phase(&state) == CJ4_PHASE_ROUND_END)
                cj4_capture_round_snapshot(&snapshot, &state);

            if (cj4_state_phase(&state) == CJ4_PHASE_SETTLE)
                cj4_update_stats_for_settle(&stats, &state, &rules, &snapshot);

            if (cj4_can_next_round(state))
            {
                fill_wall(wall);
                state = cj4_do_next_round(state, wall, &rules);
                continue;
            }

            state = cj4m_step(&state, &rules, delegates);
        }

        stats.games++;
    }

    cj4_print_stats_summary(stdout, &stats);
    return 0;
}

static void
print_usage(FILE *out, const char *program_name)
{
    fprintf(out, "Usage: %s [--all-steps] [--stats N]\n", program_name);
}

int
main(int argc, char **argv)
{
    cj4_rules rules = make_rules();
    cj4m_player_delegate delegates[CJ4_PLAYER_COUNT];
    cj4_tile_id wall[CJ4_TILE_ID_COUNT];
    cj4_mahjong state;
    cj4_player starting_dealer;
    unsigned int step = 0;
    int show_all_steps = 0;
    uint32_t stats_games = 0;

    init_default_delegates(delegates);

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--all-steps") == 0)
        {
            show_all_steps = 1;
            continue;
        }

        if (strcmp(argv[i], "--stats") == 0)
        {
            char *endptr = 0;
            unsigned long value;

            if (i + 1 >= argc)
            {
                print_usage(stderr, argv[0]);
                return 1;
            }

            if (argv[i + 1][0] == '-')
            {
                fprintf(stderr, "Invalid value for --stats: %s\n", argv[i + 1]);
                return 1;
            }

            value = strtoul(argv[i + 1], &endptr, 10);
            if (endptr == argv[i + 1] ||
                *endptr != '\0' ||
                value == 0 ||
                value > UINT32_MAX)
            {
                fprintf(stderr, "Invalid value for --stats: %s\n", argv[i + 1]);
                return 1;
            }

            stats_games = (uint32_t)value;
            ++i;
            continue;
        }

        print_usage(stderr, argv[0]);
        return 1;
    }

    if (stats_games > 0 && show_all_steps)
    {
        fputs("--all-steps cannot be combined with --stats.\n", stderr);
        return 1;
    }

    srand((unsigned int)time(0));

    if (stats_games > 0)
        return run_stats_mode(stats_games);

    fill_wall(wall);
    state = cj4_create_initial_state(wall, &rules);
    starting_dealer = state.dealer;

    while (cj4_state_phase(&state) != CJ4_PHASE_GAME_END)
    {
        if (should_render_step(&state, show_all_steps))
        {
            printf("\n=== Step %u ===\n", step);
            cj4_render_state(stdout, &state, &rules);
        }

        if (cj4_can_next_round(state))
        {
            fill_wall(wall);
            state = cj4_do_next_round(state, wall, &rules);
            ++step;
            continue;
        }

        state = cj4m_step(&state, &rules, delegates);
        ++step;
    }

    print_final_ranking(stdout, &state, starting_dealer);

    return 0;
}
#endif
