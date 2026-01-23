#!/usr/bin/env bash
set -euo pipefail

echo "🌸 Sish セットアップ開始..."
echo ""

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ---- Preflight: check required tools (no sudo / no system install) ----
missing=()
# Accept any of these C/C++ compilers
compilers=(gcc clang cc c99 c89 tcc icc icx zig clang-cl cl xlc pgcc c++)
found_compiler=""
for c in "${compilers[@]}"; do
    if command -v "$c" >/dev/null 2>&1; then
        found_compiler="$c"
        break
    fi
done
if [[ -z "$found_compiler" ]]; then
    missing+=("C compiler (gcc/clang/cc/c99/c89/tcc/icc/icx/zig/clang-cl/cl/xlc/pgcc/c++)")
fi
for cmd in make autoconf autoheader pkg-config; do
    command -v "$cmd" >/dev/null 2>&1 || missing+=("$cmd")
done

if (( ${#missing[@]} > 0 )); then
    echo "❌ Missing required build tools: ${missing[*]}" >&2
    echo "Hint: On Debian/Ubuntu, try: sudo apt-get update && sudo apt-get install -y build-essential autoconf pkg-config libncurses-dev" >&2
    exit 1
fi

# ncursesw (wide-char) is required for zle, check for it
if ! pkg-config --exists ncursesw && ! pkg-config --exists ncurses; then
    echo "❌ ncursesw (libncursesw5-dev or libncurses-dev) not found" >&2
    echo "Hint: On Debian/Ubuntu, try: sudo apt-get install -y libncursesw5-dev or libncurses-dev" >&2
    exit 1
fi

# ---- Detect already-installed local zsh ----
# Check if zsh-5.9 is already built and installed locally
if [[ -x "zsh-5.9/install/bin/zsh" ]] && compgen -G "zsh-5.9/install/lib/zsh/*/zsh/zle.so" > /dev/null; then
    echo "✅ Sish is already set up!"
    echo ""
    echo "How to start:"
    echo "  ./sish"
    echo ""
    exit 0
fi

echo "📦 Building zsh..."
cd zsh-5.9

# If configure is missing, generate it (zsh standard procedure)
if [[ ! -x "./configure" ]]; then
    echo "⚙️  Generating configure..."
    if [[ -x "./Util/preconfig" ]]; then
        ./Util/preconfig || {
            echo "❌ Failed to generate configure."
            echo "Hint: autoconf/autoheader required (e.g. sudo apt install autoconf)"
            exit 1
        }
    else
        # Fallback: autoreconf -i
        if command -v autoreconf >/dev/null 2>&1; then
            autoreconf -i || {
                echo "❌ autoreconf failed." >&2
                exit 1
            }
        else
            echo "❌ Util/preconfig not found and autoreconf not available."
            exit 1
        fi
    fi
fi

# If configure hasn't been run or prefix is wrong, run it
if [[ ! -f "Makefile" ]] || ! grep -q "^prefix.*=.*$PWD/install" Makefile 2>/dev/null; then
    echo "⚙️  Running configure..."
    ./configure --prefix="$PWD/install"
fi

JOBS="${JOBS:-}"
if [[ -z "$JOBS" ]]; then
    if command -v nproc >/dev/null 2>&1; then
        JOBS=$(nproc)
    else
        JOBS=1
    fi
fi

echo "🔨 Building with make... (this may take a while)"
make -j"$JOBS" || {
    echo "❌ Build failed."
    exit 1
}

echo "📥 Installing locally..."
# 'make install' for only required parts (man/runhelp often fails in minimal envs)
make install.bin install.modules install.fns || {
    echo "❌ Local install failed."
    echo "Hint: For more details, run: (cd zsh-5.9 && make install.bin install.modules install.fns)"
    exit 1
}

cd ..

echo ""
echo "✅ Setup complete!"
echo ""
echo "How to start:"
echo "  ./sish"
echo ""
echo "Settings menu:"
echo "  ./sish"
echo "  Sish> sish-config"
echo ""
