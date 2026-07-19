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
    [string]$ProgramBin = "tests/parity/ym2413_probe.bin",
    [string]$RegsList = "tests/parity/ym2413_probe_regs.txt",
    [string]$BaseHex = "C000",
    [string]$Emulator = "build/Debug/sony_msx_headless.exe",
    [string]$WorkDir = "build",
    [string]$DiffOut = "docs/m17-parity-trace-diff.md",
    [string]$ProbeMachine = "Sony_HB-F1XV",
    [int]$MaxSteps = 1000,
    [int]$BootSeconds = 6
)

# ---------------------------------------------------------------------------
# M17-S5 openMSX A/B parity harness (YM2413 / OPLL, Sony HB-F1XV).
#
# Two complementary, REAL captured subjects (never fabricated; the actual
# outcome -- empty diff, concrete mismatches, or BLOCKED -- is always written):
#
#   Subject 1 -- CPU-visible architectural-state parity across the write
#     sequence (mirrors tools/openmsx/io-parity.ps1's mechanism exactly: the
#     M17 probe program, run via this emulator's --parity-trace + a chained
#     single-step Tcl script on openMSX, diffed per-instruction via
#     tools/analyze/trace-diff.py). Because the OPLL is write-only (A-M17-5), this is
#     the CPU-register/flags/PC architectural check -- NOT a register-file
#     comparison (that's Subject 2).
#
#   Subject 2 -- internal YM2413 register-file comparison via each emulator's
#     own introspection (A-M17-6): this emulator's --ym2413-parity dump
#     (register_value(addr) for $00-$3F) vs openMSX's "MSX Music regs"
#     SimpleDebuggable, read via the WSL Tcl console as
#     `debug read {MSX Music regs} <addr>` -- run AFTER the SAME probe
#     program reaches HALT. This mechanism (R-M17-6) was independently
#     verified against a real WSL openMSX run before this harness was written
#     (see docs/m17-implementation-report.md) using the generic "ioports"
#     SimpleDebuggable to drive #7C/#7D and confirming `debug read {MSX Music
#     regs} <addr>` reflects the write.
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path)) { throw "$Label not found: $Path" }
}

Require-Path -Path $Emulator -Label "emulator binary (build first)"
Require-Path -Path $ProgramBin -Label "YM2413 probe program (run tools/gen/ym2413-probe.py first)"
Require-Path -Path $RegsList -Label "YM2413 probe regs list (run tools/gen/ym2413-probe.py first)"

if (-not (Test-Path -LiteralPath $WorkDir)) { New-Item -ItemType Directory -Path $WorkDir | Out-Null }

$openmsxPath = (wsl -e bash -lc "command -v openmsx || true").Trim()
if ([string]::IsNullOrWhiteSpace($openmsxPath)) { throw "openMSX not found in WSL PATH" }
$openmsxVersion = (wsl -e bash -lc "openmsx --version 2>/dev/null || true").Trim()
Write-Output "openMSX: $openmsxPath ($openmsxVersion), machine: $ProbeMachine"

# Parse the sidecar (addr,value) list the probe generator wrote.
$regPairs = @()
foreach ($line in Get-Content -LiteralPath $RegsList) {
    if ($line -match "ADDR=([0-9A-Fa-f]{2}) VALUE=([0-9A-Fa-f]{2})") {
        $regPairs += [PSCustomObject]@{ Addr = [Convert]::ToInt32($Matches[1], 16); Value = [Convert]::ToInt32($Matches[2], 16) }
    }
}
Write-Output "Probe writes $($regPairs.Count) registers."

$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $ProgramBin))
$byteList = ($bytes | ForEach-Object { "0x{0:X2}" -f $_ }) -join " "
$base = "0x$BaseHex"

# =====================================================================
# Subject 1: CPU-visible architectural-state parity (mirrors
# tools/openmsx/io-parity.ps1's mechanism, unmodified in spirit).
# =====================================================================
$traceA = Join-Path $WorkDir "m17_ym2413_trace_A.txt"
$traceBRaw = Join-Path $WorkDir "m17_ym2413_trace_B.txt"
$traceDiff = Join-Path $WorkDir "m17_ym2413_trace_diff.md"

& $Emulator --parity-trace $ProgramBin $BaseHex $MaxSteps $traceA
if ($LASTEXITCODE -ne 0) { throw "emulator parity-trace failed (exit $LASTEXITCODE)" }
$traceALineCount = (Get-Content -LiteralPath $traceA).Count
Write-Output "Subject 1: trace A (emulator) instructions: $traceALineCount"

$tcl1 = @"
set ::PROG { $byteList }
set ::BASE $base
set ::MAX $traceALineCount
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
$encoded1 = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($tcl1))
$runCmd1 = "echo $encoded1 | base64 -d > /tmp/omx_m17_ym2413_trace.tcl && " +
           "timeout 120 openmsx -machine $ProbeMachine -command 'set renderer none' " +
           "-script /tmp/omx_m17_ym2413_trace.tcl 2>&1 | grep -a -E 'OMTRACE|OMSETUP' || true"
$raw1 = wsl -e bash -lc $runCmd1
$rawLines1 = @()
if ($raw1) { $rawLines1 = @($raw1 -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" }) }
Set-Content -LiteralPath $traceBRaw -Value ($rawLines1 -join "`n") -Encoding ASCII
$traceBCount = @($rawLines1 | Where-Object { $_ -match "OMTRACE seq=" }).Count
Write-Output "Subject 1: trace B (openMSX) instructions captured: $traceBCount"

python tools/analyze/trace-diff.py $traceA $traceBRaw $traceDiff
$subject1Exit = $LASTEXITCODE
switch ($subject1Exit) {
    0 { Write-Output "Subject 1 RESULT: ARCHITECTURAL PARITY (empty diff, $traceALineCount instructions)" }
    1 { Write-Output "Subject 1 RESULT: ARCHITECTURAL DIVERGENCE captured (see $traceDiff)" }
    default { Write-Output "Subject 1 RESULT: BLOCKED / not comparable" }
}

# =====================================================================
# Subject 2: internal register-file comparison (A-M17-6).
# =====================================================================
$regsA = Join-Path $WorkDir "m17_ym2413_regs_A.txt"
$regsBRaw = Join-Path $WorkDir "m17_ym2413_regs_B.txt"

& $Emulator --ym2413-parity $ProgramBin $BaseHex $MaxSteps $regsA
if ($LASTEXITCODE -ne 0) { throw "emulator ym2413-parity failed (exit $LASTEXITCODE)" }
Write-Output "Subject 2: emulator register dump written to $regsA"

$tcl2 = @"
set renderer none
set throttle off
set ::PROG { $byteList }
set ::BASE $base
set ::MAX $MaxSteps
set ::seq 0
proc dumpregs {} {
  for {set a 0} {`$a < 0x40} {incr a} {
    set v [debug read {MSX Music regs} `$a]
    puts stderr [format "OMREG addr=%02X value=%02X" `$a `$v]
  }
}
proc emit {} {
  if {[catch {
    set pc [reg pc]
    set b0 [debug read memory `$pc]
    incr ::seq
    if {`$b0 == 0x76} { dumpregs; exit }
    if {`$::seq >= `$::MAX} { puts stderr "OMWARN max steps reached before HALT"; dumpregs; exit }
    after break emit
    step
  } err]} { puts stderr "OMTRACE ERROR seq=`$::seq msg=`$err"; exit }
}
after time $BootSeconds {
  if {[catch {
    set a `$::BASE
    foreach byte `$::PROG { debug write memory `$a `$byte; incr a }
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
$runCmd2 = "echo $encoded2 | base64 -d > /tmp/omx_m17_ym2413_regs.tcl && " +
           "timeout 120 openmsx -machine $ProbeMachine -command 'set renderer none' " +
           "-script /tmp/omx_m17_ym2413_regs.tcl 2>&1 | grep -a -E 'OMREG|OMWARN|OMTRACE ERROR' || true"
$raw2 = wsl -e bash -lc $runCmd2
$rawLines2 = @()
if ($raw2) { $rawLines2 = @($raw2 -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" }) }
Set-Content -LiteralPath $regsBRaw -Value ($rawLines2 -join "`n") -Encoding ASCII
$omregCount = @($rawLines2 | Where-Object { $_ -match "^OMREG addr=" }).Count
Write-Output "Subject 2: openMSX register dump lines captured: $omregCount"

# Parse both dumps into addr->value maps.
$mapA = @{}
foreach ($line in Get-Content -LiteralPath $regsA) {
    if ($line -match "REG addr=([0-9A-Fa-f]{2}) value=([0-9A-Fa-f]{2})") {
        $mapA[[Convert]::ToInt32($Matches[1], 16)] = [Convert]::ToInt32($Matches[2], 16)
    }
}
$mapB = @{}
foreach ($line in $rawLines2) {
    if ($line -match "OMREG addr=([0-9A-Fa-f]{2}) value=([0-9A-Fa-f]{2})") {
        $mapB[[Convert]::ToInt32($Matches[1], 16)] = [Convert]::ToInt32($Matches[2], 16)
    }
}

$subject2Blocked = ($mapA.Count -eq 0) -or ($mapB.Count -eq 0)
$subject2Mismatches = @()
if (-not $subject2Blocked) {
    foreach ($pair in $regPairs) {
        $addr = $pair.Addr
        $va = $mapA[$addr]
        $vb = $mapB[$addr]
        if ($va -ne $vb) {
            $subject2Mismatches += [PSCustomObject]@{ Addr = $addr; A = $va; B = $vb; Expected = $pair.Value }
        }
    }
}

if ($subject2Blocked) {
    Write-Output "Subject 2 RESULT: BLOCKED -- a side produced no register data (A=$($mapA.Count) entries, B=$($mapB.Count) entries)"
} elseif ($subject2Mismatches.Count -gt 0) {
    Write-Output "Subject 2 RESULT: DIVERGENCE -- $($subject2Mismatches.Count) address mismatch(es)"
} else {
    Write-Output "Subject 2 RESULT: REGISTER-FILE PARITY -- all $($regPairs.Count) written addresses match (empty diff)"
}

# =====================================================================
# Compose the final artifact.
# =====================================================================
$lines = @()
$lines += "# M17-S5 openMSX A/B Parity Trace Diff -- YM2413 (OPLL), Sony HB-F1XV"
$lines += ""
$lines += "- Milestone: M17 (FM-PAC/MSX-MUSIC YM2413 register-accurate device), slice S5."
$lines += "- Subject-A emulator: ``sony_msx_headless`` (this repo, Debug build)."
$lines += "- Reference-B emulator: $openmsxVersion (WSL, ``$openmsxPath``)."
$lines += "- Reference machine: ``$ProbeMachine`` -- ``references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml``, ``<MSX-MUSIC id=""MSX Music"">`` at slot 3-3, ``<io base=""0x7C"" num=""2"" type=""O""/>``."
$lines += "- Probe: ``$ProgramBin`` ($($regPairs.Count) two-port register writes, generated by ``tools/gen/ym2413-probe.py``), covering `$00-`$07 (user patch), `$0E (rhythm control), `$10-`$18/`$20-`$28 (F-Num/Block/Key-on), `$30-`$38 (instrument/volume, incl. `$36-`$38 rhythm volumes)."
$lines += ""
$lines += "## Subject 1 -- CPU-visible architectural-state parity across the write sequence"
$lines += ""
$lines += "Mechanism: identical to ``tools/openmsx/io-parity.ps1`` (unmodified in spirit) -- the probe runs via this emulator's ``--parity-trace`` and, on openMSX, via a chained single-step Tcl script; diffed per-instruction (PC, opcode, every register/flag) via ``tools/analyze/trace-diff.py``. Because the OPLL is write-only (A-M17-5), this proves the instruction stream executes identically when driving the real HB-F1XV I/O ports -- NOT a register-file comparison (that is Subject 2)."
$lines += ""
$lines += "- Emulator instructions (A): $traceALineCount"
$lines += "- openMSX instructions (B): $traceBCount"
switch ($subject1Exit) {
    0 { $lines += "- **Result: ARCHITECTURAL PARITY -- EMPTY DIFF** over all $traceALineCount instructions." }
    1 { $lines += "- **Result: ARCHITECTURAL DIVERGENCE captured.** See ``$traceDiff``." }
    default { $lines += "- **Result: BLOCKED / not comparable.** See ``$traceBRaw``." }
}
$lines += ""
$lines += "Raw traces: ``$traceA`` (emulator), ``$traceBRaw`` (openMSX). Diff: ``$traceDiff``."
$lines += ""
$lines += "## Subject 2 -- Internal YM2413 register-file comparison (A-M17-6)"
$lines += ""
$lines += "Mechanism: after the SAME write sequence reaches HALT on both sides, this emulator dumps ``register_value(addr)`` for `$00-`$3F (``--ym2413-parity``); openMSX's equivalent is read via the WSL Tcl console as ``debug read {MSX Music regs} <addr>`` against its ``""MSX Music regs""`` ``SimpleDebuggable`` (references/openmsx-21.0/src/sound/YM2413.hh:40-44,YM2413.cc:19-29, size 0x40). **This mechanism was independently verified against a real WSL openMSX run before this harness was built** (docs/m17-implementation-report.md records the verification: ``debug write ioports 0x7C 0x10 ; debug write ioports 0x7D 0x5A ; debug read {MSX Music regs} 0x10`` returned ``0x5A``)."
$lines += ""
$lines += "- Emulator register entries (A): $($mapA.Count)"
$lines += "- openMSX register entries (B): $($mapB.Count)"
if ($subject2Blocked) {
    $lines += "- **Result: BLOCKED** -- a side produced no register data (this would be reported honestly, never fabricated)."
} elseif ($subject2Mismatches.Count -gt 0) {
    $lines += "- **Result: DIVERGENCE** -- $($subject2Mismatches.Count) of $($regPairs.Count) written addresses mismatch:"
    $lines += ""
    $lines += "| addr | A (emulator) | B (openMSX) | expected (probe) |"
    $lines += "|------|---------------|-------------|-------------------|"
    foreach ($m in $subject2Mismatches) {
        $lines += ("| {0:X2} | {1:X2} | {2:X2} | {3:X2} |" -f $m.Addr, $m.A, $m.B, $m.Expected)
    }
} else {
    $lines += "- **Result: REGISTER-FILE PARITY -- EMPTY DIFF.** All $($regPairs.Count) written addresses match exactly between this emulator's ``register_value(addr)`` and openMSX's ``debug read {MSX Music regs} <addr>``."
}
$lines += ""
$lines += "Raw dumps: ``$regsA`` (emulator), ``$regsBRaw`` (openMSX)."
$lines += ""
$lines += "## Reproduce"
$lines += ""
$lines += '```powershell'
$lines += "cmake --build build --config Debug"
$lines += "python tools/gen/ym2413-probe.py"
$lines += "tools/openmsx/ym2413-parity.ps1"
$lines += '```'
$lines += ""

$diffOutFull = [System.IO.Path]::GetFullPath($DiffOut)
$diffDir = Split-Path -Parent $diffOutFull
if ($diffDir -and -not (Test-Path -LiteralPath $diffDir)) { New-Item -ItemType Directory -Path $diffDir | Out-Null }
Set-Content -LiteralPath $DiffOut -Value ($lines -join "`n") -Encoding ASCII

Write-Output ""
Write-Output "Artifact written to $DiffOut"
if ($subject1Exit -eq 0 -and -not $subject2Blocked -and $subject2Mismatches.Count -eq 0) {
    exit 0
} elseif ($subject1Exit -eq 2 -or $subject2Blocked) {
    exit 2
} else {
    exit 1
}
