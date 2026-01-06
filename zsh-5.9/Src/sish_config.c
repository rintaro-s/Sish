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
static char sish_cfg_theme[16] = "pink";
static int sish_cfg_error_verbosity = 1;
static int sish_cfg_tone = 0;  /* Default: SISH_TONE_STANDARD */

/* 追加設定（保存用） */
static char sish_cfg_char_name[64] = "Sish";
static char sish_cfg_char_expression[64] = "Happy";
static char sish_cfg_char_position[32] = "right-bottom";
static char sish_cfg_char_size[16] = "medium";
static int sish_cfg_char_animation = 1;

static int sish_cfg_completion_enable = 1;
static int sish_cfg_completion_fuzzy = 1;
static int sish_cfg_completion_max_candidates = 10;
static int sish_cfg_completion_dir_similarity = 1;
static int sish_cfg_completion_history = 1;

static int sish_cfg_llm_enable = 0;
static char sish_cfg_llm_endpoint[256] = "";
static char sish_cfg_llm_model[128] = "";
static int sish_cfg_llm_max_tokens = 2000;

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
        fprintf(stderr, "%s❌ sish-config: 端末設定の取得に失敗しちゃった… (ttyじゃないかも)%s\n",
                SISH_ERROR_COLOR, SISH_COLOR_RESET);
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

    /* 既存表示に合わせたデフォルト */
    (void)strncpy(sish_cfg_shortcuts[sish_cfg_shortcut_count].key, "g", sizeof(sish_cfg_shortcuts[0].key));
    (void)strncpy(sish_cfg_shortcuts[sish_cfg_shortcut_count].command, "git", sizeof(sish_cfg_shortcuts[0].command));
    sish_cfg_shortcut_count++;
    (void)strncpy(sish_cfg_shortcuts[sish_cfg_shortcut_count].key, "ga", sizeof(sish_cfg_shortcuts[0].key));
    (void)strncpy(sish_cfg_shortcuts[sish_cfg_shortcut_count].command, "git add", sizeof(sish_cfg_shortcuts[0].command));
    sish_cfg_shortcut_count++;
    (void)strncpy(sish_cfg_shortcuts[sish_cfg_shortcut_count].key, "gc", sizeof(sish_cfg_shortcuts[0].key));
    (void)strncpy(sish_cfg_shortcuts[sish_cfg_shortcut_count].command, "git commit", sizeof(sish_cfg_shortcuts[0].command));
    sish_cfg_shortcut_count++;
    (void)strncpy(sish_cfg_shortcuts[sish_cfg_shortcut_count].key, "gp", sizeof(sish_cfg_shortcuts[0].key));
    (void)strncpy(sish_cfg_shortcuts[sish_cfg_shortcut_count].command, "git push", sizeof(sish_cfg_shortcuts[0].command));
    sish_cfg_shortcut_count++;
    (void)strncpy(sish_cfg_shortcuts[sish_cfg_shortcut_count].key, "d", sizeof(sish_cfg_shortcuts[0].key));
    (void)strncpy(sish_cfg_shortcuts[sish_cfg_shortcut_count].command, "docker", sizeof(sish_cfg_shortcuts[0].command));
    sish_cfg_shortcut_count++;
    (void)strncpy(sish_cfg_shortcuts[sish_cfg_shortcut_count].key, "dc", sizeof(sish_cfg_shortcuts[0].key));
    (void)strncpy(sish_cfg_shortcuts[sish_cfg_shortcut_count].command, "docker-compose", sizeof(sish_cfg_shortcuts[0].command));
    sish_cfg_shortcut_count++;
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

        if (!strcmp(key, "SISH_THEME")) {
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
    printf("%s%s║          Sish 設定メニュー - お兄ちゃんの好みに合わせるよ！    ║%s\n",
           SISH_COLOR_BOLD, SISH_COLOR_PINK, SISH_COLOR_RESET);
    printf("%s%s╚═══════════════════════════════════════════════════════════════╝%s\n\n",
           SISH_COLOR_BOLD, SISH_COLOR_PINK, SISH_COLOR_RESET);
}

/* メインメニューを表示 */
static void show_main_menu(int selected) {
    const char *menu_items[] = {
        "1. テーマカラー設定",
        "2. 口調・パーソナリティ設定",
        "3. キャラクター設定",
        "4. ショートカット管理",
        "5. 補完機能設定",
        "6. LLM統合設定",
        "7. エラーメッセージ詳細度",
        "8. GUI連携設定",
        "9. 設定をリセット",
        "0. 設定を保存して終了",
        NULL
    };
    
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
    
    printf("\n%s↑↓キーで選択、Enterで決定、ESCでキャンセル%s\n",
           SISH_HINT_COLOR, SISH_COLOR_RESET);

    fflush(stdout);
}

/* テーマカラー設定 */
static void config_theme_color(void) {
    show_config_header();
    printf("%sテーマカラー設定%s\n\n", SISH_CMD_COLOR, SISH_COLOR_RESET);
    
    const char *themes[] = {
        "1. ピンク（デフォルト）",
        "2. ブルー",
        "3. グリーン",
        "4. パープル",
        "5. オレンジ",
        "6. レインボー",
        NULL
    };
    
    for (int i = 0; themes[i]; i++) {
        printf("  %s\n", themes[i]);
    }
    
    printf("\n%s選択してね（1-6）: %s", SISH_HINT_COLOR, SISH_COLOR_RESET);
    fflush(stdout);
    
    int choice = read_key_blocking();
    if (choice < 0) return;
    if (choice == 27 || choice == '0') {
        printf("\n%sキャンセルしたよ！%s\n", SISH_HINT_COLOR, SISH_COLOR_RESET);
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
            printf("\n%sキャンセルしたよ！%s\n", SISH_HINT_COLOR, SISH_COLOR_RESET);
            sleep(1);
            return;
    }

    sish_cfg_theme[sizeof(sish_cfg_theme) - 1] = '\0';
    printf("\n%s✅ テーマを変更したよ！（保存は『設定を保存して終了』）%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
    sleep(1);
}

/* 口調・パーソナリティ設定 */
static void config_tone(void) {
    show_config_header();
    printf("%s口調・パーソナリティ設定%s\n\n", SISH_CMD_COLOR, SISH_COLOR_RESET);
    
    const char *tones[] = {
        "1. 標準妹モード（お兄ちゃん！、心配口調）",
        "2. しっかり妹モード（断定的、無駄なし）",
        "3. 甘え妹モード（お兄ちゃん…、弱気）",
        "4. せっかち妹モード（短気、即実行）",
        "5. 教え上手妹モード（理由追加、柔らか）",
        "6. 無感情妹モード（感情語ゼロ）",
        "7. ヤンデレ妹モード（決め打ち、強引）",
        NULL
    };
    
    printf("  現在の設定: %s%s%s\n\n", 
           SISH_CHAR_COLOR, sish_tone_name(sish_cfg_tone), SISH_COLOR_RESET);
    
    for (int i = 0; tones[i]; i++) {
        if (i == sish_cfg_tone) {
            printf("  %s%s ← 現在選択中%s\n", 
                   SISH_SUGGEST_COLOR, tones[i], SISH_COLOR_RESET);
        } else {
            printf("  %s\n", tones[i]);
        }
    }
    
    printf("\n%s選択してね（1-7）: %s", SISH_HINT_COLOR, SISH_COLOR_RESET);
    fflush(stdout);
    
    int choice = read_key_blocking();
    if (choice < 0) return;
    if (choice == 27 || choice == '0') {
        printf("\n%sキャンセルしたよ！%s\n", SISH_HINT_COLOR, SISH_COLOR_RESET);
        sleep(1);
        return;
    }

    if (choice >= '1' && choice <= '7') {
        sish_cfg_tone = choice - '1';
        sish_set_tone(sish_cfg_tone);
        printf("\n%s✅ 口調を変更したよ！（保存は『設定を保存して終了』）%s\n", 
               SISH_CHAR_COLOR, SISH_COLOR_RESET);
    } else {
        printf("\n%sキャンセルしたよ！%s\n", SISH_HINT_COLOR, SISH_COLOR_RESET);
    }
    sleep(1);
}

/* キャラクター設定 */
static void config_character(void) {
    for (;;) {
        show_config_header();
        printf("%sキャラクター設定%s\n\n", SISH_CMD_COLOR, SISH_COLOR_RESET);

        printf("  1. キャラクター名: %s%s%s\n", SISH_CHAR_COLOR, sish_cfg_char_name, SISH_COLOR_RESET);
        printf("  2. デフォルト表情: %s%s%s\n", SISH_CHAR_COLOR, sish_cfg_char_expression, SISH_COLOR_RESET);
        printf("  3. 表示位置: %s%s%s\n", SISH_CHAR_COLOR, sish_cfg_char_position, SISH_COLOR_RESET);
        printf("  4. サイズ: %s%s%s\n", SISH_CHAR_COLOR, sish_cfg_char_size, SISH_COLOR_RESET);
        printf("  5. アニメーション: %s%s%s\n", SISH_CHAR_COLOR, sish_cfg_char_animation ? "有効" : "無効", SISH_COLOR_RESET);
        printf("  0. 戻る\n");

        printf("\n%s変更する項目を選択（0-5）: %s", SISH_HINT_COLOR, SISH_COLOR_RESET);
        fflush(stdout);

        int choice = read_key_blocking();
        if (choice < 0) return;
        if (choice == 27 || choice == '0') return;

        if (choice == '1') {
            char buf[64];
            if (read_line_canonical("\n新しいキャラクター名（空ならキャンセル）: ", buf, sizeof(buf)) && buf[0]) {
                strncpy(sish_cfg_char_name, buf, sizeof(sish_cfg_char_name));
                sish_cfg_char_name[sizeof(sish_cfg_char_name) - 1] = '\0';
            }
        } else if (choice == '2') {
            char buf[64];
            if (read_line_canonical("\nデフォルト表情（例: Happy/Angry/Sad… 空ならキャンセル）: ", buf, sizeof(buf)) && buf[0]) {
                strncpy(sish_cfg_char_expression, buf, sizeof(sish_cfg_char_expression));
                sish_cfg_char_expression[sizeof(sish_cfg_char_expression) - 1] = '\0';
            }
        } else if (choice == '3') {
            show_config_header();
            printf("%s表示位置%s\n\n", SISH_CMD_COLOR, SISH_COLOR_RESET);
            printf("  1. left-top\n");
            printf("  2. right-top\n");
            printf("  3. left-bottom\n");
            printf("  4. right-bottom\n\n");
            printf("%s選択（1-4、0で戻る）: %s", SISH_HINT_COLOR, SISH_COLOR_RESET);
            fflush(stdout);
            int p = read_key_blocking();
            if (p == '1') strncpy(sish_cfg_char_position, "left-top", sizeof(sish_cfg_char_position));
            else if (p == '2') strncpy(sish_cfg_char_position, "right-top", sizeof(sish_cfg_char_position));
            else if (p == '3') strncpy(sish_cfg_char_position, "left-bottom", sizeof(sish_cfg_char_position));
            else if (p == '4') strncpy(sish_cfg_char_position, "right-bottom", sizeof(sish_cfg_char_position));
            sish_cfg_char_position[sizeof(sish_cfg_char_position) - 1] = '\0';
        } else if (choice == '4') {
            show_config_header();
            printf("%sサイズ%s\n\n", SISH_CMD_COLOR, SISH_COLOR_RESET);
            printf("  1. small\n");
            printf("  2. medium\n");
            printf("  3. large\n\n");
            printf("%s選択（1-3、0で戻る）: %s", SISH_HINT_COLOR, SISH_COLOR_RESET);
            fflush(stdout);
            int s = read_key_blocking();
            if (s == '1') strncpy(sish_cfg_char_size, "small", sizeof(sish_cfg_char_size));
            else if (s == '2') strncpy(sish_cfg_char_size, "medium", sizeof(sish_cfg_char_size));
            else if (s == '3') strncpy(sish_cfg_char_size, "large", sizeof(sish_cfg_char_size));
            sish_cfg_char_size[sizeof(sish_cfg_char_size) - 1] = '\0';
        } else if (choice == '5') {
            sish_cfg_char_animation = !sish_cfg_char_animation;
        }

        printf("\n%s✅ 変更したよ！（保存は『設定を保存して終了』）%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
        sleep(1);
    }
}

/* ショートカット管理 */
static void config_shortcuts(void) {
    shortcuts_init_defaults_if_needed();

    for (;;) {
        show_config_header();
        printf("%sショートカット管理%s\n\n", SISH_CMD_COLOR, SISH_COLOR_RESET);

        printf("  1. 登録済みショートカット表示\n");
        printf("  2. 新しいショートカットを追加\n");
        printf("  3. ショートカットを削除\n");
        printf("  4. ショートカットを編集\n");
        printf("  0. 戻る\n");

        printf("\n%s選択してね（0-4）: %s", SISH_HINT_COLOR, SISH_COLOR_RESET);
        fflush(stdout);

        int choice = read_key_blocking();
        if (choice < 0) return;
        if (choice == 27 || choice == '0') return;

        if (choice == '1') {
            sish_clear_screen();
            printf("%s登録済みショートカット:%s\n\n", SISH_CMD_COLOR, SISH_COLOR_RESET);
            for (int i = 0; i < sish_cfg_shortcut_count; i++) {
                printf("  %s  → %s\n", sish_cfg_shortcuts[i].key, sish_cfg_shortcuts[i].command);
            }
            if (sish_cfg_shortcut_count == 0) {
                printf("  (なし)\n");
            }
            printf("\n%sEnter/ESCで戻る%s", SISH_HINT_COLOR, SISH_COLOR_RESET);
            fflush(stdout);
            wait_enter_or_esc();
        } else if (choice == '2') {
            char keybuf[32];
            char cmdbuf[256];
            if (!read_line_canonical("\nキー（英数字/_/-、例: g, ga。空ならキャンセル）: ", keybuf, sizeof(keybuf)) || !keybuf[0]) {
                continue;
            }
            if (!is_valid_shortcut_key(keybuf)) {
                printf("\n%s❌ キーが不正だよ…%s\n", SISH_ERROR_COLOR, SISH_COLOR_RESET);
                sleep(1);
                continue;
            }
            if (!read_line_canonical("コマンド（例: git status。空ならキャンセル）: ", cmdbuf, sizeof(cmdbuf)) || !cmdbuf[0]) {
                continue;
            }
            if (sish_cfg_shortcut_count >= SISH_MAX_SHORTCUTS) {
                printf("\n%s❌ これ以上増やせないよ…%s\n", SISH_ERROR_COLOR, SISH_COLOR_RESET);
                sleep(1);
                continue;
            }
            strncpy(sish_cfg_shortcuts[sish_cfg_shortcut_count].key, keybuf, sizeof(sish_cfg_shortcuts[0].key));
            strncpy(sish_cfg_shortcuts[sish_cfg_shortcut_count].command, cmdbuf, sizeof(sish_cfg_shortcuts[0].command));
            sish_cfg_shortcuts[sish_cfg_shortcut_count].key[sizeof(sish_cfg_shortcuts[0].key) - 1] = '\0';
            sish_cfg_shortcuts[sish_cfg_shortcut_count].command[sizeof(sish_cfg_shortcuts[0].command) - 1] = '\0';
            sish_cfg_shortcut_count++;
            printf("\n%s✅ 追加したよ！%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
            sleep(1);
        } else if (choice == '3') {
            char keybuf[32];
            if (!read_line_canonical("\n削除するキー（空ならキャンセル）: ", keybuf, sizeof(keybuf)) || !keybuf[0]) {
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
                printf("\n%s❌ 見つからなかったよ…%s\n", SISH_ERROR_COLOR, SISH_COLOR_RESET);
                sleep(1);
                continue;
            }
            for (int i = found; i < sish_cfg_shortcut_count - 1; i++) {
                sish_cfg_shortcuts[i] = sish_cfg_shortcuts[i + 1];
            }
            sish_cfg_shortcut_count--;
            printf("\n%s✅ 削除したよ！%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
            sleep(1);
        } else if (choice == '4') {
            char keybuf[32];
            char cmdbuf[256];
            if (!read_line_canonical("\n編集するキー（空ならキャンセル）: ", keybuf, sizeof(keybuf)) || !keybuf[0]) {
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
                printf("\n%s❌ 見つからなかったよ…%s\n", SISH_ERROR_COLOR, SISH_COLOR_RESET);
                sleep(1);
                continue;
            }
            if (!read_line_canonical("新しいコマンド（空ならキャンセル）: ", cmdbuf, sizeof(cmdbuf)) || !cmdbuf[0]) {
                continue;
            }
            strncpy(sish_cfg_shortcuts[found].command, cmdbuf, sizeof(sish_cfg_shortcuts[0].command));
            sish_cfg_shortcuts[found].command[sizeof(sish_cfg_shortcuts[0].command) - 1] = '\0';
            printf("\n%s✅ 編集したよ！%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
            sleep(1);
        }
    }
}

/* 補完機能設定 */
static void config_completion(void) {
    for (;;) {
        show_config_header();
        printf("%s補完機能設定%s\n\n", SISH_CMD_COLOR, SISH_COLOR_RESET);

        printf("  1. 自動補完: %s%s%s\n", SISH_CHAR_COLOR, sish_cfg_completion_enable ? "有効" : "無効", SISH_COLOR_RESET);
        printf("  2. ファジーマッチ: %s%s%s\n", SISH_CHAR_COLOR, sish_cfg_completion_fuzzy ? "有効" : "無効", SISH_COLOR_RESET);
        printf("  3. 候補表示数: %s%d%s\n", SISH_CHAR_COLOR, sish_cfg_completion_max_candidates, SISH_COLOR_RESET);
        printf("  4. ディレクトリ類似検索: %s%s%s\n", SISH_CHAR_COLOR, sish_cfg_completion_dir_similarity ? "有効" : "無効", SISH_COLOR_RESET);
        printf("  5. コマンド履歴補完: %s%s%s\n", SISH_CHAR_COLOR, sish_cfg_completion_history ? "有効" : "無効", SISH_COLOR_RESET);
        printf("  0. 戻る\n");

        printf("\n%s変更する項目を選択（0-5）: %s", SISH_HINT_COLOR, SISH_COLOR_RESET);
        fflush(stdout);

        int choice = read_key_blocking();
        if (choice < 0) return;
        if (choice == 27 || choice == '0') return;

        if (choice == '1') sish_cfg_completion_enable = !sish_cfg_completion_enable;
        else if (choice == '2') sish_cfg_completion_fuzzy = !sish_cfg_completion_fuzzy;
        else if (choice == '3') {
            int v;
            if (read_int_canonical("\n候補表示数（例: 10。空ならキャンセル）: ", &v)) {
                if (v < 1) v = 1;
                if (v > 200) v = 200;
                sish_cfg_completion_max_candidates = v;
            }
        } else if (choice == '4') sish_cfg_completion_dir_similarity = !sish_cfg_completion_dir_similarity;
        else if (choice == '5') sish_cfg_completion_history = !sish_cfg_completion_history;

        printf("\n%s✅ 変更したよ！（保存は『設定を保存して終了』）%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
        sleep(1);
    }
}

/* LLM統合設定 */
static void config_llm(void) {
    for (;;) {
        show_config_header();
        printf("%sLLM統合設定%s\n\n", SISH_CMD_COLOR, SISH_COLOR_RESET);

        printf("  1. LLM統合: %s%s%s\n", SISH_CHAR_COLOR, sish_cfg_llm_enable ? "有効" : "無効", SISH_COLOR_RESET);
        printf("  2. APIエンドポイント: %s%s%s\n", SISH_CHAR_COLOR,
               sish_cfg_llm_endpoint[0] ? sish_cfg_llm_endpoint : "(未設定)", SISH_COLOR_RESET);
        printf("  3. モデル: %s%s%s\n", SISH_CHAR_COLOR,
               sish_cfg_llm_model[0] ? sish_cfg_llm_model : "(未設定)", SISH_COLOR_RESET);
        printf("  4. 最大トークン: %s%d%s\n", SISH_CHAR_COLOR, sish_cfg_llm_max_tokens, SISH_COLOR_RESET);
        printf("  0. 戻る\n");

        printf("\n%s変更する項目を選択（0-4）: %s", SISH_HINT_COLOR, SISH_COLOR_RESET);
        fflush(stdout);

        int choice = read_key_blocking();
        if (choice < 0) return;
        if (choice == 27 || choice == '0') return;

        if (choice == '1') {
            sish_cfg_llm_enable = !sish_cfg_llm_enable;
        } else if (choice == '2') {
            char buf[256];
            if (read_line_canonical("\nAPIエンドポイント（例: http://localhost:11434。空ならキャンセル）: ", buf, sizeof(buf)) && buf[0]) {
                strncpy(sish_cfg_llm_endpoint, buf, sizeof(sish_cfg_llm_endpoint));
                sish_cfg_llm_endpoint[sizeof(sish_cfg_llm_endpoint) - 1] = '\0';
            }
        } else if (choice == '3') {
            char buf[128];
            if (read_line_canonical("\nモデル名（例: llama3。空ならキャンセル）: ", buf, sizeof(buf)) && buf[0]) {
                strncpy(sish_cfg_llm_model, buf, sizeof(sish_cfg_llm_model));
                sish_cfg_llm_model[sizeof(sish_cfg_llm_model) - 1] = '\0';
            }
        } else if (choice == '4') {
            int v;
            if (read_int_canonical("\n最大トークン（例: 2000。空ならキャンセル）: ", &v)) {
                if (v < 1) v = 1;
                if (v > 200000) v = 200000;
                sish_cfg_llm_max_tokens = v;
            }
        }

        printf("\n%s✅ 変更したよ！（保存は『設定を保存して終了』）%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
        sleep(1);
    }
}

/* エラーメッセージ詳細度設定 */
static void config_error_verbosity(void) {
    show_config_header();
    printf("%sエラーメッセージ詳細度%s\n\n", SISH_CMD_COLOR, SISH_COLOR_RESET);
    
    printf("  1. 簡潔（デフォルト） - 短いメッセージのみ\n");
    printf("  2. 標準 - メッセージ + ヒント\n");
    printf("  3. 詳細 - メッセージ + ヒント + 例\n");
    printf("  4. 超詳細 - すべての情報 + デバッグ情報\n");
    
    printf("\n%s選択してね（1-4）: %s", SISH_HINT_COLOR, SISH_COLOR_RESET);
    fflush(stdout);
    
    int choice = read_key_blocking();
    if (choice < 0) return;
    if (choice == 27 || choice == '0') {
        return;
    }

    if (choice >= '1' && choice <= '4') {
        sish_cfg_error_verbosity = (choice - '0');
        printf("\n%s✅ 詳細度を変更したよ！（保存は『設定を保存して終了』）%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
        sleep(1);
    }
}

/* GUI連携設定 */
static void config_gui(void) {
    for (;;) {
        show_config_header();
        printf("%sGUI連携設定%s\n\n", SISH_CMD_COLOR, SISH_COLOR_RESET);

        printf("  1. GUI連携: %s%s%s\n", SISH_CHAR_COLOR, sish_cfg_gui_enable ? "有効" : "無効", SISH_COLOR_RESET);
        printf("  2. ソケットパス: %s%s%s\n", SISH_CHAR_COLOR, sish_cfg_gui_socket_path, SISH_COLOR_RESET);
        printf("  3. 自動起動: %s%s%s\n", SISH_CHAR_COLOR, sish_cfg_gui_autostart ? "有効" : "無効", SISH_COLOR_RESET);
        printf("  4. 表情同期: %s%s%s\n", SISH_CHAR_COLOR, sish_cfg_gui_expression_sync ? "有効" : "無効", SISH_COLOR_RESET);
        printf("  0. 戻る\n");

        printf("\n%s変更する項目を選択（0-4）: %s", SISH_HINT_COLOR, SISH_COLOR_RESET);
        fflush(stdout);

        int choice = read_key_blocking();
        if (choice < 0) return;
        if (choice == 27 || choice == '0') return;

        if (choice == '1') {
            sish_cfg_gui_enable = !sish_cfg_gui_enable;
        } else if (choice == '2') {
            char buf[256];
            if (read_line_canonical("\nソケットパス（例: /tmp/sish-console.sock。空ならキャンセル）: ", buf, sizeof(buf)) && buf[0]) {
                strncpy(sish_cfg_gui_socket_path, buf, sizeof(sish_cfg_gui_socket_path));
                sish_cfg_gui_socket_path[sizeof(sish_cfg_gui_socket_path) - 1] = '\0';
            }
        } else if (choice == '3') {
            sish_cfg_gui_autostart = !sish_cfg_gui_autostart;
        } else if (choice == '4') {
            sish_cfg_gui_expression_sync = !sish_cfg_gui_expression_sync;
        }

        printf("\n%s✅ 変更したよ！（保存は『設定を保存して終了』）%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
        sleep(1);
    }
}

/* 設定をリセット */
static void config_reset(void) {
    show_config_header();
    printf("%s⚠️  すべての設定をデフォルトに戻すよ？%s\n\n", 
           SISH_ERROR_COLOR, SISH_COLOR_RESET);
    printf("  本当にリセットする？ (y/N): ");
    fflush(stdout);
    
    int choice = read_key_blocking();
    if (choice < 0) return;
    if (choice == 27) {
        printf("\n%sキャンセルしたよ！%s\n", SISH_HINT_COLOR, SISH_COLOR_RESET);
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
        sish_cfg_completion_max_candidates = 10;
        sish_cfg_completion_dir_similarity = 1;
        sish_cfg_completion_history = 1;

        sish_cfg_llm_enable = 0;
        sish_cfg_llm_endpoint[0] = '\0';
        sish_cfg_llm_model[0] = '\0';
        sish_cfg_llm_max_tokens = 2000;

        sish_cfg_gui_enable = 1;
        strncpy(sish_cfg_gui_socket_path, "/tmp/sish-console.sock", sizeof(sish_cfg_gui_socket_path));
        sish_cfg_gui_socket_path[sizeof(sish_cfg_gui_socket_path) - 1] = '\0';
        sish_cfg_gui_autostart = 0;
        sish_cfg_gui_expression_sync = 1;

        /* ショートカットは初期化フラグを落として再生成 */
        sish_cfg_shortcuts_initialized = 0;
        shortcuts_init_defaults_if_needed();

        printf("\n%s✅ 設定をリセットしたよ！%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
        sleep(1);
    } else {
        printf("\n%sキャンセルしたよ！%s\n", SISH_HINT_COLOR, SISH_COLOR_RESET);
        sleep(1);
    }
}

/* 設定を保存 */
static void config_save(void) {
    printf("\n%s💾 設定を保存中...%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
    
    /* ~/.sishrc に保存 */
    FILE *fp = fopen(sish_config_path, "w");
    if (fp) {
        fprintf(fp, "# Sish Configuration\n");
        fprintf(fp, "# Generated by Sish Config Tool\n\n");
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

        fprintf(fp, "\n# GUI\n");
        write_export_int(fp, "SISH_GUI_ENABLE", sish_cfg_gui_enable);
        write_export_string(fp, "SISH_GUI_SOCKET_PATH", sish_cfg_gui_socket_path);
        write_export_int(fp, "SISH_GUI_AUTOSTART", sish_cfg_gui_autostart);
        write_export_int(fp, "SISH_GUI_EXPRESSION_SYNC", sish_cfg_gui_expression_sync);

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
        fprintf(fp, "\n# (必要ならここに alias や関数を追加してね)\n");
        fclose(fp);
        printf("%s✅ 設定を保存したよ！ (~/.sishrc)%s\n", 
               SISH_CHAR_COLOR, SISH_COLOR_RESET);
    } else {
        printf("%s❌ 設定の保存に失敗しちゃった...%s\n", 
               SISH_ERROR_COLOR, SISH_COLOR_RESET);
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

    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "%s❌ sish-config: 対話端末じゃないと操作できないよ…%s\n",
                SISH_ERROR_COLOR, SISH_COLOR_RESET);
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
    printf("%sまた設定したくなったら、'sish-config'を実行してね！%s\n",
           SISH_CHAR_COLOR, SISH_COLOR_RESET);
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
