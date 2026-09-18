#include <assert.h>
#include <string.h>

#include "cjong4/core/state_init.h"
#include "cjong4/core/state_query.h"
#include "cjong4/core/tile.h"
#include "cjong4/core/tile_const.h"
#include "cjong4/manager/manager.h"
#include "cjong4/opponent/opponent_standard.h"
#include "cjong4/player/hand_analysis.h"
#include "test_state_helpers.h"

static cj4_player_view
hand_view(const char *text)
{
    cj4_player_view view;
    uint8_t copies[CJ4_TILE_TYPE_COUNT] = {0};
    test_init_player_view(&view);
    view.phase = CJ4_PHASE_DRAW;
    while (*text)
    {
        const char *end = text;
        while (*end >= '1' && *end <= '9')
            ++end;
        assert(end > text && *end);
        const int base = *end == 'm' ? 0 : *end == 'p' ? 9 : *end == 's' ? 18 : 27;
        while (text < end)
        {
            const cj4_tile_type type = (cj4_tile_type)(base + *text++ - '1');
            assert(type < CJ4_TILE_TYPE_COUNT && copies[type] < 4);
            test_view_add_hand(&view, cj4_tile_make(type, copies[type]++));
        }
        text = end + 1;
    }
    return view;
}

static cj4_action
choose(const cj4_player_view *view, cj4_action *actions, uint8_t count)
{
    cj4m_player_delegate delegate = cj4_opponent_standard(1);
    return delegate.decide(delegate.ctx, view, actions, count);
}

static cj4_action
choose_discard(const cj4_player_view *view)
{
    const cj4_hand hand = cj4_location_collect_hand(view->locations, view->player);
    cj4_action actions[14] = {0};
    for (uint8_t i = 0; i < hand.count; ++i)
        actions[i] = (cj4_action){.type = CJ4_ACTION_DISCARD, .tile = hand.items[i]};
    return choose(view, actions, hand.count);
}

static void
add_meld(cj4_player_view *view, uint8_t group, cj4_tile_type type, cj4_meld_type kind)
{
    for (uint8_t i = 0; i < (kind == CJ4_MELD_ANKAN ? 4 : 3); ++i)
        view->locations[cj4_tile_make(type, i)].placement = (uint8_t)(
            CJ4_LOCATION_PLACEMENT_MELD_FLAG |
            (view->player << CJ4_LOCATION_PLAYER_SHIFT) |
            (group << CJ4_LOCATION_MELD_GROUP_SHIFT) | kind);
}

static cj4_action
pon(cj4_tile_type type)
{
    return (cj4_action){.type = CJ4_ACTION_PON, .tile = cj4_tile_make(type, 2),
        .tiles = {cj4_tile_make(type, 0), cj4_tile_make(type, 1)}, .tile_count = 2};
}

static void
test_win_and_riichi_priority(void)
{
    cj4_player_view view = hand_view("123456m123p567s12z");
    view.is_riichi[1] = view.is_riichi[2] = 1;
    cj4_action actions[] = {
        {.type = CJ4_ACTION_DISCARD, .tile = cj4_tile_make(CJ4_TILE_TYPE_EAST, 0)},
        {.type = CJ4_ACTION_RIICHI, .tile = cj4_tile_make(CJ4_TILE_TYPE_SOUTH, 0)},
        {.type = CJ4_ACTION_TSUMO}};
    for (int ctx = -1; ctx <= 3; ++ctx)
    {
        cj4m_player_delegate delegate = cj4_opponent_standard(ctx);
        assert(delegate.decide(delegate.ctx, &view, actions, 3).type == CJ4_ACTION_TSUMO);
        assert(delegate.decide(delegate.ctx, &view, actions, 2).type == CJ4_ACTION_RIICHI);
    }
    actions[0].type = CJ4_ACTION_PASS;
    actions[1] = pon(CJ4_TILE_TYPE_HAKU);
    actions[2].type = CJ4_ACTION_RON;
    view.phase = CJ4_PHASE_DISCARD;
    assert(choose(&view, actions, 3).type == CJ4_ACTION_RON);
    view.phase = CJ4_PHASE_KAKAN_RESOLVE;
    assert(choose(&view, actions, 3).type == CJ4_ACTION_RON);
    assert(choose(&view, actions, 2).type == CJ4_ACTION_PASS);
}

static void
test_shanten_then_live_effective_tiles(void)
{
    cj4_player_view view = hand_view("123456m123p567s12z");
    /* Four complete sets: discard an honor, never break a set. */
    assert(cj4_tile_get_type(choose_discard(&view).tile) == CJ4_TILE_TYPE_EAST);
    /* South is dead. Keep the live east wait despite deterministic tie order. */
    for (uint8_t i = 1; i < 4; ++i)
        view.locations[cj4_tile_make(CJ4_TILE_TYPE_SOUTH, i)].discard = 32;
    assert(cj4_tile_get_type(choose_discard(&view).tile) == CJ4_TILE_TYPE_SOUTH);
    cj4_action riichi[] = {
        {.type = CJ4_ACTION_RIICHI, .tile = cj4_tile_make(CJ4_TILE_TYPE_EAST, 0)},
        {.type = CJ4_ACTION_RIICHI, .tile = cj4_tile_make(CJ4_TILE_TYPE_SOUTH, 0)}};
    assert(choose(&view, riichi, 2).tile == riichi[1].tile);

    view = hand_view("123m456p78s123z567m");
    assert(cj4_tile_get_type(choose_discard(&view).tile) == CJ4_TILE_TYPE_EAST);
    view.locations[cj4_tile_make(CJ4_TILE_TYPE_WEST, 1)].discard = 32;
    view.locations[cj4_tile_make(CJ4_TILE_TYPE_WEST, 2)].discard = 32;
    test_view_add_dora_indicator(&view, cj4_tile_make(CJ4_TILE_TYPE_WEST, 3));
    assert(cj4_tile_get_type(choose_discard(&view).tile) == CJ4_TILE_TYPE_WEST);

    view = hand_view("123456m123p4567s1z");
    /* Discard east: 4s/7s double-pair waits (6 tiles), versus a 3-tile
     * east wait after discarding either end of 4567s. */
    assert(cj4_tile_get_type(choose_discard(&view).tile) == CJ4_TILE_TYPE_EAST);
    for (uint8_t i = 1; i < 4; ++i)
    {
        view.locations[cj4_tile_make(CJ4_TILE_TYPE_4S, i)].discard = 32;
        view.locations[cj4_tile_make(CJ4_TILE_TYPE_7S, i)].discard = 32;
    }
    assert(cj4_tile_get_type(choose_discard(&view).tile) == CJ4_TILE_TYPE_4S);
}

static void
test_kokushi_threshold(void)
{
    cj4_player_view view = hand_view("19m195p19s1234z456m");
    cj4_shanten_result result;
    assert(cj4p_calculate_shanten(&view, &result));
    assert(result.standard == 6 && result.kokushi == 3);
    cj4_action selected = choose_discard(&view);
    assert(cj4p_calculate_shanten_after_discard(&view, selected.tile, &result));
    assert(result.kokushi == 3);

    view = hand_view("19m195p19s1123z456m");
    assert(cj4p_calculate_shanten(&view, &result));
    assert(result.standard == 5 && result.kokushi == 3);
    selected = choose_discard(&view);
    assert(cj4_tile_get_type(selected.tile) == CJ4_TILE_TYPE_SOUTH);
    assert(cj4p_calculate_shanten_after_discard(&view, selected.tile, &result));
    assert(result.standard == 5 && result.kokushi == 4);

    view = hand_view("19m19p19s1234567z5m");
    assert(cj4_tile_get_type(choose_discard(&view).tile) == CJ4_TILE_TYPE_5M);

    view = hand_view("19m19p19s1123456z");
    view.phase = CJ4_PHASE_DISCARD;
    cj4_action actions[] = {{.type = CJ4_ACTION_PASS}, pon(CJ4_TILE_TYPE_EAST)};
    assert(choose(&view, actions, 2).type == CJ4_ACTION_PASS);
}

static void
test_yakuhai_and_closed_policy(void)
{
    cj4_player_view view = hand_view("123m456p78s11223z");
    view.phase = CJ4_PHASE_DISCARD;
    view.dealer = CJ4_PLAYER_3; /* player 0 is south, round is east */
    cj4_action actions[] = {{.type = CJ4_ACTION_PASS}, pon(CJ4_TILE_TYPE_EAST)};
    assert(choose(&view, actions, 2).type == CJ4_ACTION_PON);
    actions[1] = pon(CJ4_TILE_TYPE_SOUTH);
    assert(choose(&view, actions, 2).type == CJ4_ACTION_PON);
    actions[1] = pon(CJ4_TILE_TYPE_WEST);
    assert(choose(&view, actions, 2).type == CJ4_ACTION_PASS);
    for (cj4_tile_type type = CJ4_TILE_TYPE_HAKU; type <= CJ4_TILE_TYPE_CHUN; ++type)
    {
        actions[1] = pon(type);
        assert(choose(&view, actions, 2).type == CJ4_ACTION_PON);
    }
    actions[1] = pon(CJ4_TILE_TYPE_5P);
    assert(choose(&view, actions, 2).type == CJ4_ACTION_PASS);
    actions[1].type = CJ4_ACTION_CHI;
    assert(choose(&view, actions, 2).type == CJ4_ACTION_PASS);
}

static void
test_open_calls_and_three_meld_limit(void)
{
    cj4_player_view view = hand_view("123m45p78s112z");
    view.phase = CJ4_PHASE_DISCARD;
    add_meld(&view, 0, CJ4_TILE_TYPE_HAKU, CJ4_MELD_PON);
    cj4_action actions[] = {{.type = CJ4_ACTION_PASS},
        {.type = CJ4_ACTION_CHI, .tile = cj4_tile_make(CJ4_TILE_TYPE_6P, 0),
         .tiles = {cj4_tile_make(CJ4_TILE_TYPE_4P, 0), cj4_tile_make(CJ4_TILE_TYPE_5P, 0)},
         .tile_count = 2}};
    assert(choose(&view, actions, 2).type == CJ4_ACTION_CHI);
    view = hand_view("45p78s112z");
    view.phase = CJ4_PHASE_DISCARD;
    add_meld(&view, 0, CJ4_TILE_TYPE_HAKU, CJ4_MELD_PON);
    add_meld(&view, 1, CJ4_TILE_TYPE_2M, CJ4_MELD_PON);
    assert(choose(&view, actions, 2).type == CJ4_ACTION_CHI);

    view = hand_view("1123z");
    view.phase = CJ4_PHASE_DISCARD;
    add_meld(&view, 0, CJ4_TILE_TYPE_HAKU, CJ4_MELD_PON);
    add_meld(&view, 1, CJ4_TILE_TYPE_2M, CJ4_MELD_PON);
    add_meld(&view, 2, CJ4_TILE_TYPE_3M, CJ4_MELD_PON);
    actions[1] = pon(CJ4_TILE_TYPE_EAST);
    assert(choose(&view, actions, 2).type == CJ4_ACTION_PASS);

    /* Non-yakuhai pon advances an already open hand. */
    view = hand_view("123m55p78s112z");
    view.phase = CJ4_PHASE_DISCARD;
    add_meld(&view, 0, CJ4_TILE_TYPE_HAKU, CJ4_MELD_PON);
    actions[1] = pon(CJ4_TILE_TYPE_5P);
    assert(choose(&view, actions, 2).type == CJ4_ACTION_PON);
    /* An ankan alone does not count as opening the hand. */
    add_meld(&view, 0, CJ4_TILE_TYPE_HAKU, CJ4_MELD_ANKAN);
    assert(choose(&view, actions, 2).type == CJ4_ACTION_PASS);

    /* A chi that replaces our complete 123m creates no improvement. */
    view = hand_view("123m456p78s11z");
    view.phase = CJ4_PHASE_DISCARD;
    add_meld(&view, 0, CJ4_TILE_TYPE_HAKU, CJ4_MELD_PON);
    actions[1] = (cj4_action){.type = CJ4_ACTION_CHI,
        .tile = cj4_tile_make(CJ4_TILE_TYPE_3M, 1),
        .tiles = {cj4_tile_make(CJ4_TILE_TYPE_1M, 0), cj4_tile_make(CJ4_TILE_TYPE_2M, 0)},
        .tile_count = 2};
    assert(choose(&view, actions, 2).type == CJ4_ACTION_PASS);
}

static cj4_action
checked_decide(void *ctx, const cj4_player_view *view,
               const cj4_action *actions, uint8_t count)
{
    (void)ctx;
    cj4m_player_delegate delegate = cj4_opponent_standard(1);
    cj4_action selected = delegate.decide(delegate.ctx, view, actions, count);
    bool found = false;
    for (uint8_t i = 0; i < count; ++i)
        if (memcmp(&selected, &actions[i], sizeof(selected)) == 0)
            found = true;
    assert(found);
    return selected;
}

static void
test_manager_rounds(void)
{
    cj4_rules rules = cj4_rules_default();
    cj4m_player_delegate delegates[CJ4_PLAYER_COUNT];
    for (uint8_t i = 0; i < CJ4_PLAYER_COUNT; ++i)
        delegates[i] = (cj4m_player_delegate){.decide = checked_decide};
    for (uint32_t seed = 1; seed <= 3; ++seed)
    {
        uint32_t random = seed;
        cj4_tile_id wall[CJ4_TILE_ID_COUNT];
        for (uint16_t i = 0; i < CJ4_TILE_ID_COUNT; ++i)
            wall[i] = (cj4_tile_id)i;
        for (uint16_t i = CJ4_TILE_ID_COUNT - 1; i > 0; --i)
        {
            random = random * 1664525u + 1013904223u;
            const uint16_t j = (uint16_t)(random % (i + 1));
            const cj4_tile_id swap = wall[i];
            wall[i] = wall[j];
            wall[j] = swap;
        }
        cj4_mahjong state = cj4_create_initial_state(wall, &rules);
        unsigned steps = 0;
        while (cj4_state_phase(&state) != CJ4_PHASE_ROUND_END &&
               cj4_state_phase(&state) != CJ4_PHASE_GAME_END)
        {
            assert(++steps < 1000);
            state = cj4m_step(&state, &rules, delegates);
        }
    }
}

int main(void)
{
    test_win_and_riichi_priority();
    test_shanten_then_live_effective_tiles();
    test_kokushi_threshold();
    test_yakuhai_and_closed_policy();
    test_open_calls_and_three_meld_limit();
    test_manager_rounds();
    return 0;
}
