#!/usr/bin/env bash
# ============================================================================
#  Sony HB-F1XV MSX2+ Emulator
#  Copyright (c) 2026 Patrick Shim <patrick.shim@live.co.kr>
#
#  LEGAL NOTICE - Personal reference only.
#  This source code is made available solely for personal, non-commercial
#  reference and educational study. Commercial use, sale, or redistribution
#  for profit is not permitted without the author's written consent.
#  Provided "AS IS", without warranty of any kind.
#  Proprietary BIOS/ROM/disk assets remain the property of their respective
#  rights holders and are NOT licensed by this notice.
# ============================================================================
#
# setup/build.sh -- the Linux / macOS twin of setup/build.ps1: the single way to
# build and test this project on a single-config generator (Ninja or Unix
# Makefiles) -- macOS (AppleClang) and Linux, including Raspberry Pi (GCC aarch64).
# One codebase, two toolchains, ONE build tree at <repo>/build/ (M33, DEC-0041).
#
# Unlike the Windows/Visual-Studio path (multi-config: build/Debug/*.exe), a
# single-config generator bakes the config in at CONFIGURE time (CMAKE_BUILD_TYPE)
# and takes NO --config / -C at build/test time; the executables land directly in
# build/ (sony_msx_headless, sony_msx_sdl3, msx-diskutil -- no .exe suffix).
#
# SDL3: by default this uses the SYSTEM SDL3 dev package, located by
# find_package(SDL3 CONFIG REQUIRED) with NO CMAKE_PREFIX_PATH (Homebrew on macOS:
# `brew install sdl3`; Debian/Raspberry Pi OS: `sudo apt install libsdl3-dev`, or
# a source build installed to a standard prefix). Pass --vendored-sdl3 to build
# SDL3 ONCE from the bundled, zlib-licensed src/external/sdl3/ into
# build/_sdl3_install instead (the same recipe the Windows bootstrap uses; no SDL3
# source is ever copied into src/).
#
# Usage:
#   setup/build.sh                       # configure + build (Debug, system SDL3)
#   setup/build.sh --run-tests           # ... + fast-subset ctest (no ZEXALL)
#   setup/build.sh --run-tests --slow    # ... + FULL ctest incl. ZEXALL/ZEXDOC
#   setup/build.sh --config Release      # Release build
#   setup/build.sh --vendored-sdl3       # build SDL3 from src/external/sdl3 first
#
# Wiping build/ is always safe: this script restores everything.

set -euo pipefail

CONFIG="Debug"
RUN_TESTS=0
SLOW=0
VENDORED_SDL3=0

while [ $# -gt 0 ]; do
    case "$1" in
        --config) CONFIG="${2:?--config needs a value}"; shift 2 ;;
        --run-tests|--runtests) RUN_TESTS=1; shift ;;
        --slow) SLOW=1; shift ;;
        --vendored-sdl3) VENDORED_SDL3=1; shift ;;
        -h|--help)
            sed -n '15,40p' "$0"; exit 0 ;;
        *) echo "[bootstrap] unknown argument: $1" >&2; exit 2 ;;
    esac
done

# Repo root = parent of this script's dir (works regardless of CWD).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO"

# Prefer Ninja when present (faster); otherwise fall back to the platform default
# generator (Unix Makefiles). BOTH are single-config -- the build/ layout and the
# "no --config at build/test time" rule are identical either way.
GEN_ARGS=()
if command -v ninja >/dev/null 2>&1; then
    GEN_ARGS=(-G Ninja)
    echo "[bootstrap] generator: Ninja"
else
    echo "[bootstrap] generator: CMake default (Ninja not found; Unix Makefiles)"
fi

PREFIX_ARGS=()
if [ "$VENDORED_SDL3" -eq 1 ]; then
    SDL_INSTALL="$REPO/build/_sdl3_install"
    if [ ! -f "$SDL_INSTALL/lib/cmake/SDL3/SDL3Config.cmake" ] && \
       [ ! -f "$SDL_INSTALL/cmake/SDL3Config.cmake" ]; then
        echo "[bootstrap] SDL3 dev install missing -> building once from src/external/sdl3 (zlib license)"
        cmake -S src/external/sdl3 -B build/_sdl3_src_build \
            -DCMAKE_INSTALL_PREFIX="$SDL_INSTALL" \
            -DCMAKE_BUILD_TYPE="$CONFIG" \
            -DSDL_SHARED=ON -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DSDL_EXAMPLES=OFF -DSDL_TESTS=OFF \
            -DSDL_INSTALL=ON
        cmake --build build/_sdl3_src_build
        cmake --install build/_sdl3_src_build
    else
        echo "[bootstrap] vendored SDL3 dev install present: $SDL_INSTALL"
    fi
    PREFIX_ARGS=("-DCMAKE_PREFIX_PATH=$SDL_INSTALL")
else
    echo "[bootstrap] using SYSTEM SDL3 (find_package, no CMAKE_PREFIX_PATH)"
fi

echo "[bootstrap] configuring canonical build/ (SDL3=ON superset, CMAKE_BUILD_TYPE=$CONFIG)"
# NOTE: the ${arr[@]+"${arr[@]}"} guard is REQUIRED, not decoration -- macOS ships
# bash 3.2, where `set -u` treats "${empty_array[@]}" as an unbound variable and
# aborts. Both arrays are legitimately empty on common paths (no Ninja / system SDL3).
cmake -S . -B build \
    ${GEN_ARGS[@]+"${GEN_ARGS[@]}"} \
    -DCMAKE_BUILD_TYPE="$CONFIG" -DSONY_MSX_ENABLE_SDL3=ON \
    ${PREFIX_ARGS[@]+"${PREFIX_ARGS[@]}"}

echo "[bootstrap] building"
cmake --build build

if [ "$RUN_TESTS" -eq 1 ]; then
    if [ "$SLOW" -eq 1 ]; then
        echo "[bootstrap] running FULL ctest incl. ZEXALL/ZEXDOC sweep -- RC-gate cadence"
        ctest --test-dir build --output-on-failure
    else
        echo "[bootstrap] running fast-subset ctest -- ZEXALL excluded per standing cadence"
        ctest --test-dir build --output-on-failure -LE slow_cpu_sweep
    fi
fi

echo "[bootstrap] done. Executables: build/sony_msx_headless, build/sony_msx_sdl3, build/msx-diskutil"
