
## Sish - Sister Shell

**No more boring 'black screens'—meet the shell environment with a cute little sister personality.**

> *Translated by GPT-4.1. Please refer to the original Japanese text above for reference.*

> *Most of the project has been translated, but there may still be some mistakes or untranslated parts.*

**Introduction Page：[https://rintaro-s.github.io/Sish/](https://rintaro-s.github.io/Sish/)**  

---

### Quick Start

```bash
cd Sish

# Method 1: Startup script
./sish

# Method 2: GUI with wallpaper and auto Sish launch
cd Sish-Console
cargo run
```

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
   5. Completion
   6. LLM Integration
   7. Error Verbosity
   8. GUI Integration
   9. Reset Settings
 ▶ 0. Save & Exit 

```
#### Full zsh compatibility
```bash
# All zsh commands work
$ ls | grep txt
$ for i in {1..5}; do echo $i; done
```

---

### Requirements
```bash
./zsh-5.9/Src/zsh
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




# Sish - Sister Shell

**無機質な"黒画面"から脱却する、日本語妹口調のシェル環境**

---


## 即座に使う

```bash
cd Sish

# 方法1: 起動スクリプト
./sish

# 方法2: 壁紙が使えて、自動でSishを使えるGUI
cd Sish-Console
cargo run
```

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
