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
    [string]$ProgramBin = "tests/parity/m18_peripheral_io_probe.bin",
    [string]$BaseHex = "C000",
    [string]$Emulator = "build/Debug/sony_msx_headless.exe",
    [string]$WorkDir = "build",
    [string]$TraceA = "build/m18_peripheral_io_trace_A.txt",
    [string]$TraceBRaw = "build/m18_peripheral_io_trace_B.txt",
    [string]$DiffOut = "docs/m18-parity-trace-diff.md",
    [string]$ProbeMachine = "Sony_HB-F1XV",
    [int]$MaxSteps = 400,
    [int]$BootSeconds = 8
)

# ---------------------------------------------------------------------------
# M18-S5 openMSX A/B peripheral-I/O parity harness (Kanji font ROM #D8-#DB +
# printer #90-#97 + cassette interface, Sony HB-F1XV).
#
# ONE combined architectural-trace subject (mirrors tools/openmsx-io-parity.
# ps1's exact mechanism -- no new CLI mode was needed: every read this probe
# performs (#D9/#DB Kanji data, #A2 PSG R14) lands directly in the CPU's A
# register, already captured by the existing --parity-trace per-instruction
# export) covering all three M18-S5 subjects back-to-back
# (docs/m18-planner-package.md §2.6):
#   Section A/B -- Kanji font ROM content parity (JIS1 via #D8/#D9, JIS2 via
#     #DA/#DB) -- the STRONGEST subject: real glyph bytes land in `af=`.
#   Section C -- printer write-path parity (#90/#91) -- write-only by
#     design, NEVER reads the status bit (A-M18-7 disclosed divergence).
#   Section D -- cassette idle-state (`IN A,(#A2)` with R14 selected, BEFORE
#     and AFTER a PPI port-C #AA write sequence) -- genuinely comparable
#     (both emulators default idle-high, A-M18-11).
#
# It NEVER fabricates parity: the actual outcome (empty diff, concrete field
# mismatches, or BLOCKED) is written to $DiffOut by tools/trace-diff.py.
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path)) { throw "$Label not found: $Path" }
}

Require-Path -Path $Emulator -Label "emulator binary (build first)"
Require-Path -Path $ProgramBin -Label "peripheral-I/O probe (run tools/gen-m18-peripheral-io-probe.py first)"

if (-not (Test-Path -LiteralPath $WorkDir)) { New-Item -ItemType Directory -Path $WorkDir | Out-Null }

# --- A: emulator-side per-instruction trace via the existing --parity-trace. ---
& $Emulator --parity-trace $ProgramBin $BaseHex $MaxSteps $TraceA
if ($LASTEXITCODE -ne 0) { throw "emulator parity-trace failed (exit $LASTEXITCODE)" }
Require-Path -Path $TraceA -Label "emulator trace A"
$traceALineCount = (Get-Content -LiteralPath $TraceA).Count
Write-Output "Trace A (emulator) instructions: $traceALineCount"

# --- Verify openMSX presence in WSL (honest capability, not parity). ---
$openmsxPath = (wsl -e bash -lc "command -v openmsx || true").Trim()
if ([string]::IsNullOrWhiteSpace($openmsxPath)) {
    throw "openMSX not found in WSL PATH"
}
$openmsxVersion = (wsl -e bash -lc "openmsx --version 2>/dev/null || true").Trim()
Write-Output "openMSX: $openmsxPath ($openmsxVersion)"
Write-Output "Reference machine: $ProbeMachine"

# --- Build the Tcl list of program bytes and the base. ---
$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $ProgramBin))
$byteList = ($bytes | ForEach-Object { "0x{0:X2}" -f $_ }) -join " "
$base = "0x$BaseHex"
$maxRows = $traceALineCount

# --- Tcl driver: boot HB-F1XV to a RAM-ready state, force the documented
#     initial CPU vector, write the probe into RAM, then single-step via
#     chained break-callbacks (identical mechanism to tools/openmsx-io-parity.
#     ps1 / tools/openmsx-ym2413-parity.ps1's Subject 1). ---
$tcl = @"
set renderer none
set throttle off
set ::PROG { $byteList }
set ::BASE $base
set ::MAX $maxRows
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

$encoded = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($tcl))
$runCmd = "echo $encoded | base64 -d > /tmp/omx_m18_peripheral_io.tcl && " +
          "timeout 120 openmsx -machine $ProbeMachine -command 'set renderer none' " +
          "-script /tmp/omx_m18_peripheral_io.tcl 2>&1 | grep -a -E 'OMTRACE|OMSETUP' || true"
$raw = wsl -e bash -lc $runCmd

$rawLines = @()
if ($raw) {
    $rawLines = @($raw -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" })
}
Set-Content -LiteralPath $TraceBRaw -Value ($rawLines -join "`n") -Encoding ASCII
$omtraceCount = @($rawLines | Where-Object { $_ -match "OMTRACE seq=" }).Count
Write-Output "Trace B (openMSX HB-F1XV) instructions captured: $omtraceCount"
Write-Output "openMSX raw trace written to: $TraceBRaw"

# --- Diff A vs B on architectural fields; write the REAL diff artifact. ---
$diffOutFull = [System.IO.Path]::GetFullPath($DiffOut)
$diffDir = Split-Path -Parent $diffOutFull
if ($diffDir -and -not (Test-Path -LiteralPath $diffDir)) {
    New-Item -ItemType Directory -Path $diffDir | Out-Null
}

python tools/trace-diff.py $TraceA $TraceBRaw $DiffOut
$diffExit = $LASTEXITCODE

# Retitle the comparator output for the M18 context (comparator is shared M10 code).
if (Test-Path -LiteralPath $DiffOut) {
    $content = Get-Content -LiteralPath $DiffOut -Raw
    $content = $content -replace "# M10-S4 openMSX Per-Instruction Parity Trace Diff",
                                 "# M18-S5 openMSX Peripheral-I/O Parity Trace Diff (Sony HB-F1XV)"
    Set-Content -LiteralPath $DiffOut -Value $content -Encoding ASCII -NoNewline
}

switch ($diffExit) {
    0 { Write-Output "M18 A/B RESULT: ARCHITECTURAL PARITY (empty diff). See $DiffOut" }
    1 { Write-Output "M18 A/B RESULT: ARCHITECTURAL DIVERGENCE captured. See $DiffOut" }
    default { Write-Output "M18 A/B RESULT: BLOCKED / not comparable. See $DiffOut and $TraceBRaw" }
}
exit $diffExit
