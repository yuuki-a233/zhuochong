#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/.."
# MINGW_BIN may point to an extracted Debian MinGW toolchain or a standard installation.
mingw_bin="${MINGW_BIN:-/usr/bin}"
compiler="$mingw_bin/x86_64-w64-mingw32-g++-win32"
preprocessor="$mingw_bin/x86_64-w64-mingw32-gcc-win32"
if [ ! -x "$compiler" ]; then compiler="$mingw_bin/x86_64-w64-mingw32-g++"; preprocessor="$mingw_bin/x86_64-w64-mingw32-gcc"; fi
mkdir -p build
"$mingw_bin/x86_64-w64-mingw32-windres" --preprocessor="$preprocessor" --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED -I. resources.rc build/resources.o
"$compiler" -std=c++17 -O2 -Wall -Wextra -municode -mwindows -static -static-libgcc -static-libstdc++ src/main.cpp build/resources.o -lgdiplus -lole32 -lshell32 -lgdi32 -luser32 -ladvapi32 -luuid -o build/YuukiDesktopPet.exe
"$mingw_bin/x86_64-w64-mingw32-strip" build/YuukiDesktopPet.exe
