#!/usr/bin/env bash
set -euo pipefail

echo "🌸 Sish セットアップ開始..."
echo ""

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# zsh-5.9 が既にセットアップされているかチェック（リポジトリ内のローカル install）
if [[ -x "zsh-5.9/install/bin/zsh" ]] && compgen -G "zsh-5.9/install/lib/zsh/*/zsh/zle.so" > /dev/null; then
    echo "✅ Sish は既にセットアップ済みだよ！"
    echo ""
    echo "起動方法："
    echo "  ./sish"
    echo ""
    exit 0
fi

echo "📦 zsh をビルド中..."
cd zsh-5.9

# configure が無い場合は生成する（zsh の標準手順）
if [[ ! -x "./configure" ]]; then
    echo "⚙️  configure を生成中..."
    if [[ -x "./Util/preconfig" ]]; then
        ./Util/preconfig > /dev/null 2>&1 || {
            echo "❌ configure の生成に失敗したよ..."
            echo "ヒント: autoconf/autoheader が必要だよ（例: Ubuntuなら 'sudo apt install autoconf'）"
            exit 1
        }
    else
        echo "❌ Util/preconfig が見つからないよ..."
        exit 1
    fi
fi

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

echo "📥 ローカルにインストール中..."
# 'make install' は man/runhelp の生成環境によって失敗しやすいので、必要なものだけ入れる
make install.bin install.modules install.fns > /dev/null 2>&1 || {
    echo "❌ ローカルインストールに失敗したよ..."
    echo "ヒント: もう少し詳しいログを見るには次を実行してね:"
    echo "  (cd zsh-5.9 && make install.bin install.modules install.fns)"
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
