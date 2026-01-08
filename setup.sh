#!/usr/bin/env bash
set -euo pipefail

echo "🌸 Sish セットアップ開始..."
echo ""

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# zsh-5.9 が既にビルドされているかチェック
if [[ -x "zsh-5.9/install/bin/zsh" ]]; then
    echo "✅ Sish は既にセットアップ済みだよ！"
    echo ""
    echo "起動方法："
    echo "  ./sish"
    echo ""
    exit 0
fi

echo "📦 zsh をビルド中..."
cd zsh-5.9

# configure がまだ実行されていない、または PREFIX が間違っている場合
if [[ ! -f "Makefile" ]] || ! grep -q "^prefix.*=.*$PWD/install" Makefile 2>/dev/null; then
    echo "⚙️  configure を実行中..."
    ./configure --prefix="$PWD/install"
fi

echo "🔨 make でビルド中... (少し時間がかかるよ)"
make -j$(nproc) > /dev/null 2>&1 || {
    echo "❌ ビルドに失敗したよ..."
    exit 1
}

echo "📥 インストール中..."
make install > /dev/null 2>&1 || {
    echo "❌ インストールに失敗したよ..."
    exit 1
}

cd ..

echo ""
echo "✅ セットアップ完了！"
echo ""
echo "起動方法："
echo "  ./sish"
echo ""
echo "設定メニュー："
echo "  ./sish"
echo "  Sish> sish-config"
echo ""
