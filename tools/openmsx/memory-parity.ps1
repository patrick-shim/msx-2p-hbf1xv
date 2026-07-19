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
    [string]$ProbeBin = "tests/parity/mem_probe.bin",
    [string]$ProbeBaseHex = "C000",
    [int]$ProbeSteps = 13,
    [int]$BootSteps = 24,
    [string]$Machine = "Sony_HB-F1XV",
    [string]$DiffOut = "docs/m13-parity-trace-diff.md",
    [string]$WorkDir = "build",
    [int]$BootSeconds = 6
)

# ---------------------------------------------------------------------------
# M13-S5 openMSX RAM/ROM + BIOS-boot A/B parity harness (extends the M10-S4
# per-instruction trace-parity mechanism to the genuine Sony_HB-F1XV machine).
#
# Two complementary subjects, both diffed on ARCHITECTURAL Z80 state via
# tools/analyze/trace-diff.py (never fabricated; empty B side -> BLOCKED):
#
#   1. RAM/ROM probe (primary, BIOS-independent): tests/parity/mem_probe.bin
#      run from a RAM-mapped state at 0xC000 on both emulators. It writes+reads a
#      mapper-RAM cell, switches a mapper segment via OUT (#FC) and reads back the
#      S1985 100xxxxx value, and reads a BIOS byte via a page-0 slot switch. This
#      is the CPU-visible memory behaviour M13 delivers.
#
#   2. BIOS-boot checkpoint: authentic reset (#A8 = 0, PC = 0x0000) single-stepped
#      a bounded number of instructions on both emulators (BootSteps). The window
#      is intentionally bounded before the BIOS first READS an unimplemented device
#      (VDP #98/#99, PSG, RTC — owned by M14+); a divergence at such a read is a
#      device-scope boundary, not an M13 memory defect, and is reported honestly.
#
# VRAM/VDP state is EXCLUDED from the diff (M14).
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path)) { throw "$Label not found: $Path" }
}

Require-Path -Path $Emulator -Label "emulator binary (build first)"
Require-Path -Path $ProbeBin -Label "RAM/ROM probe"
Require-Path -Path $BiosDir -Label "bios directory"

if (-not (Test-Path -LiteralPath $WorkDir)) { New-Item -ItemType Directory -Path $WorkDir | Out-Null }

$openmsxPath = (wsl -e bash -lc "command -v openmsx || true").Trim()
if ([string]::IsNullOrWhiteSpace($openmsxPath)) { throw "openMSX not found in WSL PATH" }
$openmsxVersion = (wsl -e bash -lc "openmsx --version 2>/dev/null || true").Trim()
Write-Output "openMSX: $openmsxPath ($openmsxVersion), machine: $Machine"

# === Subject 1: RAM/ROM probe (delegated to the shared trace-parity harness) ===
$probeDiff = Join-Path $WorkDir "m13_mem_probe_diff.md"
& (Join-Path $PSScriptRoot "trace-parity.ps1") `
    -ProbeMachine $Machine -ProgramBin $ProbeBin -BaseHex $ProbeBaseHex `
    -TraceA (Join-Path $WorkDir "m13_mem_A.txt") -TraceBRaw (Join-Path $WorkDir "m13_mem_B.txt") `
    -DiffOut $probeDiff -MaxSteps $ProbeSteps -BootSeconds $BootSeconds
$probeExit = $LASTEXITCODE
Write-Output "Subject 1 (RAM/ROM probe) diff exit: $probeExit"

# === Subject 2: BIOS-boot checkpoint ===
$bootA = Join-Path $WorkDir "m13_boot_A.txt"
$bootBRaw = Join-Path $WorkDir "m13_boot_B.txt"
$bootDiff = Join-Path $WorkDir "m13_boot_diff.md"

& $Emulator --bios-boot-trace $BiosDir $BootSteps $bootA
if ($LASTEXITCODE -ne 0) { throw "emulator bios-boot-trace failed (exit $LASTEXITCODE)" }
$bootALineCount = (Get-Content -LiteralPath $bootA).Count
Write-Output "Boot trace A (emulator) instructions: $bootALineCount"

# openMSX reset-step: force the same reset register file this emulator uses
# (all-zero, SP=0xFFFF, IM1), then single-step from PC=0x0000 via chained break
# callbacks; emit architectural state BEFORE each instruction.
$tcl = @"
set ::MAX $BootSteps
set ::seq 0
proc emit {} {
  if {[catch {
    set pc [reg pc]
    set b0 [debug read memory `$pc]
    set oplen 1
    if {`$b0 == 0xCB || `$b0 == 0xED || `$b0 == 0xDD || `$b0 == 0xFD} { set oplen 2 }
    set op ""
    for {set k 0} {`$k < `$oplen} {incr k} {
      append op [format %02X [debug read memory [expr {(`$pc + `$k) & 0xFFFF}]]]
    }
    puts stderr [format "OMTRACE seq=%d pc=%04X op=%s af=%04X bc=%04X de=%04X hl=%04X af2=%04X bc2=%04X de2=%04X hl2=%04X ix=%04X iy=%04X sp=%04X i=%02X r=%02X im=%d iff=%d" \
      `$::seq [reg pc] `$op [reg af] [reg bc] [reg de] [reg hl] [reg af2] [reg bc2] [reg de2] [reg hl2] [reg ix] [reg iy] [reg sp] [reg i] [reg r] [reg im] [reg iff]]
    incr ::seq
    if {`$::seq >= `$::MAX} { exit }
    after break emit
    step
  } err]} { puts stderr "OMTRACE ERROR seq=`$::seq msg=`$err"; exit }
}
after time $BootSeconds {
  if {[catch {
    reset
    reg af 0; reg bc 0; reg de 0; reg hl 0
    reg af2 0; reg bc2 0; reg de2 0; reg hl2 0
    reg ix 0; reg iy 0; reg sp 0xFFFF; reg pc 0x0000
    reg i 0; reg r 0; reg im 1; reg iff 0
    set rb [format %02X [debug read memory 0x0000]]
    puts stderr "OMSETUP reset_vector=`$rb"
    after break emit
    debug break
  } err]} { puts stderr "OMSETUP ERROR msg=`$err"; exit }
}
"@

$encoded = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($tcl))
$runCmd = "echo $encoded | base64 -d > /tmp/omx_boot_parity.tcl && " +
          "timeout 120 openmsx -machine $Machine -command 'set renderer none' " +
          "-script /tmp/omx_boot_parity.tcl 2>&1 | grep -a -E 'OMTRACE|OMSETUP' || true"
$raw = wsl -e bash -lc $runCmd

$rawLines = @()
if ($raw) {
    $rawLines = @($raw -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" })
}
Set-Content -LiteralPath $bootBRaw -Value ($rawLines -join "`n") -Encoding ASCII
$bootBCount = @($rawLines | Where-Object { $_ -match "OMTRACE seq=" }).Count
Write-Output "Boot trace B (openMSX) instructions captured: $bootBCount"

python tools/analyze/trace-diff.py $bootA $bootBRaw $bootDiff
$bootExit = $LASTEXITCODE
switch ($bootExit) {
    0 { Write-Output "Subject 2 (BIOS boot) RESULT: ARCHITECTURAL PARITY over $BootSteps instructions" }
    1 { Write-Output "Subject 2 (BIOS boot) RESULT: DIVERGENCE captured (see $bootDiff; often the device boundary)" }
    default { Write-Output "Subject 2 (BIOS boot) RESULT: BLOCKED / not comparable" }
}

Write-Output ""
Write-Output "Per-subject diffs: $probeDiff , $bootDiff"
Write-Output "Compose the final artifact into $DiffOut."
exit 0
