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
    return (stat(SISH_SOCKET_PATH, &st) == 0);
}

/**/
int
sish_gui_connect(void)
{
    struct sockaddr_un addr;
    
    if (sish_gui_socket >= 0) {
        return sish_gui_socket;
    }
    
    if (!sish_console_is_running()) {
        return -1;
    }
    
    sish_gui_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sish_gui_socket < 0) {
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SISH_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
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
    
    /* Format JSON message */
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

/**/
int
sish_llm_query(const char *prompt, char *response, size_t response_size)
{
    /* This is a placeholder for LLM integration
     * In a real implementation, this would:
     * 1. Connect to the LLM endpoint (e.g., LMStudio)
     * 2. Send the prompt as a JSON request
     * 3. Parse the response
     * 4. Return the generated text
     */
    
    if (!sish_llm_enabled || !prompt || !response) {
        return -1;
    }
    
    /* For now, return a placeholder message */
    snprintf(response, response_size,
             "LLMへの接続は設定されていません。SISH_LLM_ENDPOINTを設定してください。");
    
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

        /* 企画書の例に寄せたワンライナー */
        fprintf(stderr, "%s%s%s：%s", SISH_CHAR_COLOR, SISH_COLOR_BOLD,
                SISH_CHARACTER_NAME, SISH_COLOR_RESET);
        fprintf(stderr, "%sお兄ちゃん！%s", SISH_CHAR_COLOR, SISH_COLOR_RESET);
        fprintf(stderr, "%s\"%s\"%sって何？%s\"%s\"%sの間違いじゃない？\n",
                SISH_CMD_COLOR, cmd, SISH_COLOR_RESET,
                SISH_SUGGEST_COLOR, suggestions[0], SISH_COLOR_RESET);

        /* 他の候補もあれば併記 */
        if (suggestion_count > 1) {
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
    
    /* Try to connect to GUI console */
    if (sish_console_is_running()) {
        sish_gui_connect();
        sish_gui_send_event("shell_start", SISH_VERSION);
    }
    
    /* Check for LLM endpoint in environment */
    char *llm_endpoint = getenv("SISH_LLM_ENDPOINT");
    if (llm_endpoint) {
        sish_llm_configure(llm_endpoint);
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
