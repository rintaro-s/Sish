/*
 * sish.c - Sish (Sister Shell) core implementation
 *
 * Sish - A friendly, Japanese-speaking shell based on Zsh
 * Copyright (c) 2025 Sish Development Team
 *
 * This file implements:
 * - Japanese error messages with character personality
 * - Command suggestion system using Levenshtein distance
 * - GUI communication via Unix Domain Socket
 * - Shortcut/alias expansion
 */

#include "zsh.mdh"
#include "sish.h"

/* External reference to zsh's path variable */
extern char **path;

/* ========================================
 * Current Personality/Tone Setting
 * ======================================== */

static SishTone current_tone = SISH_TONE_STANDARD;
static int tone_initialized = 0;

/* ========================================
 * Runtime Configuration Cache
 * ======================================== */

static int sish_cfg_initialized = 0;
static int sish_cfg_error_verbosity = 1;

static int sish_cfg_gui_enable = 1;
static int sish_cfg_gui_autostart = 0;
static int sish_cfg_gui_expression_sync = 1;
static char sish_cfg_gui_socket_path[256] = SISH_SOCKET_PATH;

static int sish_cfg_completion_enable = 1;
static int sish_cfg_completion_fuzzy = 1;
static int sish_cfg_completion_dir_similarity = 1;
static int sish_cfg_completion_history = 1;
static int sish_cfg_completion_max_candidates = 5;

static int sish_cfg_llm_enable = 0;
static char sish_cfg_llm_endpoint[256] = "";
static char sish_cfg_llm_model[128] = "";
static int sish_cfg_llm_max_tokens = 2000;

static char sish_cfg_character_name[64] = SISH_NAME;
static char sish_cfg_theme[16] = "pink";

static const char *sish_cfg_color_char = SISH_COLOR_PINK;
static const char *sish_cfg_color_cmd = SISH_COLOR_CYAN;
static const char *sish_cfg_color_error = SISH_COLOR_RED;
static const char *sish_cfg_color_suggest = SISH_COLOR_GREEN;
static const char *sish_cfg_color_hint = SISH_COLOR_YELLOW;

static void
sish_copy_bounded(char *dst, size_t dstsize, const char *src)
{
    if (!dst || dstsize == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dstsize);
    dst[dstsize - 1] = '\0';
}

static int
sish_env_int(const char *key, int def, int minv, int maxv)
{
    const char *v = getenv(key);
    if (!v || !*v) return def;
    char *end = NULL;
    long n = strtol(v, &end, 10);
    if (end == v) return def;
    if (n < minv) n = minv;
    if (n > maxv) n = maxv;
    return (int)n;
}

static void
sish_config_init_once(void)
{
    if (sish_cfg_initialized) return;
    sish_cfg_initialized = 1;

    /* Theme + colors */
    const char *theme = getenv("SISH_THEME");
    if (theme && *theme) sish_copy_bounded(sish_cfg_theme, sizeof(sish_cfg_theme), theme);

    if (!strcmp(sish_cfg_theme, "blue")) {
        sish_cfg_color_char = SISH_COLOR_BLUE;
    } else if (!strcmp(sish_cfg_theme, "green")) {
        sish_cfg_color_char = SISH_COLOR_GREEN;
    } else if (!strcmp(sish_cfg_theme, "purple")) {
        sish_cfg_color_char = SISH_COLOR_MAGENTA;
    } else if (!strcmp(sish_cfg_theme, "orange")) {
        sish_cfg_color_char = SISH_COLOR_ORANGE;
    } else if (!strcmp(sish_cfg_theme, "rainbow")) {
        /* 最低限: 見た目が変わるようにシアン寄せ */
        sish_cfg_color_char = SISH_COLOR_CYAN;
    } else {
        sish_cfg_color_char = SISH_COLOR_PINK;
    }

    /* Character */
    const char *cname = getenv("SISH_CHAR_NAME");
    if (cname && *cname) sish_copy_bounded(sish_cfg_character_name, sizeof(sish_cfg_character_name), cname);

    /* Error verbosity */
    sish_cfg_error_verbosity = sish_env_int("SISH_ERROR_VERBOSITY", 1, 1, 4);

    /* GUI */
    sish_cfg_gui_enable = sish_env_int("SISH_GUI_ENABLE", 1, 0, 1);
    sish_cfg_gui_autostart = sish_env_int("SISH_GUI_AUTOSTART", 0, 0, 1);
    sish_cfg_gui_expression_sync = sish_env_int("SISH_GUI_EXPRESSION_SYNC", 1, 0, 1);
    const char *sock = getenv("SISH_GUI_SOCKET_PATH");
    if (sock && *sock) sish_copy_bounded(sish_cfg_gui_socket_path, sizeof(sish_cfg_gui_socket_path), sock);

    /* Completion */
    sish_cfg_completion_enable = sish_env_int("SISH_COMPLETION_ENABLE", 1, 0, 1);
    sish_cfg_completion_fuzzy = sish_env_int("SISH_COMPLETION_FUZZY", 1, 0, 1);
    sish_cfg_completion_dir_similarity = sish_env_int("SISH_COMPLETION_DIR_SIMILARITY", 1, 0, 1);
    sish_cfg_completion_history = sish_env_int("SISH_COMPLETION_HISTORY", 1, 0, 1);
    sish_cfg_completion_max_candidates = sish_env_int("SISH_COMPLETION_MAX_CANDIDATES", 5, 1, 1000);

    /* LLM */
    sish_cfg_llm_enable = sish_env_int("SISH_LLM_ENABLE", 0, 0, 1);
    const char *ep = getenv("SISH_LLM_ENDPOINT");
    if (ep && *ep) sish_copy_bounded(sish_cfg_llm_endpoint, sizeof(sish_cfg_llm_endpoint), ep);
    const char *model = getenv("SISH_LLM_MODEL");
    if (model && *model) sish_copy_bounded(sish_cfg_llm_model, sizeof(sish_cfg_llm_model), model);
    sish_cfg_llm_max_tokens = sish_env_int("SISH_LLM_MAX_TOKENS", 2000, 1, 200000);
}

/**/
const char *
sish_character_name(void)
{
    sish_config_init_once();
    return sish_cfg_character_name[0] ? sish_cfg_character_name : SISH_NAME;
}

/**/
int
sish_error_verbosity(void)
{
    sish_config_init_once();
    return sish_cfg_error_verbosity;
}

/**/
int
sish_gui_enabled(void)
{
    sish_config_init_once();
    return sish_cfg_gui_enable;
}

/**/
int
sish_gui_autostart(void)
{
    sish_config_init_once();
    return sish_cfg_gui_autostart;
}

/**/
int
sish_gui_expression_sync(void)
{
    sish_config_init_once();
    return sish_cfg_gui_expression_sync;
}

/**/
const char *
sish_gui_socket_path(void)
{
    sish_config_init_once();
    return sish_cfg_gui_socket_path[0] ? sish_cfg_gui_socket_path : SISH_SOCKET_PATH;
}

/**/
int
sish_completion_enabled(void)
{
    sish_config_init_once();
    return sish_cfg_completion_enable;
}

/**/
int
sish_completion_fuzzy(void)
{
    sish_config_init_once();
    return sish_cfg_completion_fuzzy;
}

/**/
int
sish_completion_dir_similarity(void)
{
    sish_config_init_once();
    return sish_cfg_completion_dir_similarity;
}

/**/
int
sish_completion_history(void)
{
    sish_config_init_once();
    return sish_cfg_completion_history;
}

/**/
int
sish_completion_max_candidates(void)
{
    sish_config_init_once();
    return sish_cfg_completion_max_candidates;
}

/**/
int
sish_llm_enabled_setting(void)
{
    sish_config_init_once();
    return sish_cfg_llm_enable && sish_cfg_llm_endpoint[0];
}

/**/
const char *
sish_llm_endpoint_setting(void)
{
    sish_config_init_once();
    return sish_cfg_llm_endpoint;
}

/**/
const char *
sish_llm_model_setting(void)
{
    sish_config_init_once();
    return sish_cfg_llm_model;
}

/**/
int
sish_llm_max_tokens_setting(void)
{
    sish_config_init_once();
    return sish_cfg_llm_max_tokens;
}

/**/
const char *
sish_color_char(void)
{
    sish_config_init_once();
    return sish_cfg_color_char;
}

/**/
const char *
sish_color_cmd(void)
{
    sish_config_init_once();
    return sish_cfg_color_cmd;
}

/**/
const char *
sish_color_error(void)
{
    sish_config_init_once();
    return sish_cfg_color_error;
}

/**/
const char *
sish_color_suggest(void)
{
    sish_config_init_once();
    return sish_cfg_color_suggest;
}

/**/
const char *
sish_color_hint(void)
{
    sish_config_init_once();
    return sish_cfg_color_hint;
}

static void
sish_init_tone_from_env_once(void)
{
    if (tone_initialized) return;
    tone_initialized = 1;

    const char *env = getenv("SISH_TONE");
    if (!env || !*env) return;

    char *end = NULL;
    long v = strtol(env, &end, 10);
    if (end == env) return;
    if (v < 0) v = 0;
    if (v >= (long)SISH_TONE_COUNT) v = (long)SISH_TONE_COUNT - 1;
    current_tone = (SishTone)v;
}

/* ========================================
 * Tone-specific Message Templates
 * ======================================== */

/* メッセージ構造: prefix, main_message, hint */
typedef struct {
    const char *prefix;
    const char *main_msg;
    const char *hint;
} ToneMessage;

/* 各口調ごとのコマンド未検出メッセージ */
static const ToneMessage tone_cmd_not_found[] = {
    /* SISH_TONE_STANDARD */
    {"お兄ちゃん！", "\"%s\"って無いよ…", "\"%s\"の間違いじゃない？"},
    /* SISH_TONE_RELIABLE */
    {"", "\"%s\" は存在しない。", "\"%s\" を実行する？"},
    /* SISH_TONE_SWEET */
    {"お兄ちゃん…", "\"%s\"って無いみたい…", "\"%s\"なら、あるよ…？"},
    /* SISH_TONE_QUICK */
    {"", "\"%s\" → \"%s\"。", "実行するね"},
    /* SISH_TONE_TEACHER */
    {"お兄ちゃん、", "\"%s\"はコマンドに無いよ", "\"%s\"はバージョン管理のコマンドだよ"},
    /* SISH_TONE_EMOTIONLESS */
    {"", "\"%s\" 不明。", "\"%s\" 提案。"},
    /* SISH_TONE_YANDERE */
    {"お兄ちゃん…", "\"%s\"なんて使わないで…", "\"%s\"だけ使って…絶対に…"}
};

/* 各口調ごとのファイル未検出メッセージ */
static const ToneMessage tone_file_not_found[] = {
    /* SISH_TONE_STANDARD */
    {"お兄ちゃん！", "\"%s\"って無いよ…", "パスを確認してね！"},
    /* SISH_TONE_RELIABLE */
    {"", "\"%s\" は存在しない。", "パスを確認すること。"},
    /* SISH_TONE_SWEET */
    {"お兄ちゃん…", "\"%s\"って無いみたい…", "パス、間違ってない…？"},
    /* SISH_TONE_QUICK */
    {"", "\"%s\" 無し。", "確認して"},
    /* SISH_TONE_TEACHER */
    {"お兄ちゃん、", "\"%s\"というファイルは無いよ", "ファイルパスは絶対パスか相対パスで指定できるよ"},
    /* SISH_TONE_EMOTIONLESS */
    {"", "\"%s\" 不在。", ""},
    /* SISH_TONE_YANDERE */
    {"お兄ちゃん…", "\"%s\"なんて要らないよ…", "私だけ見て…"}
};

/* ========================================
 * Common Commands Database
 * ======================================== */

const char *sish_common_commands[] = {
    /* Version Control */
    "git", "svn", "hg",
    /* Package Managers */
    "apt", "apt-get", "dnf", "yum", "pacman", "brew", "npm", "pip", "cargo",
    /* File Operations */
    "ls", "cd", "cp", "mv", "rm", "mkdir", "rmdir", "touch", "cat", "less",
    "more", "head", "tail", "find", "grep", "sed", "awk", "chmod", "chown",
    /* System */
    "ps", "top", "htop", "kill", "killall", "sudo", "su", "systemctl", "service",
    /* Network */
    "ping", "curl", "wget", "ssh", "scp", "rsync", "netstat", "ss", "ip",
    /* Text Editors */
    "vim", "nvim", "nano", "emacs", "code", "subl",
    /* Compilers/Interpreters */
    "python", "python3", "node", "ruby", "perl", "java", "javac", "gcc", "g++",
    "clang", "make", "cmake", "cargo", "rustc", "go",
    /* Docker/Containers */
    "docker", "podman", "kubectl",
    /* Common Typos targets */
    "clear", "exit", "history", "man", "which", "where", "echo", "printf",
    NULL
};

/* ========================================
 * Command Shortcuts Database
 * ======================================== */

const SishShortcut sish_shortcuts[] = {
    {"g", "git", "Git version control"},
    {"gs", "git status", "Git status"},
    {"ga", "git add", "Git add"},
    {"gc", "git commit", "Git commit"},
    {"gp", "git push", "Git push"},
    {"gl", "git log", "Git log"},
    {"gd", "git diff", "Git diff"},
    {"gco", "git checkout", "Git checkout"},
    {"gbr", "git branch", "Git branch"},
    {"a-ins", "sudo apt install", "APT install"},
    {"a-up", "sudo apt update && sudo apt upgrade", "APT update & upgrade"},
    {"a-rm", "sudo apt remove", "APT remove"},
    {"p-ins", "pip install", "Pip install"},
    {"n-ins", "npm install", "NPM install"},
    {"dk", "docker", "Docker"},
    {"dkc", "docker-compose", "Docker Compose"},
    {"dkps", "docker ps", "Docker process list"},
    {"py", "python3", "Python 3"},
    {"cls", "clear", "Clear screen"},
    {"l", "ls -la", "List all files"},
    {"ll", "ls -l", "List files long format"},
    {"la", "ls -la", "List all files"},
    {"...", "cd ../..", "Go up two directories"},
    {"....", "cd ../../..", "Go up three directories"},
    {NULL, NULL, NULL}
};

/* ========================================
 * Japanese Error Messages with Personality
 * ======================================== */

typedef struct {
    const char *prefix;      /* Character speech prefix */
    const char *message;     /* Main error message */
    const char *hint;        /* Helpful hint */
    SishEmotion emotion;     /* Associated emotion */
} SishErrorTemplate;

static const SishErrorTemplate sish_error_templates[SISH_ERR_COUNT] = {
    /* SISH_ERR_COMMAND_NOT_FOUND */
    {
        "お兄ちゃん！",
        "\"%s\"って何？コマンドが見つからないよ〜",
        "もしかして: %s",
        SISH_EMOTION_CONFUSED
    },
    /* SISH_ERR_PERMISSION_DENIED */
    {
        "えっと...",
        "\"%s\"を実行する権限がないみたい...",
        "sudoを使ってみて！",
        SISH_EMOTION_SAD
    },
    /* SISH_ERR_FILE_NOT_FOUND */
    {
        "あれれ？",
        "ファイル\"%s\"が見つからないよ〜",
        "パスを確認してね！",
        SISH_EMOTION_CONFUSED
    },
    /* SISH_ERR_SYNTAX_ERROR */
    {
        "ちょっと待って！",
        "構文がおかしいよ...\"%s\"",
        "閉じ括弧とかクォートを確認してみて！",
        SISH_EMOTION_THINKING
    },
    /* SISH_ERR_EXEC_FORMAT */
    {
        "うーん...",
        "\"%s\"は実行できない形式みたい",
        "ファイル形式を確認してね！",
        SISH_EMOTION_THINKING
    },
    /* SISH_ERR_NO_SUCH_FILE_OR_DIR */
    {
        "あれれ？",
        "\"%s\"というファイルやディレクトリはないよ〜",
        "lsで確認してみて！",
        SISH_EMOTION_CONFUSED
    },
    /* SISH_ERR_IS_DIRECTORY */
    {
        "ちょっと！",
        "\"%s\"はディレクトリだよ〜ファイルじゃないよ！",
        "cdで移動してみる？",
        SISH_EMOTION_EXCITED
    },
    /* SISH_ERR_NOT_DIRECTORY */
    {
        "えっと...",
        "\"%s\"はディレクトリじゃないよ〜",
        "ファイルパスを確認してね！",
        SISH_EMOTION_CONFUSED
    },
    /* SISH_ERR_PIPE_ERROR */
    {
        "あわわ...",
        "パイプでエラーが起きちゃった...",
        "コマンドの接続を確認してね！",
        SISH_EMOTION_SAD
    },
    /* SISH_ERR_MEMORY_ERROR */
    {
        "大変！",
        "メモリが足りないみたい...",
        "他のプロセスを終了してみて！",
        SISH_EMOTION_SAD
    },
    /* SISH_ERR_GENERAL */
    {
        "あれ？",
        "なんかエラーになっちゃった...\"%s\"",
        "もう一度試してみて！",
        SISH_EMOTION_CONFUSED
    }
};

/* ========================================
 * Levenshtein Distance Implementation
 * ======================================== */

/**/
int
sish_levenshtein_distance(const char *s1, const char *s2)
{
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    
    if (len1 == 0) return len2;
    if (len2 == 0) return len1;
    
    /* Use two rows for memory efficiency */
    int *prev_row = (int *)zalloc((len2 + 1) * sizeof(int));
    int *curr_row = (int *)zalloc((len2 + 1) * sizeof(int));
    
    if (!prev_row || !curr_row) {
        if (prev_row) zfree(prev_row, (len2 + 1) * sizeof(int));
        if (curr_row) zfree(curr_row, (len2 + 1) * sizeof(int));
        return -1;
    }
    
    /* Initialize first row */
    for (size_t j = 0; j <= len2; j++) {
        prev_row[j] = j;
    }
    
    /* Fill in the rest */
    for (size_t i = 1; i <= len1; i++) {
        curr_row[0] = i;
        for (size_t j = 1; j <= len2; j++) {
            int cost = (s1[i-1] == s2[j-1]) ? 0 : 1;
            curr_row[j] = sish_min3(
                prev_row[j] + 1,      /* deletion */
                curr_row[j-1] + 1,    /* insertion */
                prev_row[j-1] + cost  /* substitution */
            );
        }
        /* Swap rows */
        int *temp = prev_row;
        prev_row = curr_row;
        curr_row = temp;
    }
    
    int result = prev_row[len2];
    zfree(prev_row, (len2 + 1) * sizeof(int));
    zfree(curr_row, (len2 + 1) * sizeof(int));
    
    return result;
}

/* ========================================
 * Tone/Personality Management
 * ======================================== */

/**/
void
sish_set_tone(SishTone sish_tone)
{
    if (sish_tone >= 0 && sish_tone < SISH_TONE_COUNT) {
        current_tone = sish_tone;
        tone_initialized = 1;
    }
}

/**/
SishTone
sish_get_tone(void)
{
    sish_init_tone_from_env_once();
    return current_tone;
}

/**/
const char *
sish_tone_name(SishTone sish_tone)
{
    static const char *names[] = {
        "標準妹モード",
        "しっかり妹モード",
        "甘え妹モード",
        "せっかち妹モード",
        "教え上手妹モード",
        "無感情妹モード",
        "ヤンデレ妹モード"
    };
    if (sish_tone >= 0 && sish_tone < SISH_TONE_COUNT) {
        return names[sish_tone];
    }
    return "不明";
}

/* ========================================
 * Command Suggestion System
 * ======================================== */

typedef struct {
    char *cmd;
    int distance;
} SishSuggestion;

static int
compare_suggestions(const void *a, const void *b)
{
    return ((SishSuggestion *)a)->distance - ((SishSuggestion *)b)->distance;
}

/**/
char **
sish_find_similar_commands(const char *cmd, int *count)
{
    SishSuggestion suggestions[256];
    int num_suggestions = 0;
    char **result;
    
    if (!cmd || strlen(cmd) < SISH_MIN_CMD_LEN) {
        *count = 0;
        return NULL;
    }
    
    /* Search in common commands database */
    for (int i = 0; sish_common_commands[i] != NULL; i++) {
        int dist = sish_levenshtein_distance(cmd, sish_common_commands[i]);
        if (dist >= 0 && dist <= SISH_MAX_DISTANCE && dist > 0) {
            suggestions[num_suggestions].cmd = ztrdup(sish_common_commands[i]);
            suggestions[num_suggestions].distance = dist;
            num_suggestions++;
            if (num_suggestions >= 256) break;
        }
    }
    
    /* Search in PATH for more commands */
    char **pp;
    for (pp = path; *pp; pp++) {
        DIR *dir = opendir(*pp);
        if (!dir) continue;
        
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && num_suggestions < 256) {
            if (entry->d_name[0] == '.') continue;
            
            int dist = sish_levenshtein_distance(cmd, entry->d_name);
            if (dist >= 0 && dist <= SISH_MAX_DISTANCE && dist > 0) {
                /* Check if already in suggestions */
                int found = 0;
                for (int i = 0; i < num_suggestions; i++) {
                    if (strcmp(suggestions[i].cmd, entry->d_name) == 0) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    suggestions[num_suggestions].cmd = ztrdup(entry->d_name);
                    suggestions[num_suggestions].distance = dist;
                    num_suggestions++;
                }
            }
        }
        closedir(dir);
    }
    
    if (num_suggestions == 0) {
        *count = 0;
        return NULL;
    }
    
    /* Sort by distance */
    qsort(suggestions, num_suggestions, sizeof(SishSuggestion), compare_suggestions);
    
    /* Return top suggestions */
    int result_count = num_suggestions < SISH_MAX_SUGGESTIONS ? 
                       num_suggestions : SISH_MAX_SUGGESTIONS;
    
    result = (char **)zalloc((result_count + 1) * sizeof(char *));
    for (int i = 0; i < result_count; i++) {
        result[i] = suggestions[i].cmd;
    }
    result[result_count] = NULL;
    
    /* Free unused suggestions */
    for (int i = result_count; i < num_suggestions; i++) {
        zsfree(suggestions[i].cmd);
    }
    
    *count = result_count;
    return result;
}

/**/
void
sish_free_suggestions(char **suggestions, int count)
{
    if (!suggestions) return;
    for (int i = 0; i < count; i++) {
        if (suggestions[i]) zsfree(suggestions[i]);
    }
    zfree(suggestions, (count + 1) * sizeof(char *));
}

/* ========================================
 * Error Message Functions
 * ======================================== */

/**/
void
sish_print_error(SishErrorType type, const char *cmd, const char *arg)
{
    const SishErrorTemplate *tmpl;
    char **suggestions = NULL;
    int suggestion_count = 0;
    
    if (type >= SISH_ERR_COUNT) {
        type = SISH_ERR_GENERAL;
    }
    tmpl = &sish_error_templates[type];
    
    /* Print character prefix with color */
    fprintf(stderr, "%s%s%s：%s",
            SISH_CHAR_COLOR, SISH_COLOR_BOLD,
            SISH_CHARACTER_NAME, SISH_COLOR_RESET);
    
    /* Print error prefix */
    fprintf(stderr, "%s%s%s ",
            SISH_CHAR_COLOR, tmpl->prefix, SISH_COLOR_RESET);
    
    /* Print main error message */
    fprintf(stderr, "%s", SISH_ERROR_COLOR);
    fprintf(stderr, tmpl->message, arg ? arg : cmd);
    fprintf(stderr, "%s\n", SISH_COLOR_RESET);
    
    /* For command not found, find and print suggestions */
    if (type == SISH_ERR_COMMAND_NOT_FOUND && cmd) {
        suggestions = sish_find_similar_commands(cmd, &suggestion_count);
        
        if (suggestions && suggestion_count > 0) {
            fprintf(stderr, "%s%s       💡 もしかして: %s",
                    SISH_CHAR_COLOR, SISH_COLOR_BOLD, SISH_COLOR_RESET);
            
            for (int i = 0; i < suggestion_count; i++) {
                fprintf(stderr, "%s%s%s",
                        SISH_SUGGEST_COLOR, suggestions[i], SISH_COLOR_RESET);
                if (i < suggestion_count - 1) {
                    fprintf(stderr, ", ");
                }
            }
            fprintf(stderr, "\n");
            
            /* Show hint for first suggestion */
fprintf(stderr, "%s       ヒント: 正しいコマンドを確認してね！%s\n",
                    SISH_HINT_COLOR, SISH_COLOR_RESET);
            
            sish_free_suggestions(suggestions, suggestion_count);
        }
    } else if (tmpl->hint && strlen(tmpl->hint) > 0) {
        /* Print generic hint */
        fprintf(stderr, "%s       💡 %s%s\n",
                SISH_HINT_COLOR, tmpl->hint, SISH_COLOR_RESET);
    }
    
    /* Send emotion to GUI if connected */
    sish_gui_send_emotion(tmpl->emotion);

    /*
     * exec失敗などで子プロセスが `_exit()` する経路があるため、
     * ここでflushしておかないと stderr がパイプのときに出力が落ちる。
     */
    fflush(stderr);
}

/**/
void
sish_print_suggestion(const char *typed_cmd, const char *suggested_cmd)
{
    fprintf(stderr, "%s%s%s：%s",
            SISH_CHAR_COLOR, SISH_COLOR_BOLD,
            SISH_CHARACTER_NAME, SISH_COLOR_RESET);
    fprintf(stderr, "%s\"%s\"%sって、%s\"%s\"%sのこと？\n",
            SISH_CMD_COLOR, typed_cmd, SISH_COLOR_RESET,
            SISH_SUGGEST_COLOR, suggested_cmd, SISH_COLOR_RESET);
}

/* ========================================
 * GUI Communication (Unix Domain Socket)
 * ======================================== */

static int sish_gui_socket = -1;

/**/
int
sish_console_is_running(void)
{
    struct stat st;
    if (!sish_gui_enabled()) return 0;
    return (stat(sish_gui_socket_path(), &st) == 0);
}

/**/
int
sish_gui_connect(void)
{
    struct sockaddr_un addr;
    
    if (sish_gui_socket >= 0) {
        return sish_gui_socket;
    }
    
    if (!sish_gui_enabled()) return -1;

    if (!sish_console_is_running()) {
        if (sish_gui_autostart()) {
            /* best-effort: sish-console がPATHにあれば起動 */
            (void)system("sish-console >/dev/null 2>&1 &");
            /* すぐにはソケットが出ないかもしれないので、少しだけ待つ */
            usleep(150 * 1000);
        }
        if (!sish_console_is_running()) return -1;
    }
    
    sish_gui_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sish_gui_socket < 0) {
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sish_gui_socket_path(), sizeof(addr.sun_path) - 1);
    
    if (connect(sish_gui_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sish_gui_socket);
        sish_gui_socket = -1;
        return -1;
    }
    
    return sish_gui_socket;
}

/**/
void
sish_gui_disconnect(int sock)
{
    if (sock >= 0) {
        close(sock);
    }
    if (sock == sish_gui_socket) {
        sish_gui_socket = -1;
    }
}

/**/
int
sish_gui_send_event(const char *event_type, const char *data)
{
    char buffer[SISH_MAX_MSG_LEN];
    int sock;
    
    sock = sish_gui_connect();
    if (sock < 0) {
        return -1;
    }
    
    /* Format JSON message (data は厳密なJSONエスケープ未対応: 既存仕様維持) */
    snprintf(buffer, sizeof(buffer),
             "{\"type\":\"%s\",\"data\":\"%s\",\"timestamp\":%ld}",
             event_type, data ? data : "", (long)time(NULL));
    
    ssize_t sent = write(sock, buffer, strlen(buffer));
    return (sent > 0) ? 0 : -1;
}

/**/
int
sish_gui_send_emotion(SishEmotion emotion)
{
    const char *emotion_names[] = {
        "happy", "sad", "confused", "angry",
        "thinking", "excited", "sleepy", "neutral"
    };
    
    if (!sish_gui_enabled()) return -1;
    if (!sish_gui_expression_sync()) return 0;
    if (emotion >= 0 && emotion < 8) {
        return sish_gui_send_event("emotion", emotion_names[emotion]);
    }
    return -1;
}

/* ========================================
 * Shortcut/Alias Expansion
 * ======================================== */

/**/
const char *
sish_expand_shortcut(const char *input)
{
    if (!input) return NULL;
    
    for (int i = 0; sish_shortcuts[i].shortcut != NULL; i++) {
        if (strcmp(input, sish_shortcuts[i].shortcut) == 0) {
            return sish_shortcuts[i].expansion;
        }
    }
    
    return NULL;  /* No expansion found */
}

/* ========================================
 * LLM Integration (Optional)
 * ======================================== */

/* LLM endpoint configuration */
static char sish_llm_endpoint[256] = "";
static int sish_llm_enabled = 0;

/**/
void
sish_llm_configure(const char *endpoint)
{
    if (endpoint && strlen(endpoint) > 0) {
        strncpy(sish_llm_endpoint, endpoint, sizeof(sish_llm_endpoint) - 1);
        sish_llm_enabled = 1;
    } else {
        sish_llm_enabled = 0;
    }
}

/* シンプルなHTTP POST用の構造体 */
struct sish_http_response {
    char *data;
    size_t size;
};

static size_t
sish_http_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct sish_http_response *mem = (struct sish_http_response *)userp;
    
    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) return 0;
    
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';
    
    return realsize;
}

/**/
int
sish_llm_query(const char *prompt, char *response, size_t response_size)
{
    if (!sish_llm_enabled_setting()) {
        if (response && response_size > 0) {
            snprintf(response, response_size, "LLM統合が無効です。sish-config で有効にしてください。");
        }
        return -1;
    }
    
    const char *endpoint = sish_llm_endpoint_setting();
    const char *model = sish_llm_model_setting();
    int max_tokens = sish_llm_max_tokens_setting();
    
    if (!endpoint || !*endpoint) {
        if (response && response_size > 0) {
            snprintf(response, response_size, "SISH_LLM_ENDPOINTが設定されていません。");
        }
        return -1;
    }
    
    if (!prompt || !response || response_size == 0) return -1;
    
    /* JSONペイロード構築（エスケープ簡略版） */
    char json_payload[4096];
    char escaped_prompt[2048];
    const char *p = prompt;
    char *ep = escaped_prompt;
    while (*p && (ep - escaped_prompt < (int)sizeof(escaped_prompt) - 3)) {
        if (*p == '"') {
            *ep++ = '\\'; *ep++ = '"';
        } else if (*p == '\\') {
            *ep++ = '\\'; *ep++ = '\\';
        } else if (*p == '\n') {
            *ep++ = '\\'; *ep++ = 'n';
        } else {
            *ep++ = *p;
        }
        p++;
    }
    *ep = '\0';
    
    snprintf(json_payload, sizeof(json_payload),
             "{\"model\": \"%s\", \"messages\": [{\"role\": \"user\", \"content\": \"%s\"}], \"max_tokens\": %d}",
             model && *model ? model : "default", escaped_prompt, max_tokens);
    
    /* このプロセスで外部コマンド curl を使う簡易実装（libcurlを使うには追加ビルド設定が必要） */
    /* 本格実装ではlibcurlを使うべきですが、今はcurlコマンドで代用 */
    char cmd[8192];
    char tmpfile[256];
    snprintf(tmpfile, sizeof(tmpfile), "/tmp/sish_llm_response_%d.txt", (int)getpid());
    snprintf(cmd, sizeof(cmd),
             "curl -s -X POST '%s/v1/chat/completions' "
             "-H 'Content-Type: application/json' "
             "-d '%s' 2>/dev/null > '%s'",
             endpoint, json_payload, tmpfile);
    
    int ret = system(cmd);
    if (ret != 0) {
        snprintf(response, response_size, "LLMへのリクエストに失敗しました（curl実行エラー）。");
        (void)unlink(tmpfile);
        return -1;
    }
    
    FILE *fp = fopen(tmpfile, "r");
    if (!fp) {
        snprintf(response, response_size, "LLM応答ファイルが開けませんでした。");
        (void)unlink(tmpfile);
        return -1;
    }
    
    /* 簡易JSON解析：choices[0].message.content を探す */
    char line[2048];
    int found = 0;
    response[0] = '\0';
    while (fgets(line, sizeof(line), fp)) {
        /* "content":"..." の部分を抜き出す（超簡易） */
        char *content_start = strstr(line, "\"content\":");
        if (content_start) {
            content_start += strlen("\"content\":");
            while (*content_start && isspace((unsigned char)*content_start)) content_start++;
            if (*content_start == '"') {
                content_start++;
                char *content_end = strchr(content_start, '"');
                if (content_end) {
                    size_t len = (size_t)(content_end - content_start);
                    if (len >= response_size) len = response_size - 1;
                    memcpy(response, content_start, len);
                    response[len] = '\0';
                    found = 1;
                    break;
                }
            }
        }
    }
    fclose(fp);
    (void)unlink(tmpfile);
    
    if (!found) {
        snprintf(response, response_size, "LLM応答の解析に失敗しました。");
        return -1;
    }
    
    return 0;
}


/* ========================================
 * Wrapper Functions for Zsh Integration
 * ======================================== */

static void sish_store_last_suggestion(const char *typed, const char *suggested);

/*
 * This function is called instead of the standard "command not found" error.
 * It provides friendly error messages and suggestions.
 */
/**/
mod_export void
sish_command_not_found(const char *cmd)
{
    char **suggestions = NULL;
    int suggestion_count = 0;

    /* Ensure tone is loaded from environment (for -c mode) */
    (void)sish_get_tone();

    /* forkしても拾えるよう、親PID単位で提案を保存 */
    /* (prototype is static below) */

    /* よく使うコマンド（未インストール時の案内） */
    if (cmd && *cmd) {
        if (!strcmp(cmd, "sl") || !strcmp(cmd, "cowsay") ||
            !strcmp(cmd, "fortune") || !strcmp(cmd, "oneko")) {
            fprintf(stderr, "%s%s%s：%s", SISH_CHAR_COLOR, SISH_COLOR_BOLD,
                    SISH_CHARACTER_NAME, SISH_COLOR_RESET);
            fprintf(stderr, "%sお兄ちゃん！%s", SISH_CHAR_COLOR, SISH_COLOR_RESET);
            fprintf(stderr, "%s\"%s\"%sが見つからないよ〜。", SISH_CMD_COLOR, cmd, SISH_COLOR_RESET);
            fprintf(stderr, "%s（入ってなかったらインストールしてね）%s\n",
                    SISH_HINT_COLOR, SISH_COLOR_RESET);

            if (!strcmp(cmd, "sl"))
                fprintf(stderr, "%s       例: sudo apt install sl%s\n", SISH_HINT_COLOR, SISH_COLOR_RESET);
            else if (!strcmp(cmd, "cowsay"))
                fprintf(stderr, "%s       例: sudo apt install cowsay%s\n", SISH_HINT_COLOR, SISH_COLOR_RESET);
            else if (!strcmp(cmd, "fortune"))
                fprintf(stderr, "%s       例: sudo apt install fortune-mod%s\n", SISH_HINT_COLOR, SISH_COLOR_RESET);
            else if (!strcmp(cmd, "oneko"))
                fprintf(stderr, "%s       例: sudo apt install oneko%s\n", SISH_HINT_COLOR, SISH_COLOR_RESET);

            fflush(stderr);
            return;
        }
    }

    if (!cmd || !*cmd) {
        sish_print_error(SISH_ERR_COMMAND_NOT_FOUND, cmd, cmd);
        return;
    }

    suggestions = sish_find_similar_commands(cmd, &suggestion_count);
    if (suggestions && suggestion_count > 0 && suggestions[0]) {
        sish_store_last_suggestion(cmd, suggestions[0]);

        /* 口調に応じたメッセージ */
        const ToneMessage *tmsg = &tone_cmd_not_found[sish_get_tone()];
        
        fprintf(stderr, "%s%s%s：%s", SISH_CHAR_COLOR, SISH_COLOR_BOLD,
                SISH_CHARACTER_NAME, SISH_COLOR_RESET);
        
        /* Prefix（呼びかけ）がある場合のみ出力 */
        if (tmsg->prefix && strlen(tmsg->prefix) > 0) {
            fprintf(stderr, "%s%s%s", SISH_CHAR_COLOR, tmsg->prefix, SISH_COLOR_RESET);
        }
        
        /* ヒント・提案部分 */
        if (current_tone == SISH_TONE_QUICK) {
            /* せっかち妹：即実行形式 */
            fprintf(stderr, "%s%s%s → %s%s%s。%s\n", 
                    SISH_CMD_COLOR, cmd, SISH_COLOR_RESET,
                    SISH_SUGGEST_COLOR, suggestions[0], SISH_COLOR_RESET, 
                    tmsg->hint);
        } else if (current_tone == SISH_TONE_EMOTIONLESS) {
            /* 無感情妹：淡々と */
            fprintf(stderr, "%s%s%s 不明。%s%s%s 提案。\n", 
                    SISH_CMD_COLOR, cmd, SISH_COLOR_RESET,
                    SISH_SUGGEST_COLOR, suggestions[0], SISH_COLOR_RESET);
        } else if (current_tone == SISH_TONE_YANDERE) {
            /* ヤンデレ妹：決め打ち */
            fprintf(stderr, "%s%s%sなんて使わないで…%s%s%sだけ使って…絶対に…\n",
                    SISH_CMD_COLOR, cmd, SISH_COLOR_RESET,
                    SISH_SUGGEST_COLOR, suggestions[0], SISH_COLOR_RESET);
        } else {
            /* その他の口調：コマンド名から始める */
            /* メインメッセージ */
            fprintf(stderr, "%s%s%s", SISH_CMD_COLOR, cmd, SISH_COLOR_RESET);
            
            if (current_tone == SISH_TONE_RELIABLE) {
                /* しっかり妹：断定的 */
                fprintf(stderr, " は存在しない。%s%s%s を実行する？\n", 
                        SISH_SUGGEST_COLOR, suggestions[0], SISH_COLOR_RESET);
            } else if (current_tone == SISH_TONE_SWEET) {
                /* 甘え妹：弱気 */
                fprintf(stderr, "って無いみたい…\n");
                fprintf(stderr, "%s       %s%s%sなら、あるよ…？%s\n",
                        SISH_HINT_COLOR, SISH_SUGGEST_COLOR, suggestions[0], 
                        SISH_COLOR_RESET, SISH_COLOR_RESET);
            } else if (current_tone == SISH_TONE_TEACHER) {
                /* 教え上手妹：説明追加 */
                fprintf(stderr, "はコマンドに無いよ\n");
                fprintf(stderr, "%s       もしかして: %s%s%s\n",
                        SISH_HINT_COLOR, SISH_SUGGEST_COLOR, suggestions[0], SISH_COLOR_RESET);
            } else {
                /* 標準妹：デフォルト */
                fprintf(stderr, "って無いよ…%s%s%sの間違いじゃない？\n",
                        SISH_SUGGEST_COLOR, suggestions[0], SISH_COLOR_RESET);
            }
        }

        /* 他の候補もあれば併記（標準・教え上手・しっかり妹のみ） */
        if (suggestion_count > 1 && 
            (current_tone == SISH_TONE_STANDARD || 
             current_tone == SISH_TONE_TEACHER ||
             current_tone == SISH_TONE_RELIABLE)) {
            fprintf(stderr, "%s       ほかにも: %s", SISH_HINT_COLOR, SISH_COLOR_RESET);
            for (int i = 1; i < suggestion_count; i++) {
                fprintf(stderr, "%s%s%s", SISH_SUGGEST_COLOR, suggestions[i], SISH_COLOR_RESET);
                if (i < suggestion_count - 1) fprintf(stderr, ", ");
            }
            fprintf(stderr, "\n");
        }

        sish_free_suggestions(suggestions, suggestion_count);
        fflush(stderr);
        return;
    }

    if (suggestions) {
        sish_free_suggestions(suggestions, suggestion_count);
    }

    if (cmd && *cmd) {
        sish_store_last_suggestion(cmd, NULL);
    }
    sish_print_error(SISH_ERR_COMMAND_NOT_FOUND, cmd, cmd);
    fflush(stderr);
}

/*
 * Save the last suggestion to a per-parent-PID file in /tmp.
 * This allows the interactive shell to accept suggestions even if
 * the message originated from a forked child.
 */
static void
sish_store_last_suggestion(const char *typed, const char *suggested)
{
    char path[128];
    pid_t ppid = getppid();

    (void)typed;
    snprintf(path, sizeof(path), "/tmp/sish-last-suggestion-%ld", (long)ppid);

    if (!suggested || !*suggested) {
        unlink(path);
        return;
    }

    FILE *fp = fopen(path, "w");
    if (!fp)
        return;
    fprintf(fp, "%s\n", suggested);
    fclose(fp);
}

/*
 * cd でディレクトリが見つからない時の、妹口調案内 + 候補提示
 */
/**/
mod_export void
sish_cd_not_found(const char *dest)
{
    DIR *dirp;
    struct dirent *dp;
    char *best[3] = {NULL, NULL, NULL};
    int bestdist[3] = {9999, 9999, 9999};

    if (!dest) dest = "";

    dirp = opendir(".");
    if (dirp) {
        while ((dp = readdir(dirp)) != NULL) {
            struct stat st;
            int dist;

            if (dp->d_name[0] == '.')
                continue;
            if (stat(dp->d_name, &st) != 0)
                continue;
            if (!S_ISDIR(st.st_mode))
                continue;

            dist = sish_levenshtein_distance(dest, dp->d_name);
            if (dist <= SISH_MAX_DISTANCE) {
                for (int i = 0; i < 3; i++) {
                    if (dist < bestdist[i]) {
                        for (int j = 2; j > i; j--) {
                            bestdist[j] = bestdist[j-1];
                            best[j] = best[j-1];
                        }
                        bestdist[i] = dist;
                        best[i] = ztrdup(dp->d_name);
                        break;
                    }
                }
            }
        }
        closedir(dirp);
    }

    fprintf(stderr, "%s%s%s：%s", SISH_CHAR_COLOR, SISH_COLOR_BOLD,
            SISH_CHARACTER_NAME, SISH_COLOR_RESET);
    fprintf(stderr, "%sお兄ちゃん！%s", SISH_CHAR_COLOR, SISH_COLOR_RESET);
    fprintf(stderr, "%s\"%s\"%sってディレクトリが見つからないよ？ ",
            SISH_CMD_COLOR, dest, SISH_COLOR_RESET);

    if (best[0]) {
        fprintf(stderr, "%s\"%s\"%sの間違いじゃない？%s\n",
                SISH_SUGGEST_COLOR, best[0], SISH_COLOR_RESET, SISH_COLOR_RESET);
        fprintf(stderr, "%s       それっぽいフォルダ: %s%s%s",
                SISH_HINT_COLOR, SISH_SUGGEST_COLOR, best[0], SISH_COLOR_RESET);
        if (best[1]) {
            fprintf(stderr, ", %s%s%s", SISH_SUGGEST_COLOR, best[1], SISH_COLOR_RESET);
        }
        if (best[2]) {
            fprintf(stderr, ", %s%s%s", SISH_SUGGEST_COLOR, best[2], SISH_COLOR_RESET);
        }
        fprintf(stderr, "\n");
        fprintf(stderr, "%s       ヒント: パスを確認してね！%s\n",
                SISH_HINT_COLOR, SISH_COLOR_RESET);
    } else {
        fprintf(stderr, "%sここにはそれっぽいフォルダ無かった…%s\n",
                SISH_HINT_COLOR, SISH_COLOR_RESET);
    }

    fflush(stderr);

    for (int i = 0; i < 3; i++) {
        if (best[i]) zsfree(best[i]);
    }
}

/*
 * This function is called for permission denied errors.
 */
/**/
mod_export void
sish_permission_denied(const char *path)
{
    sish_print_error(SISH_ERR_PERMISSION_DENIED, path, path);
}

/*
 * This function is called for file not found errors.
 */
/**/
mod_export void
sish_file_not_found(const char *path)
{
    sish_print_error(SISH_ERR_FILE_NOT_FOUND, path, path);

    if (sish_completion_enabled()) {
        char **suggestions = NULL;
        int count = sish_complete_path(path, &suggestions);
        if (count > 0) {
            sish_show_completions(suggestions, count);
        }
        sish_free_completion_suggestions(suggestions, count);
    }
}

/*
 * Initialize Sish-specific features.
 * Called during shell startup.
 */
/**/
mod_export void
sish_init(void)
{
    /* Disable zsh's built-in correction to avoid conflicts with Sish */
    opts[CORRECT] = 0;
    opts[CORRECTALL] = 0;
    
    /* Load tone setting from environment */
    (void)sish_get_tone();
    
    /* Try to connect to GUI console */
    if (sish_console_is_running()) {
        sish_gui_connect();
        sish_gui_send_event("shell_start", SISH_VERSION);
    }
    
    /* Check for LLM endpoint in environment */
    if (sish_llm_enabled_setting() && sish_llm_endpoint_setting()) {
        sish_llm_configure(sish_llm_endpoint_setting());
    }
}

/*
 * Cleanup Sish resources.
 * Called during shell exit.
 */
/**/
mod_export void
sish_cleanup(void)
{
    if (sish_gui_socket >= 0) {
        sish_gui_send_event("shell_exit", "");
        sish_gui_disconnect(sish_gui_socket);
    }
}

/*
 * Send pre-execution event to GUI.
 */
/**/
mod_export void
sish_preexec(const char *cmdline)
{
    if (sish_gui_socket >= 0 || sish_console_is_running()) {
        sish_gui_send_event("preexec", cmdline);
        sish_gui_send_emotion(SISH_EMOTION_THINKING);
    }
}

/*
 * Send post-execution event to GUI.
 */
/**/
mod_export void
sish_precmd(int exit_status)
{
    char status_str[32];
    snprintf(status_str, sizeof(status_str), "%d", exit_status);
    
    if (sish_gui_socket >= 0 || sish_console_is_running()) {
        sish_gui_send_event("precmd", status_str);
        sish_gui_send_emotion(exit_status == 0 ? 
                             SISH_EMOTION_HAPPY : SISH_EMOTION_SAD);
    }
}
