#!/bin/sh
# Builds NexaC from this folder:  sh build.sh [--installer] [compiler]
# The compiler is the argument, else $CXX, else clang++, else g++.
#
# Linux and macOS: NexaC, as `clang++ -std=c++17 -O2 NexaC.cpp -o NexaC`.
# Windows (Git Bash, MSYS2, or a Linux cross-compiler that targets Windows): a
# static NexaC.exe with the Nexa icon in it -- NexaC.rc, compiled with llvm-rc for an
# MSVC-target clang++ or with windres for a MinGW one. With neither tool it says so
# and builds NexaC.exe without the icon. From cmd.exe, use build.cmd.
#
# --installer: then the installer too, built by the NexaC just made from
# Installer/main.nxa. On Windows it gets the Nexa icon through --link (a resource
# object, so it stays as small as without; the programs NexaC builds get no icon of
# their own). Elsewhere programs keep no icon in the file.
set -e
cd "$(dirname "$0")"

installer=0
if [ "${1:-}" = "--installer" ]; then
    installer=1
    shift
fi

CXX=${1:-${CXX:-}}
if [ -z "$CXX" ]; then
    for c in clang++ g++; do
        if command -v "$c" >/dev/null 2>&1; then CXX=$c; break; fi
    done
fi
if [ -z "$CXX" ]; then
    echo "build.sh: no clang++ or g++ on PATH" >&2
    exit 1
fi

target=$("$CXX" -dumpmachine 2>/dev/null || echo unknown)
case "$target" in
    *mingw* | *windows* | *cygwin* | *msys*) ;;
    *)
        "$CXX" -std=c++17 -O2 NexaC.cpp -o NexaC
        echo "build.sh: built NexaC with $CXX"
        if [ "$installer" = 1 ]; then
            # Not ./Installer: that is the folder of its source.
            ./NexaC Installer/main.nxa -o Installer/Installer >/dev/null
            echo "build.sh: built Installer/Installer"
        fi
        exit 0
        ;;
esac

# The first of these that is installed: beside the compiler first, so a toolchain's
# own tool wins over another one on PATH.
first_tool() {
    dir=$(dirname "$(command -v "$CXX")")
    for t in "$@"; do
        if [ -x "$dir/$t" ] || [ -x "$dir/$t.exe" ]; then echo "$dir/$t"; return; fi
    done
    for t in "$@"; do
        if command -v "$t" >/dev/null 2>&1; then command -v "$t"; return; fi
    done
}

icon=""
case "$target" in
    *msvc*)
        flags="-std=c++17 -O2 -Wno-deprecated-declarations"
        rc=$(first_tool llvm-rc)
        if [ -n "$rc" ] && "$rc" -fo NexaC.res NexaC.rc; then icon=NexaC.res; fi
        ;;
    *)
        flags="-std=c++17 -O2 -static -static-libgcc -static-libstdc++"
        prefix=$(echo "$target" | sed 's/-windows-gnu$/-w64-mingw32/')
        rc=$(first_tool "$prefix-windres" x86_64-w64-mingw32-windres llvm-windres windres)
        if [ -n "$rc" ] && "$rc" NexaC.rc -O coff -o NexaC-icon.o; then icon=NexaC-icon.o; fi
        ;;
esac
if [ -z "$icon" ]; then
    echo "build.sh: no llvm-rc or windres found, so NexaC.exe gets no icon" >&2
fi

# shellcheck disable=SC2086
"$CXX" $flags NexaC.cpp $icon -o NexaC.exe -ladvapi32
echo "build.sh: built NexaC.exe with $CXX${icon:+, with the Nexa icon}"

if [ "$installer" = 1 ]; then
    # NexaC builds Windows programs with MinGW, so the installer's icon is a COFF
    # object from windres: NexaC's own when it was made that way, else a fresh one.
    obj=""
    if [ "$icon" = NexaC-icon.o ]; then
        obj=NexaC-icon.o
    else
        rc=$(first_tool x86_64-w64-mingw32-windres llvm-windres windres)
        if [ -n "$rc" ] && "$rc" NexaC.rc -O coff -o NexaC-icon.o; then obj=NexaC-icon.o; fi
    fi
    if [ -z "$obj" ]; then
        echo "build.sh: no windres found, so Installer.exe gets no icon" >&2
        ./NexaC.exe Installer/main.nxa -o Installer.exe >/dev/null
    else
        ./NexaC.exe Installer/main.nxa --link "$obj" -o Installer.exe >/dev/null
    fi
    echo "build.sh: built Installer.exe${obj:+, with the Nexa icon}"
fi
