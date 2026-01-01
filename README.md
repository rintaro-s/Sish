# Sish - Sister Shell

**完全に動作する、日本語妹口調のシェル環境**

---

## 即座に使う

```bash
cd /home/rinta/Documents/github/Sish

# 方法1: 起動スクリプト（推奨）
./sish
```


### 注意事項

- `./sish` を使うと、oh-my-zshなどの既存設定を読み込まず、クリーンな状態で起動します
- Sish専用の設定は `config/sishrc` に書いてください
- 既存の `.zshrc` を使いたい場合は、Sish起動後に `source ~/.zshrc` を実行してください

---

## 何ができるか

### 未定義コマンドを可愛く補助
```bash

Sish:~/Documents/github/Sish % hit clone https://github.com/rintaro-s/rintaro-s.git
Sish：お兄ちゃん！"hit"って何？"git"の間違いじゃない？
       ほかにも: hg, apt, pip, cat

Sish:~/Documents/github/Sish % y
Cloning into 'rintaro-s'...
```

### パスタイポも自動で修正実行

```bash
$ cd github
Sish：お兄ちゃん！"github"ってディレクトリが見つからないよ？ "GitHub"の間違いじゃない？
       それっぽいフォルダ: GitHub
       ヒント: パスを確認してね！

# y を打つだけで自動的に cd GitHub が実行される
$ y
$ pwd
/home/rinta/Documents/GitHub
```

### 日本語エラーメッセージ

```bash

$ echo $((10 / 0))
Sish：ゼロで割ろうとしてるよ！
       割る数は0以外にしてね！
```

### 設定メニュー

```bash
$ sish-config
# → インタラクティブ設定画面が開く
```
### 完全なzsh互換

```bash
# zshの全コマンドが使える
$ ls | grep txt
$ for i in {1..5}; do echo $i; done
```

---

## 必要なもの

```bash
./zsh-5.9/Src/zsh
```

---

## 追加コマンド（デフォルト）

```bash
summon <repo>          # git clone <repo>
void <path>            # rm -rf <path>
fiat <cmd...>          # rootならそのまま実行 / それ以外はsudo
gaze [path]            # ls -lahFt を見やすく
lore <path>            # <path> を <path>.back にコピー
node <hoge> [foo]      # hoge優先でcd（なければfoo）
genesis <dir>          # git init + add . + commit "first commit"
oracle <path...>      # chmod +x を実行
```

---

## カスタムコマンドを簡単に追加

- `~/.sish/commands.zsh` を作る
- もしくは `~/.sish/commands.d/*.zsh` を置く

起動時に自動で読み込みます。

---
