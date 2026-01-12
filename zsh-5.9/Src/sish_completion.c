/*
 * sish_completion.c - 強化された補完システム
 *
 * 20の主要コマンドに対する高度な補完機能
 * ディレクトリ内の類似ファイル名提案
 */

#include "zsh.mdh"
#include "sish.h"
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

/* 主要コマンドのデータベース */
typedef struct {
    const char *cmd;
    const char *description_ja;
    const char *description_en;
    int (*completion_func)(const char *input, char ***suggestions);
} FamousCommand;

/* ディレクトリ内のファイル名を取得して類似度でソート */
static int
find_similar_files_in_dir(const char *dir, const char *partial, char ***results)
{
    DIR *dp;
    struct dirent *ep;
    char **candidates = NULL;
    int *distances = NULL;
    int count = 0, capacity = 50;
    char fullpath[PATH_MAX];
    struct stat st;
    
    const char *needle = partial ? partial : "";
    int max_candidates = sish_completion_max_candidates();
    if (max_candidates < 1) max_candidates = 1;
    if (max_candidates > 1000) max_candidates = 1000;

    dp = opendir(dir ? dir : ".");
    if (!dp) return 0;
    
    candidates = (char **)malloc(capacity * sizeof(char *));
    distances = (int *)malloc(capacity * sizeof(int));
    
    while ((ep = readdir(dp))) {
        if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
            continue;
        if (ep->d_name[0] == '.' && !needle[0])
            continue;  /* ドットファイルはスキップ */
        
        /* パス作成 */
        snprintf(fullpath, sizeof(fullpath), "%s/%s", 
                dir ? dir : ".", ep->d_name);
        
        /* 部分一致または類似度チェック */
        int ok = 0;
        int dist = 9999;

        if (needle[0] == '\0') {
            ok = 1;
            dist = 0;
        } else if (strstr(ep->d_name, needle) ||
                   strncmp(ep->d_name, needle, strlen(needle)) == 0) {
            ok = 1;
            dist = 0;
        } else if (sish_completion_dir_similarity() && sish_completion_fuzzy()) {
            dist = sish_levenshtein_distance(ep->d_name, needle);
            ok = (dist >= 0 && dist <= 3);
        }

        if (ok) {
            
            if (count >= capacity) {
                capacity *= 2;
                candidates = (char **)realloc(candidates, capacity * sizeof(char *));
                distances = (int *)realloc(distances, capacity * sizeof(int));
            }
            
            candidates[count] = strdup(ep->d_name);
            distances[count] = dist;
            
            /* ディレクトリには "/"を追加 */
            if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode)) {
                char *tmp = malloc(strlen(candidates[count]) + 2);
                sprintf(tmp, "%s/", candidates[count]);
                free(candidates[count]);
                candidates[count] = tmp;
            }
            
            count++;
        }
    }
    closedir(dp);
    
    /* 類似度でソート（バブルソート） */
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (distances[j] > distances[j + 1]) {
                /* スワップ */
                int tmp_dist = distances[j];
                distances[j] = distances[j + 1];
                distances[j + 1] = tmp_dist;
                
                char *tmp_str = candidates[j];
                candidates[j] = candidates[j + 1];
                candidates[j + 1] = tmp_str;
            }
        }
    }
    
    free(distances);

    /* max_candidates を超える分はここで解放して漏れを防ぐ */
    if (count > max_candidates) {
        for (int i = max_candidates; i < count; i++) {
            free(candidates[i]);
        }
        count = max_candidates;
    }

    *results = candidates;
    return count;
}

static int
sish_is_pathish_token(const char *tok)
{
    if (!tok || !*tok) return 0;
    /* よくある「パスっぽい」ものだけ拾う（過剰に広げない） */
    if (tok[0] == '/' || tok[0] == '.' || tok[0] == '~') return 1;
    if (strchr(tok, '/')) return 1;
    return 0;
}

static void
sish_strip_simple_quotes(char *s)
{
    if (!s) return;
    size_t n = strlen(s);
    if (n >= 2) {
        if ((s[0] == '\'' && s[n - 1] == '\'') || (s[0] == '"' && s[n - 1] == '"')) {
            memmove(s, s + 1, n - 2);
            s[n - 2] = '\0';
        }
    }
}

static int
sish_suggestion_exists(char **arr, int count, const char *s)
{
    if (!arr || !s) return 0;
    for (int i = 0; i < count; i++) {
        if (arr[i] && strcmp(arr[i], s) == 0) return 1;
    }
    return 0;
}

static int
sish_base_matches(const char *base, const char *needle)
{
    if (!needle || !*needle) return 1;
    if (!base) return 0;

    if (strncmp(base, needle, strlen(needle)) == 0) return 1;
    if (strstr(base, needle)) return 1;
    /* 履歴はディレクトリ類似度フラグに依存させず、FUZZYのみで曖昧一致を許可 */
    if (sish_completion_fuzzy()) {
        int d = sish_levenshtein_distance(base, needle);
        if (d >= 0 && d <= 3) return 1;
    }
    return 0;
}

static int
sish_append_history_path_suggestions(const char *partial_path, const char *needle_base,
                                    const char *prefix, size_t prefix_len,
                                    char **out, int out_count, int out_cap)
{
    FILE *fp = NULL;
    char pathbuf[PATH_MAX];
    char line[2048];

    if (!sish_completion_history()) return out_count;
    if (out_count >= out_cap) return out_count;

    /* できれば zsh の HISTFILE を使い、無ければ ~/.sish_history を読む */
    const char *hf = getsparam("HISTFILE");
    if (hf && *hf) {
        snprintf(pathbuf, sizeof(pathbuf), "%s", hf);
    } else {
        const char *home = getenv("HOME");
        if (!home || !*home) return out_count;
        snprintf(pathbuf, sizeof(pathbuf), "%s/.sish_history", home);
    }

    fp = fopen(pathbuf, "r");
    if (!fp) return out_count;

    /* 大きすぎる履歴は末尾寄りだけ読む（最大 256KB） */
    if (fseek(fp, 0, SEEK_END) == 0) {
        long end = ftell(fp);
        if (end > 0) {
            long start = end - (256 * 1024);
            if (start < 0) start = 0;
            (void)fseek(fp, start, SEEK_SET);
            if (start > 0) {
                /* 行途中から始まる可能性があるので1行捨てる */
                (void)fgets(line, sizeof(line), fp);
            }
        }
    } else {
        (void)fseek(fp, 0, SEEK_SET);
    }

    while (out_count < out_cap && fgets(line, sizeof(line), fp)) {
        char *cmd = line;
        /* zsh履歴形式: ": 170...:0;command..." の ';' 以降を本体にする */
        char *semi = strchr(line, ';');
        if (semi && semi[1]) cmd = semi + 1;

        /* ざっくりトークン分割（エスケープ等の完全対応はしない） */
        char *saveptr = NULL;
        for (char *tok = strtok_r(cmd, " \t\r\n", &saveptr);
             tok && out_count < out_cap;
             tok = strtok_r(NULL, " \t\r\n", &saveptr)) {

            if (!sish_is_pathish_token(tok)) continue;

            char tmp[PATH_MAX];
            snprintf(tmp, sizeof(tmp), "%s", tok);
            sish_strip_simple_quotes(tmp);

            const char *candidate_base = tmp;
            const char *ls = strrchr(tmp, '/');
            if (ls && ls[1]) candidate_base = ls + 1;

            if (!sish_base_matches(candidate_base, needle_base)) continue;

            char sug[PATH_MAX];
            if (prefix && prefix_len > 0) {
                /* 既にprefix一致しているならそのまま、そうでなければ prefix + basename */
                if (strncmp(tmp, prefix, prefix_len) == 0) {
                    snprintf(sug, sizeof(sug), "%s", tmp);
                } else {
                    snprintf(sug, sizeof(sug), "%.*s%s", (int)prefix_len, prefix, candidate_base);
                }
            } else {
                snprintf(sug, sizeof(sug), "%s", candidate_base);
            }

            if (!sug[0]) continue;
            if (sish_suggestion_exists(out, out_count, sug)) continue;

            out[out_count++] = strdup(sug);
        }
    }

    fclose(fp);
    return out_count;
}

/* 1. git - バージョン管理システム */
static int complete_git(const char *input, char ***suggestions) {
    static const char *git_commands[] = {
        "add", "commit", "push", "pull", "clone", "status", "log",
        "branch", "checkout", "merge", "rebase", "diff", "fetch",
        "reset", "stash", "tag", "remote", "init", NULL
    };
    
    *suggestions = (char **)malloc(20 * sizeof(char *));
    int count = 0;
    
    const char *needle = input ? input : "";
    int max_candidates = sish_completion_max_candidates();
    if (max_candidates < 1) max_candidates = 1;

    for (int i = 0; git_commands[i]; i++) {
        int ok = 0;
        if (strncmp(needle, git_commands[i], strlen(needle)) == 0) {
            ok = 1;
        } else if (sish_completion_fuzzy() &&
                   sish_levenshtein_distance(needle, git_commands[i]) <= 2) {
            ok = 1;
        }
        if (ok) {
            (*suggestions)[count++] = strdup(git_commands[i]);
            if (count >= max_candidates) break;
        }
    }
    
    return count;
}

/* 2. docker - コンテナ管理 */
static int complete_docker(const char *input, char ***suggestions) {
    static const char *docker_commands[] = {
        "run", "ps", "images", "pull", "push", "build", "stop",
        "start", "restart", "rm", "rmi", "exec", "logs", "inspect",
        "compose", "network", "volume", NULL
    };
    
    *suggestions = (char **)malloc(20 * sizeof(char *));
    int count = 0;
    
    const char *needle = input ? input : "";
    int max_candidates = sish_completion_max_candidates();
    if (max_candidates < 1) max_candidates = 1;

    for (int i = 0; docker_commands[i]; i++) {
        int ok = 0;
        if (strncmp(needle, docker_commands[i], strlen(needle)) == 0) {
            ok = 1;
        } else if (sish_completion_fuzzy() &&
                   sish_levenshtein_distance(needle, docker_commands[i]) <= 2) {
            ok = 1;
        }
        if (ok) {
            (*suggestions)[count++] = strdup(docker_commands[i]);
            if (count >= max_candidates) break;
        }
    }
    
    return count;
}

/* 3. npm - Node.jsパッケージマネージャ */
static int complete_npm(const char *input, char ***suggestions) {
    static const char *npm_commands[] = {
        "install", "update", "uninstall", "run", "start", "test",
        "build", "init", "publish", "search", "list", NULL
    };
    
    *suggestions = (char **)malloc(15 * sizeof(char *));
    int count = 0;
    
    const char *needle = input ? input : "";
    int max_candidates = sish_completion_max_candidates();
    if (max_candidates < 1) max_candidates = 1;

    for (int i = 0; npm_commands[i]; i++) {
        if (strncmp(needle, npm_commands[i], strlen(needle)) == 0) {
            (*suggestions)[count++] = strdup(npm_commands[i]);
            if (count >= max_candidates) break;
        }
    }
    
    return count;
}

/* 4. python/python3 - Pythonインタープリタ */
static int complete_python(const char *input, char ***suggestions) {
    /* カレントディレクトリの.pyファイルを提案 */
    return find_similar_files_in_dir(".", input, suggestions);
}

/* 5. cd - ディレクトリ変更 */
static int complete_cd(const char *input, char ***suggestions) {
    DIR *dp;
    struct dirent *ep;
    struct stat st;
    char **candidates = NULL;
    int count = 0, capacity = 50;
    
    dp = opendir(".");
    if (!dp) return 0;
    
    candidates = (char **)malloc(capacity * sizeof(char *));
    
    const char *needle = input ? input : "";
    int max_candidates = sish_completion_max_candidates();
    if (max_candidates < 1) max_candidates = 1;

    while ((ep = readdir(dp))) {
        if (ep->d_name[0] == '.' && strlen(needle) == 0)
            continue;
        
        /* ディレクトリのみ */
        if (stat(ep->d_name, &st) == 0 && S_ISDIR(st.st_mode)) {
            int ok = 0;
            if (strncmp(needle, ep->d_name, strlen(needle)) == 0) {
                ok = 1;
            } else if (sish_completion_dir_similarity() && sish_completion_fuzzy() &&
                       sish_levenshtein_distance(needle, ep->d_name) <= 2) {
                ok = 1;
            }

            if (ok) {
                
                if (count >= capacity) {
                    capacity *= 2;
                    candidates = (char **)realloc(candidates, capacity * sizeof(char *));
                }
                
                candidates[count] = malloc(strlen(ep->d_name) + 2);
                sprintf(candidates[count], "%s/", ep->d_name);
                count++;
                if (count >= max_candidates) break;
            }
        }
    }
    closedir(dp);
    
    *suggestions = candidates;
    return count;
}

/* 6-20. その他のコマンド補完 */
static int complete_generic_file(const char *input, char ***suggestions) {
    return find_similar_files_in_dir(".", input, suggestions);
}

/* 主要コマンドリスト */
static FamousCommand famous_commands[] = {
    {"git", "バージョン管理システム", "Version control system", complete_git},
    {"docker", "コンテナ管理", "Container management", complete_docker},
    {"npm", "Node.jsパッケージマネージャ", "Node.js package manager", complete_npm},
    {"yarn", "Node.jsパッケージマネージャ", "Node.js package manager", complete_npm},
    {"python", "Pythonインタープリタ", "Python interpreter", complete_python},
    {"python3", "Python 3インタープリタ", "Python 3 interpreter", complete_python},
    {"pip", "Pythonパッケージインストーラ", "Python package installer", complete_generic_file},
    {"pip3", "Python 3パッケージインストーラ", "Python 3 package installer", complete_generic_file},
    {"cd", "ディレクトリ変更", "Change directory", complete_cd},
    {"ls", "ファイル一覧表示", "List files", complete_generic_file},
    {"cat", "ファイル内容表示", "Show file contents", complete_generic_file},
    {"grep", "テキスト検索", "Search text", complete_generic_file},
    {"find", "ファイル検索", "Find files", complete_generic_file},
    {"ssh", "リモート接続", "Remote connection", complete_generic_file},
    {"sudo", "管理者権限実行", "Run as administrator", complete_generic_file},
    {"vim", "テキストエディタ", "Text editor", complete_generic_file},
    {"nano", "テキストエディタ", "Text editor", complete_generic_file},
    {"make", "ビルドツール", "Build tool", complete_generic_file},
    {"cargo", "Rustビルドツール", "Rust build tool", complete_generic_file},
    {"go", "Go言語ツール", "Go language tool", complete_generic_file},
    {NULL, NULL, NULL, NULL}
};

/*
 * コマンドに対して高度な補完を提供
 */
mod_export int
sish_smart_completion(const char *cmd, const char *arg, char ***suggestions)
{
    if (!sish_completion_enabled()) {
        *suggestions = NULL;
        return 0;
    }
    if (!cmd) {
        *suggestions = NULL;
        return 0;
    }

    const char *needle = arg ? arg : "";

    /* 主要コマンドを検索 */
    for (int i = 0; famous_commands[i].cmd; i++) {
        if (strcmp(cmd, famous_commands[i].cmd) == 0) {
            if (sish_lang_is_en()) {
                fprintf(stderr, "%s💡 Completion candidates for %s:%s\n",
                        SISH_HINT_COLOR, cmd, SISH_COLOR_RESET);
            } else {
                fprintf(stderr, "%s💡 %s の補完候補：%s\n",
                        SISH_HINT_COLOR, cmd, SISH_COLOR_RESET);
            }
            return famous_commands[i].completion_func(needle, suggestions);
        }
    }
    
    /* デフォルト：カレントディレクトリのファイル */
    return find_similar_files_in_dir(".", needle, suggestions);
}

/*
 * 補完候補を表示
 */
mod_export void
sish_show_completions(char **suggestions, int count)
{
    int max_candidates = sish_completion_max_candidates();
    if (max_candidates < 1) max_candidates = 1;
    if (count > max_candidates) count = max_candidates;

    if (count == 0) {
        fprintf(stderr, "%s%s%s\n",
            SISH_CHAR_COLOR,
            sish_lang_is_en() ? "Hmm... no completion candidates found..." : "うーん、補完候補が見つからないよ...",
            SISH_COLOR_RESET);
        fflush(stderr);
        return;
    }
    
        fprintf(stderr, "%s%s%s\n",
            SISH_CHAR_COLOR,
            sish_lang_is_en() ? "✨ How about these?" : "✨ こんなのはどう？",
            SISH_COLOR_RESET);
    
    for (int i = 0; i < count; i++) {
        fprintf(stderr, "  %s%d%s) %s%s%s\n",
                SISH_CMD_COLOR, i + 1, SISH_COLOR_RESET,
                SISH_HINT_COLOR, suggestions[i], SISH_COLOR_RESET);
    }
    
        fprintf(stderr, "%s%s%s\n",
            SISH_HINT_COLOR,
            sish_lang_is_en() ? "Press Tab to keep completing!" : "Tabキーでどんどん補完できるよ！",
            SISH_COLOR_RESET);

    fflush(stderr);
}

/*
 * パス文字列（dir/partial）に対して補完候補を返す
 * 返却されたsuggestionsは sish_free_completion_suggestions で解放すること。
 */
mod_export int
sish_complete_path(const char *partial_path, char ***suggestions)
{
    char dirbuf[PATH_MAX];
    const char *base;
    const char *slash;
    const char *prefix = NULL;
    size_t prefix_len = 0;

    if (!sish_completion_enabled()) {
        *suggestions = NULL;
        return 0;
    }

    if (!partial_path) partial_path = "";

    slash = strrchr(partial_path, '/');
    if (slash) {
        size_t dirlen = (size_t)(slash - partial_path);
        prefix = partial_path;
        prefix_len = (size_t)(slash - partial_path) + 1;
        if (dirlen == 0) {
            strcpy(dirbuf, "/");
        } else {
            if (dirlen >= sizeof(dirbuf)) dirlen = sizeof(dirbuf) - 1;
            memcpy(dirbuf, partial_path, dirlen);
            dirbuf[dirlen] = '\0';
        }
        base = slash + 1;
    } else {
        strcpy(dirbuf, ".");
        base = partial_path;
    }

    int max_candidates = sish_completion_max_candidates();
    if (max_candidates < 1) max_candidates = 1;
    if (max_candidates > 1000) max_candidates = 1000;

    int count = find_similar_files_in_dir(dirbuf, base, suggestions);

    /* 返却配列を max_candidates で確保し直す（後で履歴候補を足せるようにする） */
    char **out = (char **)malloc((size_t)max_candidates * sizeof(char *));
    if (!out) {
        return count;
    }
    for (int i = 0; i < count; i++) {
        out[i] = (*suggestions)[i];
    }
    free(*suggestions);
    *suggestions = out;

    /* ユーザーが入力した prefix（./ や ../ 等）を維持する */
    if (count > 0 && prefix && prefix_len > 0) {
        for (int i = 0; i < count; i++) {
            if (!(*suggestions)[i]) continue;
            char *orig = (*suggestions)[i];
            size_t n = prefix_len + strlen(orig) + 1;
            char *full = (char *)malloc(n);
            if (!full) continue;
            snprintf(full, n, "%.*s%s", (int)prefix_len, prefix, orig);
            free(orig);
            (*suggestions)[i] = full;
        }
    }

    /* 履歴からも候補を足す */
    count = sish_append_history_path_suggestions(partial_path, base, prefix, prefix_len,
                                                 *suggestions, count, max_candidates);

    /* 最終的に max_candidates を超えた分は切り捨て */
    if (count > max_candidates) {
        for (int i = max_candidates; i < count; i++) {
            free((*suggestions)[i]);
            (*suggestions)[i] = NULL;
        }
        count = max_candidates;
    }

    return count;
}

mod_export void
sish_free_completion_suggestions(char **suggestions, int count)
{
    if (!suggestions) return;
    for (int i = 0; i < count; i++) {
        free(suggestions[i]);
    }
    free(suggestions);
}
