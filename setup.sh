#!/usr/bin/env bash

# ===============================
# Sish Setup & Build Script
# ===============================
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

PROJECT_ROOT="$SCRIPT_DIR"
ZSH_DIR="$PROJECT_ROOT/zsh-5.9"

step() {
  echo -e "\033[1;36m==> $1\033[0m"
}
substep() {
  echo -e "  \033[1;34m- $1\033[0m"
}
success() {
  echo -e "\033[1;32m✔ $1\033[0m"
}
fail() {
  echo -e "\033[1;31m✖ $1\033[0m" >&2
}
info() {
  echo -e "\033[1;33m$1\033[0m"
}

# ---- Project Overview ----
step "Sish Project Structure Overview"
cat <<EOF
  - zsh-5.9/         : Sish core (Zsh-based, C)
  - Sish-Console/    : GUI (Rust/GTK4)
  - config/          : Config files, i18n
  - scripts, *.sh    : Build/convert/test scripts
  - Test/            : Test data
  - docs/            : Documentation
  - translate.py     : Translation/i18n helper
EOF
echo

step "Environment check (build tools, libraries)"
missing=()
compilers=(gcc clang cc c99 c89)
found_compiler=""
for c in "${compilers[@]}"; do
    if command -v "$c" >/dev/null 2>&1; then
        found_compiler="$c"
        substep "C compiler: $c"
        break
    fi
done
if [[ -z "$found_compiler" ]]; then
    fail "No C compiler found (gcc/clang/cc etc)"
    exit 2
fi

for cmd in make autoconf autoheader pkg-config; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        missing+=("$cmd")
    else
        substep "$cmd: OK"
    fi
done


if (( ${#missing[@]} > 0 )); then
        fail "Missing required build tools: ${missing[*]}"
        # Suggest apt-get install command for common cases
        apt_map=(
            [gcc]=build-essential
            [clang]=clang
            [make]=build-essential
            [autoconf]=autoconf
            [autoheader]=autoconf
            [pkg-config]=pkg-config
        )
        apt_list=()
        for m in "${missing[@]}"; do
            pkg=${apt_map[$m]:-$m}
            [[ " ${apt_list[*]} " =~ " $pkg " ]] || apt_list+=("$pkg")
        done
        if (( ${#apt_list[@]} > 0 )); then
            info "\n  To install missing tools on Ubuntu/Debian, run:"
            echo "    sudo apt-get update && sudo apt-get install -y ${apt_list[*]}"
        fi
        exit 3
fi

# Always ignore TERM, stdin, and TTY for build/install logic
export TERM=dumb
export NCURSES_NO_UTF8_ACS=1
export DEBIAN_FRONTEND=noninteractive
export LC_ALL=C
export LANG=C
export LANGUAGE=C

# Check for ncurses presence, but do not fail if only terminfo is missing

if pkg-config --exists ncursesw; then
    substep "ncursesw: OK"
elif pkg-config --exists ncurses; then
    substep "ncurses: OK"
else
    fail "ncurses library not found (libncursesw-dev etc)"
    info "\n  To install on Ubuntu/Debian, run:"
    echo "    sudo apt-get update && sudo apt-get install -y libncursesw5-dev"
    exit 4
fi

success "Environment check complete"
echo

# ---- Build zsh (Sish core) ----
step "Entering zsh-5.9 directory"
cd "$ZSH_DIR"

step "Pre-build: ensure configure script exists"
if [[ ! -x "./configure" ]]; then
    substep "Running ./Util/preconfig"
    if ! ./Util/preconfig </dev/null; then
        fail "preconfig failed: ./Util/preconfig"
        exit 10
    fi
else
    substep "configure: already exists"
fi

step "Running configure"
if [[ ! -f "Makefile" ]]; then
    if ! ./configure --prefix="$PWD/install" </dev/null; then
        fail "configure failed: ./configure"
        info "\n  See config.log for details: $ZSH_DIR/config.log"
        exit 11
    fi
else
    substep "Makefile: already exists"
fi

# Always use single-threaded make for maximum portability
step "Running make (single-threaded for reproducibility)"
if ! make -j1; then
    fail "make failed: build error"
    info "\n  See error output above."
    exit 12
fi

step "make install (bin/modules/functions)"
if ! make install.bin install.modules install.fns; then
    fail "make install failed"
    exit 13
fi

cd "$PROJECT_ROOT"
success "Build complete: ./zsh-5.9/install/bin/zsh (Sish core)"

step "Next steps (manual)"
cat <<EONEXT
  - ./sish         : Launch Sish core (TUI)
  - ./sish-config  : Settings menu (TUI)
  - Sish-Console/  : GUI (Rust/GTK4, build separately)
  - test_sish.sh   : Test script
  - translate.py   : Translation/i18n helper
  - docs/          : Documentation
EONEXT

exit 0
