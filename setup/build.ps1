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

# setup/build.ps1 — THE single way to build and test this project on Windows
# (Visual Studio / MSVC) (M33, DEC-0041). The Linux / macOS twin is setup/build.sh.
#
# Establishes the ONE canonical build tree at <repo>/build/ (single-build policy,
# CLAUDE.md "Build & test flow"): if the local SDL3 dev install is missing it is
# built ONCE from the vendored, zlib-licensed src/external/sdl3/ source into
# build/_sdl3_install (the reproducible M26 recipe, docs/m26-implementation-report.md §2
# — linking a zlib-licensed binary dependency; no SDL3 source is ever copied into src/),
# then the project is configured SDL3=ON (the superset: both executables + all tests),
# built, and optionally tested.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File setup/build.ps1              # configure + build
#   powershell -ExecutionPolicy Bypass -File setup/build.ps1 -RunTests    # ... + ctest
#   powershell -ExecutionPolicy Bypass -File setup/build.ps1 -Config Release
#   powershell -ExecutionPolicy Bypass -File setup/build.ps1 -RunTests -Slow  # ... + ZEXALL sweep
#
# Wiping build/ is always safe: this script restores everything (SDL3 rebuild ~minutes).

param(
    [string]$Config = "Debug",
    [switch]$RunTests,
    # -Slow additionally runs the ZEXALL/ZEXDOC full sweep (hours). Standing cadence
    # (memory: slow-test-cadence): RC checkpoints only, or after direct src/devices/cpu/
    # / src/core/ edits. Default is the fast subset every prior milestone used.
    [switch]$Slow
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

$sdlInstall = Join-Path $repo "build/_sdl3_install"
if (-not (Test-Path (Join-Path $sdlInstall "cmake/SDL3Config.cmake")) -and
    -not (Test-Path (Join-Path $sdlInstall "lib/cmake/SDL3/SDL3Config.cmake"))) {
    Write-Host "[bootstrap] SDL3 dev install missing -> building once from src/external/sdl3 (zlib license, M26 recipe)"
    cmake -S src/external/sdl3 -B build/_sdl3_src_build `
        -DCMAKE_INSTALL_PREFIX=build/_sdl3_install `
        -DSDL_SHARED=ON -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DSDL_EXAMPLES=OFF -DSDL_TESTS=OFF `
        -DSDL_INSTALL=ON
    if ($LASTEXITCODE -ne 0) { throw "[bootstrap] SDL3 configure failed" }
    cmake --build build/_sdl3_src_build --config $Config --target SDL3-shared SDL3-static
    if ($LASTEXITCODE -ne 0) { throw "[bootstrap] SDL3 build failed" }
    cmake --install build/_sdl3_src_build --config $Config
    if ($LASTEXITCODE -ne 0) { throw "[bootstrap] SDL3 install failed" }
} else {
    Write-Host "[bootstrap] SDL3 dev install present: $sdlInstall"
}

Write-Host "[bootstrap] configuring canonical build/ (SDL3=ON superset)"
cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=ON "-DCMAKE_PREFIX_PATH=$sdlInstall"
if ($LASTEXITCODE -ne 0) { throw "[bootstrap] project configure failed" }

Write-Host "[bootstrap] building ($Config)"
cmake --build build --config $Config
if ($LASTEXITCODE -ne 0) { throw "[bootstrap] project build failed" }

# SDL3.dll staging is owned by CMake itself (sony_msx_stage_sdl3_dll POST_BUILD
# per SDL3-linked target, root CMakeLists.txt) -- no script-side copy needed.

if ($RunTests) {
    if ($Slow) {
        Write-Host "[bootstrap] running FULL ctest incl. ZEXALL/ZEXDOC sweep ($Config) -- RC-gate cadence"
        ctest --test-dir build -C $Config --output-on-failure
    } else {
        Write-Host "[bootstrap] running fast-subset ctest ($Config) -- ZEXALL excluded per standing cadence"
        ctest --test-dir build -C $Config --output-on-failure -LE slow_cpu_sweep
    }
    if ($LASTEXITCODE -ne 0) { throw "[bootstrap] ctest reported failures" }
}

Write-Host "[bootstrap] done. Executables: build/$Config/sony_msx_headless.exe, build/$Config/sony_msx_sdl3.exe"
