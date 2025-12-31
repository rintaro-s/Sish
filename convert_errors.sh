#!/bin/bash
# convert_errors.sh - すべてのエラーメッセージをSish化するスクリプト

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/zsh-5.9/Src"

echo "========================================="
echo "Sish エラーメッセージ変換スクリプト"
echo "========================================="

# バックアップディレクトリを作成
BACKUP_DIR="$SCRIPT_DIR/backup_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BACKUP_DIR"

echo "📦 ソースファイルをバックアップ中..."
cp -r "$SRC_DIR"/*.c "$BACKUP_DIR/" 2>/dev/null || true
echo "✅ バックアップ完了: $BACKUP_DIR"

# 変換対象のファイルを検索
echo ""
echo "🔍 変換対象のファイルを検索中..."
files_to_convert=$(find "$SRC_DIR" -maxdepth 1 -name "*.c" -type f)
total_files=$(echo "$files_to_convert" | wc -l)
echo "📝 対象ファイル数: $total_files"

# 各ファイルを変換
converted_count=0
error_count=0

for file in $files_to_convert; do
    filename=$(basename "$file")
    
    # sish_*.c ファイルはスキップ
    if [[ "$filename" == sish_*.c ]]; then
        continue
    fi
    
    echo ""
    echo "🔄 変換中: $filename"
    
    # zerrをsish_zerrに置換（関数定義を除く）
    # zerrnam をsish_zerrnamに置換
    
    # 一時ファイルを作成
    temp_file="${file}.tmp"
    
    # sish.hをインクルードしていない場合は追加
    if ! grep -q "#include \"sish.h\"" "$file"; then
        # 最初の#includeの後に追加
        sed '/^#include/a \#include "sish.h"' "$file" > "$temp_file"
        mv "$temp_file" "$file"
        echo "  ✅ #include \"sish.h\" を追加"
    fi
    
    # zerr関数の呼び出しをsish_zerrに変換（関数定義は除く）
    # utils.cのzerr/zerrnam関数定義自体は変更しない
    if [[ "$filename" != "utils.c" ]]; then
        # zerr( → sish_zerr(
        sed -i 's/\bzerr(/sish_zerr(/g' "$file"
        # zerrnam( → sish_zerrnam(
        sed -i 's/\bzerrnam(/sish_zerrnam(/g' "$file"
        
        converted_count=$((converted_count + 1))
        echo "  ✅ エラーメッセージを日本語化"
    else
        echo "  ⏭  utils.c はスキップ（関数定義ファイル）"
    fi
done

# utils.cを特別に処理 - zerrとzerrnamをマクロで再定義
echo ""
echo "🔧 utils.c を特別処理中..."
UTILS_FILE="$SRC_DIR/utils.c"

# utils.cの先頭にマクロ定義を追加（既存の関数定義を残したまま）
if ! grep -q "SISH_ERROR_REDIRECT" "$UTILS_FILE"; then
    # ファイルの先頭に追加
    temp_file="${UTILS_FILE}.tmp"
    cat > "$temp_file" << 'EOF'
/* Sish error message redirection */
#include "sish.h"

/* Original zerr/zerrnam are kept for internal use */
/* External calls will use sish_zerr/sish_zerrnam */

EOF
    cat "$UTILS_FILE" >> "$temp_file"
    mv "$temp_file" "$UTILS_FILE"
    echo "  ✅ utils.c にSishサポートを追加"
fi

echo ""
echo "========================================="
echo "✅ 変換完了！"
echo "========================================="
echo "📊 統計:"
echo "  - 変換したファイル: $converted_count"
echo "  - スキップ: $(($total_files - $converted_count))"
echo ""
echo "💾 バックアップ: $BACKUP_DIR"
echo ""
echo "🔨 次のステップ:"
echo "  1. ./build.sh sish でビルド"
echo "  2. テスト実行"
echo ""
