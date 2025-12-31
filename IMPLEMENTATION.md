# Sish 完全実装ガイド

このドキュメントは、Sishを完全に実装して現場で使える汎用的なシェルにするための手順を説明します。

## 🎯 実装目標

1. **全エラーメッセージの日本語化**（200以上）
2. **20の主要コマンドに対する高度な補完**
3. **ディレクトリ内の類似ファイル検索**
4. **コンソール上での設定管理**
5. **完全なzsh互換性の維持**

---

## 📋 実装手順

### ステップ1: 新しいSishファイルを作成

以下のファイルが既に作成されています：

- ✅ `zsh-5.9/Src/sish.h` (265行) - ヘッダーファイル
- ✅ `zsh-5.9/Src/sish.c` (~600行) - コア実装
- ✅ `zsh-5.9/Src/sish_errors.c` (~350行) - エラーハンドリング
- ✅ `zsh-5.9/Src/sish_completion.c` (~450行) - 補完システム
- ✅ `zsh-5.9/Src/sish_config.c` (~400行) - 設定システム

### ステップ2: エラーメッセージを完全に日本語化

```bash
# バックアップを取った上で、全エラーを変換
./convert_errors.sh
```

このスクリプトは以下を実行します：

1. **全ての.cファイルにsish.hをインクルード**
   - utils.c以外の全ファイル

2. **zerrをsish_zerrに置換**
   - 200以上のzerr()呼び出しを変換
   - utils.c内の関数定義は保持

3. **zerr namをsish_zerrnamに置換**
   - コマンド固有のエラーメッセージを変換

4. **バックアップの作成**
   - `backup_YYYYMMDD_HHMMSS/` ディレクトリに保存

#### 変換される主なファイル

| ファイル | エラー数 | 主な内容 |
|---------|---------|----------|
| exec.c | 30+ | fork失敗、パイプエラー、コマンド未検出 |
| params.c | 40+ | 読み取り専用変数、添字エラー |
| math.c | 30+ | ゼロ除算、スタックオーバーフロー |
| subst.c | 30+ | 置換エラー、パラメータ展開 |
| lex.c | 5 | パースエラー、括弧の不一致 |
| mem.c | 4 | **重要** メモリ不足エラー |
| glob.c | 2 | グロブパターンエラー |
| jobs.c | 2 | ジョブテーブル満杯 |

### ステップ3: Sishコマンドを追加

```bash
# sish-configコマンドをビルトインに追加
./add_sish_commands.sh
```

このスクリプトは以下を実行します：

1. **builtin.cにsish.hをインクルード**

2. **bin_sish_config関数を追加**
```c
int bin_sish_config(char *nam, char **args, Options ops, int func)
{
    return sish_config_command(nam, args, ops, func);
}
```

3. **BUILTINテーブルに登録**
```c
BUILTIN("sish-config", 0, bin_sish_config, 0, 0, 0, NULL, NULL),
```

### ステップ4: ビルドシステムの更新

`zsh-5.9/Src/zsh.mdd` は既に更新済みです：

```makefile
objects="builtin.o compat.o cond.o context.o \
exec.o glob.o hashtable.o hashnameddir.o \
hist.o init.o input.o jobs.o lex.o linklist.o loop.o math.o \
mem.o module.o options.o params.o parse.o pattern.o prompt.o signals.o \
signames.o sort.o string.o subst.o text.o utils.o \
openssh_bsd_setres_id.o sish.o sish_errors.o sish_completion.o sish_config.o"

headers="../config.h zsh_system.h zsh.h sigcount.h signals.h \
prototypes.h hashtable.h ztype.h sish.h"
```

### ステップ5: ビルドとインストール

```bash
# 全てをビルド（Sishシェル + GUIコンソール）
./build.sh all

# テスト実行（インストールせずに）
./zsh-5.9/Src/zsh

# システムにインストール
sudo ./build.sh install
```

---

## 🔍 実装の詳細

### エラーハンドリングシステム

#### sish_errors.c の仕組み

1. **エラーマッピングテーブル**
```c
static const SishErrorMap sish_error_map[] = {
    {"can't open", "「%s」が開けないよ〜", "ファイルの存在とパーミッションを確認してね！"},
    {"no such file", "「%s」なんてファイル、ないよ？", "lsコマンドで確認してみて！"},
    // ... 50以上のパターン
};
```

2. **パターンマッチング**
```c
char *sish_translate_error(const char *msg)
{
    for (i = 0; sish_error_map[i].pattern != NULL; i++) {
        if (strstr(msg, sish_error_map[i].pattern)) {
            // 日本語メッセージに変換
            // ヒントを追加
            return translated_message;
        }
    }
}
```

3. **ラッパー関数**
```c
void sish_zerr(const char *fmt, ...)
{
    // フォーマット文字列を処理
    vsnprintf(buf, sizeof(buf), fmt, ap);
    
    // 日本語に変換
    translated = sish_translate_error(buf);
    
    // キャラクター付きで出力
    fprintf(stderr, "Sish：%s\n", translated);
    
    // GUIに通知
    sish_gui_send_emotion(SISH_EMOTION_SAD);
}
```

### 補完システム

#### sish_completion.c の仕組み

1. **主要コマンドデータベース**
```c
static FamousCommand famous_commands[] = {
    {"git", "バージョン管理システム", complete_git},
    {"docker", "コンテナ管理", complete_docker},
    {"npm", "Node.jsパッケージマネージャ", complete_npm},
    // ... 20コマンド
};
```

2. **類似ファイル検索**
```c
static int find_similar_files_in_dir(const char *dir, const char *partial, char ***results)
{
    // ディレクトリを開く
    // 各エントリに対して：
    //   - 部分一致チェック
    //   - Levenshtein距離計算（≤3）
    //   - ディレクトリには "/"を追加
    // 距離でソート
    // 上位10個を返す
}
```

3. **スマート補完**
```c
int sish_smart_completion(const char *cmd, const char *arg, char ***suggestions)
{
    // 主要コマンドを検索
    for (int i = 0; famous_commands[i].cmd; i++) {
        if (strcmp(cmd, famous_commands[i].cmd) == 0) {
            return famous_commands[i].completion_func(arg, suggestions);
        }
    }
    
    // デフォルト：ファイル補完
    return find_similar_files_in_dir(".", arg, suggestions);
}
```

### 設定システム

#### sish_config.c の仕組み

1. **メニュー表示**
```c
static void show_main_menu(int selected)
{
    // ヘッダー表示
    // メニュー項目を表示
    // 選択中の項目をハイライト
}
```

2. **キー入力処理**
```c
void sish_show_config_menu(void)
{
    enable_raw_mode();  // 端末をrawモードに
    
    while (running) {
        show_main_menu(selected);
        
        c = getchar();
        
        switch (c) {
            case '\033':  // 矢印キー
                // 選択を移動
                break;
            case '\n':    // Enter
                // 選択した項目を実行
                break;
        }
    }
    
    disable_raw_mode();  // 端末を元に戻す
}
```

3. **設定の保存**
```c
static void config_save(void)
{
    FILE *fp = fopen("~/.sishrc", "w");
    fprintf(fp, "SISH_THEME=pink\n");
    fprintf(fp, "SISH_CHARACTER_NAME=Sish\n");
    // ... 全設定を保存
    fclose(fp);
}
```

---

## ✅ 動作確認

### 1. エラーメッセージのテスト

```bash
# Sishを起動
./zsh-5.9/Src/zsh

# コマンド未検出エラー
giu
# 期待される出力：
# Sish：お兄ちゃん！"giu"って何？"git"の間違いじゃない？

# パーミッションエラー
cat /etc/shadow
# 期待される出力：
# Sish：「/etc/shadow」の権限がないよ...
#        💡 sudoが必要かも！

# ファイル未検出エラー
cat nonexistent.txt
# 期待される出力：
# Sish：「nonexistent.txt」なんてファイル、ないよ？
#        💡 lsコマンドで確認してみて！

# ゼロ除算エラー
echo $((10 / 0))
# 期待される出力：
# Sish：ゼロで割ろうとしてるよ！
#        💡 割る数は0以外にしてね！
```

### 2. 補完機能のテスト

```bash
# Git補完
git ad<TAB>
# → git add

# Docker補完
docker ex<TAB>
# → docker exec

# ディレクトリ補完（類似検索）
cd dokum<TAB>
# → document/ documents/ doc/
```

### 3. 設定システムのテスト

```bash
# 設定メニューを開く
sish-config

# 矢印キーで移動、Enterで選択
# ESCでキャンセル
```

### 4. GUIコンソールのテスト

```bash
# Sish-Consoleを起動
sish-console

# コマンドを実行
ls

# エラーを発生させる
giu

# キャラクターの表情が変わることを確認
```

---

## 🐛 トラブルシューティング

### ビルドエラー

#### エラー：`sish.h: No such file or directory`

**原因**: sish.hがインクルードパスに見つからない

**解決策**:
```bash
# zsh.mddを確認
grep sish.h zsh-5.9/Src/zsh.mdd

# 出力に sish.h が含まれていることを確認
# なければ追加：
echo 'sish.h' >> zsh-5.9/Src/zsh.mdd
```

#### エラー：`undefined reference to 'sish_zerr'`

**原因**: sish_errors.oがリンクされていない

**解決策**:
```bash
# zsh.mddのobjects行を確認
grep "sish_errors.o" zsh-5.9/Src/zsh.mdd

# なければ追加
```

#### エラー：`implicit declaration of function 'sish_translate_error'`

**原因**: 関数宣言がsish.hにない

**解決策**:
```bash
# sish.hに関数宣言を追加
echo 'char *sish_translate_error(const char *msg);' >> zsh-5.9/Src/sish.h
```

### 実行時エラー

#### エラー：`sish-config: command not found`

**原因**: builtin.cにコマンドが登録されていない

**解決策**:
```bash
# add_sish_commands.shを実行
./add_sish_commands.sh

# 再ビルド
./build.sh sish
```

#### エラー：英語のエラーメッセージが表示される

**原因**: 一部のzerrがsish_zerrに置換されていない

**解決策**:
```bash
# 特定のファイルを手動で確認
grep "zerr(" zsh-5.9/Src/exec.c

# 見つかった場合は手動で置換
sed -i 's/zerr(/sish_zerr(/g' zsh-5.9/Src/exec.c

# 再ビルド
./build.sh sish
```

---

## 📊 実装状況チェックリスト

### コアシステム
- [x] sish.h の作成（265行）
- [x] sish.c の作成（~600行）
- [x] sish_errors.c の作成（~350行）
- [x] sish_completion.c の作成（~450行）
- [x] sish_config.c の作成（~400行）
- [x] zsh.mdd の更新

### エラーメッセージ変換
- [ ] exec.c の全エラーを変換（30以上）
- [ ] params.c の全エラーを変換（40以上）
- [ ] math.c の全エラーを変換（30以上）
- [ ] subst.c の全エラーを変換（30以上）
- [ ] lex.c の全エラーを変換（5）
- [ ] mem.c の全エラーを変換（4 - 重要）
- [ ] glob.c の全エラーを変換（2）
- [ ] jobs.c の全エラーを変換（2）
- [ ] その他のファイル

### 補完システム
- [x] 20主要コマンドのリスト
- [x] Git補完関数
- [x] Docker補完関数
- [x] npm/yarn補完関数
- [x] Python補完関数
- [x] cd補完関数（ディレクトリのみ）
- [x] 汎用ファイル補完関数
- [x] 類似ファイル検索（Levenshtein距離）
- [x] ディレクトリマーク（"/"追加）

### 設定システム
- [x] インタラクティブメニュー
- [x] テーマカラー設定
- [x] キャラクター設定
- [x] ショートカット管理
- [x] 補完機能設定
- [x] LLM統合設定
- [x] エラーメッセージ詳細度設定
- [x] GUI連携設定
- [x] 設定リセット機能
- [x] 設定保存機能

### GUIコンソール
- [x] GTK4アプリケーション
- [x] VTE4ターミナル統合
- [x] 8つの感情表現
- [x] Unix socket通信
- [x] 80以上のショートカット
- [x] コマンドスニペット
- [x] 設定ファイル（TOML）

### ビルドシステム
- [x] build.sh スクリプト
- [x] convert_errors.sh スクリプト
- [x] add_sish_commands.sh スクリプト
- [x] 依存関係チェック
- [x] エラーハンドリング

### ドキュメント
- [x] README.md
- [x] README_COMPLETE.md
- [x] IMPLEMENTATION.md（このファイル）
- [x] サンプル設定ファイル

---

## 🚀 次のステップ

### 実装を完了するには：

1. **エラーメッセージ変換を実行**
```bash
./convert_errors.sh
```

2. **Sishコマンドを追加**
```bash
./add_sish_commands.sh
```

3. **ビルド**
```bash
./build.sh all
```

4. **テスト**
```bash
# 各種エラーメッセージをテスト
# 補完機能をテスト
# 設定メニューをテスト
# GUIコンソールをテスト
```

5. **インストール**
```bash
sudo ./build.sh install
```

### さらなる改善

- [ ] より多くのコマンドに対する専用補完
- [ ] LLM統合の実装
- [ ] より詳細なエラーメッセージ
- [ ] パフォーマンス最適化
- [ ] ユニットテストの追加
- [ ] CI/CDパイプライン
- [ ] 国際化（英語版など）
- [ ] プラグインシステム

---

## 📞 サポート

問題が発生した場合：

1. **ログを確認**
```bash
# ビルドログ
./build.sh all 2>&1 | tee build.log

# 実行ログ
./zsh-5.9/Src/zsh -x 2>&1 | tee run.log
```

2. **既知の問題を確認**
   - GitHub Issues
   - FAQ

3. **新しいIssueを作成**
   - エラーメッセージ
   - 実行環境
   - 再現手順

---

**「完全なSish実装まで、もうすぐだよ！お兄ちゃん、頑張って！💪」**
