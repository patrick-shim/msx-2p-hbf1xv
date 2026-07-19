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

# tools/openmsx/capture-frames.ps1 — M51 (DEC-0078) T0.4 openMSX discriminator leg
# (docs/m51-planner-package.md §3 T0.4, A-M51-2).
#
# Runs WSL openMSX (-machine Sony_HB-F1XV, at its DEFAULT limitsprites setting
# — the authentic-multiplex arbiter must model real per-line sprite limits)
# with frame-indexed keymatrix inputs equivalent to our input script, then
# captures a `screenshot -raw` PER FRAME over [FirstFrame, FirstFrame+Count).
# The captures feed the IDENTICAL tools/analyze/sprite-oracle.py (--ox 32 --oy 14
# for the 320x240 raw border canvas around a 256x212 active area).
#
# openMSX's transition rate over the same scene is the REFERENCE RATE that
# separates authentic sprite-multiplex flicker (present in openMSX too) from
# our pathological absence (only in ours).
#
# Input model: -KeyFrames "600,1500,2100,2700" -> SPACE down at each frame,
# up 15 frames later (the exact frame stamps of
# tools/input-scripts/play-evidence.script: T=(F-1)*59736, hold 15 frames).

param(
    [string]$Rom = "",                       # cart (carta) path; empty = disk boot
    [string]$Disk = "",                      # diska path (disk boot)
    [string]$KeyFrames = "600,1500,2100,2700",
    [int]$HoldFrames = 15,
    [string]$Key = "space",                  # space | joybutton (row8 bit0 only for now)
    [int]$FirstFrame = 2900,
    [int]$Count = 40,
    [int]$SlowSpeed = 10,                    # emulation speed % during the window
    [string]$OutDir = "debug/m51/capture-omx"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($Rom -eq "" -and $Disk -eq "") { throw "one of -Rom / -Disk is required" }
if ($Rom -ne "" -and -not (Test-Path $Rom)) { throw "missing ROM: $Rom" }
if ($Disk -ne "" -and -not (Test-Path $Disk)) { throw "missing disk: $Disk" }

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

function To-WslPath([string]$winPath) {
    $abs = (Resolve-Path $winPath).Path
    return (wsl -e wslpath -a "$abs").Trim()
}

$outWsl = To-WslPath $OutDir
$lastFrame = $FirstFrame + $Count - 1

# Frame-indexed Tcl driver (tools/openmsx/blanking-ab.ps1 precedent):
# count frames via `after frame`, inject keymatrix edges at the stamp frames,
# screenshot -raw each frame in the capture window, exit after the last.
$downs = @()
foreach ($kf in ($KeyFrames -split ",")) {
    $kf = $kf.Trim()
    if ($kf -eq "") { continue }
    $downs += "    if {`$FRAME == $kf} { keymatrixdown 8 0x01 }"
    $downs += "    if {`$FRAME == $([int]$kf + $HoldFrames)} { keymatrixup 8 0x01 }"
}
$downsText = $downs -join "`n"

# CADENCE CAVEAT (measured on openMSX 19.1/WSL): `screenshot -raw` returns the
# most recent RENDERED frame, and openMSX decouples rendering from emulation --
# at full speed consecutive per-`after frame` screenshots came back
# byte-identical for 3-4 emulated frames then jumped (render dropping). To make
# the per-frame capture TRUE per-emulated-frame, emulation is slowed to
# SlowSpeed% shortly before the window so the host renderer covers every
# emulated frame (speed alters no emulated state -- inputs are frame-indexed).
$slowAt = [Math]::Max(1, $FirstFrame - 60)
$tcl = @"
set FRAME 0
proc on_frame {} {
    global FRAME
    incr FRAME
$downsText
    if {`$FRAME == $slowAt} { set speed $SlowSpeed }
    if {`$FRAME >= $FirstFrame && `$FRAME <= $lastFrame} {
        screenshot -raw [format "$outWsl/omx_f%05d.png" `$FRAME]
    }
    if {`$FRAME > $lastFrame} { exit }
    after frame on_frame
}
after frame on_frame
"@

$tclPath = Join-Path $OutDir "m51-omx-driver.tcl"
Set-Content -Path $tclPath -Value $tcl -Encoding ascii
$tclWsl = To-WslPath $tclPath

if ($Rom -ne "") {
    $guestArg = "-carta `"$(To-WslPath $Rom)`""
} else {
    $guestArg = "-diska `"$(To-WslPath $Disk)`""
}

Write-Host "== m51-openmsx: Sony_HB-F1XV $guestArg frames [$FirstFrame..$lastFrame] -> $OutDir =="
$prevEap = $ErrorActionPreference
$ErrorActionPreference = "Continue"
cmd /c "wsl.exe -- timeout -k 5 600 openmsx -machine Sony_HB-F1XV $guestArg -script `"$tclWsl`" 2>&1" |
    Select-String -Pattern "error|Error" | ForEach-Object { Write-Host "  omx: $_" }
$ErrorActionPreference = $prevEap

$got = (Get-ChildItem -Path $OutDir -Filter "omx_f*.png" | Measure-Object).Count
Write-Host "== m51-openmsx done: $got frame screenshots under $OutDir =="
if ($got -lt $Count) { Write-Host "WARNING: expected $Count screenshots, got $got" }
