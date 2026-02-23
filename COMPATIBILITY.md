# Sish Compatibility Guide

## Platform Compatibility

Sish has been tested and confirmed to work on the following platforms:

### ✅ Tested & Working

- **Ubuntu**: 20.04, 22.04, 24.04 (ncurses 5.x and 6.x)
- **Debian**: 11 (Bullseye), 12 (Bookworm)
- **Fedora**: 38+
- **Arch Linux**: Latest rolling release
- **Docker**: ubuntu:24.04, debian:12, alpine (with build tools)

### Known Issues & Fixes

#### 1. ncurses 6.x Compatibility (Ubuntu 24.04+)

**Issue:**
```
termcap.c:45:14: error: conflicting types for 'boolcodes'; have 'char *[]'
```

**Root Cause:**
Modern ncurses (version 6.x) defines `boolcodes`, `numcodes`, and `strcodes` in `term.h`, which conflicts with zsh-5.9's internal definitions.

**Fix Applied:**
The `zsh-5.9/Src/Modules/termcap.c` file has been patched to detect and use ncurses definitions when available:

```c
/* Modern ncurses (Ubuntu 24.04+) already defines boolcodes in term.h */
#if !defined(HAVE_BOOLCODES) && !defined(boolcodes)
static char *boolcodes[] = {
    // ... fallback definition
};
#endif
```

**Manual Fix (if needed):**
If you encounter this error after pulling updates:
```bash
rm -rf zsh-5.9
git checkout zsh-5.9
./setup.sh
```

#### 2. Docker/Minimal Environments

**Issue:**
Missing build dependencies in minimal containers.

**Fix:**
Install all required packages before running setup:

```bash
# Ubuntu/Debian
apt-get update && apt-get install -y \
  build-essential autoconf pkg-config libncursesw5-dev zsh curl git

# Fedora/RHEL
dnf install -y gcc make autoconf ncurses-devel

# Arch Linux
pacman -S base-devel autoconf ncurses
```

#### 3. Read-only Variable Errors

**Issue:**
```
read-only variable: status
```

**Root Cause:**
zsh-5.9 defines `status` as a special read-only variable.

**Fix Applied:**
All instances of `local status=` have been renamed to `local exit_code=` in the sish script.

## Environment Variables

### Core Variables

- `SISH_LANG`: Language setting (`ja` or `en`)
- `SISH_TONE`: Personality mode (0-6)
- `SISH_THEME`: Color theme (`pink`, `blue`, etc.)
- `SISH_APT2PACMAN_ENABLE`: Enable apt→pacman conversion (0 or 1)

### Build Variables

These are automatically set by `setup.sh`:

- `TERM=dumb`: Prevents terminal issues during build
- `NCURSES_NO_UTF8_ACS=1`: Disables ACS characters
- `DEBIAN_FRONTEND=noninteractive`: Prevents interactive prompts
- `LC_ALL=C`, `LANG=C`: Ensures consistent locale during build

## Troubleshooting

### Build Fails with Compiler Errors

**Check ncurses version:**
```bash
pkg-config --modversion ncursesw
```

**Clean and rebuild:**
```bash
cd zsh-5.9
make clean
cd ..
./setup.sh
```

### "command not found" Errors

**Ensure PATH is correct:**
```bash
export PATH="$PWD/zsh-5.9/install/bin:$PATH"
./sish
```

### GUI Console Issues

The Rust-based GUI console (`Sish-Console`) requires additional dependencies:

```bash
# Ubuntu/Debian
sudo apt-get install -y \
  cargo libgtk-4-dev libvte-2.91-gtk4-dev \
  libgraphene-1.0-dev libcairo2-dev libpango1.0-dev
```

## Reporting Issues

When reporting build issues, please include:

1. OS and version: `cat /etc/os-release`
2. ncurses version: `pkg-config --modversion ncursesw`
3. Compiler version: `gcc --version` or `clang --version`
4. Build log: Last 100 lines of output
5. Error message from `/tmp/sish_make_*.log`

Example:
```bash
echo "OS: $(cat /etc/os-release | grep PRETTY_NAME)"
echo "ncurses: $(pkg-config --modversion ncursesw)"
echo "gcc: $(gcc --version | head -1)"
tail -100 /tmp/sish_make_*.log
```

## Contributing Compatibility Fixes

If you fix a compatibility issue on a new platform:

1. Document the issue in this file
2. Add detection in `setup.sh` if needed
3. Test on a clean environment (Docker recommended)
4. Submit a PR with detailed description

Thank you for helping make Sish more portable! 💖
