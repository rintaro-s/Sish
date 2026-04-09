# Sish Internationalization (i18n) Support
# Language configuration and message translation system

# ========================================
# Language Configuration
# ========================================

# Default language: Japanese (ja) or English (en)
: ${SISH_LANG:=ja}

# ========================================
# Message Translation Function
# ========================================

# Usage: sish_msg "message_key" [args...]
sish_msg() {
    local key="$1"
    shift
    
    case "$SISH_LANG" in
        en)
            _sish_msg_en "$key" "$@"
            ;;
        ja|*)
            _sish_msg_ja "$key" "$@"
            ;;
    esac
}

# ========================================
# Japanese Messages
# ========================================

_sish_msg_ja() {
    local key="$1"
    shift
    
    case "$key" in
        # Welcome messages
        "welcome")
            echo "Sishへようこそ！お兄ちゃん！"
            ;;
        "hint_config")
            echo "sish-config で設定メニューを開けるよ！"
            ;;
        
        # Command not found messages
        "cmd_not_found")
            local cmd="$1"
            local suggestion="$2"
            print -r -- "Sish：お兄ちゃん！\"$cmd\"って何？\"$suggestion\"の間違いじゃない？"
            ;;
        "cmd_not_found_no_suggestion")
            local cmd="$1"
            print -r -- "Sish：お兄ちゃん！\"$cmd\" は見つからないよ…"
            ;;
        "cmd_other_options")
            local options="$1"
            print -r -- "       ほかにも: $options"
            ;;
        
        # Directory correction messages
        "dir_not_found")
            local target="$1"
            local suggestion="$2"
            print -r -- "Sish：お兄ちゃん！「$target」は無かったよ… 「$suggestion」の間違いじゃない？"
            ;;
        "dir_not_found_maybe")
            local target="$1"
            local suggestion="$2"
            print -r -- "Sish：お兄ちゃん！「$target」は無かったよ… もしかして「$suggestion」のこと？"
            ;;
        "dir_similar_folders")
            local folders="$1"
            print -r -- "       それっぽいフォルダ: $folders"
            ;;
        "dir_hint")
            print -r -- "       ヒント: パスを確認してね！"
            ;;
        
        # y command messages
        "y_no_suggestion")
            print -r -- "Sish：お兄ちゃん、いま実行できる候補が無いよ…"
            ;;
        "y_empty_suggestion")
            print -r -- "Sish：お兄ちゃん、候補が空っぽだった…"
            ;;
        
        # void command messages
        "void_usage")
            print -r -- "Sish：お兄ちゃん、voidは消したいパスを指定してね！（例: void ./tmp）"
            ;;
        "void_root_warning")
            print -r -- "Sish：それはダメ！ '/' は消しちゃだめ！"
            ;;
        "void_file_not_found")
            local file="$1"
            local suggestion="$2"
            print -r -- "Sish：お兄ちゃん！\"$file\" は無かったよ… もしかして\"$suggestion\" のこと？"
            ;;
        
        # fiat command messages
        "fiat_using_sudo")
            print -r -- "Sish：お兄ちゃん、fiatはSishが管理者で動いてる時だけパスワードレスだよ…（sudoで実行するね）"
            ;;
        
        # lore command messages
        "lore_usage")
            print -r -- "Sish：loreは1つだけ指定してね！（例: lore myfile）"
            ;;
        
        # node command messages
        "node_usage")
            print -r -- "Sish：nodeは最低1つ指定してね！（例: node hoge）"
            ;;
        "node_not_found")
            print -r -- "Sish：お兄ちゃん、指定したフォルダが見つからないよ…"
            ;;
        "node_not_directory")
            print -r -- "Sish：お兄ちゃん、どっちもフォルダじゃなかった…"
            ;;
        
        # genesis command messages
        "genesis_usage")
            print -r -- "Sish：genesisはリポジトリ作成先を指定してね！（例: genesis myproject）"
            ;;
        
        # oracle command messages
        "oracle_usage")
            print -r -- "Sish：oracleで実行権をつけたいパスを指定してね！（例: oracle script.sh）"
            ;;
        
        # extract command messages
        "extract_unsupported")
            local file="$1"
            print -r -- "Sish：'$file' は解凍できないよ…"
            ;;
        "extract_invalid")
            local file="$1"
            print -r -- "Sish：'$file' は有効なファイルじゃないよ…"
            ;;
        
        # Startup script messages
        "startup_not_found")
            local path="$1"
            echo "Sish: 実行ファイルが見つからないよ: $path"
            ;;
        "startup_hint")
            local dir="$1"
            echo "ヒント: $dir で make を実行してね（または SISH_PATH=/path/to/zsh を設定）"
            ;;
        
        *)
            echo "Unknown message key: $key" >&2
            ;;
    esac
}

# ========================================
# English Messages
# ========================================

_sish_msg_en() {
    local key="$1"
    shift
    
    case "$key" in
        # Welcome messages
        "welcome")
            echo "Welcome to Sish, Onii-chan!"
            ;;
        "hint_config")
            echo "Type sish-config to open the settings menu!"
            ;;
        
        # Command not found messages
        "cmd_not_found")
            local cmd="$1"
            local suggestion="$2"
            print -r -- "Sish: Onii-chan! \"$cmd\" isn't a command... Did you mean \"$suggestion\"?"
            ;;
        "cmd_not_found_no_suggestion")
            local cmd="$1"
            print -r -- "Sish: Onii-chan! \"$cmd\" was not found..."
            ;;
        "cmd_other_options")
            local options="$1"
            print -r -- "       Other options: $options"
            ;;
        
        # Directory correction messages
        "dir_not_found")
            local target="$1"
            local suggestion="$2"
            print -r -- "Sish: Onii-chan! \"$target\" doesn't exist... Did you mean \"$suggestion\"?"
            ;;
        "dir_not_found_maybe")
            local target="$1"
            local suggestion="$2"
            print -r -- "Sish: Onii-chan! \"$target\" doesn't exist... Maybe you meant \"$suggestion\"?"
            ;;
        "dir_similar_folders")
            local folders="$1"
            print -r -- "       Similar folders: $folders"
            ;;
        "dir_hint")
            print -r -- "       Hint: Please check the path!"
            ;;
        
        # y command messages
        "y_no_suggestion")
            print -r -- "Sish: Onii-chan, there's no suggestion to execute right now..."
            ;;
        "y_empty_suggestion")
            print -r -- "Sish: Onii-chan, the suggestion was empty..."
            ;;
        
        # void command messages
        "void_usage")
            print -r -- "Sish: Onii-chan, void requires a path to delete! (Example: void ./tmp)"
            ;;
        "void_root_warning")
            print -r -- "Sish: No way! You can't delete '/'!"
            ;;
        "void_file_not_found")
            local file="$1"
            local suggestion="$2"
            print -r -- "Sish: Onii-chan! \"$file\" doesn't exist... Maybe you meant \"$suggestion\"?"
            ;;
        
        # fiat command messages
        "fiat_using_sudo")
            print -r -- "Sish: Onii-chan, fiat only runs without password when Sish has admin rights... (Using sudo)"
            ;;
        
        # lore command messages
        "lore_usage")
            print -r -- "Sish: lore requires exactly one file! (Example: lore myfile)"
            ;;
        
        # node command messages
        "node_usage")
            print -r -- "Sish: node requires at least one argument! (Example: node hoge)"
            ;;
        "node_not_found")
            print -r -- "Sish: Onii-chan, I couldn't find the specified folder..."
            ;;
        "node_not_directory")
            print -r -- "Sish: Onii-chan, neither of them is a folder..."
            ;;
        
        # genesis command messages
        "genesis_usage")
            print -r -- "Sish: genesis requires a repository path! (Example: genesis myproject)"
            ;;
        
        # oracle command messages
        "oracle_usage")
            print -r -- "Sish: oracle requires paths to make executable! (Example: oracle script.sh)"
            ;;
        
        # extract command messages
        "extract_unsupported")
            local file="$1"
            print -r -- "Sish: Can't extract '$file'..."
            ;;
        "extract_invalid")
            local file="$1"
            print -r -- "Sish: '$file' isn't a valid file..."
            ;;
        
        # Startup script messages
        "startup_not_found")
            local path="$1"
            echo "Sish: Executable not found: $path"
            ;;
        "startup_hint")
            local dir="$1"
            echo "Hint: Run make in $dir (or set SISH_PATH=/path/to/zsh)"
            ;;
        
        *)
            echo "Unknown message key: $key" >&2
            ;;
    esac
}

# ========================================
# Prompt Configuration
# ========================================

sish_setup_prompt() {
    if [[ "$SISH_LANG" == "en" ]]; then
        PROMPT='%F{213}Sish%f:%F{111}%~%f %# '
    else
        PROMPT='%F{213}Sish%f:%F{111}%~%f %# '
    fi
}
