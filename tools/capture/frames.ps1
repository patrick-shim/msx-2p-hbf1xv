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

# tools/capture/frames.ps1 — M51 (DEC-0078) Task 0 deterministic per-frame
# capture wrapper (docs/m51-planner-package.md §3 T0.1).
#
# Captures N consecutive gameplay frames [FirstFrame, FirstFrame+Count) of a
# title as decoded frame dumps + PNGs, one deterministic cold-boot run per
# frame index (the established tools/capture/play-evidence.ps1 recipe shape:
# determinism makes runs prefix-identical, so per-frame runs at increasing
# --frames yield a TRUE consecutive frame sequence).
#
# Era-safe CLI (works IDENTICALLY on tags v1.1.4/v1.1.5/v1.1.6/v1.2.0 for the
# Task 1 bisect): --stock (M46/v1.1.2+: RAM 64, accurate FDC, no FM-PAC) and
# the --cart1/--cart1-type spelling (M19+, still a silent alias on main).
#
# Determinism gate (T0.1): the FIRST and LAST frame of the window are captured
# twice and byte-hash-asserted.
#
# Usage examples:
#   powershell -File tools/capture/frames.ps1 -Rom "games/roms/<title>/game.rom" `
#       -InputScript tools/input-scripts/play-evidence.script `
#       -FirstFrame 2900 -Count 40 -OutDir debug/m51/<title>
#   powershell -File tools/capture/frames.ps1 -Disks "games/disks/<title>/game-d1.dsk" `
#       -InputScript tools/input-scripts/split-screen-title.script `
#       -FirstFrame 3000 -Count 30 -OutDir debug/m51/<title>

param(
    [string]$Emulator = "build/Debug/sony_msx_headless.exe",
    [string]$BiosDir = "bios",
    [string]$Rom = "",                    # cartridge path (empty = disk boot)
    [string]$RomType = "KonamiSCC",
    [string[]]$Disks = @(),               # repeatable --disk list (disk boot)
    [string]$InputScript = "",
    [int]$FirstFrame = 2900,
    [int]$Count = 30,
    [int]$Stride = 1,
    [string]$OutDir = "debug/m51/capture",
    [switch]$SkipDeterminismCheck,        # bisect legs: first/last dual-run off for speed
    # Per-frame F12-style debug snapshot (vdp.txt regs + vram.txt) alongside each
    # frame dump -- feeds tools/analyze/sprite-attribute-census.py (the section 1.6 prong-1 SAT
    # census + the SAT-anchored oracle boxes). One snapshot per run at exactly
    # the dumped frame.
    [switch]$Snapshot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path([string]$Path, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path)) { throw "missing ${Label}: ${Path}" }
}

Require-Path $Emulator "emulator (build first: setup/build.ps1)"
Require-Path $BiosDir "BIOS directory"
if ($Rom -ne "") { Require-Path $Rom "ROM (local dev asset)" }
foreach ($d in $Disks) { Require-Path $d "disk image" }
if ($InputScript -ne "") { Require-Path $InputScript "input script" }

New-Item -ItemType Directory -Path (Join-Path $OutDir "frames") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $OutDir "png") -Force | Out-Null

function Invoke-Capture([int]$Frame, [string]$Name) {
    $emuArgs = @("--debug-session", $BiosDir, "0", "--stock", "--frames", "$Frame",
                 "--debug-root", $OutDir, "--dump-frame", $Name)
    if ($Snapshot) { $emuArgs += @("--snapshot", $OutDir, "--snapshot-frame", "$Frame") }
    if ($Rom -ne "") {
        $emuArgs += @("--cart1", $Rom, "--cart1-type", $RomType)
    }
    foreach ($d in $Disks) { $emuArgs += @("--disk", $d) }
    if ($InputScript -ne "") { $emuArgs += @("--input-script", $InputScript) }
    # Native-command stderr is informational (banner/progress); with
    # $ErrorActionPreference=Stop a 2>&1 merge would surface it as a fake
    # NativeCommandError, so discard both streams under Continue and gate on
    # the exit code alone.
    $eap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    & $Emulator @emuArgs 2>$null | Out-Null
    $ErrorActionPreference = $eap
    if ($LASTEXITCODE -ne 0) { throw "capture at frame $Frame failed (exit $LASTEXITCODE)" }
}

$lastFrame = $FirstFrame + ($Count - 1) * $Stride
$frameList = @()
for ($f = $FirstFrame; $f -le $lastFrame; $f += $Stride) { $frameList += $f }

Write-Host "== m51-capture: $($frameList.Count) frames [$FirstFrame..$lastFrame stride $Stride] -> $OutDir =="

foreach ($f in $frameList) {
    $name = "f{0:D5}.frame" -f $f
    Invoke-Capture $f $name
    $png = Join-Path $OutDir ("png/f{0:D5}.png" -f $f)
    $eap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    python tools/convert/frame-to-png.py (Join-Path $OutDir "frames/$name") -o $png 2>$null | Out-Null
    $ErrorActionPreference = $eap
    if ($LASTEXITCODE -ne 0) { throw "frame $f PNG conversion failed" }
    Write-Host "  frame $f captured"
}

if (-not $SkipDeterminismCheck) {
    foreach ($f in @($FirstFrame, $lastFrame)) {
        $name = "f{0:D5}.frame" -f $f
        $verify = "f{0:D5}-verify.frame" -f $f
        Invoke-Capture $f $verify
        $h1 = (Get-FileHash -Algorithm SHA256 (Join-Path $OutDir "frames/$name")).Hash
        $h2 = (Get-FileHash -Algorithm SHA256 (Join-Path $OutDir "frames/$verify")).Hash
        if ($h1 -ne $h2) { throw "DETERMINISM VIOLATION at frame ${f}: two identical runs differ" }
        Remove-Item (Join-Path $OutDir "frames/$verify")
        Write-Host "  determinism: frame $f dual-run byte-identical (sha256 $($h1.Substring(0,16))...)"
    }
}

Write-Host "== m51-capture done: PNGs under $OutDir/png =="
