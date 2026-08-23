# Copilot instructions for cjong4-opponent

## Strict rules

- All responses must be in Japanese
- Do not perform commits automatically
- Do not download anything automatically
- Do not install anything automatically
- Ask for confirmation if an operation modifies the repository or environment

## Repository scope

- This repository provides eight opponent delegates on top of the read-only `external/cjong4` submodule.
- Opponent implementation, public headers, examples, tests, packaging, and documentation live at the repository root.
- Game-engine changes belong in the separate cjong4 repository, never in this repository's submodule checkout.

## Build and test commands

- Configure the library from the repository root:
  - `cmake -S . -B build/root`
- Configure with tests enabled:
  - `cmake -S . -B build/root -DCJ4_OPPONENT_BUILD_TESTS=ON`
- Build:
  - `cmake --build build/root`
- Run the full test suite:
  - `ctest --test-dir build/root --output-on-failure`
- Run the opponent-selection tests directly:
  - `./build/root/cj4_opponent_tests`
- Run the CLI/rendering tests directly:
  - `./build/root/cj4_winning_results_tests`
- There is no dedicated lint target in the repository. Warning checks come from the CMake build itself (`-Wall -Wextra -Wpedantic` on non-MSVC).
- There is no built-in per-test-case filter yet: all test cases are compiled into the single `cj4_tests` executable.

## High-level architecture

- `external/cjong4` is split into two layers:
  - `include/cjong4/core/` + `src/core/`: the pure game engine. These functions evaluate legality (`cj4_can_*`), apply transitions (`cj4_do_*`), score hands, settle rounds, and advance overall game state.
  - `include/cjong4/manager/` + `src/manager/`: the orchestration layer. It turns full state into a player-specific view, collects legal actions, asks delegates to choose, and resolves call priority across players.
- `cj4_mahjong` in `external/cjong4/include/cjong4/core/state.h` is the central state value. In cjong4 v2 it stores canonical tile placement in `locations[]` and exposes packed state through `cj4_state_*` accessors.
- The engine is intentionally value-oriented: state transition functions take a full `cj4_mahjong` and return a new one rather than mutating hidden global state.
- Tile identity is position-based, not count-based. Code works with `cj4_tile_id` and the `locations[]` array, so do not reduce logic to suit-count histograms unless the surrounding code already does that.
- The manager flow in `src/manager/manager_flow.c` has three distinct responsibilities:
  - For the active player in draw/after-call phases, build `cj4_player_view`, collect legal actions, and let that player's delegate choose one.
  - For discard and kakan-resolve phases, ask every non-active player for a reaction.
  - Resolve reactions in Mahjong priority order: ron first, then pon/minkan by seat distance from the discarder, then chi, otherwise pass and advance the round.
- Hidden information is enforced through `cj4m_make_player_view()`: delegates see their own hand plus public state, but not other players' concealed tiles or unrevealed wall state.
- The outer game loop is external to the library. Callers step a round with `cj4m_step()`, then supply the next wall themselves through `cj4_do_next_round()` when `cj4_can_next_round()` becomes true.

## Submodule protection

- The directory external/cjong4 is a git submodule
- NEVER modify any files under external/cjong4
- NEVER create, edit, or delete files in external/cjong4
- Treat external/cjong4 as read-only

If any change is required, instruct the user instead of modifying it

## Key conventions

- Public core APIs use the `cj4_` prefix; manager APIs use `cj4m_`. Keep new public names aligned with that split.
- Prefer existing pairs of legality and transition functions:
  - legality/query: `cj4_can_*`, `cj4_count_*`, `cj4_get_*`
  - state transitions: `cj4_do_*`
- Keep the functional style intact. Avoid introducing global mutable state, hidden caches, or in-place stateful helpers unless there is already an established pattern for it.
- Preserve the partial-information boundary. Logic that belongs to delegates should consume `cj4_player_view`; logic that needs omniscient state should stay in core/manager internals.
- Use the fixed-size domain constants already embedded in the model (`CJ4_PLAYER_COUNT`, `CJ4_TILE_ID_COUNT`, `CJ4_MAX_DISCARDS`, `CJ4_MAX_MELDS`) instead of adding dynamic containers.
- When adding a new opponent, create exactly one public header and one implementation file for that opponent: `include/cjong4/opponent/opponent_<name>.h` and `src/opponent_<name>.c`.
- Expose each opponent factory as `cj4_opponent_<name>(int ctx_level)` and keep the public declaration in that opponent's dedicated header.
- When adding a new opponent, update `CMakeLists.txt` and `tests/test_opponents.c` together with the new source/header so build coverage and basic behavior checks stay in sync.
- Action selection is strict: `cj4m_step()` asserts that a delegate returns one of the offered `cj4_action` values. If you change action generation, keep the selected/offered action structures exactly consistent.
- Tests are plain C executables using `assert`, not a third-party framework. New tests usually follow the existing pattern: build a `cj4_player_view` or small `cj4_mahjong` fixture and assert the selected action or rendered result.
- Root tests are split between `tests/test_opponents.c` and `tests/test_winning_results.c`.
- The checked-in submodule already has its own `.github/copilot-instructions.md`; keep this root file aligned with it, but prefer the root file for repository-level guidance such as submodule location and root-level build commands.
