# Sish - Sister Shell

**完全に動作する、日本語妹口調のシェル環境**

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