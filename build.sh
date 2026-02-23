#!/bin/bash
#
# Sish Build Script
# Builds both the Sish shell (modified zsh) and Sish-Console
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ZSH_DIR="$SCRIPT_DIR/zsh-5.9"
CONSOLE_DIR="$SCRIPT_DIR/Sish-Console"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${BLUE}[*]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

# Check dependencies
check_dependencies() {
    print_status "Checking dependencies..."
    
    local missing_deps=()
    
    # Check for C compiler
    if ! command -v gcc &> /dev/null && ! command -v clang &> /dev/null; then
        missing_deps+=("gcc or clang")
    fi
    
    # Check for make
    if ! command -v make &> /dev/null; then
        missing_deps+=("make")
    fi
    
    # Check for autoconf
    if ! command -v autoconf &> /dev/null; then
        missing_deps+=("autoconf")
    fi
    
    # Check for Rust (for Sish-Console)
    if ! command -v cargo &> /dev/null; then
        missing_deps+=("cargo (Rust)")
    fi
    
    # Check for GTK4 development files
    if ! pkg-config --exists gtk4 2>/dev/null; then
        missing_deps+=("libgtk-4-dev")
    fi
    
    # Check for VTE4
    if ! pkg-config --exists vte-2.91-gtk4 2>/dev/null; then
        print_warning "VTE4 not found, Sish-Console may not build"
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing dependencies:"
        for dep in "${missing_deps[@]}"; do
            echo "  - $dep"
        done
        echo ""
        echo "On Ubuntu/Debian, install with:"
        echo "  sudo apt install build-essential autoconf libncurses-dev texinfo"
        echo "  sudo apt install libgtk-4-dev libvte-2.91-gtk4-dev"
        echo "  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh"
        exit 1
    fi
    
    print_success "All dependencies satisfied"
}

# Build Sish (modified zsh)
build_sish() {
    print_status "Building Sish shell..."
    
    cd "$ZSH_DIR"
    
    # Clean previous build if exists
    if [ -f Makefile ]; then
        make clean || true
    fi
    
    # Configure
    print_status "Running configure..."
    if [ ! -f configure ]; then
        autoconf
    fi
    
    ./configure --prefix=/usr/local \
                --enable-multibyte \
                --enable-pcre \
                --enable-cap \
                --with-tcsetpgrp
    
    # Build
    print_status "Compiling..."
    if ! make -j$(nproc) 2>&1 | tee /tmp/sish_build.log; then
        print_error "Build failed!"
        
        # Check for common errors
        if grep -q "conflicting types for 'boolcodes'" /tmp/sish_build.log; then
            print_warning "Detected ncurses compatibility issue"
            print_status "This is a known issue with ncurses 6.x on modern systems"
            print_status "The source code has been patched to handle this"
            print_status "Please try: rm -rf zsh-5.9 && git checkout zsh-5.9 && ./setup.sh"
        fi
        
        echo ""
        print_error "Last 50 lines of build log:"
        tail -n 50 /tmp/sish_build.log
        exit 1
    fi
    
    print_success "Sish shell built successfully"
    
    cd "$SCRIPT_DIR"
}

# Build Sish-Console
build_console() {
    print_status "Building Sish-Console..."
    
    cd "$CONSOLE_DIR"
    
    # Build with cargo
    cargo build --release
    
    print_success "Sish-Console built successfully"
    
    cd "$SCRIPT_DIR"
}

# Install Sish
install_sish() {
    print_status "Installing Sish..."
    
    cd "$ZSH_DIR"
    
    sudo make install
    
    # Create sish symlink
    sudo ln -sf /usr/local/bin/zsh /usr/local/bin/sish
    
    # Add to /etc/shells if not already there
    if ! grep -q "/usr/local/bin/sish" /etc/shells; then
        echo "/usr/local/bin/sish" | sudo tee -a /etc/shells
    fi
    
    print_success "Sish shell installed to /usr/local/bin/sish"
    
    cd "$SCRIPT_DIR"
}

# Install Sish-Console
install_console() {
    print_status "Installing Sish-Console..."
    
    sudo cp "$CONSOLE_DIR/target/release/sish-console" /usr/local/bin/
    
    # Create desktop entry
    cat > /tmp/sish-console.desktop << EOF
[Desktop Entry]
Name=Sish Console
Comment=A friendly terminal emulator
Exec=/usr/local/bin/sish-console
Icon=utilities-terminal
Terminal=false
Type=Application
Categories=System;TerminalEmulator;
Keywords=terminal;console;shell;sish;
EOF
    
    sudo mv /tmp/sish-console.desktop /usr/share/applications/
    
    print_success "Sish-Console installed"
}

# Main
main() {
    echo ""
    echo "╔═══════════════════════════════════════════════╗"
    echo "║                                               ║"
    echo "║   🐚 Sish - Sister Shell Build Script 🐚     ║"
    echo "║                                               ║"
    echo "╚═══════════════════════════════════════════════╝"
    echo ""
    
    case "${1:-all}" in
        deps)
            check_dependencies
            ;;
        sish)
            check_dependencies
            build_sish
            ;;
        console)
            check_dependencies
            build_console
            ;;
        install)
            install_sish
            install_console
            ;;
        install-sish)
            install_sish
            ;;
        install-console)
            install_console
            ;;
        all)
            check_dependencies
            build_sish
            build_console
            print_success "Build complete!"
            echo ""
            echo "To install, run: $0 install"
            ;;
        clean)
            print_status "Cleaning build files..."
            cd "$ZSH_DIR" && make clean || true
            cd "$CONSOLE_DIR" && cargo clean || true
            print_success "Clean complete"
            ;;
        *)
            echo "Usage: $0 [command]"
            echo ""
            echo "Commands:"
            echo "  all             Build both Sish and Sish-Console (default)"
            echo "  sish            Build only Sish shell"
            echo "  console         Build only Sish-Console"
            echo "  install         Install both Sish and Sish-Console"
            echo "  install-sish    Install only Sish shell"
            echo "  install-console Install only Sish-Console"
            echo "  deps            Check dependencies"
            echo "  clean           Clean build files"
            ;;
    esac
}

main "$@"
