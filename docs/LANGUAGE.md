# 言語設定 / Language Configuration

Sishは日本語と英語の両方に対応しています。

## 設定方法

### 1. 環境変数で設定（推奨）

```bash
# 英語で使用
export SISH_LANG=en
./sish

# 日本語で使用（デフォルト）
export SISH_LANG=ja
./sish
```

### 2. 設定ファイルで設定

`~/.sishrc` または `config/sishrc` の先頭に以下を追加：

```bash
# 英語で使用
SISH_LANG=en

# 日本語で使用（デフォルト）
SISH_LANG=ja
```

## 対応言語

- `ja` : 日本語（デフォルト）
- `en` : English

## 使用例

### 日本語モード

```bash
$ export SISH_LANG=ja
$ ./sish

🌸 Sish - Sister Shell
💡 ヒント: sish-config で設定メニュー

Sish:~ % hit clone https://github.com/user/repo.git
Sish：お兄ちゃん！"hit"って何？"git"の間違いじゃない？
       ほかにも: hg, apt, pip, cat

Sish:~ % y
Cloning into 'repo'...
```

### English Mode

```bash
$ export SISH_LANG=en
$ ./sish

🌸 Sish - Sister Shell
💡 Hint: Type sish-config for settings menu

Sish:~ % hit clone https://github.com/user/repo.git
Sish: Onii-chan! "hit" isn't a command... Did you mean "git"?
       Other options: hg, apt, pip, cat

Sish:~ % y
Cloning into 'repo'...
```

## カスタムメッセージの追加

言語ファイル `config/sish_i18n.zsh` を編集することで、独自のメッセージを追加できます。

---

# Language Configuration

Sish supports both Japanese and English.

## Configuration Methods

### 1. Using Environment Variables (Recommended)

```bash
# Use English
export SISH_LANG=en
./sish

# Use Japanese (Default)
export SISH_LANG=ja
./sish
```

### 2. Using Configuration File

Add to the beginning of `~/.sishrc` or `config/sishrc`:

```bash
# Use English
SISH_LANG=en

# Use Japanese (Default)
SISH_LANG=ja
```

## Supported Languages

- `ja` : Japanese (Default)
- `en` : English

## Examples

### Japanese Mode

```bash
$ export SISH_LANG=ja
$ ./sish

🌸 Sish - Sister Shell
💡 ヒント: sish-config で設定メニュー

Sish:~ % hit clone https://github.com/user/repo.git
Sish：お兄ちゃん！"hit"って何？"git"の間違いじゃない？
       ほかにも: hg, apt, pip, cat

Sish:~ % y
Cloning into 'repo'...
```

### English Mode

```bash
$ export SISH_LANG=en
$ ./sish

🌸 Sish - Sister Shell
💡 Hint: Type sish-config for settings menu

Sish:~ % hit clone https://github.com/user/repo.git
Sish: Onii-chan! "hit" isn't a command... Did you mean "git"?
       Other options: hg, apt, pip, cat

Sish:~ % y
Cloning into 'repo'...
```

## Adding Custom Messages

You can add custom messages by editing the language file `config/sish_i18n.zsh`.
