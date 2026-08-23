# cjong4-opponent

[![CI](https://github.com/sh-lab/cjong4-opponent/actions/workflows/ci.yml/badge.svg)](https://github.com/sh-lab/cjong4-opponent/actions/workflows/ci.yml)

`cjong4-opponent` は、[cjong4](https://github.com/sh-lab/cjong4) 3.1.0 の manager API で使う `cj4m_player_delegate` をまとめた opponent 集です。ルート側では「どの行動を選ぶか」に集中し、麻雀の状態遷移や得点計算などの本体ロジックはサブモジュール `external/cjong4` が担います。

## このリポジトリで提供するもの

- `cj4_opponents` 静的ライブラリ
- 既存 opponent 8 種
  - `cj4_opponent_betaori(int ctx_level)` : 他家リーチ時の守備徹底度を `ctx=0/1/2` で切り替えられる守備型
  - `cj4_opponent_chanta(int ctx_level)` : チャンタ・ジュンチャン志向で、`ctx=0/1/2` で早鳴きの踏み込みが変わる
  - `cj4_opponent_chiitoi(int ctx_level)` : 七対子を主軸に、`ctx=0/1/2` で断念タイミングを切り替え、断念後は対々和へ寄せる
  - `cj4_opponent_kokushi(int ctx_level)` : 国士無双をどこまで粘るかを `ctx=0/1/2` で切り替えられる特殊手志向
  - `cj4_opponent_pinfu(int ctx_level)` : 他家リーチへの押し引きを `ctx=0/1/2` で切り替えられる平和即リー志向
  - `cj4_opponent_somete(int ctx_level)` : 清一色・混一色を主軸に、配牌次第で字一色も狙う染め手志向
  - `cj4_opponent_tanyao(int ctx_level)` : 么九牌を整理して喰いタンで速度を出し、`ctx=0/1/2` で鳴きやすさと押し引きが変わる速攻型
  - `cj4_opponent_toitoi(int ctx_level)` : 鳴き始めと対々和継続の強さを `ctx=0/1/2` で切り替えられる方針
- 動作例としての CLI サンプル (`examples/cli/main.c`)
- opponent の選択傾向を確認するテスト

## リポジトリ構成

| パス | 役割 |
| --- | --- |
| `include/cjong4/opponent/` | 公開ヘッダ。各 opponent のファクトリ関数を宣言 |
| `src/` | opponent 実装と共通補助 (`opponent_internal.h`) |
| `examples/cli/` | opponent を 4 人に割り当てて対局を進める CLI 例 |
| `tests/` | opponent の選択ロジックと勝利結果表示補助のテスト |
| `external/cjong4/` | ゲーム本体ライブラリのサブモジュール。読み取り専用前提 |

## 前提

このリポジトリは、[sh-lab/cjong4](https://github.com/sh-lab/cjong4) v3.1.0 を `external/cjong4` サブモジュールとして利用します。初回セットアップ時はサブモジュールを取得した状態で作業してください。

```sh
git submodule update --init --recursive
```

GitHubの自動生成ソースアーカイブなど、サブモジュール本体を含まない配布物からビルドする場合は、cjong4 v3.1.0を先にインストールして次のように指定できます。

```sh
cmake -S . -B build \
  -DCJ4_OPPONENT_USE_SYSTEM_CJONG4=ON \
  -DCMAKE_PREFIX_PATH=/path/to/cjong4
```

## ビルド

ライブラリだけをビルドする場合:

```sh
cmake -S . -B build
cmake --build build
```

CLI 例もビルドする場合:

```sh
cmake -S . -B build -DCJ4_OPPONENT_BUILD_EXAMPLES=ON
cmake --build build
```

テストを有効にする場合:

```sh
cmake -S . -B build \
  -DCJ4_OPPONENT_BUILD_TESTS=ON \
  -DCJ4_OPPONENT_BUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## インストール

既定のサブモジュール構成では、任意のprefixへcjong4本体、opponentライブラリ、公開ヘッダ、CMake package filesをインストールできます。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
cmake --install build --config Release --prefix /path/to/prefix
```

CMakeプロジェクトから利用する場合:

```cmake
find_package(cjong4-opponent 1 CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE cjong4::opponents)
```

標準の探索先以外へインストールした場合は、利用側の構成時にprefixを指定します。

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/prefix
```

## 使い方

公開 API はすべて `cj4m_player_delegate` を返します。`external/cjong4` の manager 層に渡して利用します。

```c
#include "cjong4/manager/manager.h"
#include "cjong4/opponent/opponent_betaori.h"
#include "cjong4/opponent/opponent_chanta.h"
#include "cjong4/opponent/opponent_chiitoi.h"
#include "cjong4/opponent/opponent_kokushi.h"
#include "cjong4/opponent/opponent_pinfu.h"
#include "cjong4/opponent/opponent_somete.h"
#include "cjong4/opponent/opponent_tanyao.h"
#include "cjong4/opponent/opponent_toitoi.h"

cj4m_player_delegate delegates[CJ4_PLAYER_COUNT] = {
    cj4_opponent_kokushi(1),
    cj4_opponent_toitoi(1),
    cj4_opponent_somete(1),
    cj4_opponent_chiitoi(1),
};

state = cj4m_step(&state, &rules, delegates);
```

`examples/cli/main.c` では実際にこの形で 4 人分の delegate を組み、`cj4m_step()` を回して対局を進めています。

CLI 例の実行方法:

```sh
./build/cj4_cli
./build/cj4_cli --all-steps
./build/cj4_cli --stats 100
```

`--stats N` は東風戦を `N` ゲーム自動実行し、プレイヤーごとのツモ率・ロン率・リーチ率・放銃率・ノーテン率を分子/分母つきで集計表示します。`--all-steps` とは同時指定できません。

`ctx_level` は opponent ごとに意味が異なりますが、共通して次の 3 段階です。

- `0` : その opponent らしさを弱める / 慎重にする / 早めに諦める
- `1` : 標準
- `2` : その opponent らしさを強める / 強気にする / 粘る

## opponent の実装方針の違い

既存 opponent はいずれも、`cj4_player_view` と `cj4_action[]` を受け取り、その場で 1 手を返すだけの軽量な実装です。

cjong4 3.1.0 の `locations` から手牌・河・副露・ドラ表示牌を復元し、公開済みの牌を打牌評価に利用します。嶺上牌ツモ後も通常のツモ後と同じ一度の action 選択で、全 opponent が和了を最優先します。

- `betaori`
  - 和了可能なら即和了
  - 他家リーチが見えているときは、その相手の河にある牌種を安全牌として優先
  - `ctx=0/1/2` でどこまで守備を徹底するかが変わる
- `chanta`
  - 和了可能なら即和了
  - リーチ可能なら即リーチ
  - 端牌・字牌や 123 / 789 に寄る形を残し、中張牌を優先して整理する
  - `ctx=0/1/2` で副露の早さが変わり、断念後は役牌や対子を残す通常寄り打牌へ切り替わる
- `chiitoi`
  - 和了可能なら即和了
  - リーチ可能なら即リーチ
  - 対子維持を優先し、孤立牌や 3 枚目以降を先に整理する
  - `ctx=0/1/2` で七対子を諦めるタイミングが変わり、諦め後は対々和 fallback に切り替わる
- `kokushi`
  - 和了可能なら即和了
  - リーチ可能なら即リーチ
  - 么九牌以外を高優先度で切り、么九牌は重複や見え枚数を見て整理
  - `ctx=0/1/2` で国士を諦めるタイミングが変わり、諦め後は役牌 fallback に寄る
- `pinfu`
  - 和了可能なら即和了
  - リーチ可能なら即リーチ
  - 字牌の打牌を優先し、そうでなければ先頭の打牌候補を選択
  - `ctx=0/1/2` で他家リーチへの押し引きが変わる
- `somete`
  - 和了可能なら即和了
  - リーチ可能なら即リーチ
  - 配牌ベースで清一色・混一色・字一色の狙いを切り替え、染め方針に合わない牌を優先して整理
  - `ctx=0/1/2` で染め手への副露の踏み込み方が変わる
- `tanyao`
  - 和了可能なら即和了
  - リーチ可能なら、他家リーチへの押し引きを考慮してリーチ
  - 么九牌を優先して整理し、2〜8の数牌による対子や連続形を残す
  - タンヤオを壊さないチー・ポンを採用し、`ctx=0/1/2` で鳴きやすさと対リーチ時の安全牌優先度が変わる
- `toitoi`
  - 和了可能なら即和了
  - リーチ、加槓、大明槓、暗槓、ポンを積極採用
  - 対子や刻子候補を残すように打牌を採点
  - `ctx=0/1/2` で早鳴きしやすさと対々和継続の強さが変わる

## テスト

`tests/test_opponents.c` では各 opponent の基本方針を検証しています。たとえば次のような観点が含まれます。

- betaori がリーチ相手に対して安全牌を優先する
- ドラ表示牌を公開済みの牌として見え枚数に含める
- 大明槓・加槓後の槓ドラ公開前でも、全 opponent が嶺上ツモを優先する
- chanta が中張牌を先に切り、`ctx=0/1/2` で早鳴きが変わる
- chiitoi が対子を残し、`ctx=0/1/2` で七対子継続判断が変わる
- pinfu と betaori が副露より `PASS` を選ぶ
- kokushi が不要牌を優先して整理する
- somete が染め手に不要な牌を切り、染めに合う副露を優先する
- tanyao が么九牌を先に切り、タンヤオを壊す副露を避け、`ctx=0/1/2` で早鳴きが変わる
- toitoi が刻子系アクションや対子維持を優先する
- `ctx=0/1/2` で選択が変化する代表ケース

`tests/test_winning_results.c` は CLI 表示補助が `cj4_collect_winning_results()` を正しく扱えるかを確認する補助テストです。

## 新しい opponent を追加したいとき

実装手順と注意点は `docs/OPPONENT_GUIDE.md` にまとめています。新規 opponent の追加時は、公開ヘッダ、実装ファイル、`CMakeLists.txt`、`tests/test_opponents.c` をセットで更新してください。

## ステータス / Status

1.0.0リリース<br>
1.0.0 release

## ライセンス

このプロジェクトは [MIT License](LICENSE) のもとで公開されています。
