param(
    [string]$ProgramBin = "tests/parity/m11_bus_probe.bin",
    [string]$BaseHex = "C000",
    [string]$Emulator = "build/Debug/sony_msx_headless.exe",
    [string]$TraceA = "build/m11_parity_trace_A.txt",
    [string]$TraceBRaw = "build/m11_parity_trace_B.txt",
    [string]$DiffOut = "docs/m11-parity-trace-diff.md",
    [string]$ProbeMachine = "Sony_HB-F1XV",
    [int]$MaxSteps = 64,
    [int]$BootSeconds = 8
)

# ---------------------------------------------------------------------------
# M11-S6 openMSX S1985 I/O parity harness.
#
# Captures a REAL architectural-state trace diff for the S1985 "MSX-ENGINE" I/O
# behaviours between:
#   A = this emulator (M10-S1 trace-export via --parity-trace), and
#   B = openMSX 19.1 on WSL running the GENUINE Sony HB-F1XV machine (with a real
#       <S1985> in its machine XML — the exact target hardware), single-stepped
#       over the SAME probe program at the SAME base with the SAME forced initial
#       CPU vector.
#
# The probe (tests/parity/m11_bus_probe.bin) is deliberately slot-safe and
# BIOS-independent: it exercises only S1985 I/O whose result lands in a CPU
# register and does NOT remap the running code's page:
#   - mapper readback: OUT (#FC),#25 ; IN A,(#FC) -> #85 (0x80|(seg&0x1F), §4)
#   - switched I/O:    OUT (#40),#FE ; IN A,(#40) -> #01 (~ID, §6)
#   - backup RAM:      OUT (#41),#05 ; OUT (#42),#3C ; IN A,(#42) -> #3C (§6)
# so the same architectural register sequence is reproducible on the real
# machine regardless of BIOS/RAM slot layout differences.
#
# It NEVER fabricates parity: the actual outcome (empty diff, concrete field
# mismatches, or BLOCKED) is written to $DiffOut by tools/trace-diff.py. If
# openMSX cannot be driven, the diff tool reports BLOCKED (empty B side).
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
    }
}

Require-Path -Path $ProgramBin -Label "probe program"
Require-Path -Path $Emulator -Label "emulator binary (build first)"

# --- A: emulator-side per-instruction trace via the S1 trace-export. ---
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
Write-Output "Reference machine: $ProbeMachine (genuine S1985 in machine XML)"

# --- Build the Tcl list of program bytes and the base. ---
$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $ProgramBin))
$byteList = ($bytes | ForEach-Object { "0x{0:X2}" -f $_ }) -join " "
$base = "0x$BaseHex"
$maxRows = $traceALineCount

# --- Tcl driver: boot HB-F1XV to a RAM-ready state, force the documented initial
#     CPU vector, write the probe into page-3 RAM, then single-step via chained
#     break-callbacks (the mechanism that makes `step` actually advance PC in a
#     headless script context). ---
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
$runCmd = "echo $encoded | base64 -d > /tmp/omx_io_parity.tcl && " +
          "timeout 120 openmsx -machine $ProbeMachine -command 'set renderer none' " +
          "-script /tmp/omx_io_parity.tcl 2>&1 | grep -a -E 'OMTRACE|OMSETUP' || true"
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

# Retitle the comparator output for the M11 context (comparator is shared M10 code).
if (Test-Path -LiteralPath $DiffOut) {
    $content = Get-Content -LiteralPath $DiffOut -Raw
    $content = $content -replace "# M10-S4 openMSX Per-Instruction Parity Trace Diff",
                                 "# M11-S6 openMSX S1985 I/O Parity Trace Diff (Sony HB-F1XV)"
    Set-Content -LiteralPath $DiffOut -Value $content -Encoding ASCII -NoNewline
}

switch ($diffExit) {
    0 { Write-Output "M11 A/B RESULT: ARCHITECTURAL PARITY (empty diff). See $DiffOut" }
    1 { Write-Output "M11 A/B RESULT: ARCHITECTURAL DIVERGENCE captured. See $DiffOut" }
    default { Write-Output "M11 A/B RESULT: BLOCKED / not comparable. See $DiffOut and $TraceBRaw" }
}
exit $diffExit
