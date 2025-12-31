#!/bin/bash
# add_sish_commands.sh - Sishコマンドをビルトインに追加

set -e

BUILTIN_FILE="/home/rinta/Documents/github/Sish/zsh-5.9/Src/builtin.c"

echo "========================================="
echo "Sishコマンド追加スクリプト"
echo "========================================="

# バックアップ
cp "$BUILTIN_FILE" "${BUILTIN_FILE}.backup"
echo "✅ builtin.c をバックアップ"

# sish.hをインクルード
if ! grep -q '#include "sish.h"' "$BUILTIN_FILE"; then
    # 最初の#include行の後に追加
    sed -i '/#include "builtin.pro"/a #include "sish.h"' "$BUILTIN_FILE"
    echo "✅ #include \"sish.h\" を追加"
fi

# sish-configコマンドのハンドラ関数を追加
# builtin.cの適切な位置に追加するため、bin_true関数の後に追加
if ! grep -q 'bin_sish_config' "$BUILTIN_FILE"; then
    # bin_true関数の後に追加（行番号を検索）
    LINE_NUM=$(grep -n '^bin_true' "$BUILTIN_FILE" | head -1 | cut -d: -f1)
    
    if [ -n "$LINE_NUM" ]; then
        # 関数の終わりを見つける
        END_LINE=$((LINE_NUM + 10))
        
        # ファイルに追加
        {
            head -n $END_LINE "$BUILTIN_FILE"
            cat << 'EOF'

/* Sish configuration command */
/**/
int
bin_sish_config(char *nam, char **args, Options ops, int func)
{
    return sish_config_command(nam, args, ops, func);
}

EOF
            tail -n +$((END_LINE + 1)) "$BUILTIN_FILE"
        } > "${BUILTIN_FILE}.tmp"
        
        mv "${BUILTIN_FILE}.tmp" "$BUILTIN_FILE"
        echo "✅ bin_sish_config 関数を追加"
    fi
fi

# BUILTINテーブルにsish-configを追加
# hashコマンドの後に追加
if ! grep -q 'BUILTIN("sish-config"' "$BUILTIN_FILE"; then
    sed -i '/BUILTIN("hash",.*$/a\    BUILTIN("sish-config", 0, bin_sish_config, 0, 0, 0, NULL, NULL),' "$BUILTIN_FILE"
    echo "✅ sish-config をBUILTINテーブルに追加"
fi

echo ""
echo "========================================="
echo "✅ 完了！"
echo "========================================="
echo ""
echo "追加されたコマンド:"
echo "  - sish-config : インタラクティブ設定メニュー"
echo ""
