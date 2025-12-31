/*
 * sish_errors.c - 完全な日本語エラーメッセージシステム
 *
 * すべてのzshエラーを妹口調の日本語に変換
 */

#include "zsh.mdh"
#include "sish.h"

/* エラーメッセージの日本語変換テーブル */
typedef struct {
    const char *pattern;       /* 英語メッセージのパターン */
    const char *japanese;      /* 日本語メッセージ */
    const char *hint;          /* ヒント */
} SishErrorMap;

static const SishErrorMap sish_error_map[] = {
    /* ファイル・ディレクトリ関連 */
    {"can't open", "「%s」が開けないよ〜", "ファイルの存在とパーミッションを確認してね！"},
    {"no such file", "「%s」なんてファイル、ないよ？", "lsコマンドで確認してみて！"},
    {"not found", "「%s」が見つからないよ〜", "スペルミスとかない？"},
    {"restricted", "「%s」は制限されてるみたい...", "sudoが必要かも！"},
    {"permission denied", "「%s」の権限がないよ...", "chmodで権限を変更してみて！"},
    {"read-only", "「%s」は読み取り専用だよ！", "書き込みできないよ〜"},
    {"is a directory", "「%s」はディレクトリだよ！", "cdで移動してみる？"},
    {"not a directory", "「%s」はディレクトリじゃないよ〜", "パスを確認してね！"},
    
    /* コマンド実行関連 */
    {"command not found", "「%s」なんてコマンド、知らないよ〜", "もしかして: %s"},
    {"command too long", "コマンドが長すぎるよ！", "もう少し短くしてみて！"},
    {"exec failed", "「%s」の実行に失敗しちゃった...", "ファイルの形式を確認してね！"},
    {"fork failed", "プロセスの作成に失敗しちゃった...", "システムリソースを確認してね！"},
    {"bad interpreter", "「%s」のインタープリタがおかしいよ...", "shebang行を確認してね！"},
    
    /* パイプ・リダイレクト */
    {"pipe failed", "パイプでエラーが起きちゃった...", "コマンドの接続を確認してね！"},
    {"cannot duplicate fd", "ファイルディスクリプタ%dの複製に失敗...", "開いているファイルが多すぎるかも！"},
    {"redirection", "リダイレクトでエラーが起きたよ...", "構文を確認してね！"},
    
    /* 変数・パラメータ */
    {"parameter not set", "変数「%s」が設定されてないよ〜", "値を代入してから使ってね！"},
    {"not an identifier", "「%s」は変数名として使えないよ！", "英数字とアンダースコアだけにしてね！"},
    {"bad subscript", "添字「%s」がおかしいよ...", "配列のインデックスを確認してね！"},
    {"invalid subscript", "添字が無効だよ〜", "数値または正しい形式で指定してね！"},
    {"subscript too big", "添字%dが大きすぎるよ！", "配列のサイズを確認してね！"},
    {"subscript too small", "添字%dが小さすぎるよ！", "正の値を使ってね！"},
    {"array too large", "配列が大きすぎるよ！", "もう少し小さくしてみて！"},
    {"attempt to assign array", "配列を普通の変数に代入しようとしてるよ！", "配列構文を使ってね！"},
    {"attempt to set associative array", "連想配列の操作がおかしいよ...", "正しい構文で指定してね！"},
    {"nested associative arrays", "連想配列のネストはまだサポートされてないよ〜", "別の方法を考えてね！"},
    
    /* 数式 */
    {"bad math expression", "数式がおかしいよ〜", "括弧や演算子を確認してね！"},
    {"division by zero", "ゼロで割ろうとしてるよ！", "割る数は0以外にしてね！"},
    {"bad floating point", "浮動小数点数の形式がおかしいよ...", "数値の書き方を確認してね！"},
    {"stack overflow", "スタックオーバーフローが起きちゃった！", "式が複雑すぎるかも！"},
    {"recursion limit", "再帰の深さが限界を超えたよ！", "FUNCNESTの値を増やしてみて！"},
    
    /* パース・構文エラー */
    {"parse error", "構文エラーだよ〜", "コマンドの書き方を確認してね！"},
    {"closing bracket", "閉じ括弧が足りないよ！", "括弧の対応を確認してね！"},
    {"unmatched", "対応する「%c」がないよ！", "閉じ忘れてない？"},
    {"bad pattern", "パターン「%s」がおかしいよ...", "正規表現の書き方を確認してね！"},
    {"bad substitution", "置換の書き方がおかしいよ〜", "${...}の構文を確認してね！"},
    
    /* ジョブ制御 */
    {"job table full", "ジョブテーブルがいっぱいだよ！", "バックグラウンドジョブを減らしてね！"},
    {"no such job", "そんなジョブ、ないよ？", "jobsコマンドで確認してね！"},
    
    /* メモリ */
    {"out of memory", "メモリが足りないよ！", "他のプロセスを終了してみて！"},
    {"out of heap memory", "ヒープメモリが不足してるよ！", "システムのメモリを確認してね！"},
    
    /* モジュール */
    {"invalid module", "モジュール「%s」が無効だよ...", "モジュール名を確認してね！"},
    {"circular dependencies", "モジュール「%s」で循環依存が起きてるよ！", "依存関係を見直してね！"},
    {"autoloading failed", "自動ロード「%s」に失敗しちゃった...", "モジュールが正しくインストールされてるか確認してね！"},
    
    /* グロブ */
    {"no match", "パターンに一致するものがないよ〜", "ファイル名パターンを確認してね！"},
    {"current directory lost", "カレントディレクトリが見失われちゃった！", "ディレクトリが削除されたかも！"},
    
    /* その他 */
    {"failed to change user ID", "ユーザーIDの変更に失敗したよ...", "権限を確認してね！"},
    {"failed to change group ID", "グループIDの変更に失敗したよ...", "権限を確認してね！"},
    {"bad set of key/value", "キー/値のペアがおかしいよ〜", "連想配列の構文を確認してね！"},
    {"closing brace", "閉じ波括弧「}」が足りないよ！", "括弧の対応を確認してね！"},
    {"write error", "書き込みエラーが起きたよ...", "ディスク容量を確認してね！"},
    {"read error", "読み込みエラーが起きたよ...", "ファイルが壊れてるかも！"},
    {"lseek", "ファイルシークに失敗したよ...", "ファイルの状態を確認してね！"},
    {"can't trap", "シグナル「%s」はトラップできないよ〜", "別のシグナルを使ってね！"},
    
    {NULL, NULL, NULL}  /* 終端 */
};

/*
 * 英語エラーメッセージを日本語に変換
 */
mod_export char *
sish_translate_error(const char *msg)
{
    static char buf[1024];
    const char *p, *q;
    int i;
    
    if (!msg) return NULL;
    
    /* パターンマッチング */
    for (i = 0; sish_error_map[i].pattern != NULL; i++) {
        p = strstr(msg, sish_error_map[i].pattern);
        if (p) {
            /* マッチした場合、日本語に変換 */
            snprintf(buf, sizeof(buf), "%s💡 %s%s%s",
                    SISH_CHAR_COLOR,
                    sish_error_map[i].japanese,
                    SISH_COLOR_RESET,
                    sish_error_map[i].hint ? "\n       " : "");
            
            if (sish_error_map[i].hint) {
                strncat(buf, SISH_HINT_COLOR, sizeof(buf) - strlen(buf) - 1);
                strncat(buf, sish_error_map[i].hint, sizeof(buf) - strlen(buf) - 1);
                strncat(buf, SISH_COLOR_RESET, sizeof(buf) - strlen(buf) - 1);
            }
            
            return buf;
        }
    }
    
    /* マッチしない場合は一般的なメッセージ */
    snprintf(buf, sizeof(buf), "%sなんかエラーが起きちゃった...%s\n       %s元のメッセージ: %s%s",
            SISH_ERROR_COLOR, SISH_COLOR_RESET,
            SISH_HINT_COLOR, msg, SISH_COLOR_RESET);
    return buf;
}

/*
 * zerrの日本語ラッパー - 全てのエラーをSish化
 */
mod_export void
sish_zerr(const char *fmt, ...)
{
    va_list ap;
    char buf[1024];
    char *translated;
    
    if (errflag || noerrs) {
        if (noerrs < 2)
            errflag |= ERRFLAG_ERROR;
        return;
    }
    errflag |= ERRFLAG_ERROR;
    
    /* フォーマット文字列を処理 */
    VA_START(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    
    /* 日本語に変換 */
    translated = sish_translate_error(buf);
    
    /* キャラクター付きで出力 */
    fprintf(stderr, "%s%sSish%s：", 
            SISH_CHAR_COLOR, SISH_COLOR_BOLD, SISH_COLOR_RESET);
    fprintf(stderr, "%s\n", translated);
    
    /* GUIに通知 */
    sish_gui_send_emotion(SISH_EMOTION_SAD);
}

/*
 * zerr  namの日本語ラッパー
 */
mod_export void
sish_zerrnam(const char *cmd, const char *fmt, ...)
{
    va_list ap;
    char buf[1024];
    char *translated;
    
    if (errflag || noerrs)
        return;
    errflag |= ERRFLAG_ERROR;
    
    /* フォーマット文字列を処理 */
    VA_START(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    
    /* 日本語に変換 */
    translated = sish_translate_error(buf);
    
    /* コマンド名付きで出力 */
    fprintf(stderr, "%s%s%s%s：", 
            SISH_CMD_COLOR, cmd, SISH_COLOR_RESET, SISH_CHAR_COLOR);
    fprintf(stderr, "%s\n", translated);
    
    /* GUIに通知 */
    sish_gui_send_emotion(SISH_EMOTION_CONFUSED);
}
