/*
 * sish.h - Sish (Sister Shell) header file
 *
 * Sish - A friendly, Japanese-speaking shell based on Zsh
 * Copyright (c) 2025 Sish Development Team
 *
 * This is an extension header for the Sish shell, providing:
 * - Japanese error messages with character personality
 * - Command suggestion system (Levenshtein distance)
 * - GUI communication via Unix Domain Socket
 * - LLM integration support (optional)
 */

#ifndef _SISH_H
#define _SISH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>

/* ========================================
 * Sish Version and Branding
 * ======================================== */

#define SISH_VERSION "1.0.0"
#define SISH_NAME "Sish"

/* ========================================
 * Runtime Configuration (from env)
 * ======================================== */

/* Language: controlled by SISH_LANG ("ja" default, "en" for English) */
int sish_lang_is_en(void);

#ifndef SISH_TR
#define SISH_TR(ja, en) (sish_lang_is_en() ? (en) : (ja))
#endif

const char *sish_character_name(void);

int sish_error_verbosity(void);

int sish_gui_enabled(void);
int sish_gui_autostart(void);
int sish_gui_expression_sync(void);
const char *sish_gui_socket_path(void);

int sish_completion_enabled(void);
int sish_completion_fuzzy(void);
int sish_completion_dir_similarity(void);
int sish_completion_history(void);
int sish_completion_max_candidates(void);

int sish_show_welcome(void);
int sish_show_hint(void);
int sish_live_completion_enabled(void);
int sish_live_completion_max_candidates(void);

int sish_llm_enabled_setting(void);
const char *sish_llm_endpoint_setting(void);
const char *sish_llm_model_setting(void);
int sish_llm_max_tokens_setting(void);
int sish_llm_auto_explain_setting(void);

const char *sish_color_char(void);
const char *sish_color_cmd(void);
const char *sish_color_error(void);
const char *sish_color_suggest(void);
const char *sish_color_hint(void);

/* ========================================
 * ANSI Color Codes for Terminal Output
 * ======================================== */

/* Character name (runtime-configurable) */
#undef SISH_CHARACTER_NAME
#define SISH_CHARACTER_NAME (sish_character_name())

#define SISH_COLOR_RESET    "\033[0m"
#define SISH_COLOR_RED      "\033[31m"
#define SISH_COLOR_GREEN    "\033[32m"
#define SISH_COLOR_YELLOW   "\033[33m"
#define SISH_COLOR_BLUE     "\033[34m"
#define SISH_COLOR_MAGENTA  "\033[35m"
#define SISH_COLOR_CYAN     "\033[36m"
#define SISH_COLOR_WHITE    "\033[37m"
#define SISH_COLOR_BOLD     "\033[1m"
#define SISH_COLOR_PINK     "\033[38;5;213m"
#define SISH_COLOR_ORANGE   "\033[38;5;208m"

/* Character-specific colors (theme-aware) */
#undef SISH_CHAR_COLOR
#undef SISH_CMD_COLOR
#undef SISH_ERROR_COLOR
#undef SISH_SUGGEST_COLOR
#undef SISH_HINT_COLOR
#define SISH_CHAR_COLOR     (sish_color_char())
#define SISH_CMD_COLOR      (sish_color_cmd())
#define SISH_ERROR_COLOR    (sish_color_error())
#define SISH_SUGGEST_COLOR  (sish_color_suggest())
#define SISH_HINT_COLOR     (sish_color_hint())

/* ========================================
 * Error Type Definitions
 * ======================================== */

typedef enum {
    SISH_ERR_COMMAND_NOT_FOUND = 0,
    SISH_ERR_PERMISSION_DENIED,
    SISH_ERR_FILE_NOT_FOUND,
    SISH_ERR_SYNTAX_ERROR,
    SISH_ERR_EXEC_FORMAT,
    SISH_ERR_NO_SUCH_FILE_OR_DIR,
    SISH_ERR_IS_DIRECTORY,
    SISH_ERR_NOT_DIRECTORY,
    SISH_ERR_PIPE_ERROR,
    SISH_ERR_MEMORY_ERROR,
    SISH_ERR_GENERAL,
    SISH_ERR_COUNT
} SishErrorType;

/* ========================================
 * Emotion Types for GUI Communication
 * ======================================== */

typedef enum {
    SISH_EMOTION_HAPPY = 0,
    SISH_EMOTION_SAD,
    SISH_EMOTION_CONFUSED,
    SISH_EMOTION_ANGRY,
    SISH_EMOTION_THINKING,
    SISH_EMOTION_EXCITED,
    SISH_EMOTION_SLEEPY,
    SISH_EMOTION_NEUTRAL
} SishEmotion;

/* ========================================
 * Personality/Tone Types
 * ======================================== */

typedef enum {
    SISH_TONE_STANDARD = 0,      /* 標準妹モード（お兄ちゃん！、心配口調） */
    SISH_TONE_RELIABLE,          /* しっかり妹モード（断定的、無駄なし） */
    SISH_TONE_SWEET,             /* 甘え妹モード（お兄ちゃん…、弱気） */
    SISH_TONE_QUICK,             /* せっかち妹モード（短気、即実行） */
    SISH_TONE_TEACHER,           /* 教え上手妹モード（理由追加、柔らか） */
    SISH_TONE_EMOTIONLESS,       /* 無感情妹モード（感情語ゼロ） */
    SISH_TONE_YANDERE,           /* ヤンデレ妹モード（決め打ち、強引） */
    SISH_TONE_COUNT
} SishTone;

/* ========================================
 * GUI Communication Socket Path
 * ======================================== */

#define SISH_SOCKET_PATH "/tmp/sish-console.sock"
#define SISH_MAX_MSG_LEN 4096

/* ========================================
 * Command Suggestion Settings
 * ======================================== */

#define SISH_MAX_SUGGESTIONS 5
#define SISH_MAX_DISTANCE 3  /* Maximum Levenshtein distance for suggestions */
#define SISH_MIN_CMD_LEN 2   /* Minimum command length for suggestions */

/* ========================================
 * Common Commands Database
 * (for quick suggestions without PATH search)
 * ======================================== */

extern const char *sish_common_commands[];

/* ========================================
 * Command Shortcuts/Aliases Database
 * ======================================== */

typedef struct {
    const char *shortcut;
    const char *expansion;
    const char *description;
} SishShortcut;

extern const SishShortcut sish_shortcuts[];

/* ========================================
 * Function Declarations
 * ======================================== */

/* Levenshtein distance calculation */
int sish_levenshtein_distance(const char *s1, const char *s2);

/* Find similar commands */
char **sish_find_similar_commands(const char *cmd, int *count);

/* Free suggestions array */
void sish_free_suggestions(char **suggestions, int count);

/* Get Japanese error message with character personality */
const char *sish_get_error_message(SishErrorType type, const char *arg);

/* Print Sish-style error */
void sish_print_error(SishErrorType type, const char *cmd, const char *arg);

/* Print command suggestion */
void sish_print_suggestion(const char *typed_cmd, const char *suggested_cmd);

/* GUI Communication */
int sish_gui_connect(void);
void sish_gui_disconnect(int sock);
int sish_gui_send_event(const char *event_type, const char *data);
int sish_gui_send_emotion(SishEmotion emotion);

/* Shortcut expansion */
const char *sish_expand_shortcut(const char *input);

/* Check if Sish-Console is running */
int sish_console_is_running(void);

/* LLM integration (optional) */
int sish_llm_query(const char *prompt, char *response, size_t response_size);

/* Tone/Personality management */
void sish_set_tone(SishTone sish_tone);
SishTone sish_get_tone(void);
const char *sish_tone_name(SishTone sish_tone);

/* ========================================
 * Inline Helper Functions
 * ======================================== */

/* Minimum of three integers */
static inline int sish_min3(int a, int b, int c) {
    int min = a;
    if (b < min) min = b;
    if (c < min) min = c;
    return min;
}

/* Check if character is similar (keyboard proximity) */
static inline int sish_is_similar_char(char a, char b) {
    if (a == b) return 1;
    /* Common QWERTY keyboard adjacencies */
    const char *adjacent[] = {
        "qwa", "wqeas", "erwd", "rtef", "tyrg", "yuth", "uiyj", "oiuk", "polk",
        "azsq", "sxadw", "dcse", "fvdr", "gbtf", "hnyg", "jmuh", "kij", "lko",
        "zxa", "czvs", "vbcd", "nmbf", "mn",
        NULL
    };
    for (int i = 0; adjacent[i]; i++) {
        if (strchr(adjacent[i], a) && strchr(adjacent[i], b)) return 1;
    }
    return 0;
}

/* Function declarations */
void sish_init(void);
void sish_command_not_found(const char *cmd);
void sish_cd_not_found(const char *dest);
void sish_permission_denied(const char *path);
void sish_file_not_found(const char *path);
void sish_invalid_option(const char *opt);
void sish_syntax_error(const char *message);
char **sish_find_similar_commands(const char *cmd, int *num_suggestions);
int sish_levenshtein_distance(const char *s1, const char *s2);
void sish_preexec(const char *cmd);
void sish_precmd(int status);

/* New comprehensive error handling */
char *sish_translate_error(const char *msg);
void sish_zerr(const char *fmt, ...);
void sish_zerrnam(const char *cmd, const char *fmt, ...);

/* Enhanced completion system */
int sish_smart_completion(const char *cmd, const char *arg, char ***suggestions);
void sish_show_completions(char **suggestions, int count);
int sish_complete_path(const char *partial_path, char ***suggestions);
void sish_free_completion_suggestions(char **suggestions, int count);

/* Interactive configuration system */
void sish_show_config_menu(void);
int sish_config_command(char *nam, char **args, Options ops, int func);

#endif /* _SISH_H */

