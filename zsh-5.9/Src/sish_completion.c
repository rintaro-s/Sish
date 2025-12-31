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

/* 主要コマンドのデータベース */
typedef struct {
    const char *cmd;
    const char *description;
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
    
    dp = opendir(dir ? dir : ".");
    if (!dp) return 0;
    
    candidates = (char **)malloc(capacity * sizeof(char *));
    distances = (int *)malloc(capacity * sizeof(int));
    
    while ((ep = readdir(dp))) {
        if (ep->d_name[0] == '.' && !partial[0])
            continue;  /* ドットファイルはスキップ */
        
        /* パス作成 */
        snprintf(fullpath, sizeof(fullpath), "%s/%s", 
                dir ? dir : ".", ep->d_name);
        
        /* 部分一致または類似度チェック */
        if (strstr(ep->d_name, partial) || 
            sish_levenshtein_distance(ep->d_name, partial) <= 3) {
            
            if (count >= capacity) {
                capacity *= 2;
                candidates = (char **)realloc(candidates, capacity * sizeof(char *));
                distances = (int *)realloc(distances, capacity * sizeof(int));
            }
            
            candidates[count] = strdup(ep->d_name);
            distances[count] = sish_levenshtein_distance(ep->d_name, partial);
            
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
    *results = candidates;
    return count > 10 ? 10 : count;  /* 最大10個 */
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
    
    for (int i = 0; git_commands[i]; i++) {
        if (strncmp(input, git_commands[i], strlen(input)) == 0 ||
            sish_levenshtein_distance(input, git_commands[i]) <= 2) {
            (*suggestions)[count++] = strdup(git_commands[i]);
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
    
    for (int i = 0; docker_commands[i]; i++) {
        if (strncmp(input, docker_commands[i], strlen(input)) == 0 ||
            sish_levenshtein_distance(input, docker_commands[i]) <= 2) {
            (*suggestions)[count++] = strdup(docker_commands[i]);
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
    
    for (int i = 0; npm_commands[i]; i++) {
        if (strncmp(input, npm_commands[i], strlen(input)) == 0) {
            (*suggestions)[count++] = strdup(npm_commands[i]);
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
    
    while ((ep = readdir(dp))) {
        if (ep->d_name[0] == '.' && strlen(input) == 0)
            continue;
        
        /* ディレクトリのみ */
        if (stat(ep->d_name, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (strncmp(input, ep->d_name, strlen(input)) == 0 ||
                sish_levenshtein_distance(input, ep->d_name) <= 2) {
                
                if (count >= capacity) {
                    capacity *= 2;
                    candidates = (char **)realloc(candidates, capacity * sizeof(char *));
                }
                
                candidates[count] = malloc(strlen(ep->d_name) + 2);
                sprintf(candidates[count], "%s/", ep->d_name);
                count++;
            }
        }
    }
    closedir(dp);
    
    *suggestions = candidates;
    return count > 10 ? 10 : count;
}

/* 6-20. その他のコマンド補完 */
static int complete_generic_file(const char *input, char ***suggestions) {
    return find_similar_files_in_dir(".", input, suggestions);
}

/* 主要コマンドリスト */
static FamousCommand famous_commands[] = {
    {"git", "バージョン管理システム", complete_git},
    {"docker", "コンテナ管理", complete_docker},
    {"npm", "Node.jsパッケージマネージャ", complete_npm},
    {"yarn", "Node.jsパッケージマネージャ", complete_npm},
    {"python", "Pythonインタープリタ", complete_python},
    {"python3", "Python 3インタープリタ", complete_python},
    {"pip", "Pythonパッケージインストーラ", complete_generic_file},
    {"pip3", "Python 3パッケージインストーラ", complete_generic_file},
    {"cd", "ディレクトリ変更", complete_cd},
    {"ls", "ファイル一覧表示", complete_generic_file},
    {"cat", "ファイル内容表示", complete_generic_file},
    {"grep", "テキスト検索", complete_generic_file},
    {"find", "ファイル検索", complete_generic_file},
    {"ssh", "リモート接続", complete_generic_file},
    {"sudo", "管理者権限実行", complete_generic_file},
    {"vim", "テキストエディタ", complete_generic_file},
    {"nano", "テキストエディタ", complete_generic_file},
    {"make", "ビルドツール", complete_generic_file},
    {"cargo", "Rustビルドツール", complete_generic_file},
    {"go", "Go言語ツール", complete_generic_file},
    {NULL, NULL, NULL}
};

/*
 * コマンドに対して高度な補完を提供
 */
mod_export int
sish_smart_completion(const char *cmd, const char *arg, char ***suggestions)
{
    /* 主要コマンドを検索 */
    for (int i = 0; famous_commands[i].cmd; i++) {
        if (strcmp(cmd, famous_commands[i].cmd) == 0) {
            fprintf(stderr, "%s💡 %s の補完候補：%s\n",
                    SISH_HINT_COLOR, cmd, SISH_COLOR_RESET);
            return famous_commands[i].completion_func(arg, suggestions);
        }
    }
    
    /* デフォルト：カレントディレクトリのファイル */
    return find_similar_files_in_dir(".", arg, suggestions);
}

/*
 * 補完候補を表示
 */
mod_export void
sish_show_completions(char **suggestions, int count)
{
    if (count == 0) {
        fprintf(stderr, "%sうーん、補完候補が見つからないよ...%s\n",
                SISH_CHAR_COLOR, SISH_COLOR_RESET);
        return;
    }
    
    fprintf(stderr, "%s✨ こんなのはどう？%s\n", 
            SISH_CHAR_COLOR, SISH_COLOR_RESET);
    
    for (int i = 0; i < count; i++) {
        fprintf(stderr, "  %s%d%s) %s%s%s\n",
                SISH_CMD_COLOR, i + 1, SISH_COLOR_RESET,
                SISH_HINT_COLOR, suggestions[i], SISH_COLOR_RESET);
    }
    
    fprintf(stderr, "%sTabキーでどんどん補完できるよ！%s\n",
            SISH_HINT_COLOR, SISH_COLOR_RESET);
}
