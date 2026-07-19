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
  M38 Phase-A V9958 horizontal-scroll A/B harness driver.

.DESCRIPTION
  For each generated scenario cartridge ROM (tools/gen/vdp-scroll-cart.py):
    A (ours) : sony_msx_headless --debug-session --cart1 <rom> --frames N
               --dump-frame -> tools/convert/frame-to-png.py -> ours.png
    B (omx)  : WSL openMSX -machine Sony_HB-F1XV -cart <rom>, screenshot -raw
               after <Seconds> emulated seconds -> omx_raw.png
    diff     : tools/analyze/scroll-ab-diff.py -> <scenario>_ab.png + a metric line
  Writes a markdown A/B table to <OutDir>/m38-ab-report.md.

  Deterministic: both emulators boot the SAME cart to a settled static scene;
  identical inputs -> identical PNGs across runs.

.EXAMPLE
  pwsh -File tools/capture/scroll-scenarios-run.ps1
  pwsh -File tools/capture/scroll-scenarios-run.ps1 -Scenarios s06_g4_multipage,s04_g4_coarse_fine
#>
# M51 (DEC-0078) EG-3 variant of tools/capture/scroll-scenarios-run.ps1 with a parameterized
# -Headless exe path (same rationale as tools/capture/sprite-scenarios-run.ps1). NO other change.
param(
    [string]$Headless = "build/Debug/sony_msx_headless.exe",
    [string]$BiosDir = "bios",
    [string]$CartDir = "debug/m38/carts",
    [string]$OutDir  = "debug/m38/ab",
    [int]$Frames     = 700,     # ours: frames to run (cart autostarts ~400)
    [int]$Seconds    = 10,      # omx: emulated seconds before screenshot
    [string[]]$Scenarios = @()  # empty = all *.rom in CartDir
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path ".").Path
$headless = $Headless
if (-not [System.IO.Path]::IsPathRooted($headless)) { $headless = Join-Path $repo $headless }
if (-not (Test-Path $headless)) { Write-Error "headless exe missing: $headless (build first)"; exit 1 }

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

if ($Scenarios.Count -eq 0) {
    $roms = Get-ChildItem -Path $CartDir -Filter "*.rom" | Sort-Object Name
} else {
    $roms = $Scenarios | ForEach-Object { Get-Item (Join-Path $CartDir ($_ + ".rom")) }
}

function To-WslPath([string]$winPath) {
    $abs = (Resolve-Path $winPath).Path
    return (wsl -e wslpath -a "$abs").Trim()
}

# --- openMSX capture (WSL): boot cart, screenshot -raw after N emu seconds ---
function Invoke-Openmsx([string]$romWin, [string]$outRawWin, [int]$seconds) {
    $romWsl = To-WslPath $romWin
    $tmpWsl = "/tmp/m38_omx_capture.png"
    $tcl = @"
after time $seconds {
  screenshot -raw $tmpWsl
  puts stderr "M38OMX mode=[get_screen_mode_number] size=[file size $tmpWsl]"
  exit
}
"@
    $enc = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($tcl))
    $outRawWsl = (wsl -e wslpath -a "$((Resolve-Path (Split-Path $outRawWin -Parent)).Path)").Trim() + "/" + (Split-Path $outRawWin -Leaf)
    $cmd = "echo $enc | base64 -d > /tmp/m38_cap.tcl && " +
           "timeout -k 3 90 openmsx -machine Sony_HB-F1XV -cart '$romWsl' -script /tmp/m38_cap.tcl 2>&1 | grep -a M38OMX; " +
           "cp $tmpWsl '$outRawWsl'"
    $out = wsl -e bash -lc $cmd
    return $out
}

$rows = @()
foreach ($rom in $roms) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($rom.Name)
    $sdir = Join-Path $OutDir $name
    New-Item -ItemType Directory -Path $sdir -Force | Out-Null
    Write-Host "=== $name ===" -ForegroundColor Cyan

    # A: ours
    $emuArgs = @("--debug-session", $BiosDir, "99999999", "--cart1", $rom.FullName,
              "--frames", "$Frames", "--debug-root", $sdir, "--dump-frame", "frame")
    $log = Join-Path $sdir "ours.log"
    $p = Start-Process -FilePath $headless -ArgumentList $emuArgs -NoNewWindow -Wait -PassThru `
         -RedirectStandardError $log -RedirectStandardOutput (Join-Path $sdir "ours.out")
    $frameDump = Join-Path $sdir "frames/frame"
    $oursPng = Join-Path $sdir "ours.png"
    python tools/convert/frame-to-png.py $frameDump -o $oursPng | Out-Null

    # B: openMSX
    $omxRaw = Join-Path $sdir "omx_raw.png"
    $omxOut = Invoke-Openmsx $rom.FullName $omxRaw $Seconds
    Write-Host "  omx: $omxOut"

    # diff
    $ab = Join-Path $sdir ($name + "_ab.png")
    $diffLine = python tools/analyze/scroll-ab-diff.py --ours $oursPng --omx $omxRaw --out $ab --label $name
    Write-Host "  $diffLine"

    # parse metric
    $mean = ($diffLine | Select-String "mean_dist=([\d.]+)").Matches.Groups[1].Value
    $pct  = ($diffLine | Select-String "pct_mismatch=([\d.]+)").Matches.Groups[1].Value
    $verd = ($diffLine | Select-String "verdict=(\w+)").Matches.Groups[1].Value
    $rows += [pscustomobject]@{ scenario=$name; mean_dist=$mean; pct_mismatch=$pct; verdict=$verd; composite=$ab }
}

# --- markdown report ---
$report = Join-Path $OutDir "m38-ab-report.md"
$lines = @("# M38 Phase-A V9958 Horizontal-Scroll A/B Report", "",
    "Generated: $(Get-Date -Format o)", "",
    "A = ours (sony_msx_headless active-display dump); B = openMSX 19.1 Sony_HB-F1XV raw screenshot (active area cropped, offset 32,24).",
    "", "| scenario | mean_dist/ch | mismatch % | verdict | composite |",
    "|---|---|---|---|---|")
foreach ($r in $rows) {
    $lines += "| $($r.scenario) | $($r.mean_dist) | $($r.pct_mismatch) | $($r.verdict) | $($r.composite) |"
}
Set-Content -LiteralPath $report -Value ($lines -join [Environment]::NewLine) -Encoding UTF8
Write-Host "Report: $report" -ForegroundColor Green
$rows | Format-Table -AutoSize
