/*
 * sish_errors.c - 完全な日本語エラーメッセージシステム
 *
 * すべてのzshエラーを妹口調の日本語に変換
 */

#include "zsh.mdh"
#include "sish.h"

static const char *
sish_error_lead(SishTone sish_tone)
{
    switch (sish_tone) {
    case SISH_TONE_RELIABLE:
        return "確認して。";
    case SISH_TONE_SWEET:
        return "お兄ちゃん…。";
    case SISH_TONE_QUICK:
        return "急いで直して。";
    case SISH_TONE_TEACHER:
        return "一緒に確認しよう。";
    case SISH_TONE_EMOTIONLESS:
        return "エラー。";
    case SISH_TONE_YANDERE:
        return "…ねえ。";
    case SISH_TONE_STANDARD:
    default:
        return "お兄ちゃん、";
    }
}

static const char *
sish_hint_lead(SishTone sish_tone)
{
    switch (sish_tone) {
    case SISH_TONE_RELIABLE:
        return "ヒント: ";
    case SISH_TONE_SWEET:
        return "ねぇ…: ";
    case SISH_TONE_QUICK:
        return "ヒント: ";
    case SISH_TONE_TEACHER:
        return "ヒント: ";
    case SISH_TONE_EMOTIONLESS:
        return "ヒント: ";
    case SISH_TONE_YANDERE:
        return "…これ。";
    case SISH_TONE_STANDARD:
    default:
        return "ヒント: ";
    }
}

static void
sish_template_format(char *dst, size_t dstsize, const char *tmpl, const char *arg_s, int arg_d, char arg_c)
{
    if (!dst || dstsize == 0) return;
    dst[0] = '\0';
    if (!tmpl) return;

    size_t w = 0;
    for (const char *p = tmpl; *p && w + 1 < dstsize; p++) {
        if (*p != '%') {
            dst[w++] = *p;
            continue;
        }
        p++;
        if (*p == '%') {
            dst[w++] = '%';
            continue;
        }
        if (*p == 's') {
            const char *s = arg_s ? arg_s : "";
            while (*s && w + 1 < dstsize) dst[w++] = *s++;
            continue;
        }
        if (*p == 'd') {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), "%d", arg_d);
            for (const char *q = tmp; *q && w + 1 < dstsize; q++) dst[w++] = *q;
            continue;
        }
        if (*p == 'c') {
            if (w + 1 < dstsize) dst[w++] = arg_c;
            continue;
        }
        /* 未知のフォーマットはそのまま */
        if (w + 1 < dstsize) dst[w++] = '%';
        if (*p && w + 1 < dstsize) dst[w++] = *p;
    }
    dst[w] = '\0';
}

static void
sish_extract_subject(const char *msg, const char *pattern, char *out, size_t outsize)
{
    if (!out || outsize == 0) return;
    out[0] = '\0';
    if (!msg) return;

    const char *p = NULL;
    if (pattern) p = strstr(msg, pattern);
    if (p) {
        p += strlen(pattern);
    } else {
        p = msg;
    }

    /* よくある形式: "...: subject" */
    const char *colon = strrchr(msg, ':');
    if (colon && colon[1]) {
        p = colon + 1;
    }
    while (*p && isspace((unsigned char)*p)) p++;

    strncpy(out, p, outsize);
    out[outsize - 1] = '\0';

    /* 末尾の空白を落とす */
    size_t len = strlen(out);
    while (len > 0 && isspace((unsigned char)out[len - 1])) {
        out[len - 1] = '\0';
        len--;
    }
    if (out[0] == '\0') {
        strncpy(out, msg, outsize);
        out[outsize - 1] = '\0';
    }
}

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
    char ja[512];
    char hint[512];
    char subject[256];
    const char *p;
    int i;
    
    if (!msg) return NULL;

    SishTone sish_tone = sish_get_tone();
    
    /* パターンマッチング */
    for (i = 0; sish_error_map[i].pattern != NULL; i++) {
        p = strstr(msg, sish_error_map[i].pattern);
        if (p) {
            sish_extract_subject(msg, sish_error_map[i].pattern, subject, sizeof(subject));

            int id = 0;
            /* メッセージ末尾に数字があるケース用（雑に拾う） */
            for (const char *t = msg; *t; t++) {
                if (isdigit((unsigned char)*t)) {
                    id = atoi(t);
                    break;
                }
            }
            char ch = '?';
            for (const char *t = msg; *t; t++) {
                if (*t == '\'' && t[1] && t[2] == '\'') {
                    ch = t[1];
                    break;
                }
            }

            sish_template_format(ja, sizeof(ja), sish_error_map[i].japanese, subject, id, ch);
            if (sish_error_map[i].hint) {
                sish_template_format(hint, sizeof(hint), sish_error_map[i].hint, subject, id, ch);
            } else {
                hint[0] = '\0';
            }

            /* マッチした場合、日本語に変換 */
            snprintf(buf, sizeof(buf), "%s%s%s %s%s%s",
                    SISH_CHAR_COLOR,
                    sish_error_lead(sish_tone),
                    SISH_COLOR_RESET,
                    SISH_CHAR_COLOR,
                    ja,
                    SISH_COLOR_RESET);
            
            if (hint[0]) {
                strncat(buf, "\n       ", sizeof(buf) - strlen(buf) - 1);
                strncat(buf, SISH_HINT_COLOR, sizeof(buf) - strlen(buf) - 1);
                strncat(buf, sish_hint_lead(sish_tone), sizeof(buf) - strlen(buf) - 1);
                strncat(buf, hint, sizeof(buf) - strlen(buf) - 1);
                strncat(buf, SISH_COLOR_RESET, sizeof(buf) - strlen(buf) - 1);
            }
            
            return buf;
        }
    }
    
    /* マッチしない場合は一般的なメッセージ */
        snprintf(buf, sizeof(buf), "%s%s%s\n       %s元のメッセージ: %s%s",
            SISH_ERROR_COLOR, sish_error_lead(sish_tone), SISH_COLOR_RESET,
            SISH_HINT_COLOR, msg, SISH_COLOR_RESET);
    return buf;
}

/*
 * zerrの日本語ラッパー - 全てのエラーをSish化
 */

static char *
sish_format_zsh_error(const char *fmt, va_list ap)
{
    FILE *fp;
    char *buf;
    long len;

    fp = tmpfile();
    if (!fp) {
        return ztrdup("");
    }

    /* zshの独自フォーマット（%e/%zなど）を解釈できるのは zerrmsg */
    zerrmsg(fp, fmt ? fmt : "", ap);
    fflush(fp);

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return ztrdup("");
    }
    len = ftell(fp);
    if (len < 0) {
        fclose(fp);
        return ztrdup("");
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return ztrdup("");
    }

    buf = (char *)zalloc((size_t)len + 1);
    if (!buf) {
        fclose(fp);
        return ztrdup("");
    }

    if (len > 0) {
        (void)fread(buf, 1, (size_t)len, fp);
    }
    buf[len] = '\0';
    fclose(fp);
    return buf;
}
mod_export void
sish_zerr(const char *fmt, ...)
{
    va_list ap;
    char *raw;
    char *translated;
    
    if (errflag || noerrs) {
        if (noerrs < 2)
            errflag |= ERRFLAG_ERROR;
        return;
    }
    errflag |= ERRFLAG_ERROR;
    
    /* フォーマット文字列を処理（zsh独自の %e/%z 等に対応） */
    VA_START(ap, fmt);
    raw = sish_format_zsh_error(fmt, ap);
    va_end(ap);

    /* 日本語に変換 */
    translated = sish_translate_error(raw);
    zsfree(raw);
    
    /* キャラクター付きで出力 */
    fprintf(stderr, "%s%sSish%s：", 
            SISH_CHAR_COLOR, SISH_COLOR_BOLD, SISH_COLOR_RESET);
    fprintf(stderr, "%s\n", translated);
        fflush(stderr);
    
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
    char *raw;
    char *translated;
    
    if (errflag || noerrs)
        return;
    errflag |= ERRFLAG_ERROR;
    
    /* フォーマット文字列を処理（zsh独自の %e/%z 等に対応） */
    VA_START(ap, fmt);
    raw = sish_format_zsh_error(fmt, ap);
    va_end(ap);

    /* 日本語に変換 */
    translated = sish_translate_error(raw);
    zsfree(raw);
    
    /* コマンド名付きで出力 */
    fprintf(stderr, "%s%s%s%s：", 
            SISH_CMD_COLOR, cmd, SISH_COLOR_RESET, SISH_CHAR_COLOR);
    fprintf(stderr, "%s\n", translated);
        fflush(stderr);
    
    /* GUIに通知 */
    sish_gui_send_emotion(SISH_EMOTION_CONFUSED);
}
