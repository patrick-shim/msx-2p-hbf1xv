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
  M41 generalized dual-emulator A/B capture/compare harness
  (docs/m41-production-qa-plan.md §5.4).

.DESCRIPTION
  Generalizes tools/capture/scroll-scenarios-run.ps1 (video) + tools/openmsx/fm-audio-ab.ps1
  (audio + WSL/base64-Tcl) into one scenario-manifest-driven harness. Each
  scenario names a guest (disk image built from an ASCII BASIC source via
  tools/gen/basic-to-disk.py, or a cart ROM), a capture kind (video|audio),
  settle timing, crop, and an expected disposition. For each scenario it:

    OURS   : build/Debug/sony_msx_headless.exe --debug-session <bios> <max>
             (--disk <img> | --cart1 <rom>) [--input-script <s>] --frames N
             video : --dump-frame frame -> tools/convert/frame-to-png.py -> ours.png
             audio : --audio-dump snd --audio-sync [--audio-psg-only]
                     -> tools/convert/audio-dump-to-wav.py -> ours.wav
    openMSX: WSL openMSX -machine Sony_HB-F1XV (-diska <img> | -cart <rom>),
             video : after time <s> { screenshot -raw omx_raw.png; exit }
             audio : set throttle off; record start/stop -audioonly -> omx.wav
    DIFF   : video -> tools/analyze/scroll-ab-diff.py ; audio -> tools/analyze/typed-audio-ab.py
    ROW    : appended to <OutDir>/m41-ab-report.md as MATCH / KNOWN-DELTA:<id>
             / BLOCKED:<reason>.

  Idempotent + re-runnable. If -FindingsFile is given its markdown is placed at
  the top of the report (the Phase-0 A-1..A-6 findings block).

.EXAMPLE
  powershell -File tools/capture/av-scenarios-run.ps1 -Manifest tools/capture/scenarios.json `
      -FindingsFile debug/m41/ab/phase0-findings.md
#>
# M51 (DEC-0078) EG-3 variant of tools/capture/av-scenarios-run.ps1: identical harness with a
# parameterized -Headless exe path, so the 13-mode A/B can run against the
# M51 worktree build while the canonical build/Debug exe is locked by a
# concurrent process (see docs/m51-implementation-report.md, interference
# incident). NO other change vs tools/capture/av-scenarios-run.ps1.
param(
    [string]$Headless    = "build/Debug/sony_msx_headless.exe",
    [string]$Manifest    = "tools/capture/scenarios.json",
    [string]$BiosDir     = "bios",
    [string]$OutDir      = "debug/m41/ab",
    [string]$FindingsFile = "",
    [string[]]$Only      = @()   # optional: restrict to these scenario ids
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repo = (Resolve-Path ".").Path
$headless = $Headless
if (-not [System.IO.Path]::IsPathRooted($headless)) { $headless = Join-Path $repo $headless }
if (-not (Test-Path $headless)) { Write-Error "headless exe missing: $headless (build first)"; exit 1 }
if (-not (Test-Path $Manifest)) { Write-Error "manifest missing: $Manifest"; exit 1 }

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

function To-WslPath([string]$winPath) {
    $abs = (Resolve-Path $winPath).Path
    return (wsl -e wslpath -a "$abs").Trim()
}

# Build a guest disk from an ASCII BASIC source via the injector.
function Build-GuestDisk([object]$sc, [string]$sdir) {
    $dsk = Join-Path $sdir "guest.dsk"
    $injArgs = @("tools/gen/basic-to-disk.py", "--out", $dsk)
    if ($sc.PSObject.Properties.Name -contains "autoexec") {
        $injArgs += @("--autoexec", (Join-Path $repo $sc.autoexec))
    }
    if ($sc.PSObject.Properties.Name -contains "ascii") {
        foreach ($a in $sc.ascii) { $injArgs += @("--ascii", $a) }
    }
    python @injArgs | Write-Host
    return $dsk
}

# --- OURS: video capture -> PNG ---
function Invoke-OursVideo([object]$sc, [string]$sdir, [string]$guestArgKind, [string]$guestPath) {
    $emuArgs = @("--debug-session", $BiosDir, "99999999", "--stock", $guestArgKind, $guestPath,
                 "--frames", "$($sc.frames)", "--debug-root", $sdir, "--dump-frame", "frame")
    if ($sc.PSObject.Properties.Name -contains "input_script" -and $sc.input_script) {
        $emuArgs += @("--input-script", (Join-Path $repo $sc.input_script))
    }
    $log = Join-Path $sdir "ours.log"
    Start-Process -FilePath $headless -ArgumentList $emuArgs -NoNewWindow -Wait `
        -RedirectStandardError $log -RedirectStandardOutput (Join-Path $sdir "ours.out") | Out-Null
    $oursPng = Join-Path $sdir "ours.png"
    python tools/convert/frame-to-png.py (Join-Path $sdir "frames/frame") -o $oursPng | Out-Null
    return $oursPng
}

# --- OURS: audio capture -> WAV ---
function Invoke-OursAudio([object]$sc, [string]$sdir, [string]$guestArgKind, [string]$guestPath) {
    $emuArgs = @("--debug-session", $BiosDir, "99999999", "--stock", $guestArgKind, $guestPath,
                 "--frames", "$($sc.frames)", "--debug-root", $sdir,
                 "--audio-dump", "snd", "--audio-sync")
    if ($sc.PSObject.Properties.Name -contains "psg_only" -and $sc.psg_only) {
        $emuArgs += "--audio-psg-only"
    }
    if ($sc.PSObject.Properties.Name -contains "audio_from") { $emuArgs += @("--audio-from", "$($sc.audio_from)") }
    if ($sc.PSObject.Properties.Name -contains "audio_to")   { $emuArgs += @("--audio-to", "$($sc.audio_to)") }
    if ($sc.PSObject.Properties.Name -contains "input_script" -and $sc.input_script) {
        $emuArgs += @("--input-script", (Join-Path $repo $sc.input_script))
    }
    $log = Join-Path $sdir "ours.log"
    Start-Process -FilePath $headless -ArgumentList $emuArgs -NoNewWindow -Wait `
        -RedirectStandardError $log -RedirectStandardOutput (Join-Path $sdir "ours.out") | Out-Null
    $oursWav = Join-Path $sdir "ours.wav"
    python tools/convert/audio-dump-to-wav.py (Join-Path $sdir "sounds/snd") -o $oursWav | Out-Null
    return $oursWav
}

# --- openMSX: video screenshot -raw after N emu seconds ---
function Invoke-OmxVideo([object]$sc, [string]$sdir, [string]$omxDiskArg, [string]$guestPath) {
    $guestWsl = To-WslPath $guestPath
    $tmpWsl = "/tmp/m41_omx_capture.png"
    $sec = $sc.omx_seconds
    $tcl = @"
after time $sec {
  screenshot -raw $tmpWsl
  puts stderr "M41OMX mode=[get_screen_mode_number] size=[file size $tmpWsl]"
  exit
}
"@
    $enc = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($tcl))
    $outWsl = (wsl -e wslpath -a "$((Resolve-Path $sdir).Path)").Trim() + "/omx_raw.png"
    $cmd = "echo $enc | base64 -d > /tmp/m41_cap.tcl && " +
           "timeout -k 3 120 openmsx -machine Sony_HB-F1XV $omxDiskArg '$guestWsl' -script /tmp/m41_cap.tcl 2>&1 | grep -a M41OMX; " +
           "cp $tmpWsl '$outWsl'"
    $out = wsl -e bash -lc $cmd
    Write-Host "  omx: $out"
    return (Join-Path $sdir "omx_raw.png")
}

# --- openMSX: audio record over [t0,t1] ---
function Invoke-OmxAudio([object]$sc, [string]$sdir, [string]$omxDiskArg, [string]$guestPath) {
    $guestWsl = To-WslPath $guestPath
    $outBaseWsl = (wsl -e wslpath -a "$((Resolve-Path $sdir).Path)").Trim() + "/omx"
    $t0 = $sc.omx_t0; $t1 = $sc.omx_t1
    $tcl = @"
set throttle off
after time $t0 { record start -audioonly "$outBaseWsl" }
after time $t1 { record stop; exit }
"@
    $enc = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($tcl))
    $cmd = "echo $enc | base64 -d > /tmp/m41_aud.tcl && " +
           "timeout -k 3 180 openmsx -machine Sony_HB-F1XV $omxDiskArg '$guestWsl' -script /tmp/m41_aud.tcl 2>&1 | grep -a -i record || true"
    wsl -e bash -lc $cmd | Write-Host
    return (Join-Path $sdir "omx.wav")
}

$manifestObj = Get-Content -Raw -LiteralPath $Manifest | ConvertFrom-Json
$rows = @()

foreach ($sc in $manifestObj) {
    if ($Only.Count -gt 0 -and ($Only -notcontains $sc.id)) { continue }
    $sdir = Join-Path $OutDir $sc.id
    New-Item -ItemType Directory -Path (Join-Path $sdir "frames") -Force | Out-Null
    Write-Host "=== $($sc.id) [$($sc.kind)] ===" -ForegroundColor Cyan

    # Resolve the guest for both sides.
    if ($sc.guest -eq "disk") {
        $guestPath = Build-GuestDisk $sc $sdir
        $oursGuestKind = "--disk"; $omxGuestArg = "-diska"
    } elseif ($sc.guest -eq "cart") {
        $guestPath = Join-Path $repo $sc.guest_path
        $oursGuestKind = "--cart1"; $omxGuestArg = "-cart"
    } else {
        Write-Host "  SKIP: unknown guest kind '$($sc.guest)'"; continue
    }

    $disposition = ""
    $metric = ""
    try {
        if ($sc.kind -eq "video") {
            $oursPng = Invoke-OursVideo $sc $sdir $oursGuestKind $guestPath
            $omxRaw  = Invoke-OmxVideo  $sc $sdir $omxGuestArg    $guestPath
            $ab = Join-Path $sdir "$($sc.id)_ab.png"
            $diffLine = python tools/analyze/scroll-ab-diff.py --ours $oursPng --omx $omxRaw `
                --ox $sc.ox --oy $sc.oy --out $ab --label $sc.id
            Write-Host "  $diffLine"
            $verd = ($diffLine | Select-String "verdict=(\w+)").Matches.Groups[1].Value
            $pct  = ($diffLine | Select-String "pct_mismatch=([\d.]+)").Matches.Groups[1].Value
            $metric = "pct_mismatch=$pct"
            if ($verd -eq "MATCH") { $disposition = "MATCH" }
            elseif ($sc.expected -like "KNOWN-DELTA*") { $disposition = $sc.expected }
            else { $disposition = "MISMATCH (escalate)" }
        }
        elseif ($sc.kind -eq "audio") {
            $oursWav = Invoke-OursAudio $sc $sdir $oursGuestKind $guestPath
            $omxWav  = Invoke-OmxAudio  $sc $sdir $omxGuestArg    $guestPath
            if (-not (Test-Path $omxWav)) {
                $disposition = "BLOCKED:openMSX-record-produced-no-wav"
            } else {
                $diffLine = python tools/analyze/typed-audio-ab.py --ours $oursWav --omx $omxWav --label $sc.id 2>&1
                $diffLine | ForEach-Object { Write-Host "  $_" }
                $verd = ($diffLine | Select-String "verdict=(\w+)").Matches.Groups[1].Value
                $metric = ($diffLine | Select-String "peak_delta_cents=([-\d.]+)").Matches.Groups[1].Value
                if ($verd -eq "MATCH") { $disposition = "MATCH" }
                elseif ($sc.expected -like "KNOWN-DELTA*") { $disposition = $sc.expected }
                else { $disposition = "DELTA (review)" }
            }
        }
        else { $disposition = "SKIP:unknown-kind" }
    } catch {
        $disposition = "BLOCKED:harness-error"
        Write-Host "  ERROR: $_" -ForegroundColor Red
    }

    Write-Host "  -> $disposition" -ForegroundColor Yellow
    $rows += [pscustomobject]@{
        id = $sc.id; kind = $sc.kind; expected = $sc.expected;
        metric = $metric; disposition = $disposition
    }
}

# --- compose report (Phase-0 findings header + scenario table) ---
$report = Join-Path $OutDir "m41-ab-report.md"
$lines = @()
if ($FindingsFile -and (Test-Path $FindingsFile)) {
    $lines += (Get-Content -LiteralPath $FindingsFile -Encoding UTF8)
    $lines += ""
}
$lines += @(
    "## M41 A/B scenario rows",
    "",
    "Generated: $(Get-Date -Format o)",
    "OURS = sony_msx_headless; openMSX = 19.1 Sony_HB-F1XV (WSL).",
    "",
    "| id | kind | expected | metric | disposition |",
    "|---|---|---|---|---|")
foreach ($r in $rows) {
    $lines += "| $($r.id) | $($r.kind) | $($r.expected) | $($r.metric) | $($r.disposition) |"
}
Set-Content -LiteralPath $report -Value ($lines -join [Environment]::NewLine) -Encoding UTF8
Write-Host "Report: $report" -ForegroundColor Green
$rows | Format-Table -AutoSize
