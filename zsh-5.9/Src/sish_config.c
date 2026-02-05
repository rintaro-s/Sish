/*
 * sish_config.c - インタラクティブ設定システム
 *
 * コンソール上で様々な設定を変更できるTUIシステム
 */

#include "zsh.mdh"
#include "sish.h"
#include <termios.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* 設定項目 */
typedef struct {
    const char *name;
    const char *description;
    const char *current_value;
    void (*change_func)(void);
} SishConfigItem;

/* 設定ファイルのパス */
static char sish_config_path[PATH_MAX];

/* ここで選んだ設定値（保存用） */
static char sish_cfg_lang[8] = "ja";
static char sish_cfg_theme[16] = "pink";
static int sish_cfg_error_verbosity = 1;
static int sish_cfg_tone = 0;  /* Default: SISH_TONE_STANDARD */

/* 追加設定（保存用） */
static char sish_cfg_char_name[64] = "Sish";
static char sish_cfg_char_expression[64] = "Happy";
static char sish_cfg_char_position[32] = "right-bottom";
static char sish_cfg_char_size[16] = "medium";
static int sish_cfg_char_animation = 1;

/* ウェルカムメッセージ・ヒント表示設定 */
static int sish_cfg_show_welcome = 1;
static int sish_cfg_show_hint = 1;

/* リアルタイム補完（入力中候補表示） */
static int sish_cfg_live_completion_enable = 1;
static int sish_cfg_live_completion_max_candidates = 5;

static int sish_cfg_completion_enable = 1;
static int sish_cfg_completion_fuzzy = 1;
static int sish_cfg_completion_max_candidates = 5;
static int sish_cfg_completion_dir_similarity = 1;
static int sish_cfg_completion_history = 1;

static int sish_cfg_llm_enable = 0;
static char sish_cfg_llm_endpoint[256] = "";
static char sish_cfg_llm_model[128] = "";
static int sish_cfg_llm_max_tokens = 2000;
static int sish_cfg_llm_auto_explain = 0;

static int
sish_cfg_llm_ready(void)
{
    return sish_cfg_llm_enable && sish_cfg_llm_endpoint[0] != '\0';
}

static int sish_cfg_gui_enable = 1;
static char sish_cfg_gui_socket_path[256] = "/tmp/sish-console.sock";
static int sish_cfg_gui_autostart = 0;
static int sish_cfg_gui_expression_sync = 1;

typedef struct {
    char key[32];
    char command[256];
} SishConfigShortcut;

#define SISH_MAX_SHORTCUTS 64
static SishConfigShortcut sish_cfg_shortcuts[SISH_MAX_SHORTCUTS];
static int sish_cfg_shortcut_count = 0;
static int sish_cfg_shortcuts_initialized = 0;

/* 端末設定を保存 */
static struct termios orig_termios;
static int raw_mode_enabled = 0;

/* Raw modeに切り替え */
static void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) != 0) {
        /* 非TTYなどではTUIが動かないので、最低限メッセージだけ出す */
    fprintf(stderr, "%s❌ sish-config: %s%s\n",
        SISH_ERROR_COLOR,
        SISH_TR("端末設定の取得に失敗しちゃった… (ttyじゃないかも)", "Failed to get terminal settings... (maybe not a tty)"),
        SISH_COLOR_RESET);
        raw_mode_enabled = 0;
        return;
    }
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    /* ESC単体や矢印キーのシーケンスを安全に判定するため、短いタイムアウトを入れる */
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1; /* 0.1 sec */
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0) {
        raw_mode_enabled = 1;
    } else {
        raw_mode_enabled = 0;
    }
}

/* 元の端末設定に戻す */
static void disable_raw_mode(void) {
    if (raw_mode_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_enabled = 0;
    }
}

static int read_key_raw(void) {
    unsigned char c;
    for (;;) {
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n == 1) {
            return (int)c;
        }
        if (n == 0) {
            /* タイムアウト or EOF */
            return 0;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        return -1;
    }
}

/* Raw mode (VMIN=0/VTIME>0) で「キーが押されるまで待つ」 */
static int read_key_blocking(void) {
    for (;;) {
        int k = read_key_raw();
        if (k < 0) return -1;
        if (k == 0) continue;
        return k;
    }
}

static void wait_enter_or_esc(void) {
    for (;;) {
        int k = read_key_blocking();
        if (k < 0) return;
        if (k == 27 || k == '\n' || k == '\r') return;
    }
}

static void trim_newline(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

static int is_valid_shortcut_key(const char *s) {
    if (!s || !*s) return 0;
    size_t n = strlen(s);
    if (n >= sizeof(sish_cfg_shortcuts[0].key)) return 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!(isalnum(c) || c == '_' || c == '-' )) {
            return 0;
        }
    }
    return 1;
}

static void shortcuts_init_defaults_if_needed(void) {
    if (sish_cfg_shortcuts_initialized) return;
    sish_cfg_shortcuts_initialized = 1;
    sish_cfg_shortcut_count = 0;
}

static int read_line_canonical(const char *prompt, char *buf, size_t bufsize) {
    if (!buf || bufsize == 0) return 0;

    disable_raw_mode();
    if (prompt) {
        fputs(prompt, stdout);
        fflush(stdout);
    }

    if (!fgets(buf, (int)bufsize, stdin)) {
        enable_raw_mode();
        return 0;
    }
    enable_raw_mode();

    trim_newline(buf);
    return 1;
}

static int read_int_canonical(const char *prompt, int *out_value) {
    char tmp[64];
    if (!read_line_canonical(prompt, tmp, sizeof(tmp))) return 0;
    if (tmp[0] == '\0') return 0;
    char *end = NULL;
    long v = strtol(tmp, &end, 10);
    if (end == tmp) return 0;
    if (out_value) *out_value = (int)v;
    return 1;
}

static void fprint_shell_single_quoted(FILE *fp, const char *s) {
    fputc('\'', fp);
    for (const char *p = s ? s : ""; *p; p++) {
        if (*p == '\'') {
            fputs("'\\''", fp);
        } else {
            fputc(*p, fp);
        }
    }
    fputc('\'', fp);
}

static void write_export_string(FILE *fp, const char *key, const char *value) {
    fprintf(fp, "export %s=", key);
    fprint_shell_single_quoted(fp, value ? value : "");
    fputc('\n', fp);
}

static void write_export_int(FILE *fp, const char *key, int value) {
    fprintf(fp, "export %s=%d\n", key, value);
}

static void copy_string_bounded(char *dst, size_t dstsize, const char *src) {
    if (!dst || dstsize == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dstsize);
    dst[dstsize - 1] = '\0';
}

static void trim_inplace(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
}

static int parse_shell_single_quoted(const char *in, char *out, size_t outsize) {
    /* 期待: '...'(内部の ' は '\'' 形式) */
    if (!in || !out || outsize == 0) return 0;
    out[0] = '\0';

    const char *p = in;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '\'') return 0;
    p++; /* skip opening quote */

    size_t w = 0;
    while (*p) {
        if (*p == '\'') {
            /* end quote OR '\'' sequence */
            if (p[1] == '\\' && p[2] == '\'' && p[3] == '\'') {
                if (w + 1 < outsize) out[w++] = '\'';
                p += 4;
                continue;
            }
            /* end */
            out[w] = '\0';
            return 1;
        }
        if (w + 1 < outsize) out[w++] = *p;
        p++;
    }
    out[w] = '\0';
    return 0;
}

static int parse_line_kv(const char *line, char *key, size_t keysize, char *value, size_t valuesize) {
    if (!line || !key || !value || keysize == 0 || valuesize == 0) return 0;
    key[0] = '\0';
    value[0] = '\0';

    const char *p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '\0' || *p == '#') return 0;

    if (!strncmp(p, "export ", 7)) {
        p += 7;
        while (*p && isspace((unsigned char)*p)) p++;
    }

    const char *eq = strchr(p, '=');
    if (!eq) return 0;

    size_t klen = (size_t)(eq - p);
    if (klen == 0) return 0;
    if (klen >= keysize) klen = keysize - 1;
    memcpy(key, p, klen);
    key[klen] = '\0';
    trim_inplace(key);

    p = eq + 1;
    while (*p && isspace((unsigned char)*p)) p++;
    copy_string_bounded(value, valuesize, p);
    trim_inplace(value);
    return key[0] != '\0';
}

static void config_load(void) {
    FILE *fp = fopen(sish_config_path, "r");
    if (!fp) return;

    char line[1024];
    char key[128];
    char rawval[768];
    char sval[768];

    while (fgets(line, (int)sizeof(line), fp)) {
        trim_newline(line);
        if (!parse_line_kv(line, key, sizeof(key), rawval, sizeof(rawval))) {
            /* alias 行は別扱い */
            const char *p = line;
            while (*p && isspace((unsigned char)*p)) p++;
            if (!strncmp(p, "alias ", 6)) {
                p += 6;
                while (*p && isspace((unsigned char)*p)) p++;
                const char *eq = strchr(p, '=');
                if (!eq) continue;

                char akey[32];
                size_t klen = (size_t)(eq - p);
                if (klen >= sizeof(akey)) klen = sizeof(akey) - 1;
                memcpy(akey, p, klen);
                akey[klen] = '\0';
                trim_inplace(akey);
                if (!is_valid_shortcut_key(akey)) continue;

                if (parse_shell_single_quoted(eq + 1, sval, sizeof(sval))) {
                    shortcuts_init_defaults_if_needed();
                    /* 既存キーがあれば上書き、なければ追加 */
                    int found = 0;
                    for (int i = 0; i < sish_cfg_shortcut_count; i++) {
                        if (!strcmp(sish_cfg_shortcuts[i].key, akey)) {
                            copy_string_bounded(sish_cfg_shortcuts[i].command, sizeof(sish_cfg_shortcuts[i].command), sval);
                            found = 1;
                            break;
                        }
                    }
                    if (!found && sish_cfg_shortcut_count < SISH_MAX_SHORTCUTS) {
                        copy_string_bounded(sish_cfg_shortcuts[sish_cfg_shortcut_count].key, sizeof(sish_cfg_shortcuts[0].key), akey);
                        copy_string_bounded(sish_cfg_shortcuts[sish_cfg_shortcut_count].command, sizeof(sish_cfg_shortcuts[0].command), sval);
                        sish_cfg_shortcut_count++;
                    }
                }
            }
            continue;
        }

        /* 値が '...' なら展開して sval に */
        if (rawval[0] == '\'' && parse_shell_single_quoted(rawval, sval, sizeof(sval))) {
            /* sval に展開済み */
        } else {
            copy_string_bounded(sval, sizeof(sval), rawval);
        }

        if (!strcmp(key, "SISH_LANG")) {
            copy_string_bounded(sish_cfg_lang, sizeof(sish_cfg_lang), sval);
        } else if (!strcmp(key, "SISH_THEME")) {
            copy_string_bounded(sish_cfg_theme, sizeof(sish_cfg_theme), sval);
        } else if (!strcmp(key, "SISH_ERROR_VERBOSITY")) {
            int v = atoi(sval);
            if (v < 1) v = 1;
            if (v > 4) v = 4;
            sish_cfg_error_verbosity = v;
        } else if (!strcmp(key, "SISH_TONE")) {
            int v = atoi(sval);
            if (v < 0) v = 0;
            if (v >= (int)SISH_TONE_COUNT) v = (int)SISH_TONE_COUNT - 1;
            sish_cfg_tone = v;
        } else if (!strcmp(key, "SISH_CHAR_NAME")) {
            copy_string_bounded(sish_cfg_char_name, sizeof(sish_cfg_char_name), sval);
        } else if (!strcmp(key, "SISH_CHAR_EXPRESSION")) {
            copy_string_bounded(sish_cfg_char_expression, sizeof(sish_cfg_char_expression), sval);
        } else if (!strcmp(key, "SISH_CHAR_POSITION")) {
            copy_string_bounded(sish_cfg_char_position, sizeof(sish_cfg_char_position), sval);
        } else if (!strcmp(key, "SISH_CHAR_SIZE")) {
            copy_string_bounded(sish_cfg_char_size, sizeof(sish_cfg_char_size), sval);
        } else if (!strcmp(key, "SISH_CHAR_ANIMATION")) {
            sish_cfg_char_animation = atoi(sval) ? 1 : 0;
        } else if (!strcmp(key, "SISH_SHOW_WELCOME")) {
            sish_cfg_show_welcome = atoi(sval) ? 1 : 0;
        } else if (!strcmp(key, "SISH_SHOW_HINT")) {
            sish_cfg_show_hint = atoi(sval) ? 1 : 0;
        } else if (!strcmp(key, "SISH_LIVE_COMPLETION_ENABLE")) {
            sish_cfg_live_completion_enable = atoi(sval) ? 1 : 0;
        } else if (!strcmp(key, "SISH_LIVE_COMPLETION_MAX_CANDIDATES")) {
            int v = atoi(sval);
            if (v < 1) v = 1;
            if (v > 100) v = 100;
            sish_cfg_live_completion_max_candidates = v;
        } else if (!strcmp(key, "SISH_COMPLETION_ENABLE")) {
            sish_cfg_completion_enable = atoi(sval) ? 1 : 0;
        } else if (!strcmp(key, "SISH_COMPLETION_FUZZY")) {
            sish_cfg_completion_fuzzy = atoi(sval) ? 1 : 0;
        } else if (!strcmp(key, "SISH_COMPLETION_MAX_CANDIDATES")) {
            int v = atoi(sval);
            if (v < 1) v = 1;
            if (v > 1000) v = 1000;
            sish_cfg_completion_max_candidates = v;
        } else if (!strcmp(key, "SISH_COMPLETION_DIR_SIMILARITY")) {
            sish_cfg_completion_dir_similarity = atoi(sval) ? 1 : 0;
        } else if (!strcmp(key, "SISH_COMPLETION_HISTORY")) {
            sish_cfg_completion_history = atoi(sval) ? 1 : 0;
        } else if (!strcmp(key, "SISH_LLM_ENABLE")) {
            sish_cfg_llm_enable = atoi(sval) ? 1 : 0;
        } else if (!strcmp(key, "SISH_LLM_ENDPOINT")) {
            copy_string_bounded(sish_cfg_llm_endpoint, sizeof(sish_cfg_llm_endpoint), sval);
        } else if (!strcmp(key, "SISH_LLM_MODEL")) {
            copy_string_bounded(sish_cfg_llm_model, sizeof(sish_cfg_llm_model), sval);
        } else if (!strcmp(key, "SISH_LLM_MAX_TOKENS")) {
            int v = atoi(sval);
            if (v < 1) v = 1;
            if (v > 200000) v = 200000;
            sish_cfg_llm_max_tokens = v;
        } else if (!strcmp(key, "SISH_LLM_AUTO_EXPLAIN")) {
            sish_cfg_llm_auto_explain = atoi(sval) ? 1 : 0;
        } else if (!strcmp(key, "SISH_GUI_ENABLE")) {
            sish_cfg_gui_enable = atoi(sval) ? 1 : 0;
        } else if (!strcmp(key, "SISH_GUI_SOCKET_PATH")) {
            copy_string_bounded(sish_cfg_gui_socket_path, sizeof(sish_cfg_gui_socket_path), sval);
        } else if (!strcmp(key, "SISH_GUI_AUTOSTART")) {
            sish_cfg_gui_autostart = atoi(sval) ? 1 : 0;
        } else if (!strcmp(key, "SISH_GUI_EXPRESSION_SYNC")) {
            sish_cfg_gui_expression_sync = atoi(sval) ? 1 : 0;
        }
    }

    fclose(fp);
    sish_cfg_theme[sizeof(sish_cfg_theme) - 1] = '\0';
}

/* 画面クリア */
static void sish_clear_screen(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

/* カーソル位置設定 */
static void set_cursor(int row, int col) {
    printf("\033[%d;%dH", row, col);
}

/* 設定ヘッダーを表示 */
static void show_config_header(void) {
    sish_clear_screen();
    set_cursor(1, 1);
    printf("%s%s╔═══════════════════════════════════════════════════════════════╗%s\n",
           SISH_COLOR_BOLD, SISH_COLOR_PINK, SISH_COLOR_RESET);
    printf("%s%s║          %s    ║%s\n",
        SISH_COLOR_BOLD, SISH_COLOR_PINK,
        sish_lang_is_en()
         ? "Sish Settings Menu - I'll match your preferences!"
         : "Sish 設定メニュー - お兄ちゃんの好みに合わせるよ！",
        SISH_COLOR_RESET);
    printf("%s%s╚═══════════════════════════════════════════════════════════════╝%s\n\n",
           SISH_COLOR_BOLD, SISH_COLOR_PINK, SISH_COLOR_RESET);
}

/* メインメニューを表示 */
static void show_main_menu(int selected) {
    const char *menu_items_ja[] = {
        "1. テーマカラー設定",
        "2. 口調・パーソナリティ設定",
        "3. キャラクター設定（言語を含む）",
        "4. ショートカット管理",
        "5. 補完機能設定",
        "6. LLM統合設定",
        "7. エラーメッセージ詳細度",
        "8. GUI連携設定・表示設定",
        "9. 設定をリセット",
        "0. 設定を保存して終了",
        NULL
    };
    const char *menu_items_en[] = {
        "1. Theme Color",
        "2. Tone / Personality",
        "3. Character (includes Language)",
        "4. Shortcuts",
        "5. Completion Settings",
        "6. LLM Integration",
        "7. Error Verbosity",
        "8. GUI Integration & Display",
        "9. Reset Settings",
        "0. Save & Exit",
        NULL
    };
    const char **menu_items = sish_lang_is_en() ? menu_items_en : menu_items_ja;
    
    show_config_header();
    
    for (int i = 0; menu_items[i]; i++) {
        if (i == selected) {
            printf("%s%s ▶ %s %s\n", 
                   SISH_COLOR_BOLD, SISH_CMD_COLOR, 
                   menu_items[i], SISH_COLOR_RESET);
        } else {
            printf("   %s\n", menu_items[i]);
        }
    }
    
    printf("\n%s%s%s\n",
           SISH_HINT_COLOR,
           sish_lang_is_en()
               ? "Use ↑↓ to select, Enter to confirm, ESC to cancel"
               : "↑↓キーで選択、Enterで決定、ESCでキャンセル",
           SISH_COLOR_RESET);

    fflush(stdout);
}

/* テーマカラー設定 */
static void config_theme_color(void) {
    show_config_header();
    printf("%s%s%s\n\n", SISH_CMD_COLOR,
           sish_lang_is_en() ? "Theme Color" : "テーマカラー設定",
           SISH_COLOR_RESET);
    
    const char *themes_ja[] = {
        "1. ピンク（デフォルト）",
        "2. ブルー",
        "3. グリーン",
        "4. パープル",
        "5. オレンジ",
        "6. レインボー",
        NULL
    };
    const char *themes_en[] = {
        "1. Pink (default)",
        "2. Blue",
        "3. Green",
        "4. Purple",
        "5. Orange",
        "6. Rainbow",
        NULL
    };
    const char **themes = sish_lang_is_en() ? themes_en : themes_ja;
    
    for (int i = 0; themes[i]; i++) {
        printf("  %s\n", themes[i]);
    }
    
        printf("\n%s%s%s", SISH_HINT_COLOR,
            sish_lang_is_en() ? "Choose (1-6): " : "選択してね（1-6）: ",
            SISH_COLOR_RESET);
    fflush(stdout);
    
    int choice = read_key_blocking();
    if (choice < 0) return;
    if (choice == 27 || choice == '0') {
        printf("\n%s%s%s\n", SISH_HINT_COLOR,
               sish_lang_is_en() ? "Canceled." : "キャンセルしたよ！",
               SISH_COLOR_RESET);
        sleep(1);
        return;
    }

    switch (choice) {
        case '1': strncpy(sish_cfg_theme, "pink", sizeof(sish_cfg_theme)); break;
        case '2': strncpy(sish_cfg_theme, "blue", sizeof(sish_cfg_theme)); break;
        case '3': strncpy(sish_cfg_theme, "green", sizeof(sish_cfg_theme)); break;
        case '4': strncpy(sish_cfg_theme, "purple", sizeof(sish_cfg_theme)); break;
        case '5': strncpy(sish_cfg_theme, "orange", sizeof(sish_cfg_theme)); break;
        case '6': strncpy(sish_cfg_theme, "rainbow", sizeof(sish_cfg_theme)); break;
        default:
            printf("\n%s%s%s\n", SISH_HINT_COLOR,
                   sish_lang_is_en() ? "Canceled." : "キャンセルしたよ！",
                   SISH_COLOR_RESET);
            sleep(1);
            return;
    }

    sish_cfg_theme[sizeof(sish_cfg_theme) - 1] = '\0';
    printf("\n%s✅ %s%s\n", SISH_CHAR_COLOR,
           sish_lang_is_en() ? "Theme updated! (Use 'Save & Exit' to persist)" : "テーマを変更したよ！（保存は『設定を保存して終了』）",
           SISH_COLOR_RESET);
    sleep(1);
}

/* 口調・パーソナリティ設定 */
static void config_tone(void) {
    show_config_header();
    printf("%s%s%s\n\n", SISH_CMD_COLOR,
           sish_lang_is_en() ? "Tone / Personality" : "口調・パーソナリティ設定",
           SISH_COLOR_RESET);
    
    const char *tones_ja[] = {
        "1. 標準妹モード（お兄ちゃん！、心配口調）",
        "2. しっかり妹モード（断定的、無駄なし）",
        "3. 甘え妹モード（お兄ちゃん…、弱気）",
        "4. せっかち妹モード（短気、即実行）",
        "5. 教え上手妹モード（LLM設定が必要）",
        "6. 無感情妹モード（感情語ゼロ）",
        "7. ヤンデレ妹モード（決め打ち、強引）",
        NULL
    };
    const char *tones_en[] = {
        "1. Standard Sister (worried, caring)",
        "2. Reliable Sister (direct, no fluff)",
        "3. Sweet Sister (soft, timid)",
        "4. Impatient Sister (short-tempered, runs immediately)",
        "5. Teacher Sister (requires LLM)",
        "6. Emotionless Sister (no emotional words)",
        "7. Yandere Sister (forceful, fixated)",
        NULL
    };
    const char **tones = sish_lang_is_en() ? tones_en : tones_ja;
    
        printf("  %s %s%s%s\n\n",
            sish_lang_is_en() ? "Current:" : "現在の設定:",
            SISH_CHAR_COLOR, sish_tone_name(sish_cfg_tone), SISH_COLOR_RESET);
    
    for (int i = 0; tones[i]; i++) {
        if (i == sish_cfg_tone) {
            printf("  %s%s%s%s\n", 
                   SISH_SUGGEST_COLOR, tones[i],
                   sish_lang_is_en() ? "  <- selected" : " ← 現在選択中",
                   SISH_COLOR_RESET);
        } else {
            printf("  %s\n", tones[i]);
        }
    }
    
    printf("\n%s%s%s", SISH_HINT_COLOR,
           sish_lang_is_en() ? "Choose (1-7): " : "選択してね（1-7）: ",
           SISH_COLOR_RESET);
    fflush(stdout);
    
    int choice = read_key_blocking();
    if (choice < 0) return;
    if (choice == 27 || choice == '0') {
        printf("\n%s%s%s\n", SISH_HINT_COLOR,
               sish_lang_is_en() ? "Canceled." : "キャンセルしたよ！",
               SISH_COLOR_RESET);
        sleep(1);
        return;
    }

    if (choice >= '1' && choice <= '7') {
        int new_tone = choice - '1';
        if (new_tone == (int)SISH_TONE_TEACHER && !sish_cfg_llm_ready()) {
            printf("\n%s%s%s\n", SISH_HINT_COLOR,
                   sish_lang_is_en()
                       ? "Tutor Sister requires LLM. Configure LLM settings first."
                       : "教え上手妹モードはLLM設定が必要です。先にLLM設定をしてください。",
                   SISH_COLOR_RESET);
        } else {
            sish_cfg_tone = new_tone;
            sish_set_tone(sish_cfg_tone);
            printf("\n%s✅ %s%s\n", 
                   SISH_CHAR_COLOR,
                   sish_lang_is_en() ? "Tone updated! (Use 'Save & Exit' to persist)" : "口調を変更したよ！（保存は『設定を保存して終了』）",
                   SISH_COLOR_RESET);
        }
    } else {
        printf("\n%s%s%s\n", SISH_HINT_COLOR,
               sish_lang_is_en() ? "Canceled." : "キャンセルしたよ！",
               SISH_COLOR_RESET);
    }
    sleep(1);
}

/* キャラクター設定 */
static void config_character(void) {
    for (;;) {
        show_config_header();
     printf("%s%s%s\n\n", SISH_CMD_COLOR,
         sish_lang_is_en() ? "Character / Language" : "キャラクター設定（言語）",
         SISH_COLOR_RESET);

     printf("  1. %s: %s%s%s\n",
         sish_lang_is_en() ? "Language" : "言語",
         SISH_CHAR_COLOR,
         (!strcmp(sish_cfg_lang, "en") || !strcmp(sish_cfg_lang, "EN")) ? "English" : "日本語",
         SISH_COLOR_RESET);

     printf("  2. %s: %s%s%s\n", sish_lang_is_en() ? "Character Name" : "キャラクター名", SISH_CHAR_COLOR, sish_cfg_char_name, SISH_COLOR_RESET);
     printf("  3. %s: %s%s%s\n", sish_lang_is_en() ? "Default Expression" : "デフォルト表情", SISH_CHAR_COLOR, sish_cfg_char_expression, SISH_COLOR_RESET);
     printf("  4. %s: %s%s%s\n", sish_lang_is_en() ? "Position" : "表示位置", SISH_CHAR_COLOR, sish_cfg_char_position, SISH_COLOR_RESET);
     printf("  5. %s: %s%s%s\n", sish_lang_is_en() ? "Size" : "サイズ", SISH_CHAR_COLOR, sish_cfg_char_size, SISH_COLOR_RESET);
     printf("  6. %s: %s%s%s\n", sish_lang_is_en() ? "Animation" : "アニメーション", SISH_CHAR_COLOR, sish_cfg_char_animation ? (sish_lang_is_en() ? "Enabled" : "有効") : (sish_lang_is_en() ? "Disabled" : "無効"), SISH_COLOR_RESET);
        printf("  0. %s\n", sish_lang_is_en() ? "Back" : "戻る");

     printf("\n%s%s%s", SISH_HINT_COLOR,
         sish_lang_is_en() ? "Choose (0-6): " : "変更する項目を選択（0-6）: ",
         SISH_COLOR_RESET);
        fflush(stdout);

        int choice = read_key_blocking();
        if (choice < 0) return;
        if (choice == 27 || choice == '0') return;

        if (choice == '1') {
            show_config_header();
            printf("%s%s%s\n\n", SISH_CMD_COLOR,
                   sish_lang_is_en() ? "Language" : "言語",
                   SISH_COLOR_RESET);
             printf("  1. %s\n", sish_lang_is_en() ? "Japanese" : "日本語");
            printf("  2. English\n\n");
            printf("%s%s%s", SISH_HINT_COLOR,
                   sish_lang_is_en() ? "Choose (1-2, 0 to back): " : "選択（1-2、0で戻る）: ",
                   SISH_COLOR_RESET);
            fflush(stdout);
            int l = read_key_blocking();
            if (l == '1') {
                strncpy(sish_cfg_lang, "ja", sizeof(sish_cfg_lang));
            } else if (l == '2') {
                strncpy(sish_cfg_lang, "en", sizeof(sish_cfg_lang));
            }
            sish_cfg_lang[sizeof(sish_cfg_lang) - 1] = '\0';
            if (sish_cfg_lang[0]) {
                setenv("SISH_LANG", sish_cfg_lang, 1);
            }
        } else if (choice == '2') {
            char buf[64];
            if (read_line_canonical(sish_lang_is_en() ? "\nNew character name (empty to cancel): " : "\n新しいキャラクター名（空ならキャンセル）: ", buf, sizeof(buf)) && buf[0]) {
                strncpy(sish_cfg_char_name, buf, sizeof(sish_cfg_char_name));
                sish_cfg_char_name[sizeof(sish_cfg_char_name) - 1] = '\0';
            }
        } else if (choice == '3') {
            char buf[64];
            if (read_line_canonical(sish_lang_is_en() ? "\nDefault expression (e.g. Happy/Angry/Sad... empty to cancel): " : "\nデフォルト表情（例: Happy/Angry/Sad… 空ならキャンセル）: ", buf, sizeof(buf)) && buf[0]) {
                strncpy(sish_cfg_char_expression, buf, sizeof(sish_cfg_char_expression));
                sish_cfg_char_expression[sizeof(sish_cfg_char_expression) - 1] = '\0';
            }
        } else if (choice == '4') {
            show_config_header();
            printf("%s%s%s\n\n", SISH_CMD_COLOR, sish_lang_is_en() ? "Position" : "表示位置", SISH_COLOR_RESET);
            printf("  1. left-top\n");
            printf("  2. right-top\n");
            printf("  3. left-bottom\n");
            printf("  4. right-bottom\n\n");
            printf("%s%s%s", SISH_HINT_COLOR,
                   sish_lang_is_en() ? "Choose (1-4, 0 to back): " : "選択（1-4、0で戻る）: ",
                   SISH_COLOR_RESET);
            fflush(stdout);
            int p = read_key_blocking();
            if (p == '1') strncpy(sish_cfg_char_position, "left-top", sizeof(sish_cfg_char_position));
            else if (p == '2') strncpy(sish_cfg_char_position, "right-top", sizeof(sish_cfg_char_position));
            else if (p == '3') strncpy(sish_cfg_char_position, "left-bottom", sizeof(sish_cfg_char_position));
            else if (p == '4') strncpy(sish_cfg_char_position, "right-bottom", sizeof(sish_cfg_char_position));
            sish_cfg_char_position[sizeof(sish_cfg_char_position) - 1] = '\0';
        } else if (choice == '5') {
            show_config_header();
            printf("%s%s%s\n\n", SISH_CMD_COLOR, sish_lang_is_en() ? "Size" : "サイズ", SISH_COLOR_RESET);
            printf("  1. small\n");
            printf("  2. medium\n");
            printf("  3. large\n\n");
            printf("%s%s%s", SISH_HINT_COLOR,
                   sish_lang_is_en() ? "Choose (1-3, 0 to back): " : "選択（1-3、0で戻る）: ",
                   SISH_COLOR_RESET);
            fflush(stdout);
            int s = read_key_blocking();
            if (s == '1') strncpy(sish_cfg_char_size, "small", sizeof(sish_cfg_char_size));
            else if (s == '2') strncpy(sish_cfg_char_size, "medium", sizeof(sish_cfg_char_size));
            else if (s == '3') strncpy(sish_cfg_char_size, "large", sizeof(sish_cfg_char_size));
            sish_cfg_char_size[sizeof(sish_cfg_char_size) - 1] = '\0';
        } else if (choice == '6') {
            sish_cfg_char_animation = !sish_cfg_char_animation;
        }

        printf("\n%s✅ %s%s\n", SISH_CHAR_COLOR,
               sish_lang_is_en() ? "Updated! (Use 'Save & Exit' to persist)" : "変更したよ！（保存は『設定を保存して終了』）",
               SISH_COLOR_RESET);
        sleep(1);
    }
}

/* ショートカット管理 */
static void config_shortcuts(void) {
    shortcuts_init_defaults_if_needed();

    for (;;) {
        show_config_header();
         printf("%s%s%s\n\n", SISH_CMD_COLOR,
             sish_lang_is_en() ? "Shortcuts" : "ショートカット管理",
             SISH_COLOR_RESET);

         printf("  1. %s\n", sish_lang_is_en() ? "List shortcuts" : "登録済みショートカット表示");
         printf("  2. %s\n", sish_lang_is_en() ? "Add shortcut" : "新しいショートカットを追加");
         printf("  3. %s\n", sish_lang_is_en() ? "Delete shortcut" : "ショートカットを削除");
         printf("  4. %s\n", sish_lang_is_en() ? "Edit shortcut" : "ショートカットを編集");
        printf("  0. %s\n", sish_lang_is_en() ? "Back" : "戻る");

         printf("\n%s%s%s", SISH_HINT_COLOR,
             sish_lang_is_en() ? "Choose (0-4): " : "選択してね（0-4）: ",
             SISH_COLOR_RESET);
        fflush(stdout);

        int choice = read_key_blocking();
        if (choice < 0) return;
        if (choice == 27 || choice == '0') return;

        if (choice == '1') {
            sish_clear_screen();
            printf("%s%s:%s\n\n", SISH_CMD_COLOR,
                   sish_lang_is_en() ? "Shortcuts" : "登録済みショートカット",
                   SISH_COLOR_RESET);
            for (int i = 0; i < sish_cfg_shortcut_count; i++) {
                printf("  %s  → %s\n", sish_cfg_shortcuts[i].key, sish_cfg_shortcuts[i].command);
            }
            if (sish_cfg_shortcut_count == 0) {
                printf("  %s\n", sish_lang_is_en() ? "(none)" : "(なし)");
            }
            printf("\n%s%s%s", SISH_HINT_COLOR,
                   sish_lang_is_en() ? "Press Enter/ESC to go back" : "Enter/ESCで戻る",
                   SISH_COLOR_RESET);
            fflush(stdout);
            wait_enter_or_esc();
        } else if (choice == '2') {
            char keybuf[32];
            char cmdbuf[256];
            if (!read_line_canonical(sish_lang_is_en() ? "\nKey (A-Z0-9/_/-, e.g. g, ga. empty to cancel): " : "\nキー（英数字/_/-、例: g, ga。空ならキャンセル）: ", keybuf, sizeof(keybuf)) || !keybuf[0]) {
                continue;
            }
            if (!is_valid_shortcut_key(keybuf)) {
                printf("\n%s❌ %s%s\n", SISH_ERROR_COLOR,
                       sish_lang_is_en() ? "Invalid key." : "キーが不正だよ…",
                       SISH_COLOR_RESET);
                sleep(1);
                continue;
            }
            if (!read_line_canonical(sish_lang_is_en() ? "Command (e.g. git status. empty to cancel): " : "コマンド（例: git status。空ならキャンセル）: ", cmdbuf, sizeof(cmdbuf)) || !cmdbuf[0]) {
                continue;
            }
            if (sish_cfg_shortcut_count >= SISH_MAX_SHORTCUTS) {
                printf("\n%s❌ %s%s\n", SISH_ERROR_COLOR,
                       sish_lang_is_en() ? "Too many shortcuts." : "これ以上増やせないよ…",
                       SISH_COLOR_RESET);
                sleep(1);
                continue;
            }
            strncpy(sish_cfg_shortcuts[sish_cfg_shortcut_count].key, keybuf, sizeof(sish_cfg_shortcuts[0].key));
            strncpy(sish_cfg_shortcuts[sish_cfg_shortcut_count].command, cmdbuf, sizeof(sish_cfg_shortcuts[0].command));
            sish_cfg_shortcuts[sish_cfg_shortcut_count].key[sizeof(sish_cfg_shortcuts[0].key) - 1] = '\0';
            sish_cfg_shortcuts[sish_cfg_shortcut_count].command[sizeof(sish_cfg_shortcuts[0].command) - 1] = '\0';
            sish_cfg_shortcut_count++;
            printf("\n%s✅ %s%s\n", SISH_CHAR_COLOR,
                   sish_lang_is_en() ? "Added." : "追加したよ！",
                   SISH_COLOR_RESET);
            sleep(1);
        } else if (choice == '3') {
            char keybuf[32];
            if (!read_line_canonical(sish_lang_is_en() ? "\nKey to delete (empty to cancel): " : "\n削除するキー（空ならキャンセル）: ", keybuf, sizeof(keybuf)) || !keybuf[0]) {
                continue;
            }
            int found = -1;
            for (int i = 0; i < sish_cfg_shortcut_count; i++) {
                if (!strcmp(sish_cfg_shortcuts[i].key, keybuf)) {
                    found = i;
                    break;
                }
            }
            if (found < 0) {
                printf("\n%s❌ %s%s\n", SISH_ERROR_COLOR,
                       sish_lang_is_en() ? "Not found." : "見つからなかったよ…",
                       SISH_COLOR_RESET);
                sleep(1);
                continue;
            }
            for (int i = found; i < sish_cfg_shortcut_count - 1; i++) {
                sish_cfg_shortcuts[i] = sish_cfg_shortcuts[i + 1];
            }
            sish_cfg_shortcut_count--;
                 printf("\n%s✅ %s%s\n", SISH_CHAR_COLOR,
                     sish_lang_is_en() ? "Deleted." : "削除したよ！",
                     SISH_COLOR_RESET);
            sleep(1);
        } else if (choice == '4') {
            char keybuf[32];
            char cmdbuf[256];
            if (!read_line_canonical(sish_lang_is_en() ? "\nKey to edit (empty to cancel): " : "\n編集するキー（空ならキャンセル）: ", keybuf, sizeof(keybuf)) || !keybuf[0]) {
                continue;
            }
            int found = -1;
            for (int i = 0; i < sish_cfg_shortcut_count; i++) {
                if (!strcmp(sish_cfg_shortcuts[i].key, keybuf)) {
                    found = i;
                    break;
                }
            }
            if (found < 0) {
                printf("\n%s❌ %s%s\n", SISH_ERROR_COLOR,
                       sish_lang_is_en() ? "Not found." : "見つからなかったよ…",
                       SISH_COLOR_RESET);
                sleep(1);
                continue;
            }
            if (!read_line_canonical(sish_lang_is_en() ? "New command (empty to cancel): " : "新しいコマンド（空ならキャンセル）: ", cmdbuf, sizeof(cmdbuf)) || !cmdbuf[0]) {
                continue;
            }
            strncpy(sish_cfg_shortcuts[found].command, cmdbuf, sizeof(sish_cfg_shortcuts[0].command));
            sish_cfg_shortcuts[found].command[sizeof(sish_cfg_shortcuts[0].command) - 1] = '\0';
            printf("\n%s✅ %s%s\n", SISH_CHAR_COLOR,
                   sish_lang_is_en() ? "Updated." : "編集したよ！",
                   SISH_COLOR_RESET);
            sleep(1);
        }
    }
}

/* 補完機能設定 */
static void config_completion(void) {
    for (;;) {
        show_config_header();
        printf("%s%s%s\n\n", SISH_CMD_COLOR,
            sish_lang_is_en() ? "Completion Settings" : "補完機能設定",
            SISH_COLOR_RESET);

        printf("  1. %s: %s%s%s\n", sish_lang_is_en() ? "Live completion" : "リアルタイム補完", SISH_CHAR_COLOR, sish_cfg_live_completion_enable ? (sish_lang_is_en() ? "Enabled" : "有効") : (sish_lang_is_en() ? "Disabled" : "無効"), SISH_COLOR_RESET);
        printf("  2. %s: %s%d%s\n",
            sish_lang_is_en() ? "Live max candidates" : "リアルタイム候補数",
            SISH_CHAR_COLOR, sish_cfg_live_completion_max_candidates, SISH_COLOR_RESET);
        printf("  3. %s: %s%s%s\n", sish_lang_is_en() ? "Mistype suggestions" : "ミス時候補表示", SISH_CHAR_COLOR, sish_cfg_completion_enable ? (sish_lang_is_en() ? "Enabled" : "有効") : (sish_lang_is_en() ? "Disabled" : "無効"), SISH_COLOR_RESET);
        printf("  4. %s: %s%s%s\n", sish_lang_is_en() ? "Fuzzy match" : "ファジーマッチ", SISH_CHAR_COLOR, sish_cfg_completion_fuzzy ? (sish_lang_is_en() ? "Enabled" : "有効") : (sish_lang_is_en() ? "Disabled" : "無効"), SISH_COLOR_RESET);
        printf("  5. %s: %s%d%s\n",
            sish_lang_is_en() ? "Mistype max candidates" : "ミス時候補数",
            SISH_CHAR_COLOR, sish_cfg_completion_max_candidates, SISH_COLOR_RESET);
        printf("  6. %s: %s%s%s\n", sish_lang_is_en() ? "Directory similarity" : "ディレクトリ類似検索", SISH_CHAR_COLOR, sish_cfg_completion_dir_similarity ? (sish_lang_is_en() ? "Enabled" : "有効") : (sish_lang_is_en() ? "Disabled" : "無効"), SISH_COLOR_RESET);
        printf("  7. %s: %s%s%s\n", sish_lang_is_en() ? "History completion" : "コマンド履歴補完", SISH_CHAR_COLOR, sish_cfg_completion_history ? (sish_lang_is_en() ? "Enabled" : "有効") : (sish_lang_is_en() ? "Disabled" : "無効"), SISH_COLOR_RESET);
        printf("  0. %s\n", sish_lang_is_en() ? "Back" : "戻る");

        printf("\n%s%s%s", SISH_HINT_COLOR,
            sish_lang_is_en() ? "Choose (0-7): " : "変更する項目を選択（0-7）: ",
            SISH_COLOR_RESET);
        fflush(stdout);

        int choice = read_key_blocking();
        if (choice < 0) return;
        if (choice == 27 || choice == '0') return;

        if (choice == '1') sish_cfg_live_completion_enable = !sish_cfg_live_completion_enable;
        else if (choice == '2') {
            int v;
            if (read_int_canonical(sish_lang_is_en() ? "\nLive max candidates (e.g. 5. empty to cancel): " : "\nリアルタイム候補数（例: 5。空ならキャンセル）: ", &v)) {
                if (v < 1) v = 1;
                if (v > 100) v = 100;
                sish_cfg_live_completion_max_candidates = v;
            }
        } else if (choice == '3') sish_cfg_completion_enable = !sish_cfg_completion_enable;
        else if (choice == '4') sish_cfg_completion_fuzzy = !sish_cfg_completion_fuzzy;
        else if (choice == '5') {
            int v;
            if (read_int_canonical(sish_lang_is_en() ? "\nMistype max candidates (e.g. 5. empty to cancel): " : "\nミス時候補数（例: 5。空ならキャンセル）: ", &v)) {
                if (v < 1) v = 1;
                if (v > 200) v = 200;
                sish_cfg_completion_max_candidates = v;
            }
        } else if (choice == '6') sish_cfg_completion_dir_similarity = !sish_cfg_completion_dir_similarity;
        else if (choice == '7') sish_cfg_completion_history = !sish_cfg_completion_history;

        printf("\n%s✅ %s%s\n", SISH_CHAR_COLOR,
               sish_lang_is_en() ? "Updated! (Use 'Save & Exit' to persist)" : "変更したよ！（保存は『設定を保存して終了』）",
               SISH_COLOR_RESET);
        sleep(1);
    }
}

/* LLM統合設定 */
static void config_llm(void) {
    for (;;) {
        show_config_header();
         printf("%s%s%s\n\n", SISH_CMD_COLOR,
             sish_lang_is_en() ? "LLM Integration" : "LLM統合設定",
             SISH_COLOR_RESET);

         printf("  1. %s: %s%s%s\n", sish_lang_is_en() ? "Enable" : "LLM統合", SISH_CHAR_COLOR, sish_cfg_llm_enable ? (sish_lang_is_en() ? "Enabled" : "有効") : (sish_lang_is_en() ? "Disabled" : "無効"), SISH_COLOR_RESET);
         printf("  2. %s: %s%s%s\n", sish_lang_is_en() ? "API endpoint" : "APIエンドポイント", SISH_CHAR_COLOR,
             sish_cfg_llm_endpoint[0] ? sish_cfg_llm_endpoint : (sish_lang_is_en() ? "(not set)" : "(未設定)"), SISH_COLOR_RESET);
         printf("  3. %s: %s%s%s\n", sish_lang_is_en() ? "Model" : "モデル", SISH_CHAR_COLOR,
             sish_cfg_llm_model[0] ? sish_cfg_llm_model : (sish_lang_is_en() ? "(not set)" : "(未設定)"), SISH_COLOR_RESET);
         printf("  4. %s: %s%d%s\n", sish_lang_is_en() ? "Max tokens" : "最大トークン", SISH_CHAR_COLOR, sish_cfg_llm_max_tokens, SISH_COLOR_RESET);
         printf("  5. %s: %s%s%s\n", sish_lang_is_en() ? "Auto explain last failure" : "直前失敗の自動解説", SISH_CHAR_COLOR, sish_cfg_llm_auto_explain ? (sish_lang_is_en() ? "Enabled" : "有効") : (sish_lang_is_en() ? "Disabled" : "無効"), SISH_COLOR_RESET);
        printf("  0. %s\n", sish_lang_is_en() ? "Back" : "戻る");

         printf("\n%s%s%s", SISH_HINT_COLOR,
             sish_lang_is_en() ? "Choose (0-5): " : "変更する項目を選択（0-5）: ",
             SISH_COLOR_RESET);
        fflush(stdout);

        int choice = read_key_blocking();
        if (choice < 0) return;
        if (choice == 27 || choice == '0') return;

        if (choice == '1') {
            sish_cfg_llm_enable = !sish_cfg_llm_enable;
        } else if (choice == '2') {
            char buf[256];
            if (read_line_canonical(sish_lang_is_en() ? "\nAPI endpoint (e.g. http://localhost:11434. empty to cancel): " : "\nAPIエンドポイント（例: http://localhost:11434。空ならキャンセル）: ", buf, sizeof(buf)) && buf[0]) {
                strncpy(sish_cfg_llm_endpoint, buf, sizeof(sish_cfg_llm_endpoint));
                sish_cfg_llm_endpoint[sizeof(sish_cfg_llm_endpoint) - 1] = '\0';
            }
        } else if (choice == '3') {
            char buf[128];
            if (read_line_canonical(sish_lang_is_en() ? "\nModel name (e.g. llama3. empty to cancel): " : "\nモデル名（例: llama3。空ならキャンセル）: ", buf, sizeof(buf)) && buf[0]) {
                strncpy(sish_cfg_llm_model, buf, sizeof(sish_cfg_llm_model));
                sish_cfg_llm_model[sizeof(sish_cfg_llm_model) - 1] = '\0';
            }
        } else if (choice == '4') {
            int v;
            if (read_int_canonical(sish_lang_is_en() ? "\nMax tokens (e.g. 2000. empty to cancel): " : "\n最大トークン（例: 2000。空ならキャンセル）: ", &v)) {
                if (v < 1) v = 1;
                if (v > 200000) v = 200000;
                sish_cfg_llm_max_tokens = v;
            }
        } else if (choice == '5') {
            sish_cfg_llm_auto_explain = !sish_cfg_llm_auto_explain;
        }

        printf("\n%s✅ %s%s\n", SISH_CHAR_COLOR,
               sish_lang_is_en() ? "Updated! (Use 'Save & Exit' to persist)" : "変更したよ！（保存は『設定を保存して終了』）",
               SISH_COLOR_RESET);
        sleep(1);
    }
}

/* エラーメッセージ詳細度設定 */
static void config_error_verbosity(void) {
    show_config_header();
    printf("%s%s%s\n\n", SISH_CMD_COLOR,
           sish_lang_is_en() ? "Error Verbosity" : "エラーメッセージ詳細度",
           SISH_COLOR_RESET);
    
    if (sish_lang_is_en()) {
        printf("  1. Concise (default) - short message only\n");
        printf("  2. Standard - message + hint\n");
        printf("  3. Detailed - message + hint + examples\n");
        printf("  4. Ultra - everything + debug info\n");
    } else {
        printf("  1. 簡潔（デフォルト） - 短いメッセージのみ\n");
        printf("  2. 標準 - メッセージ + ヒント\n");
        printf("  3. 詳細 - メッセージ + ヒント + 例\n");
        printf("  4. 超詳細 - すべての情報 + デバッグ情報\n");
    }
    
    printf("\n%s%s%s", SISH_HINT_COLOR,
           sish_lang_is_en() ? "Choose (1-4): " : "選択してね（1-4）: ",
           SISH_COLOR_RESET);
    fflush(stdout);
    
    int choice = read_key_blocking();
    if (choice < 0) return;
    if (choice == 27 || choice == '0') {
        return;
    }

    if (choice >= '1' && choice <= '4') {
        sish_cfg_error_verbosity = (choice - '0');
        printf("\n%s✅ %s%s\n", SISH_CHAR_COLOR,
               sish_lang_is_en() ? "Verbosity updated! (Use 'Save & Exit' to persist)" : "詳細度を変更したよ！（保存は『設定を保存して終了』）",
               SISH_COLOR_RESET);
        sleep(1);
    }
}

/* GUI連携設定 */
static void config_gui(void) {
    for (;;) {
        show_config_header();
         printf("%s%s%s\n\n", SISH_CMD_COLOR,
             sish_lang_is_en() ? "GUI Integration & Display Settings" : "GUI連携・表示設定",
             SISH_COLOR_RESET);

         printf("  1. %s: %s%s%s\n", sish_lang_is_en() ? "Enable GUI" : "GUI連携", SISH_CHAR_COLOR, sish_cfg_gui_enable ? (sish_lang_is_en() ? "Enabled" : "有効") : (sish_lang_is_en() ? "Disabled" : "無効"), SISH_COLOR_RESET);
         printf("  2. %s: %s%s%s\n", sish_lang_is_en() ? "Socket path" : "ソケットパス", SISH_CHAR_COLOR, sish_cfg_gui_socket_path, SISH_COLOR_RESET);
         printf("  3. %s: %s%s%s\n", sish_lang_is_en() ? "Autostart" : "自動起動", SISH_CHAR_COLOR, sish_cfg_gui_autostart ? (sish_lang_is_en() ? "Enabled" : "有効") : (sish_lang_is_en() ? "Disabled" : "無効"), SISH_COLOR_RESET);
         printf("  4. %s: %s%s%s\n", sish_lang_is_en() ? "Expression sync" : "表情同期", SISH_CHAR_COLOR, sish_cfg_gui_expression_sync ? (sish_lang_is_en() ? "Enabled" : "有効") : (sish_lang_is_en() ? "Disabled" : "無効"), SISH_COLOR_RESET);
         printf("  5. %s: %s%s%s\n", sish_lang_is_en() ? "Show welcome message" : "ウェルカムメッセージ表示", SISH_CHAR_COLOR, sish_cfg_show_welcome ? (sish_lang_is_en() ? "Enabled" : "表示") : (sish_lang_is_en() ? "Disabled" : "非表示"), SISH_COLOR_RESET);
         printf("  6. %s: %s%s%s\n", sish_lang_is_en() ? "Show hints" : "ヒント表示", SISH_CHAR_COLOR, sish_cfg_show_hint ? (sish_lang_is_en() ? "Enabled" : "表示") : (sish_lang_is_en() ? "Disabled" : "非表示"), SISH_COLOR_RESET);
        printf("  0. %s\n", sish_lang_is_en() ? "Back" : "戻る");

         printf("\n%s%s%s", SISH_HINT_COLOR,
             sish_lang_is_en() ? "Choose (0-6): " : "変更する項目を選択（0-6）: ",
             SISH_COLOR_RESET);
        fflush(stdout);

        int choice = read_key_blocking();
        if (choice < 0) return;
        if (choice == 27 || choice == '0') return;

        if (choice == '1') {
            sish_cfg_gui_enable = !sish_cfg_gui_enable;
        } else if (choice == '2') {
            char buf[256];
            if (read_line_canonical(sish_lang_is_en() ? "\nSocket path (e.g. /tmp/sish-console.sock. empty to cancel): " : "\nソケットパス（例: /tmp/sish-console.sock。空ならキャンセル）: ", buf, sizeof(buf)) && buf[0]) {
                strncpy(sish_cfg_gui_socket_path, buf, sizeof(sish_cfg_gui_socket_path));
                sish_cfg_gui_socket_path[sizeof(sish_cfg_gui_socket_path) - 1] = '\0';
            }
        } else if (choice == '3') {
            sish_cfg_gui_autostart = !sish_cfg_gui_autostart;
        } else if (choice == '4') {
            sish_cfg_gui_expression_sync = !sish_cfg_gui_expression_sync;
        } else if (choice == '5') {
            sish_cfg_show_welcome = !sish_cfg_show_welcome;
        } else if (choice == '6') {
            sish_cfg_show_hint = !sish_cfg_show_hint;
        }

        printf("\n%s✅ %s%s\n", SISH_CHAR_COLOR,
               sish_lang_is_en() ? "Updated! (Use 'Save & Exit' to persist)" : "変更したよ！（保存は『設定を保存して終了』）",
               SISH_COLOR_RESET);
        sleep(1);
    }
}

/* 設定をリセット */
static void config_reset(void) {
    show_config_header();
    printf("%s⚠️  %s%s\n\n", 
        SISH_ERROR_COLOR,
        sish_lang_is_en() ? "Reset all settings to default?" : "すべての設定をデフォルトに戻すよ？",
        SISH_COLOR_RESET);
    printf("  %s", sish_lang_is_en() ? "Really reset? (y/N): " : "本当にリセットする？ (y/N): ");
    fflush(stdout);
    
    int choice = read_key_blocking();
    if (choice < 0) return;
    if (choice == 27) {
        printf("\n%s%s%s\n", SISH_HINT_COLOR,
               sish_lang_is_en() ? "Canceled." : "キャンセルしたよ！",
               SISH_COLOR_RESET);
        sleep(1);
        return;
    }

    
    if (choice == 'y' || choice == 'Y') {
        /* いま保持している値をデフォルトに戻す */
        strncpy(sish_cfg_theme, "pink", sizeof(sish_cfg_theme));
        sish_cfg_theme[sizeof(sish_cfg_theme) - 1] = '\0';
        sish_cfg_error_verbosity = 1;

        strncpy(sish_cfg_char_name, "Sish", sizeof(sish_cfg_char_name));
        strncpy(sish_cfg_char_expression, "Happy", sizeof(sish_cfg_char_expression));
        strncpy(sish_cfg_char_position, "right-bottom", sizeof(sish_cfg_char_position));
        strncpy(sish_cfg_char_size, "medium", sizeof(sish_cfg_char_size));
        sish_cfg_char_name[sizeof(sish_cfg_char_name) - 1] = '\0';
        sish_cfg_char_expression[sizeof(sish_cfg_char_expression) - 1] = '\0';
        sish_cfg_char_position[sizeof(sish_cfg_char_position) - 1] = '\0';
        sish_cfg_char_size[sizeof(sish_cfg_char_size) - 1] = '\0';
        sish_cfg_char_animation = 1;

        sish_cfg_completion_enable = 1;
        sish_cfg_completion_fuzzy = 1;
        sish_cfg_completion_max_candidates = 5;
        sish_cfg_completion_dir_similarity = 1;
        sish_cfg_completion_history = 1;

        sish_cfg_live_completion_enable = 1;
        sish_cfg_live_completion_max_candidates = 5;

        sish_cfg_llm_enable = 0;
        sish_cfg_llm_endpoint[0] = '\0';
        sish_cfg_llm_model[0] = '\0';
        sish_cfg_llm_max_tokens = 2000;
        sish_cfg_llm_auto_explain = 0;

        sish_cfg_gui_enable = 1;
        strncpy(sish_cfg_gui_socket_path, "/tmp/sish-console.sock", sizeof(sish_cfg_gui_socket_path));
        sish_cfg_gui_socket_path[sizeof(sish_cfg_gui_socket_path) - 1] = '\0';
        sish_cfg_gui_autostart = 0;
        sish_cfg_gui_expression_sync = 1;

        /* Display settings */
        sish_cfg_show_welcome = 1;
        sish_cfg_show_hint = 1;

        /* ショートカットは初期化フラグを落として再生成 */
        sish_cfg_shortcuts_initialized = 0;
        shortcuts_init_defaults_if_needed();

        printf("\n%s✅ %s%s\n", SISH_CHAR_COLOR,
               sish_lang_is_en() ? "Settings reset." : "設定をリセットしたよ！",
               SISH_COLOR_RESET);
        sleep(1);
    } else {
        printf("\n%s%s%s\n", SISH_HINT_COLOR,
               sish_lang_is_en() ? "Canceled." : "キャンセルしたよ！",
               SISH_COLOR_RESET);
        sleep(1);
    }
}

/* 設定を保存 */
static void config_save(void) {
    printf("\n%s💾 %s%s\n", SISH_CHAR_COLOR,
           sish_lang_is_en() ? "Saving settings..." : "設定を保存中...",
           SISH_COLOR_RESET);
    
    /* ~/.sishrc に保存 */
    FILE *fp = fopen(sish_config_path, "w");
    if (fp) {
        fprintf(fp, "# Sish Configuration\n");
        fprintf(fp, "# Generated by Sish Config Tool\n\n");
        write_export_string(fp, "SISH_LANG", sish_cfg_lang);
        write_export_string(fp, "SISH_THEME", sish_cfg_theme);
        write_export_int(fp, "SISH_ERROR_VERBOSITY", sish_cfg_error_verbosity);
        write_export_int(fp, "SISH_TONE", sish_cfg_tone);

        fprintf(fp, "\n# Character\n");
        write_export_string(fp, "SISH_CHAR_NAME", sish_cfg_char_name);
        write_export_string(fp, "SISH_CHAR_EXPRESSION", sish_cfg_char_expression);
        write_export_string(fp, "SISH_CHAR_POSITION", sish_cfg_char_position);
        write_export_string(fp, "SISH_CHAR_SIZE", sish_cfg_char_size);
        write_export_int(fp, "SISH_CHAR_ANIMATION", sish_cfg_char_animation);

        fprintf(fp, "\n# Completion\n");
        write_export_int(fp, "SISH_LIVE_COMPLETION_ENABLE", sish_cfg_live_completion_enable);
        write_export_int(fp, "SISH_LIVE_COMPLETION_MAX_CANDIDATES", sish_cfg_live_completion_max_candidates);
        write_export_int(fp, "SISH_COMPLETION_ENABLE", sish_cfg_completion_enable);
        write_export_int(fp, "SISH_COMPLETION_FUZZY", sish_cfg_completion_fuzzy);
        write_export_int(fp, "SISH_COMPLETION_MAX_CANDIDATES", sish_cfg_completion_max_candidates);
        write_export_int(fp, "SISH_COMPLETION_DIR_SIMILARITY", sish_cfg_completion_dir_similarity);
        write_export_int(fp, "SISH_COMPLETION_HISTORY", sish_cfg_completion_history);

        fprintf(fp, "\n# LLM\n");
        write_export_int(fp, "SISH_LLM_ENABLE", sish_cfg_llm_enable);
        write_export_string(fp, "SISH_LLM_ENDPOINT", sish_cfg_llm_endpoint);
        write_export_string(fp, "SISH_LLM_MODEL", sish_cfg_llm_model);
        write_export_int(fp, "SISH_LLM_MAX_TOKENS", sish_cfg_llm_max_tokens);
        write_export_int(fp, "SISH_LLM_AUTO_EXPLAIN", sish_cfg_llm_auto_explain);

        fprintf(fp, "\n# GUI\n");
        write_export_int(fp, "SISH_GUI_ENABLE", sish_cfg_gui_enable);
        write_export_string(fp, "SISH_GUI_SOCKET_PATH", sish_cfg_gui_socket_path);
        write_export_int(fp, "SISH_GUI_AUTOSTART", sish_cfg_gui_autostart);
        write_export_int(fp, "SISH_GUI_EXPRESSION_SYNC", sish_cfg_gui_expression_sync);

        fprintf(fp, "\n# Display Settings\n");
        write_export_int(fp, "SISH_SHOW_WELCOME", sish_cfg_show_welcome);
        write_export_int(fp, "SISH_SHOW_HINT", sish_cfg_show_hint);

        fprintf(fp, "\n# Shortcuts (aliases)\n");
        shortcuts_init_defaults_if_needed();
        for (int i = 0; i < sish_cfg_shortcut_count; i++) {
            if (!is_valid_shortcut_key(sish_cfg_shortcuts[i].key)) {
                continue;
            }
            fprintf(fp, "alias %s=", sish_cfg_shortcuts[i].key);
            fprint_shell_single_quoted(fp, sish_cfg_shortcuts[i].command);
            fputc('\n', fp);
        }
        fprintf(fp, "\n# Prompt (theme-aware)\n");
        if (!strcmp(sish_cfg_theme, "blue")) {
            fprintf(fp, "PROMPT='%%F{111}🌸 Sish%%f:%%F{111}%%~%%f %%# '\n");
        } else if (!strcmp(sish_cfg_theme, "green")) {
            fprintf(fp, "PROMPT='%%F{82}🌿 Sish%%f:%%F{111}%%~%%f %%# '\n");
        } else if (!strcmp(sish_cfg_theme, "purple")) {
            fprintf(fp, "PROMPT='%%F{141}🪻 Sish%%f:%%F{111}%%~%%f %%# '\n");
        } else if (!strcmp(sish_cfg_theme, "orange")) {
            fprintf(fp, "PROMPT='%%F{208}🍊 Sish%%f:%%F{111}%%~%%f %%# '\n");
        } else if (!strcmp(sish_cfg_theme, "rainbow")) {
            fprintf(fp, "PROMPT='%%F{219}🌈 Sish%%f:%%F{111}%%~%%f %%# '\n");
        } else {
            fprintf(fp, "PROMPT='%%F{213}🌸 Sish%%f:%%F{111}%%~%%f %%# '\n");
        }
         fprintf(fp, "\n# %s\n",
              sish_lang_is_en()
                  ? "(Add aliases/functions below if you want)"
                  : "(必要ならここに alias や関数を追加してね)");
        fclose(fp);
         printf("%s✅ %s%s\n", 
             SISH_CHAR_COLOR,
             sish_lang_is_en() ? "Saved! (~/.sishrc)" : "設定を保存したよ！ (~/.sishrc)",
             SISH_COLOR_RESET);
    } else {
         printf("%s❌ %s%s\n", 
             SISH_ERROR_COLOR,
             sish_lang_is_en() ? "Failed to save settings." : "設定の保存に失敗しちゃった...",
             SISH_COLOR_RESET);
    }
    
    sleep(2);
}

/*
 * インタラクティブ設定メニューを表示
 */
mod_export void
sish_show_config_menu(void)
{
    int selected = 0;
    int running = 1;
    int key;
    int needs_redraw = 1;
    
    /* 設定ファイルパスを設定 */
    snprintf(sish_config_path, sizeof(sish_config_path), 
             "%s/.sishrc", getenv("HOME"));

    /* 既存設定をロードしてUIに反映 */
    shortcuts_init_defaults_if_needed();
    config_load();

    /* UI 表示言語は、明示的な環境変数を優先する */
    {
        const char *env_lang = getenv("SISH_LANG");
        if (env_lang && env_lang[0]) {
            strncpy(sish_cfg_lang, env_lang, sizeof(sish_cfg_lang));
            sish_cfg_lang[sizeof(sish_cfg_lang) - 1] = '\0';
        } else if (sish_cfg_lang[0]) {
            setenv("SISH_LANG", sish_cfg_lang, 1);
        }
    }

    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "%s❌ sish-config: %s%s\n",
            SISH_ERROR_COLOR,
            sish_lang_is_en() ? "Requires an interactive terminal." : "対話端末じゃないと操作できないよ…",
            SISH_COLOR_RESET);
        return;
    }
    
    enable_raw_mode();
    
    while (running) {
        if (needs_redraw) {
            show_main_menu(selected);
            needs_redraw = 0;
        }

        key = read_key_raw();
        if (key < 0) {
            running = 0;
            break;
        }

        if (key == 0) {
            continue;
        }

        switch (key) {
            case 27: { /* ESC */
                int next = read_key_raw();
                if (next < 0) {
                    running = 0;
                    break;
                }
                if (next == 0) {
                    /* タイムアウト＝ESC単体 */
                    running = 0;
                    break;
                }
                if (next != '[') {
                    break;
                }
                int code = read_key_raw();
                if (code == 'A' && selected > 0) {
                    selected--;
                    needs_redraw = 1;
                } else if (code == 'B' && selected < 9) {
                    selected++;
                    needs_redraw = 1;
                }
                break;
            }

            case '\n':
            case '\r':  /* Enter */
                switch (selected) {
                    case 0: config_theme_color(); break;
                    case 1: config_tone(); break;
                    case 2: config_character(); break;
                    case 3: config_shortcuts(); break;
                    case 4: config_completion(); break;
                    case 5: config_llm(); break;
                    case 6: config_error_verbosity(); break;
                    case 7: config_gui(); break;
                    case 8: config_reset(); break;
                    case 9:
                        config_save();
                        running = 0;
                        break;
                }
                needs_redraw = 1;
                break;

            case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9': case '0':
                if (key == '0') {
                    selected = 9;
                } else {
                    selected = (key - '1');
                }
                needs_redraw = 1;
                break;
                
            case 'q':
            case 'Q':
                running = 0;
                break;
        }
    }
    
    disable_raw_mode();
    sish_clear_screen();
        printf("%s%s%s\n",
            SISH_CHAR_COLOR,
            sish_lang_is_en() ? "Run 'sish-config' anytime to adjust settings!" : "また設定したくなったら、'sish-config'を実行してね！",
            SISH_COLOR_RESET);
}

/*
 * sish-configコマンドのエントリーポイント
 */
mod_export int
sish_config_command(char *nam, char **args, Options ops, int func)
{
    sish_show_config_menu();
    return 0;
}
