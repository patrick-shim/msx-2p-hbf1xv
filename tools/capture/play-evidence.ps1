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

param(
    [string]$Emulator = "build/Debug/sony_msx_headless.exe",
    [string]$BiosDir = "bios",
    [string]$RomPath = "roms/game.rom",
    [string]$InputScript = "tools/input-scripts/play-evidence.script"
)

# ---------------------------------------------------------------------------
# M34-closure gameplay-evidence capture — THE RECORDED RECIPE
# (QA condition, docs/m34-qa-signoff.md §4; the M32 AC-6 supersession pattern).
#
# Regenerates the four committed gameplay frames deterministically:
#   * real Sony HB-F1XV BIOS cold boot from <BiosDir>;
#   * roms/game.rom loaded EXPLICITLY as mapper type KonamiSCC (auto-ID
#     resolves identically per M30; the explicit type keeps this recipe
#     self-contained and database-independent);
#   * driven through the REAL frame loop (--debug-session --frames, the
#     Sdl3App::run_one_frame() shape: step to each 59,736-cycle boundary,
#     then on_vsync_boundary());
#   * scripted input from tools/input-scripts/play-evidence.script
#     (HBF1XV-INPUT-SCRIPT v1): SPACE held ~15 frames at frames 600/1500/
#     2100/2700 (cycle stamps (F-1)x59,736, the committed cartridge-evidence-script
#     convention) — the presses that advance title -> weapon select ->
#     gameplay;
#   * frames 2600/2900/3000/3200/3600 dumped and converted to PNG via
#     tools/convert/frame-to-png.py.
#
# HISTORY / SUPERSESSION NOTE: the ORIGINAL m32-play-*.png frames were
# captured at v1.0.33 (M32) via a temporary --m32-evidence probe whose exact
# input-stamp mechanics were reverted WITHOUT a committed script (the recorded
# recipe was prose-level, "SPACE holds at 600/1500/2100/2700"); those bytes
# remain retrievable at git tag v1.0.34. This tool is the permanent, recorded
# replacement. Its output documents the CURRENT (M34) renderer, in which the
# four GAMEPLAY frames legitimately differ from the v1.0.34 bytes on two
# independent, QA-confirmed axes (docs/m34-qa-signoff.md escalations 1+2):
#   (1) rows 14-15 now correctly show the game's own BL=0 display-blank band
#       (the game drives R#1 bit6 low at display line 13 every gameplay frame —
#       the Defect-B fix rendering real hardware behavior); and
#   (2) on f3000/3200/3600, the moving-sprite rows (~99-117) differ because the
#       exact M32 input mechanics were unrecoverable — an M34-INDEPENDENT
#       recipe-reconstruction residue (proven: the divergence persists with the
#       BL gate removed). f2600 (weapon select, BL=1) is byte-stable.
#
# Determinism: a pure function of (BIOS bytes, ROM bytes, frame count, script
# stamps). Each frame is captured TWICE and asserted byte-identical.
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path([string]$Path, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path)) { throw "missing ${Label}: ${Path}" }
}

Require-Path $Emulator "emulator (build first: setup/build.ps1)"
Require-Path $BiosDir "BIOS directory"
Require-Path $RomPath "ROM (local dev asset)"
Require-Path $InputScript "input script"

$frames = @(2600, 2900, 3000, 3200, 3600)
foreach ($f in $frames) {
    $name = "m32-play-f$f.frame"
    $verify = "m32-play-f$f-verify.frame"
    $png = "debug/frames/m32-play-f$f.png"

    Write-Host "== frame $f : capture run 1/2 =="
    & $Emulator --debug-session $BiosDir 0 --stock --frames $f `
        --cart1 $RomPath --cart1-type KonamiSCC `
        --input-script $InputScript --dump-frame $name
    if ($LASTEXITCODE -ne 0) { throw "frame $f run 1 failed (exit $LASTEXITCODE)" }

    Write-Host "== frame $f : capture run 2/2 (determinism) =="
    & $Emulator --debug-session $BiosDir 0 --stock --frames $f `
        --cart1 $RomPath --cart1-type KonamiSCC `
        --input-script $InputScript --dump-frame $verify
    if ($LASTEXITCODE -ne 0) { throw "frame $f run 2 failed (exit $LASTEXITCODE)" }

    $h1 = (Get-FileHash -Algorithm SHA256 "debug/frames/$name").Hash
    $h2 = (Get-FileHash -Algorithm SHA256 "debug/frames/$verify").Hash
    if ($h1 -ne $h2) { throw "DETERMINISM VIOLATION at frame ${f}: two identical runs differ" }
    Remove-Item "debug/frames/$verify"

    python tools/convert/frame-to-png.py "debug/frames/$name" -o $png
    if ($LASTEXITCODE -ne 0) { throw "frame $f PNG conversion failed" }
    Write-Host "== frame $f : $png regenerated (sha256 $($h1.Substring(0,16))...) =="
}

Write-Host "== done: four m32-play gameplay PNGs regenerated from the recorded recipe =="
