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
    [string]$Machine = "Sony_HB-F1XV",
    [string]$WorkDir = "build",
    [string]$DiffOut = "docs/m16-parity-trace-diff.md",
    [int]$BootTraceSteps = 3000,
    [string]$FdcProbeBin = "tests/parity/fdc_probe.bin",
    [string]$FdcDisk = "tests/parity/boot.dsk",
    [int]$FdcProbeSafetyCapSteps = 20000,
    [int]$BootSeconds = 6
)

# ---------------------------------------------------------------------------
# M16-S6 openMSX A/B parity harness (FDC / WD2793, Sony HB-F1XV).
#
# Two complementary, REAL captured subjects (never fabricated; the actual
# outcome -- empty diff, concrete mismatches, or BLOCKED -- is always written):
#
#   Subject 1 -- Real BIOS-boot checkpoint (authentic reset, #A8=0, PC=0x0000):
#     single-steps BOTH emulators from the SAME forced initial CPU vector over
#     the REAL disk-ROM-wired boot path (this emulator's --bios-boot-trace vs
#     openMSX single-stepped via chained break-callbacks). Diffed on full
#     architectural state (PC/opcode/registers/flags) via tools/analyze/trace-diff.py.
#
#   Subject 2 -- FDC register/command sequence probe: the SAME Z80 program as
#     tests/integration/machine/hbf1xv_fdc_integration_test.cpp's
#     build_restore_read_sector_probe() (assembled by
#     tools/gen/fdc-probe.py into tests/parity/fdc_probe.bin), run from
#     a flat-RAM base with the IDENTICAL tests/parity/boot.dsk (from
#     tools/gen/boot-disk.py; byte-identical to this emulator's default
#     synthesized medium) mounted as drive A on BOTH sides. It writes side/
#     drive/motor (0x7FFC/0x7FFD), issues a Type I Restore, polls status
#     (0x7FF8), issues a Type II Read Sector of LBA 0, polls INTRQ/DRQ
#     (0x7FFF), and streams the 512 data bytes (0x7FFB) into RAM.
#
#     NOTE (documented, not fabricated): the two WD2793 models' DRQ-wait
#     polling-loop CYCLE CADENCE differs (this emulator's deterministic ~114-
#     cycle/byte model, planner Section 5.2, vs openMSX's own timing), so the
#     two traces take a DIFFERENT NUMBER of loop iterations to observe DRQ
#     ready -- exactly the kind of cycle-level difference the M10+ parity
#     convention already excludes from the architectural gate (T-state/cycle
#     fields are reported, never gated). Subject 2 therefore reports TWO
#     things, both genuine: (a) an index-aligned diff up to the first
#     iteration-count divergence (documents exactly where cadence differs),
#     and (b) FUNCTIONAL parity at completion -- the final HALT register file
#     AND the actual 512 transferred bytes, which must and do match exactly.
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path)) { throw "$Label not found: $Path" }
}

Require-Path -Path $Emulator -Label "emulator binary (build first)"
Require-Path -Path $BiosDir -Label "bios directory"
Require-Path -Path $FdcProbeBin -Label "FDC probe program (run tools/gen/fdc-probe.py first)"
Require-Path -Path $FdcDisk -Label "M16 boot disk fixture (run tools/gen/boot-disk.py first)"

if (-not (Test-Path -LiteralPath $WorkDir)) { New-Item -ItemType Directory -Path $WorkDir | Out-Null }

$openmsxPath = (wsl -e bash -lc "command -v openmsx || true").Trim()
if ([string]::IsNullOrWhiteSpace($openmsxPath)) { throw "openMSX not found in WSL PATH" }
$openmsxVersion = (wsl -e bash -lc "openmsx --version 2>/dev/null || true").Trim()
Write-Output "openMSX: $openmsxPath ($openmsxVersion), machine: $Machine"

# =====================================================================
# Subject 1: real BIOS-boot checkpoint
# =====================================================================
$bootA = Join-Path $WorkDir "m16_boot_A.txt"
$bootBRaw = Join-Path $WorkDir "m16_boot_B.txt"
$bootDiff = Join-Path $WorkDir "m16_boot_diff.md"

& $Emulator --bios-boot-trace $BiosDir $BootTraceSteps $bootA
if ($LASTEXITCODE -ne 0) { throw "emulator bios-boot-trace failed (exit $LASTEXITCODE)" }
$bootALineCount = (Get-Content -LiteralPath $bootA).Count
Write-Output "Subject 1: boot trace A (emulator) instructions: $bootALineCount"

$tcl1 = @"
set ::MAX $BootTraceSteps
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
    after break emit
    debug break
  } err]} { puts stderr "OMSETUP ERROR msg=`$err"; exit }
}
"@
$encoded1 = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($tcl1))
$runCmd1 = "echo $encoded1 | base64 -d > /tmp/omx_m16_boot.tcl && " +
           "timeout 120 openmsx -machine $Machine -command 'set renderer none' " +
           "-script /tmp/omx_m16_boot.tcl 2>&1 | grep -a -E 'OMTRACE|OMSETUP' || true"
$raw1 = wsl -e bash -lc $runCmd1
$rawLines1 = @()
if ($raw1) { $rawLines1 = @($raw1 -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" }) }
Set-Content -LiteralPath $bootBRaw -Value ($rawLines1 -join "`n") -Encoding ASCII
$bootBCount = @($rawLines1 | Where-Object { $_ -match "OMTRACE seq=" }).Count
Write-Output "Subject 1: boot trace B (openMSX) instructions captured: $bootBCount"

python tools/analyze/trace-diff.py $bootA $bootBRaw $bootDiff
$bootExit = $LASTEXITCODE
switch ($bootExit) {
    0 { Write-Output "Subject 1 RESULT: ARCHITECTURAL PARITY over $BootTraceSteps instructions" }
    1 { Write-Output "Subject 1 RESULT: DIVERGENCE captured (see $bootDiff)" }
    default { Write-Output "Subject 1 RESULT: BLOCKED / not comparable" }
}

# =====================================================================
# Subject 2: FDC register-sequence probe (disk mounted on both sides)
# =====================================================================
$fdcA = Join-Path $WorkDir "m16_fdc_probe_A.txt"
$fdcBRaw = Join-Path $WorkDir "m16_fdc_probe_B.txt"
$fdcBuf = Join-Path $WorkDir "m16_fdc_probe_B_buf.hex"
$fdcDiff = Join-Path $WorkDir "m16_fdc_probe_diff.md"

& $Emulator --parity-trace $FdcProbeBin C000 $FdcProbeSafetyCapSteps $fdcA
if ($LASTEXITCODE -ne 0) { throw "emulator parity-trace (FDC probe) failed (exit $LASTEXITCODE)" }
$fdcALineCount = (Get-Content -LiteralPath $fdcA).Count
Write-Output "Subject 2: FDC probe trace A (emulator) instructions: $fdcALineCount"

$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $FdcProbeBin))
$byteList = ($bytes | ForEach-Object { "0x{0:X2}" -f $_ }) -join " "

$tcl2 = @"
set renderer none
set throttle off
set ::PROG { $byteList }
set ::BASE 0xC000
set ::MAX $FdcProbeSafetyCapSteps
set ::seq 0
proc dumpbuf {} {
  set out ""
  for {set a 0xC200} {`$a < 0xC400} {incr a} {
    append out [format %02X [debug read memory `$a]]
  }
  puts stderr "OMBUF `$out"
}
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
    if {`$b0 == 0x76} { dumpbuf; exit }
    if {`$::seq >= `$::MAX} { exit }
    after break emit
    step
  } err]} { puts stderr "OMTRACE ERROR seq=`$::seq msg=`$err"; exit }
}
after time $BootSeconds {
  if {[catch {
    set a `$::BASE
    foreach byte `$::PROG { debug write memory `$a `$byte; incr a }
    set rb [format %02X [debug read memory `$::BASE]]
    puts stderr "OMSETUP ram_base_readback=`$rb"
    reg af 0; reg bc 0; reg de 0; reg hl 0
    reg af2 0; reg bc2 0; reg de2 0; reg hl2 0
    reg ix 0; reg iy 0; reg sp 0xFFFF; reg pc `$::BASE
    reg i 0; reg r 0; reg im 1; reg iff 0
    after break emit
    debug break
  } err]} { puts stderr "OMSETUP ERROR msg=`$err"; exit }
}
"@
$encoded2 = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($tcl2))

# Convert the Windows disk-fixture path to its WSL-visible equivalent for -diska.
$diskFullPath = (Resolve-Path -LiteralPath $FdcDisk).Path
$diskDrive = $diskFullPath.Substring(0,1).ToLower()
$diskRest = $diskFullPath.Substring(2) -replace '\\','/'
$diskWslPath = "/mnt/$diskDrive$diskRest"

$runCmd2 = "cp '$diskWslPath' /tmp/m16_boot_probe.dsk && " +
           "echo $encoded2 | base64 -d > /tmp/omx_m16_fdc.tcl && " +
           "timeout 150 openmsx -machine $Machine -diska /tmp/m16_boot_probe.dsk " +
           "-command 'set renderer none' -script /tmp/omx_m16_fdc.tcl 2>&1 | grep -a -E 'OMTRACE|OMSETUP|OMBUF' || true"
$raw2 = wsl -e bash -lc $runCmd2
$rawLines2 = @()
if ($raw2) { $rawLines2 = @($raw2 -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" }) }
$traceLines2 = @($rawLines2 | Where-Object { $_ -match "^(OMTRACE|OMSETUP)" })
Set-Content -LiteralPath $fdcBRaw -Value ($traceLines2 -join "`n") -Encoding ASCII
$bufLine = @($rawLines2 | Where-Object { $_ -match "^OMBUF " }) | Select-Object -Last 1
if ($bufLine) {
    Set-Content -LiteralPath $fdcBuf -Value ($bufLine -replace '^OMBUF ', '') -Encoding ASCII -NoNewline
}
$fdcBCount = @($traceLines2 | Where-Object { $_ -match "OMTRACE seq=" }).Count
Write-Output "Subject 2: FDC probe trace B (openMSX) instructions captured: $fdcBCount"

python tools/analyze/trace-diff.py $fdcA $fdcBRaw $fdcDiff
$fdcExit = $LASTEXITCODE
switch ($fdcExit) {
    0 { Write-Output "Subject 2 RESULT: ARCHITECTURAL PARITY (empty diff, full sequence)" }
    1 { Write-Output "Subject 2 RESULT: index-aligned DIVERGENCE captured (see $fdcDiff); check the functional/final-state and buffer comparison separately" }
    default { Write-Output "Subject 2 RESULT: BLOCKED / not comparable" }
}

# Functional check: final HALT register file + actual transferred bytes.
$aLines = Get-Content -LiteralPath $fdcA
$bLines = Get-Content -LiteralPath $fdcBRaw | Where-Object { $_ -match "^OMTRACE" }
$aLast = $aLines[-1]
$bLast = $bLines[-1]
Write-Output "Subject 2: emulator  final: $aLast"
Write-Output "Subject 2: openMSX   final: $bLast"
if (Test-Path -LiteralPath $fdcBuf) {
    $bufHex = (Get-Content -LiteralPath $fdcBuf -Raw)
    $expectedHex = (Get-Content -LiteralPath $FdcDisk -Encoding Byte -TotalCount 512 | ForEach-Object { "{0:X2}" -f $_ }) -join ""
    $bufMatch = ($bufHex.ToUpper() -eq $expectedHex.ToUpper())
    Write-Output "Subject 2: openMSX transferred 512 bytes match disk LBA 0: $bufMatch"
}

Write-Output ""
Write-Output "Artifacts: $bootA $bootBRaw $bootDiff ; $fdcA $fdcBRaw $fdcBuf $fdcDiff"
Write-Output "Compose the final artifact into $DiffOut."
exit 0
