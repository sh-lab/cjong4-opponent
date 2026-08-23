#!/usr/bin/env bash
set -euo pipefail

# ビルド（標準出力はそのまま表示）
cmake -S . -B build/root -DCJ4_OPPONENT_BUILD_EXAMPLES=ON
cmake --build build/root

# outputs ディレクトリが無い場合のみ作成
OUTPUT_DIR="outputs"
mkdir -p "$OUTPUT_DIR"

# タイムスタンプ
TIMESTAMP=$(date +%Y%m%d-%H%M%S)

# 出力ファイル
OUTPUT_FILE="$OUTPUT_DIR/output-$TIMESTAMP.txt"

# 実行結果を保存（stdout + stderr）
./build/root/cj4_cli > "$OUTPUT_FILE" 2>&1

echo "Output saved to $OUTPUT_FILE"
