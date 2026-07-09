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
    [string]$RegressionTest = "build/tests/Debug/hbf1xv_m34_aleste_ultrasonic_regression_system_test.exe",
    [string]$AlesteRom = "roms/aleste.rom",
    [string]$WorkDir = "build/m34-ab",
    [int]$RecordStartSeconds = 33,
    [int]$RecordStopSeconds = 46
)

# ---------------------------------------------------------------------------
# M34 openMSX audio A/B harness -- the Aleste 2 ultrasonic-silence property
# (DEC-0043 Defect A; docs/m34-planner-package.md section 2.7).
#
# STRUCTURAL A/B (silence-property parity, never sample-level -- the two
# pipelines have different resamplers/gains by design):
#
#   Side A (ours): the committed regression test
#   tests/system/hbf1xv_m34_aleste_ultrasonic_regression_system_test.cpp --
#   the recorded m32 smoke recipe (KonamiSCC explicit, SPACE holds at frames
#   600/1500/2100, real frame loop), section-2.6.4 ZCR burst metric over
#   frames 2150-2350. Its oracle: ZERO burst blocks on both stereo sides
#   (the identical metric measured 17/35 burst blocks on the PRE-M34
#   point-sampling build -- the recorded discrimination baseline).
#
#   Side B (real openMSX on WSL, /usr/bin/openmsx, Sony_HB-F1XV,
#   power-cycle-fresh session per the DEC-0028 standing rule): the same
#   Aleste 2 ROM with SPACE injected via the debugger `keymatrixdown`/`keymatrixup` surface
#   (MSX matrix row 8 bit 0; the m32 Tcl-injection precedent,
#   tools/openmsx-m32-split-ab.ps1) at emulated-time ~10s/25s/35s (~frames
#   600/1500/2100), audio recorded headlessly via `record start -audioonly`
#   (A-M34-3, VERIFIED on openMSX 19.1/WSL: produces a mono 16-bit 44.1 kHz
#   WAV with no host audio device) across the title -> weapon-select ->
#   game-start transition window.
#
#   Comparator: tools/m34-audio-ab-analyze.py applies the IDENTICAL
#   section-2.6.4 metric to the reference WAV. PARITY = the reference shows
#   ZERO burst blocks (real-lineage emulation renders the tone-period-0
#   ~112 kHz idiom silent -- openMSX generates chips at native rate and
#   band-limit-resamples, references/openmsx-21.0/src/sound/AY8910.cc:38-39,
#   :482 + ResampledSoundDevice.hh:23,29,46-48; behaviour reference only,
#   never copied) AND our side's regression oracle is green.
#
# Results are recorded in docs/m34-audio-ab.md.
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path([string]$Path, [string]$Label) {
    if (-not (Test-Path $Path)) { throw "missing ${Label}: ${Path}" }
}

Require-Path $RegressionTest "regression test (build first)"
Require-Path $AlesteRom "Aleste 2 ROM (local dev asset)"
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null

$RepoRoot = (Get-Location).Path
$WslRoot = "/mnt/" + ($RepoRoot.Substring(0, 1).ToLower()) + ($RepoRoot.Substring(2) -replace "\\", "/")
$WslWork = "$WslRoot/$WorkDir"

# --- Side A: our committed regression oracle (also runs inside ctest). ---
Write-Host "== side A: the M34 Aleste ultrasonic regression oracle (ours) =="
& $RegressionTest 2>&1 | Tee-Object -FilePath "$WorkDir/side_a_regression.log" |
    Select-String "summary|BURST|passed|SKIP" | ForEach-Object { Write-Host "  [side A] $_" }
$SideAOk = ($LASTEXITCODE -eq 0)
if (-not $SideAOk) { Write-Host "  side A regression test FAILED" }

# --- Side B: openMSX/WSL record sound over the same transition. ---
$tcl = @"
set throttle off
proc press_space {} {
    keymatrixdown 8 0x01
    after time 0.25 { keymatrixup 8 0x01 }
}
after time 10 { press_space }
after time 25 { press_space }
after time $RecordStartSeconds { record start -audioonly "$WslWork/openmsx_aleste_transition" }
after time 35 { press_space }
after time $RecordStopSeconds {
    record stop
    exit
}
"@
Set-Content -Path "$WorkDir/side_b_aleste.tcl" -Value $tcl -Encoding ascii
Write-Host "== side B: openMSX Sony_HB-F1XV + Aleste 2 via WSL (record sound) =="
$prevEap = $ErrorActionPreference
$ErrorActionPreference = "Continue"
cmd /c "wsl.exe -- openmsx -machine Sony_HB-F1XV -carta `"$WslRoot/roms/aleste.rom`" -script `"$WslWork/side_b_aleste.tcl`" 2>nul" | Out-Null
$ErrorActionPreference = $prevEap
Require-Path "$WorkDir/openmsx_aleste_transition.wav" "side-B recording (record sound failed?)"

# --- Comparator: the identical section-2.6.4 metric on the reference WAV. ---
Write-Host "== comparator: section-2.6.4 burst metric on the reference capture =="
python tools/m34-audio-ab-analyze.py "$WorkDir/openmsx_aleste_transition.wav" |
    Tee-Object -FilePath "$WorkDir/side_b_metric.log" | ForEach-Object { Write-Host "  [side B] $_" }
$SideBOk = ($LASTEXITCODE -eq 0)

$Disposition = if ($SideAOk -and $SideBOk) { "PARITY (silence property)" } else { "DIVERGENCE" }
Write-Host "== disposition: $Disposition =="
Write-Host "(record the run's outputs in docs/m34-audio-ab.md per the package section 2.7)"
if ($Disposition -notlike "PARITY*") { exit 1 }
