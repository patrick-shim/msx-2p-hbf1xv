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

<#
.SYNOPSIS
  M41-S3 typed-at-Ok AUDIO A/B runner (PSG P1-P5 + FM/OPLL F1-F4).

.DESCRIPTION
  Drives the IDENTICAL MSX keyboard-matrix (row,col) event sequence into both
  engines at the settled MSX-BASIC `Ok` prompt (NO disk -- both reach it), the
  guest program being typed BASIC that plays a DETERMINISTIC, SUSTAINED sound,
  then captures a fixed steady-state audio window on both sides and compares
  SPECTRALLY / by RMS-envelope (never sample-exact) via
  tools/analyze/typed-audio-ab.py.  (docs/m41-production-qa-plan.md S3 / S3.3-S3.4.)

    guest  : tools/gen/typed-basic-program.py  ->  our --input-script  +  openMSX
             `keymatrixdown/up` Tcl (+ the S3 `record` audio tail).
    OURS   : build/Debug/sony_msx_headless.exe --debug-session bios <max>
             --input-script <s> --frames N --audio-dump snd --audio-sync
             [--audio-psg-only] --audio-from <f> --audio-to <t>
             -> tools/convert/audio-dump-to-wav.py -> ours.wav
    openMSX: wsl openmsx -machine Sony_HB-F1XV -script <tcl>
             (set throttle off; type; record start/stop -audioonly) -> omx.wav
    DIFF   : tools/analyze/typed-audio-ab.py (dominant-peak cents, band cosine,
             RMS-envelope correlation).  Thresholds are per-subsystem (PSG tight,
             FM = KD-FM band).  MEASUREMENT ONLY -- never rebaseline.

  Emits one report row to stdout AND appends it to <OutRoot>/rows.md.
#>
param(
    [Parameter(Mandatory)][string]$Id,
    [Parameter(Mandatory)][string]$Text,          # literal; \n = RETURN
    [switch]$PsgOnly,
    [int]$StartFrame = 1350,
    [int]$Frames     = 0,          # 0 => auto from text length
    [int]$AudioFrom  = 0,          # 0 => auto (last keystroke + settle)
    [int]$AudioTo    = 0,          # 0 => auto
    [int]$SettleFrames = 40,       # steady-state settle after last keystroke
    [int]$WindowFrames = 180,      # steady audio window length (~3s)
    [double]$OmxStart = 24.0,   # openMSX boot->Ok ~20s (phase0); 24s = safe margin
                                # (22s occasionally drops early keystrokes -> flaky)
    [double]$RecAfter = 2.0,
    [double]$RecDur   = 3.0,
    [double]$CentsTol  = 5.0,
    [double]$CosineMin = 0.98,
    [double]$EnvMin    = 0.95,
    [string]$Expected = "MATCH",
    [string]$OutRoot  = "debug/m41/s3",
    [string]$BiosDir  = "bios"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repo = (Resolve-Path ".").Path
$headless = Join-Path $repo "build/Debug/sony_msx_headless.exe"
if (-not (Test-Path $headless)) { Write-Error "headless exe missing: $headless (build first)"; exit 1 }

$sdir = Join-Path $OutRoot $Id
New-Item -ItemType Directory -Force -Path (Join-Path $sdir "frames") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $sdir "sounds") | Out-Null

# Native tools (tools/convert/audio-dump-to-wav.py, the comparator) write progress to stderr;
# under ErrorActionPreference=Stop PowerShell turns that into a terminating
# NativeCommandError. Switch to Continue now that the pre-flight checks passed.
$ErrorActionPreference = "Continue"

$RepoWin = $repo
$WslRoot = "/mnt/" + ($RepoWin.Substring(0,1).ToLower()) + ($RepoWin.Substring(2) -replace "\\","/")
$sdirRel = $sdir -replace "\\","/"
$omxBaseWsl = "$WslRoot/$sdirRel/omx"       # openMSX record base -> <base>.wav
$tclWsl     = "$WslRoot/$sdirRel/g.tcl"

$scriptPath = Join-Path $sdir "g.script"
$tclPath    = Join-Path $sdir "g.tcl"

# Auto-derive the capture window from the typed program length. tools/gen/typed-basic-program.py
# advances 6 frames per character (hold 3 + gap 3); SHIFT is same-frame. So the
# last keystroke lands at StartFrame + nchars*6; capture a steady window past it.
$expanded = $Text -replace '\\n', "`n"
$nchars = $expanded.Length
$lastFrame = $StartFrame + $nchars * 6
if ($AudioFrom -le 0) { $AudioFrom = $lastFrame + $SettleFrames }
if ($AudioTo   -le 0) { $AudioTo   = $AudioFrom + $WindowFrames }
if ($Frames    -le 0) { $Frames    = $AudioTo + 30 }

Write-Host "=== $Id  (psg_only=$PsgOnly expected=$Expected) ===" -ForegroundColor Cyan
Write-Host "  auto-timing: nchars=$nchars last~$lastFrame audio=[$AudioFrom,$AudioTo] frames=$Frames"

# --- 1. Generate the shared (row,col) matrix sequence: our script + omx tcl. ---
python tools/gen/typed-basic-program.py --text $Text `
    --out-script $scriptPath --out-tcl $tclPath `
    --start-frame $StartFrame --omx-start $OmxStart `
    --audio-record $omxBaseWsl --rec-after $RecAfter --rec-dur $RecDur | Write-Host

# --- 2. OURS: headless typed run + steady-window audio dump. ---
$emuArgs = @("--debug-session", $BiosDir, "99999999", "--stock",
             "--input-script", $scriptPath, "--frames", "$Frames",
             "--debug-root", $sdir, "--audio-dump", "snd", "--audio-sync",
             "--audio-from", "$AudioFrom", "--audio-to", "$AudioTo")
if ($PsgOnly) { $emuArgs += "--audio-psg-only" }
$oursLog = Join-Path $sdir "ours.log"
Start-Process -FilePath $headless -ArgumentList $emuArgs -NoNewWindow -Wait `
    -RedirectStandardError $oursLog -RedirectStandardOutput (Join-Path $sdir "ours.out") | Out-Null
Get-Content $oursLog | Select-String "audio dump|steps=" | ForEach-Object { Write-Host "  ours: $_" }
$oursWav = Join-Path $sdir "ours.wav"
python tools/convert/audio-dump-to-wav.py (Join-Path $sdir "sounds/snd") -o $oursWav 2>&1 | Write-Host

# --- 3. openMSX: typed run + record over the steady window. ---
# Reliability: run openMSX in REALTIME (strip the `set throttle off` gen-typed-ab
# emits). Throttle-off makes matrix typing flaky (keystrokes drop / lag); the S2
# realtime typed-video path proved realtime injection reliable. Cost: ~30-40s
# wall per run (reach the emu record window at realtime).
(Get-Content $tclPath) | Where-Object { $_ -notmatch '^set throttle off' } | Set-Content $tclPath
$omxWav = Join-Path $sdir "omx.wav"
if (Test-Path $omxWav) { Remove-Item $omxWav -Force }
$prevEap = $ErrorActionPreference
$ErrorActionPreference = "Continue"
cmd /c "wsl.exe -- timeout -k 3 240 openmsx -machine Sony_HB-F1XV -script `"$tclWsl`" 2>nul" | Out-Null
$ErrorActionPreference = $prevEap
if (-not (Test-Path $omxWav)) {
    Write-Host "  BLOCKED: openMSX produced no wav ($omxWav)" -ForegroundColor Red
    $row = "| $Id | $Expected | (no omx wav) | BLOCKED:openMSX-record-produced-no-wav |"
    Add-Content -Path (Join-Path $OutRoot "rows.md") -Value $row
    Write-Output $row
    return
}

# --- 4. Compare (spectral / RMS envelope). ---
$diff = python tools/analyze/typed-audio-ab.py --ours $oursWav --omx $omxWav --label $Id `
    --cents-tol $CentsTol --cosine-min $CosineMin --env-min $EnvMin 2>&1
$diff | ForEach-Object { Write-Host "  $_" }
$verd  = ($diff | Select-String "verdict=(\w+)").Matches.Groups[1].Value
$cents = ($diff | Select-String "peak_delta_cents=([-\d.nan]+)").Matches.Groups[1].Value
$peakO = ($diff | Select-String "peak_ours=([\d.]+)").Matches.Groups[1].Value
$peakX = ($diff | Select-String "peak_omx=([\d.]+)").Matches.Groups[1].Value
$cos   = ($diff | Select-String "band_cosine=([-\d.]+)").Matches.Groups[1].Value
$env   = ($diff | Select-String "env_correlation=([-\d.]+)").Matches.Groups[1].Value
$metric = "peak_ours=${peakO}Hz peak_omx=${peakX}Hz cents=$cents cosine=$cos env=$env"

if ($verd -eq "MATCH") { $disposition = "MATCH" }
elseif ($Expected -like "KNOWN-DELTA*") { $disposition = "$Expected (within-band check by reviewer)" }
else { $disposition = "DELTA (review)" }

Write-Host "  -> $disposition  [$metric]" -ForegroundColor Yellow
$row = "| $Id | $Expected | $metric | $disposition |"
Add-Content -Path (Join-Path $OutRoot "rows.md") -Value $row
Write-Output $row
