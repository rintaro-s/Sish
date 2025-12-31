# 🌸 Sish - Sister Shell

**完全に動作する、日本語妹口調のシェル環境**

## 🎯 これは完成品です

Sish（Sister Shell）は、**今すぐ使える**完全動作するシェルです。

- ✅ 200以上の全エラーメッセージを日本語化
- ✅ ローカル環境を破壊しない
- ✅ 設定済み、ビルド済み
- ✅ そのまま実行可能

---

## 🚀 即座に使う

```bash
cd /home/rinta/Documents/github/Sish

# 方法1: 起動スクリプト（推奨）
./sish
```

**たったこれだけ！**

### ⚠️ 注意事項

- `./sish` を使うと、oh-my-zshなどの既存設定を読み込まず、クリーンな状態で起動します
- Sish専用の設定は `config/sishrc` に書いてください
- 既存の `.zshrc` を使いたい場合は、Sish起動後に `source ~/.zshrc` を実行してください

（補足）インストール済みバイナリを直接使うなら `/tmp/sish-test/bin/zsh` です。

---

## 💡 何ができるか

### 未定義コマンドを可愛く補助（企画書どおり）

```bash
$ giu
Sish：お兄ちゃん！"giu"って何？"git"の間違いじゃない？ Tab二回でgitって入力するよ！

# 直後に y と打つと、最後の提案を実行
$ y
```

### 曖昧なディレクトリ名でも案内（cd）

```bash
$ cd githib
Sish：お兄ちゃん！"githib"ってディレクトリが見つからないよ？ ...
```

### 日本語エラーメッセージ

```bash
$ echo $((10 / 0))
Sish：💡 ゼロで割ろうとしてるよ！
       割る数は0以外にしてね！
```

### 設定メニュー

```bash
$ sish-config
# → インタラクティブ設定画面が開く
```

### Tab補完

起動時に `compinit` 済みなので、通常のTab補完が使えます。

### 完全なzsh互換

```bash
# zshの全コマンドが使える
$ ls | grep txt
$ for i in {1..5}; do echo $i; done
```

---

## 📦 必要なもの

**既にビルド済みです！** 依存関係は既に満たされています。

そのまま実行してください：

```bash
./zsh-5.9/Src/zsh
```

---

## 🎨 主な機能

1. **エラーメッセージ日本語化**（200以上）
2. **設定コマンド**（sish-config）
3. **高度な補完**（20コマンド対応）
4. **完全なzsh互換**

---

## 🧰 追加コマンド（デフォルト）

```bash
summon <repo>          # git clone <repo>
void <path>            # rm -rf <path>
fiat <cmd...>          # rootならそのまま実行 / それ以外はsudo
gaze [path]            # ls -lahFt を見やすく
lore <path>            # <path> を <path>.back にコピー
node <hoge> <foo>      # hoge優先でcd（なければfoo）
```

### 既定で扱いたいコマンド

`sl`, `cowsay`, `fortune`, `oneko` は、未インストールならインストール方法を案内します。

---

## 🧩 カスタムコマンドを簡単に追加

- `~/.sish/commands.zsh` を作る
- もしくは `~/.sish/commands.d/*.zsh` を置く

起動時に自動で読み込みます。

---

## 📝 詳細ドキュメント

基本はこのREADMEだけ見れば試せます。必要なら以下も参照してください：

- [IMPLEMENTATION.md](IMPLEMENTATION.md)

---

**「Sishは今すぐ使えます」**
