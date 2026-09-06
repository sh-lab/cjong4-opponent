# Opponent 実装ガイド

このドキュメントは、`cjong4-opponent` に新しい opponent を追加する開発者向けの実装ガイドです。ここでいう opponent は、`external/cjong4` の manager 層から渡される `cj4_player_view` と `cj4_action[]` を見て、1 つの行動を返す `cj4m_player_delegate` 実装を指します。

## 先に押さえる前提

- ゲーム本体の状態遷移や合法手生成は `external/cjong4` が担当する
- このリポジトリ側の責務は「提示された合法手から何を選ぶか」のみ
- `external/cjong4` はサブモジュールなので変更対象にしない
- opponent は部分情報しか見えない。判断材料は `cj4_player_view` に含まれる情報に限定する
- `cj4m_step()` は、delegate が提示されていない行動を返すと失敗する前提で使う

## 既存実装の共通パターン

既存の `betaori` / `kokushi` / `pinfu` / `toitoi` は、いずれも次の形に沿っています。

1. 公開ヘッダで `cj4_opponent_<name>(int ctx_level)` を宣言する
2. `src/opponent_<name>.c` で `static cj4_action ..._decide(...)` を実装する
3. 最後に `cj4m_player_delegate` を返す薄いファクトリ関数を置く

共通補助は `src/opponent_internal.h` にあります。

- `cj4_opponent_find_action()` : 行動列から特定の `cj4_action_type` を探す
- `cj4_opponent_choose_win_or_pass()` : `RON` → `TSUMO` → `PASS` の順で返す
- `cj4_opponent_make_ctx()` : `ctx_level` を delegate の `ctx` に詰める
- `cj4_opponent_ctx_level()` : `void *ctx` から 0 / 1 / 2 を取り出す
- `cj4_opponent_count_riichi_threats()` / `cj4_opponent_tile_is_safe()` : リーチ脅威と安全牌判定の共通補助

まずはこの 2 つを再利用し、必要になってから opponent 固有の補助関数を追加する形が安全です。

## 追加するファイル

新しい opponent を `attack` という名前で追加するなら、変更対象は次の 4 か所です。

| パス | 内容 |
| --- | --- |
| `include/cjong4/opponent/opponent_attack.h` | 公開宣言 |
| `src/opponent_attack.c` | 判断ロジック本体 |
| `CMakeLists.txt` | ライブラリとテスト対象への追加 |
| `tests/test_opponents.c` | 基本方針を検証するテスト追加 |

このリポジトリでは、opponent ごとに公開ヘッダ 1 つ、実装ファイル 1 つを持つ構成でそろえます。

## 公開ヘッダの書き方

既存ヘッダと同じく、`cjong4/manager/delegate.h` を include し、`extern "C"` を付けた最小構成にします。

```c
#ifndef CJ4M_OPPONENT_ATTACK_H
#define CJ4M_OPPONENT_ATTACK_H

#include "cjong4/manager/delegate.h"

#ifdef __cplusplus
extern "C"
{
#endif

cj4m_player_delegate
cj4_opponent_attack(int ctx_level);

#ifdef __cplusplus
}
#endif

#endif /* CJ4M_OPPONENT_ATTACK_H */
```

## 実装ファイルの基本形

実装ファイルでは、まず公開ヘッダと `opponent_internal.h` を include し、必要な query 系ヘッダだけを追加します。

```c
#include "cjong4/opponent/opponent_attack.h"

#include "opponent_internal.h"

static cj4_action
cj4_opponent_attack_decide(
    void *ctx,
    const cj4_player_view *view,
    const cj4_action *actions,
    uint8_t action_count)
{
    const uint8_t level = cj4_opponent_ctx_level(ctx);

    for (uint8_t i = 0; i < action_count; ++i)
    {
        if (actions[i].type == CJ4_ACTION_RON || actions[i].type == CJ4_ACTION_TSUMO)
            return actions[i];
    }

    (void)level;
    return actions[0];
}

cj4m_player_delegate
cj4_opponent_attack(int ctx_level)
{
    return (cj4m_player_delegate){
        .ctx = cj4_opponent_make_ctx(ctx_level),
        .decide = cj4_opponent_attack_decide};
}
```

最初の実装では、次の優先順で考えると破綻しにくくなります。

1. 和了 (`RON` / `TSUMO`) を最優先する
2. phase ごとに許容したい反応を分ける
3. 明確な優先アクション (`RIICHI`, `PON`, `KAN` など) があれば先に拾う
4. 複数の打牌候補がある場合だけ scoring で比較する
5. 最後に安全なフォールバックを置く

## `ctx=0/1/2` の扱い

既存 opponent はすべて `ctx_level` を受け取り、`ctx=1` を標準挙動として扱います。

- `ctx=0`: その opponent らしさを弱める / 慎重にする / 早めに諦める
- `ctx=1`: 標準
- `ctx=2`: その opponent らしさを強める / 強気にする / 粘る

新しい opponent でも、次のルールでそろえるのが安全です。

1. 値域外は共通 helper で 1 に正規化する
2. `ctx=1` を基準挙動として固定する
3. 乱数や hidden state を導入せず、同じ入力に対して同じ action を返す
4. 大きな別ロジックは増やさず、既存の優先度や閾値の差し替えで表現する

## `cj4_player_view` で見てよいもの

既存実装で実際に使っているのは主に次の情報です。

- `view->phase`
- `view->player`
- `view->locations`
- `view->is_riichi[]`

手牌・河・副露・ドラ表示牌は `view->locations` を次の v3 API に渡して取得します。

- `cj4_location_collect_hand(view->locations, view->player)`
- `cj4_location_collect_discards(view->locations)`
- `cj4_location_collect_melds(view->locations, player)`
- `cj4_location_collect_dora_indicators(view->locations)`

たとえば:

- `betaori` は他家リーチと河を見て安全牌を判定する
- `kokushi` は手牌の重複と公開情報から不要牌のスコアを付ける
- `pinfu` は字牌を先に落とす単純ルールを使う
- `toitoi` は対子・刻子候補、見え枚数、リーチ脅威を合わせて採点する

重要なのは、他家の伏せ牌を直接知っている前提でロジックを書かないことです。見えていない情報に依存するアルゴリズムはこの層には置けません。

## phase ごとの考え方

`cj4m_step()` は phase に応じて candidate action を渡してきます。既存実装を見ると、phase ごとに次のように振る舞いを分けています。

| phase | よくある扱い |
| --- | --- |
| `CJ4_PHASE_DRAW` | 自摸後の通常選択。打牌、リーチ、暗槓などを比較する |
| `CJ4_PHASE_DISCARD` | 他家打牌への反応。鳴きを抑える opponent なら `PASS` を選びやすい |
| `CJ4_PHASE_KAKAN_RESOLVE` | 槍槓判定。和了できなければ `PASS` に寄せる実装が多い |
| `CJ4_PHASE_AFTER_CALL` | 鳴き後の打牌選択。通常の打牌選択に近い |

特に `DISCARD` と `KAKAN_RESOLVE` では、無理に鳴かず `cj4_opponent_choose_win_or_pass()` を使うと安全です。`ctx` を導入する場合も、まずは phase ごとの基本方針を固定し、その上で鳴き可否や撤退条件だけを段階化すると整理しやすくなります。

cjong4 3.3.0 では delegate は action 選択ごとに一度だけ呼ばれます。嶺上牌ツモ後も、ツモ和了・打牌・連続槓の候補が同じ `actions` にまとめて渡されます。delegate は各回の `view` と `actions` だけから合法手を返してください。

手牌のシャンテン数、形テン、待ち牌種を判断材料にする場合は、
`cjong4/player/hand_analysis.h` の `cj4p_*` APIを使用できます。これらは
マスク済みの `cj4_player_view` だけを入力とし、他家の非公開情報を参照しません。
通常ツモの残数は `view->live_wall_remaining` から取得できます。

## 打牌選択を実装するときのコツ

単純な先頭優先でも動きますが、既存実装は「残したい牌」を暗黙に表現する scoring を使っています。

### 例: kokushi

- 数牌の中張牌を高スコアで捨てる
- 么九牌は重複しているほど捨てやすくする
- 河や副露で見えている枚数も評価に使う

### 例: toitoi

- 単騎候補は捨てやすく、刻子候補は残す
- 唯一の対子候補は守る
- 他家リーチ時は安全牌に加点する
- 河に同種牌があり、かつ対子でない牌は少し捨てやすくする

打牌ルールを増やす場合も、最終的には「`actions[]` に含まれる `CJ4_ACTION_DISCARD` の中から 1 つ返す」形に落とすと整理しやすくなります。

## CMake の更新

`CMakeLists.txt` では、少なくとも `cj4_opponents` の source list に新しい `src/opponent_<name>.c` を追加します。

```cmake
add_library(cj4_opponents STATIC
    src/opponent_betaori.c
    src/opponent_kokushi.c
    src/opponent_pinfu.c
    src/opponent_toitoi.c
    src/opponent_attack.c
)
```

追加した opponent を CLI やテストで使うなら、対応する source 側の include も必要です。

## テスト追加の考え方

`tests/test_opponents.c` は、opponent ごとに小さな fixture を作って「その方針が出るか」を assert する構成です。新規 opponent でも同じ粒度で追加してください。

おすすめの観点:

1. 和了可能時に `RON` または `TSUMO` を優先する
2. その opponent の特徴的なアクション選択を 1 つ以上確認する
3. 打牌方針があるなら、捨てる牌と残す牌を明示して検証する
4. `ctx=0/1/2` で差が出る代表ケースを 1 つ以上固定する
5. `ctx=1` が基準挙動であることと、同一入力で結果がぶれないことを確認する

テスト実装では、既存の補助関数をそのまま流用できます。

- `make_empty_state()`
- `set_hand()`
- `add_discard()`
- `make_action()`

## CLI で試すとき

`examples/cli/main.c` では `cj4m_player_delegate delegates[CJ4_PLAYER_COUNT]` に opponent を並べ、`cj4m_step(&state, &rules, delegates)` で 1 手ずつ進めています。新しい opponent を手早く観察したい場合は、この配列に差し替えて挙動を見るのが分かりやすいです。

標準挙動を使う場合は、既存 opponent と同じく `1` を渡します。

```c
cj4m_player_delegate delegates[CJ4_PLAYER_COUNT] = {
    cj4_opponent_kokushi(1),
    cj4_opponent_toitoi(1),
    cj4_opponent_pinfu(1),
    cj4_opponent_betaori(1)};
```

## 実装時の注意

- `actions[0]` を返すフォールバックは残すが、そこに頼りすぎない
- `ctx` を使う場合は `cj4_opponent_ctx_level(ctx)` で正規化してから分岐する
- `ctx` を使わない場合だけ `(void)ctx;` を入れる
- 牌 ID そのものではなく、必要に応じて `cj4_tile_get_type()` で牌種評価する
- 鳴き判断を入れるときは、`DISCARD` phase と `DRAW` phase を混同しない
- `external/cjong4` 側の API や構造体を変えない

## 追加後の確認項目

1. 公開ヘッダを include してライブラリ利用側から参照できる
2. `CMakeLists.txt` に source 追加が漏れていない
3. `tests/test_opponents.c` に opponent の性格を表すテストが入っている
4. README など利用者向け文書に、必要なら新 opponent の説明を反映している

このガイドの範囲は opponent 層です。ゲームルールや manager の内部進行そのものを変えたい場合は、`external/cjong4` 側のドキュメントと実装を参照してください。
