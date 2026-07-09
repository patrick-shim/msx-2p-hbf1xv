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
    [string]$RomPath = "roms/metalgear.rom",
    [string]$InputScript = "tools/metalgear-evidence-input.script",
    [int]$Frames = 1100,
    [string]$FrameName = "m31-rc-metalgear-f1100.frame",
    [string]$PngOut = "debug/frames/m31-rc-metalgear-f1100.png"
)

# ---------------------------------------------------------------------------
# M32-closure Metal Gear gameplay-evidence capture — THE RECORDED RECIPE
# (QA condition, docs/m32-qa-signoff.md; docs/m32-implementation-report.md
# "Evidence supersession (M32 closure)").
#
# Regenerates `debug/frames/m31-rc-metalgear-f1100.png` deterministically:
#   * real Sony HB-F1XV BIOS cold boot from <BiosDir>;
#   * `roms/metalgear.rom` loaded EXPLICITLY as mapper type `Konami`
#     (auto-ID would resolve identically per M30, but the explicit type keeps
#     this recipe self-contained and database-independent);
#   * driven for exactly 1,100 frames through the REAL frame loop
#     (`--debug-session --frames`, the Sdl3App::run_one_frame() shape:
#     step_cpu_instruction() to each 59,736-cycle boundary, then
#     on_vsync_boundary() — VBlank interrupts delivered);
#   * scripted input from `tools/metalgear-evidence-input.script`
#     (HBF1XV-INPUT-SCRIPT v1): SPACE held during frames [300,315) and
#     [600,615) — cycle stamps 299/314/599/614 x 59,736 — the M29/M31
#     smoke-shape presses that start the game past the title;
#   * frame 1,100 (Snake gameplay: sprite among trucks, LIFE/CLASS HUD)
#     dumped and converted to PNG via tools/frame-to-png.py (deterministic
#     encoder — byte-identical PNG for identical pixels).
#
# HISTORY / SUPERSESSION NOTE: the ORIGINAL m31-rc-metalgear-f1100.png was
# captured at v1.0.32 (M31 RC smoke) via a temporary probe whose input
# script was never recorded — that recipe is unrecoverable, and the original
# bytes remain retrievable at git tag v1.0.32. This tool is the permanent,
# recorded replacement recipe; its output documents the CURRENT (M32,
# raster-accurate per-line) renderer, in which the frame legitimately
# differs from the v1.0.32 snapshot render inside a ~17-row moving-sprite
# band (the D9-class one-frame sprite-table lag in mid-frame-committed rows
# — root-caused in docs/m32-implementation-report.md §5, QA-confirmed as
# the intended raster-truth direction).
#
# Determinism: the run is a pure function of (BIOS bytes, ROM bytes, frame
# count, script cycle stamps). This tool runs the capture TWICE and asserts
# the two frame dumps are byte-identical before converting.
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path([string]$Path, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path)) { throw "missing ${Label}: ${Path}" }
}

Require-Path $Emulator "emulator (build first: cmake --build build --config Debug)"
Require-Path $BiosDir "BIOS directory"
Require-Path $RomPath "ROM (local dev asset)"
Require-Path $InputScript "input script"

$FramePath = "debug/frames/$FrameName"
$VerifyName = [System.IO.Path]::GetFileNameWithoutExtension($FrameName) + "-verify.frame"
$VerifyPath = "debug/frames/$VerifyName"

Write-Host "== capture run 1/2 =="
& $Emulator --debug-session $BiosDir 0 --frames $Frames `
    --cart1 $RomPath --cart1-type Konami `
    --input-script $InputScript --dump-frame $FrameName
if ($LASTEXITCODE -ne 0) { throw "capture run 1 failed (exit $LASTEXITCODE)" }

Write-Host "== capture run 2/2 (determinism check) =="
& $Emulator --debug-session $BiosDir 0 --frames $Frames `
    --cart1 $RomPath --cart1-type Konami `
    --input-script $InputScript --dump-frame $VerifyName
if ($LASTEXITCODE -ne 0) { throw "capture run 2 failed (exit $LASTEXITCODE)" }

$h1 = (Get-FileHash -Algorithm SHA256 $FramePath).Hash
$h2 = (Get-FileHash -Algorithm SHA256 $VerifyPath).Hash
if ($h1 -ne $h2) { throw "DETERMINISM VIOLATION: two identical runs produced different frames" }
Remove-Item $VerifyPath
Write-Host "two-run determinism: OK (sha256 $($h1.Substring(0,16))...)"

Write-Host "== converting to PNG (deterministic encoder) =="
python tools/frame-to-png.py $FramePath -o $PngOut
if ($LASTEXITCODE -ne 0) { throw "frame-to-png conversion failed" }

Write-Host "== done: $PngOut regenerated from the recorded recipe =="
