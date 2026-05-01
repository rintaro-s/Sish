
## Sish - Sister-Shell

**No more boring 'black screens'—meet the shell environment with a cute little sister personality.**

Sish is a zsh-compatible shell experience with typo recovery, friendly explanations, an integrated `nicu` TUI workspace, and configurable LLM assistance for failed commands.

> *Translated by GPT-4.1. Please refer to the original Japanese text above for reference.*

> *Most of the project has been translated, but there may still be some mistakes or untranslated parts.*

**Introduction Page：[https://りん.com/pages/sish.html](https://りん.com/pages/sish.html)**  

---

### Quick Start

/**
 * This has been tested only on Debian and Ubuntu.
 */

```bash
cd Sish

# First time only (builds Sish zsh locally)
./setup.sh

# Method 1: Start the shell itself
./sish

# Method 2: Start nicu workspace (terminal + explorer + LLM Assist)
./nicu
```

- **`./sish`**
  Use the shell directly with Sish's typo recovery, personality modes, and friendly command help.
- **`./nicu`**
  Open the keyboard-first TUI workspace that embeds Sish, adds a file explorer, and shows LLM activity in the UI.
  > **Note:** Using nicu mode requires Rust to be installed on your system.

#### Language Configuration

Sish supports both Japanese and English.

```bash
# Use English
export SISH_LANG=en
./sish

# Use Japanese (Default)
export SISH_LANG=ja
./sish
```

See [docs/LANGUAGE.md](docs/LANGUAGE.md) for details.

#### Notes

- Using `./sish` starts with a clean environment, ignoring oh-my-zsh and other configs
- Sish-specific settings go in `config/sishrc`
- If you want to use your existing `.zshrc`, run `source ~/.zshrc` after starting Sish

---

### What Sish Can Do

#### nicu workspace

`nicu` is the keyboard-first TUI workspace for Sish. It runs Sish inside an integrated terminal, adds a file explorer, and surfaces shell-side assist features such as LLM status directly in the UI.

```bash
# Launch nicu
./nicu
```

- **Terminal + Explorer**  
  Use Sish in the main terminal pane while browsing files in the side explorer.
- **LLM Assist panel**  
  See the current LLM querying state, explanation results, and error states without leaving the workspace.
- **Keyboard-first navigation**  
  Use `j` / `k` to move, `h` to go to the parent directory, `l` or `Enter` to open, and `g` / `G` to jump to top or bottom.
- **Keyboard shortcuts**  
  - `Ctrl+E`: Switch focus between terminal and explorer
  - `Alt+E`: Open explorer
  - `Ctrl+G`: Launch nicu workspace (from sish)
  - `Alt+Q`: Quit/escape

#### LLM assist

In `LLM Integration`, you can configure the endpoint, model, max tokens, auto explanation for failed commands, and the response language.

- **Failure explanation**  
  Sish can summarize failed commands into cause, detail, and concrete next steps.
- **Inline or in nicu**  
  In the normal terminal flow, the explanation appears inline. Inside `nicu`, the status and preview are reflected in the LLM Assist area.
- **Configurable backend**  
  Use your preferred compatible endpoint and model.

#### Cute help for unknown commands
```bash
Sish:~/Documents/github/Sish % hit clone https://github.com/rintaro-s/rintaro-s.git
Sish: Onii-chan! "hit" isn't a command… Did you mean "git"?
         Other options: hg, apt, pip, cat

Sish:~/Documents/github/Sish % y
Cloning into 'rintaro-s'...
```

#### Auto typo correction
```bash
Sish:~/Documents/github/Sish % cd Github
Sish: Onii-chan! "Github" doesn't exist… Did you mean "GitHub"?
         Similar folders: GitHub
         Hint: Please check the path!

Sish:~/Documents/github/Sish % y
Sish:~/Documents/github/Sish/GitHub %
```

#### Auto case correction
```bash
Sish:~/Documents/github/Sish % cd github

Sish:~/Documents/github/Sish/GitHub % cd ..

Sish:~/Documents/github/Sish % mkdir github

Sish:~/Documents/github/Sish % cd github

Sish:~/Documents/github/Sish/github %
```

#### Settings menu
```bash
$ sish-config
╔═══════════════════════════════════════════════════════════════╗
║          Sish Settings Menu - I'll match your preferences!    ║
╚═══════════════════════════════════════════════════════════════╝

   1. Theme Color
   2. Tone / Personality
   3. Character (includes Language)
   4. Shortcuts
   5. Completion Settings
   6. LLM Integration
   7. Error Verbosity
   8. GUI Integration & Display
   9. Reset Settings
 ▶ 0. Save & Exit 

```

In `LLM Integration`, you can configure the endpoint, model, max tokens, auto explanation for failed commands, and the response language. When auto explanation runs, Sish now shows a visible querying state, boxed result, and boxed error output in both the normal terminal flow and inside `nicu`.

#### Full zsh compatibility
```bash
# All zsh commands work
$ ls | grep txt
$ for i in {1..5}; do echo $i; done
```

---


### Requirements

**Minimum requirements for Sish core (TUI only):**
```bash
# Ubuntu/Debian
sudo apt-get update && sudo apt-get install -y \
  build-essential autoconf pkg-config libncursesw5-dev

# Fedora/RHEL
sudo dnf install -y gcc make autoconf ncurses-devel

# Arch Linux
sudo pacman -S base-devel autoconf ncurses
```

**Known compatibility:**
- Ubuntu 20.04, 22.04, 24.04 (ncurses 5.x and 6.x)
- Debian 11, 12
- Docker (ubuntu:24.04, debian:12, etc.)
- Arch (Nyarch-2026/04/20)

**Build output:**
```bash
./zsh-5.9/install/bin/zsh  # Sish core executable
```

---

### Default Extra Commands
```bash
summon <repo>          # git clone <repo>
void <path>            # rm -rf <path>
fiat <cmd...>          # run as root if possible / otherwise use sudo
gaze [path]            # easy-to-read ls -lahFt
lore <path>            # copy <path> to <path>.back
genesis <dir>          # git init + add . + commit "first commit"
oracle <path...>      # chmod +x
```

---

### Custom Command Examples
```bash
Sish:~/Documents/github/Sish/GitHub % summon https://github.com/rintaro-s/rintaro-s # git clone
Cloning into 'rintaro-s'...

Sish:~/Documents/github/Sish/GitHub % gaze ./rintaro-s  #  ls -lahFt ./rintaro-s
Total 16K
drwxrwxr-x 8 rinta rinta 4.0K Jan  1 21:22 .git/
drwxrwxr-x 3 rinta rinta 4.0K Jan  1 21:22 ./
-rw-rw-r-- 1 rinta rinta 2.0K Jan  1 21:22 README.md
drwxrwxr-x 3 rinta rinta 4.0K Jan  1 21:22 ../

Sish:~/Documents/github/Sish/GitHub % cd rintaro-s

Sish:~/Documents/github/Sish/GitHub/rintaro-s % lore README.md  # create backup file
Sish:~/Documents/github/Sish/GitHub/rintaro-s % ls
README.md  README.md.back
Sish:~/Documents/github/Sish/GitHub/rintaro-s % gaze . #ls -lahFt .
Total 20K
drwxrwxr-x 3 rinta rinta 4.0K Jan  1 21:24 ./
drwxrwxr-x 8 rinta rinta 4.0K Jan  1 21:22 .git/
-rw-rw-r-- 1 rinta rinta 2.0K Jan  1 21:22 README.md
-rw-rw-r-- 1 rinta rinta 2.0K Jan  1 21:22 README.md.back
drwxrwxr-x 3 rinta rinta 4.0K Jan  1 21:22 ../

Sish:~/Documents/github/Sish/GitHub/rintaro-s % void .git # rm -rf .git

Sish:~/Documents/github/Sish/GitHub/rintaro-s % genesis . # git init + add . + commit "first commit"
Initialized empty Git repository in /home/rinta/Documents/github/Sish/GitHub/rintaro-s/.git/
[master (root-commit) ea82b91] first commit
 2 files changed, 40 insertions(+)
 create mode 100644 README.md
 create mode 100644 README.md.back
```

---

## About Sister Roles (Modes)

Sish has several "Sister Roles (Modes)". You can switch the sister's personality and speech style for fun.

| Mode Name | Features | Example |
|:---|:---|:---|
| Standard Sister | Honest, caring | Sish: Onii-chan! There's no "hit"... You meant "git", right? |
| Reliable Sister | Capable, decisive | Sish: "hit" does not exist. Execute "git" instead?|
| Spoiled Sister | Clingy, a bit timid | Sish: Onii-chan... "hit" isn't there... but "git" is... maybe...? |
| Impatient Sister | Quick, no preamble .(Run the candidate without authentication.)| Sish: "hit" -> "git". Executing now! |
| Tutor Sister | Good at explaining | Sish: Onii-chan, "hit" isn't a command. "git" is for version control! |
| Null Sister | Quiet, no emotion | Sish: "hit" unknown. "git" suggested. |
| Yandere(Obsessive) Sister | Possessive, forceful | Sish: Onii-chan... don't use "hit"... only use "git"... promise me... forever... |



## Useful Installation Commands

### 1. Run Sish immediately after cloning (no global install, works inside repo)

```bash
git clone https://github.com/rintaro-s/Sish.git && cd Sish && cd zsh-5.9 && ./Util/preconfig && ./configure --prefix="$PWD/install" && make -j"$(nproc)" && make install.bin install.modules install.fns && cd .. && SISH_LANG=en ./sish
```

### 2. Fix zsh/zle errors in an existing clone (e.g. `/tmp/Sish`)

```bash
cd /tmp/Sish && rm -rf install && cd zsh-5.9 && (test -f Makefile && make distclean || true) && ./Util/preconfig && ./configure --prefix="$PWD/install" && make -j"$(nproc)" && make install.bin install.modules install.fns && cd .. && SISH_LANG=en ./sish
```

### 3. Use Sish anywhere (global install for your user)

```bash
git clone https://github.com/rintaro-s/Sish.git ~/.local/share/Sish && cd ~/.local/share/Sish && cd zsh-5.9 && ./Util/preconfig && ./configure --prefix="$PWD/install" && make -j"$(nproc)" && make install.bin install.modules install.fns && cd .. && mkdir -p ~/.local/bin && ln -sf "$PWD/sish" sish && export PATH="$HOME/.local/bin:$PATH" && SISH_LANG=en sish
```


# Sish - Sister Shell

(英語を編集して反映させたので逆翻訳になってます。見づらくてすみません。間違ってないかは確認しました)

**無機質な"黒画面"から脱却する、日本語妹口調のシェル環境**

Sish は、zsh 互換の操作性に、タイポ補助・やさしい説明・`nicu` ワークスペース・失敗時の LLM 支援を重ねたシェル体験です。

---


## 即座に使う

DebianとUbuntuのみで動作確認をしています

```bash
cd Sish

# 初回のみ（ローカルに Sish 用 zsh をビルド）
./setup.sh

# 方法1: シェルとして使う
./sish

# 方法2: nicu ワークスペースを起動する
./nicu
```

- **`./sish`**  
  Sish 本体をそのまま使います。タイポ補助、妹口調の説明、独自コマンドを軽く試すならこちらです。
- **`./nicu`**  
  統合ターミナル、ファイルエクスプローラ、LLM Assist をまとめたキーボード中心の作業空間を開きます。
  > **注意:** nicu モードを使用するには、システムに Rust がインストールされている必要があります。

### 言語設定 / Language Configuration

Sishは日本語と英語の両方に対応しています。

```bash
# 英語で使用
export SISH_LANG=en
./sish

# 日本語で使用（デフォルト）
export SISH_LANG=ja
./sish
```

詳細は [docs/LANGUAGE.md](docs/LANGUAGE.md) を参照してください。


### 注意事項

- `./sish` を使うと、oh-my-zshなどの既存設定を読み込まず、クリーンな状態で起動します
- Sish専用の設定は `config/sishrc` に書いてください
- 既存の `.zshrc` を使いたい場合は、Sish起動後に `source ~/.zshrc` を実行してください
- `Ctrl+G` で nicu を起動できます

---

## 何ができるか

### nicu ワークスペース

`nicu` は、Sish のためのキーボード中心 TUI ワークスペースです。Sish を統合ターミナル内で動かしつつ、ファイルエクスプローラや LLM Assist を同じ画面で扱えます。

```bash
# nicu を起動
./nicu
```

- **ターミナル + エクスプローラ**  
  メインのターミナルで Sish を使いながら、横のエクスプローラでファイルを確認できます。
- **LLM Assist パネル**  
  問い合わせ中、結果プレビュー、エラー状態を UI 上で追えます。
- **キーボード中心の移動**  
  `j` / `k` で移動、`h` で親ディレクトリへ、`l` または `Enter` で開く、`g` / `G` で先頭・末尾へ移動できます。
- **キーボードショートカット**  
  - `Ctrl+E`: ターミナルとエクスプローラのフォーカス切替
  - `Alt+E`: エクスプローラを開く
  - `Ctrl+G`: nicu ワークスペースを起動（sishから）
  - `Alt+Q`: 終了/エスケープ

### LLM Assist

`LLM統合設定` では、エンドポイント、モデル、最大トークン数、失敗時の自動解説、生成言語を設定できます。

- **失敗原因の要約**  
  失敗したコマンドを、その場で原因・補足・次の一手に短く整理します。
- **通常ターミナルでも nicu でも見える**  
  通常のターミナルではインライン表示、`nicu` では LLM Assist パネルに状態が反映されます。
- **バックエンドを調整可能**  
  使いたい互換エンドポイントとモデルを選べます。

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
Sish:~/Documents/github/Sish % cd Github
Sish：お兄ちゃん！「Github」は無かったよ… 「GitHub」の間違いじゃない？
       それっぽいフォルダ: GitHub
       ヒント: パスを確認してね！

Sish:~/Documents/github/Sish % yy
Sish：お兄ちゃん！"yy"って何？"hg"の間違いじゃない？
       ほかにも: yum, ls, cd, cp

Sish:~/Documents/github/Sish % y
Sish:~/Documents/github/Sish/GitHub % 

```

### 自動大文字小文字修正

```bash
Sish:~/Documents/github/Sish % cd github 

Sish:~/Documents/github/Sish/GitHub % cd ..

Sish:~/Documents/github/Sish % mkdir github

Sish:~/Documents/github/Sish % cd github

Sish:~/Documents/github/Sish/github %
```

### 日本語エラーメッセージ

```bash

Sish:~/Documents/github/Sish/GitHub % echo $((10 / 0)) 
Sish：💡 ゼロで割ろうとしてるよ！                                                                                                    
       割る数は0以外にしてね！
```

### 設定メニュー

```bash
$ sish-config
╔═══════════════════════════════════════════════════════════════╗
║        Sish 設定メニュー - お兄ちゃんの好みに合わせるよ！    ║
╚═══════════════════════════════════════════════════════════════╝

   1. テーマカラー設定
   2. 口調・パーソナリティ設定
   3. キャラクター設定（言語を含む）
   4. ショートカット管理
   5. 補完機能設定
   6. LLM統合設定
   7. エラーメッセージ詳細度
   8. GUI連携・表示設定
   9. 設定をリセット
 ▶ 0. 設定を保存して終了

```

`LLM統合設定` では、エンドポイント、モデル、最大トークン数、失敗時の自動解説、生成言語を設定できます。自動解説が動いたときは、通常ターミナルでも `nicu` 内でも、問い合わせ中表示・枠付き結果・枠付きエラーが見えるようになりました。

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
genesis <dir>          # git init + add . + commit "first commit"
oracle <path...>      # chmod +x を実行
```

---

## カスタムコマンド例

```bash
Sish:~/Documents/github/Sish/GitHub % summon https://github.com/rintaro-s/rintaro-s # git clone
Cloning into 'rintaro-s'...

Sish:~/Documents/github/Sish/GitHub % gaze ./rintaro-s  #  ls -lahFt ./rintaro-s
合計 16K
drwxrwxr-x 8 rinta rinta 4.0K Jan  1 21:22 .git/
drwxrwxr-x 3 rinta rinta 4.0K Jan  1 21:22 ./
-rw-rw-r-- 1 rinta rinta 2.0K Jan  1 21:22 README.md
drwxrwxr-x 3 rinta rinta 4.0K Jan  1 21:22 ../

Sish:~/Documents/github/Sish/GitHub % cd rintaro-s 

Sish:~/Documents/github/Sish/GitHub/rintaro-s % lore README.md  #バックアップファイル作成
Sish:~/Documents/github/Sish/GitHub/rintaro-s % ls
README.md  README.md.back
Sish:~/Documents/github/Sish/GitHub/rintaro-s % gaze . #ls -lahFt .
合計 20K
drwxrwxr-x 3 rinta rinta 4.0K Jan  1 21:24 ./
drwxrwxr-x 8 rinta rinta 4.0K Jan  1 21:22 .git/
-rw-rw-r-- 1 rinta rinta 2.0K Jan  1 21:22 README.md
-rw-rw-r-- 1 rinta rinta 2.0K Jan  1 21:22 README.md.back
drwxrwxr-x 3 rinta rinta 4.0K Jan  1 21:22 ../

Sish:~/Documents/github/Sish/GitHub/rintaro-s % void .git # rm -rf .git

Sish:~/Documents/github/Sish/GitHub/rintaro-s % genesis . # git init + add . + commit "first commit"
Initialized empty Git repository in /home/rinta/Documents/github/Sish/GitHub/rintaro-s/.git/
[master (root-commit) ea82b91] first commit
 2 files changed, 40 insertions(+)
 create mode 100644 README.md
 create mode 100644 README.md.back
```

---
## ロール（妹モード）について

Sishは複数の「妹ロール（モード）」を搭載しています。妹の性格や口調を切り替えて楽しめます。

| モード名 | 特徴 | 例 |
|:---|:---|:---|
| 標準妹モード | 素直で世話焼き | Sish：お兄ちゃん！"hit"って無いよ… "git"の間違いじゃない？ |
| しっかり妹モード | 有能・断定的 | Sish："hit" は存在しない。"git" を実行する？ |
| 甘え妹モード | 距離が近い・弱気 | Sish：お兄ちゃん…"hit"って無いみたい… "git"なら、あるよ…？ |
| せっかち妹モード | 短気・即断即決 | Sish："hit" → "git"。実行するね |
| 教え上手妹モード | 説明がうまい | Sish：お兄ちゃん、"hit"はコマンドに無いよ "git"はバージョン管理のコマンドだよ |
| 無感情妹モード | 感情薄い・静か | Sish："hit" 不明。"git" 提案。 |
| ヤンデレ妹モード | 執着心・命令的 | Sish：お兄ちゃん…"hit"なんて使わないで… "git"だけ使って…絶対に… |

---
