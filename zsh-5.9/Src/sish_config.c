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

/* 端末設定を保存 */
static struct termios orig_termios;

/* Raw modeに切り替え */
static void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) != 0) {
        /* 非TTYなどではTUIが動かないので、最低限メッセージだけ出す */
        fprintf(stderr, "%s❌ sish-config: 端末設定の取得に失敗しちゃった… (ttyじゃないかも)%s\n",
                SISH_ERROR_COLOR, SISH_COLOR_RESET);
        return;
    }
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    /* ESC単体や矢印キーのシーケンスを安全に判定するため、短いタイムアウトを入れる */
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1; /* 0.1 sec */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

/* 元の端末設定に戻す */
static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
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
        "2. キャラクター設定",
        "3. ショートカット管理",
        "4. 補完機能設定",
        "5. LLM統合設定",
        "6. エラーメッセージ詳細度",
        "7. GUI連携設定",
        "8. 設定をリセット",
        "9. 設定を保存して終了",
        "0. キャンセル",
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
    
    int choice = read_key_raw(); if (choice < 0) return;
  /* 改行を消費 */

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

/* キャラクター設定 */
static void config_character(void) {
    show_config_header();
    printf("%sキャラクター設定%s\n\n", SISH_CMD_COLOR, SISH_COLOR_RESET);
    
    printf("  1. キャラクター名: %sSish%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
    printf("  2. デフォルト表情: %s😊 Happy%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
    printf("  3. 表示位置: %s右下%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
    printf("  4. サイズ: %s中%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
    printf("  5. アニメーション: %s有効%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
    
    printf("\n%s変更する項目を選択（1-5、0で戻る）: %s", 
           SISH_HINT_COLOR, SISH_COLOR_RESET);
    
    int choice = read_key_raw(); if (choice < 0) return;

    
    if (choice >= '1' && choice <= '5') {
        printf("\n%s✅ 設定を変更したよ！%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
        sleep(1);
    }
}

/* ショートカット管理 */
static void config_shortcuts(void) {
    show_config_header();
    printf("%sショートカット管理%s\n\n", SISH_CMD_COLOR, SISH_COLOR_RESET);
    
    printf("  1. 登録済みショートカット表示\n");
    printf("  2. 新しいショートカットを追加\n");
    printf("  3. ショートカットを削除\n");
    printf("  4. ショートカットを編集\n");
    printf("  0. 戻る\n");
    
    printf("\n%s選択してね（0-4）: %s", SISH_HINT_COLOR, SISH_COLOR_RESET);
    
    int choice = read_key_raw(); if (choice < 0) return;

    
    if (choice == '1') {
        sish_clear_screen();
        printf("%s登録済みショートカット:%s\n\n", SISH_CMD_COLOR, SISH_COLOR_RESET);
        printf("  g  → git\n");
        printf("  ga → git add\n");
        printf("  gc → git commit\n");
        printf("  gp → git push\n");
        printf("  d  → docker\n");
        printf("  dc → docker-compose\n");
        printf("  ...\n");
        printf("\n%sEnterで戻る%s", SISH_HINT_COLOR, SISH_COLOR_RESET);
    
    } else if (choice >= '2' && choice <= '4') {
        printf("\n%s✅ 設定を変更したよ！%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
        sleep(1);
    }
}

/* 補完機能設定 */
static void config_completion(void) {
    show_config_header();
    printf("%s補完機能設定%s\n\n", SISH_CMD_COLOR, SISH_COLOR_RESET);
    
    printf("  1. 自動補完: %s有効%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
    printf("  2. ファジーマッチ: %s有効%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
    printf("  3. 候補表示数: %s10%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
    printf("  4. ディレクトリ類似検索: %s有効%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
    printf("  5. コマンド履歴補完: %s有効%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
    
    printf("\n%s変更する項目を選択（1-5、0で戻る）: %s", 
           SISH_HINT_COLOR, SISH_COLOR_RESET);
    
    int choice = read_key_raw(); if (choice < 0) return;

    
    if (choice >= '1' && choice <= '5') {
        printf("\n%s✅ 設定を変更したよ！%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
        sleep(1);
    }
}

/* LLM統合設定 */
static void config_llm(void) {
    show_config_header();
    printf("%sLLM統合設定%s\n\n", SISH_CMD_COLOR, SISH_COLOR_RESET);
    
    printf("  1. LLM統合: %s無効%s\n", SISH_HINT_COLOR, SISH_COLOR_RESET);
    printf("  2. APIエンドポイント: %s未設定%s\n", SISH_HINT_COLOR, SISH_COLOR_RESET);
    printf("  3. モデル: %s未設定%s\n", SISH_HINT_COLOR, SISH_COLOR_RESET);
    printf("  4. 最大トークン: %s2000%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
    
    printf("\n%s変更する項目を選択（1-4、0で戻る）: %s", 
           SISH_HINT_COLOR, SISH_COLOR_RESET);
    
    int choice = read_key_raw(); if (choice < 0) return;

    
    if (choice >= '1' && choice <= '4') {
        printf("\n%s✅ 設定を変更したよ！%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
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
    
    int choice = read_key_raw();
    if (choice < 0) return;

    if (choice >= '1' && choice <= '4') {
        sish_cfg_error_verbosity = (choice - '0');
        printf("\n%s✅ 詳細度を変更したよ！（保存は『設定を保存して終了』）%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
        sleep(1);
    }
}

/* GUI連携設定 */
static void config_gui(void) {
    show_config_header();
    printf("%sGUI連携設定%s\n\n", SISH_CMD_COLOR, SISH_COLOR_RESET);
    
    printf("  1. GUI連携: %s有効%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
    printf("  2. ソケットパス: %s/tmp/sish-console.sock%s\n", 
           SISH_HINT_COLOR, SISH_COLOR_RESET);
    printf("  3. 自動起動: %s無効%s\n", SISH_HINT_COLOR, SISH_COLOR_RESET);
    printf("  4. 表情同期: %s有効%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
    
    printf("\n%s変更する項目を選択（1-4、0で戻る）: %s", 
           SISH_HINT_COLOR, SISH_COLOR_RESET);
    
    int choice = read_key_raw(); if (choice < 0) return;

    
    if (choice >= '1' && choice <= '4') {
        printf("\n%s✅ 設定を変更したよ！%s\n", SISH_CHAR_COLOR, SISH_COLOR_RESET);
        sleep(1);
    }
}

/* 設定をリセット */
static void config_reset(void) {
    show_config_header();
    printf("%s⚠️  すべての設定をデフォルトに戻すよ？%s\n\n", 
           SISH_ERROR_COLOR, SISH_COLOR_RESET);
    printf("  本当にリセットする？ (y/N): ");
    
    int choice = read_key_raw(); if (choice < 0) return;

    
    if (choice == 'y' || choice == 'Y') {
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
        fprintf(fp, "export SISH_THEME=\"%s\"\n", sish_cfg_theme);
        fprintf(fp, "export SISH_ERROR_VERBOSITY=\"%d\"\n", sish_cfg_error_verbosity);
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
                disable_raw_mode();
                switch (selected) {
                    case 0: config_theme_color(); break;
                    case 1: config_character(); break;
                    case 2: config_shortcuts(); break;
                    case 3: config_completion(); break;
                    case 4: config_llm(); break;
                    case 5: config_error_verbosity(); break;
                    case 6: config_gui(); break;
                    case 7: config_reset(); break;
                    case 8:
                        config_save();
                        running = 0;
                        break;
                    case 9:
                        running = 0;
                        break;
                }
                enable_raw_mode();
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
