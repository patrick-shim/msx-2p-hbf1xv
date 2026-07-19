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
    [string]$ProgramBin = "tests/parity/cartridge_probe.bin",
    [string]$CartridgeRom = "tests/parity/cartridge.rom",
    [string]$BaseHex = "C000",
    [string]$Emulator = "build/Debug/sony_msx_headless.exe",
    [string]$WorkDir = "build",
    [string]$DiffOut = "docs/m19-parity-trace-diff.md",
    [string]$ProbeMachine = "Sony_HB-F1XV",
    [int]$MaxSteps = 100,
    [int]$BootSeconds = 6
)

# ---------------------------------------------------------------------------
# M19-S6 openMSX A/B parity harness (external cartridge slot 1, Sony HB-F1XV,
# backlog B7). SYNTHETIC content-bearing check: both sides load the IDENTICAL
# authored `Generic8kB` cartridge image (tools/gen/cartridge-probe.py) and
# run the IDENTICAL Z80 driver program, which:
#   - OUT (#A8), 0xF7  -- repoints CPU page 1 (0x4000-0x7FFF) at primary slot 1
#     (pages 0/2/3 stay at primary slot 3 RAM); 0xF7 derived from THIS
#     project's own established SlotBus bit layout
#     (src/devices/chipset/slot_bus.cpp:47-49).
#   - LD A,(0x4000)         -- reads the reset-default marker (bank 0 -> 0x00)
#   - LD (0x4000),3         -- bank-switches window-slot 2 to bank 3
#     (RomGeneric8kB.cc:24-27, "writable at ANY address, slot = addr>>13")
#   - LD A,(0x4000)         -- re-reads (now bank 3 -> 0x03)
#   - LD A,(0x6000)         -- reads the UNAFFECTED window-slot 3 (still bank
#     1 -> 0x01)
#   - HALT
# Because every `LD A,(addr)` result lands in the accumulator, the EXISTING
# per-instruction trace mechanism (identical to every prior milestone's
# --parity-trace / openMSX chained-single-step Tcl script, diffed via
# tools/analyze/trace-diff.py) captures the read-back byte VALUES in the AF field --
# an empty diff across the WHOLE trace is simultaneously the architectural
# parity proof (PC/registers/flags per instruction) AND the content-bearing
# proof (both sides load an identically-authored file, so the expected AF
# values -- 0x00, then 0x03, then 0x01 -- are fully known in advance,
# planner §2.7), with no second subject/mechanism needed.
#
# Slot-lettering (R-M19-6): openMSX's `-carta` was EMPIRICALLY confirmed
# (live WSL Tcl probe, see docs/m19-parity-trace-diff.md "Slot-lettering
# verification" section) to land in THIS machine's primary slot 1 -- not
# assumed from openMSX's general CLI docs alone.
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path)) { throw "$Label not found: $Path" }
}

Require-Path -Path $Emulator -Label "emulator binary (build first)"
Require-Path -Path $ProgramBin -Label "M19 cartridge probe program (run tools/gen/cartridge-probe.py first)"
Require-Path -Path $CartridgeRom -Label "M19 synthetic cartridge image (run tools/gen/cartridge-probe.py first)"

if (-not (Test-Path -LiteralPath $WorkDir)) { New-Item -ItemType Directory -Path $WorkDir | Out-Null }

$openmsxPath = (wsl -e bash -lc "command -v openmsx || true").Trim()
if ([string]::IsNullOrWhiteSpace($openmsxPath)) { throw "openMSX not found in WSL PATH" }
$openmsxVersion = (wsl -e bash -lc "openmsx --version 2>/dev/null || true").Trim()
Write-Output "openMSX: $openmsxPath ($openmsxVersion), machine: $ProbeMachine"

$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $ProgramBin))
$byteList = ($bytes | ForEach-Object { "0x{0:X2}" -f $_ }) -join " "
$base = "0x$BaseHex"

# =====================================================================
# Subject A (this emulator): --parity-trace, extended (M19-S5) to load
# --cart1/--cart1-type via the SAME parser/loader as the default run path.
# =====================================================================
$traceA = Join-Path $WorkDir "m19_cartridge_trace_A.txt"
$traceBRaw = Join-Path $WorkDir "m19_cartridge_trace_B.txt"
$traceDiff = Join-Path $WorkDir "m19_cartridge_trace_diff.md"

& $Emulator --parity-trace $ProgramBin $BaseHex $MaxSteps $traceA --cart1 $CartridgeRom --cart1-type 8kB
if ($LASTEXITCODE -ne 0) { throw "emulator parity-trace failed (exit $LASTEXITCODE)" }
$traceALineCount = (Get-Content -LiteralPath $traceA).Count
Write-Output "Trace A (emulator) instructions: $traceALineCount"

# =====================================================================
# Subject B (openMSX, WSL): mount the SAME cartridge via -carta (empirically
# confirmed = primary slot 1), inject the SAME driver-program bytes into RAM,
# then chained-single-step exactly like every prior milestone's harness.
# =====================================================================
$cartRomAbs = (Resolve-Path -LiteralPath $CartridgeRom).Path
$cartRomWsl = "/mnt/" + $cartRomAbs.Substring(0,1).ToLower() + $cartRomAbs.Substring(2).Replace('\','/')

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
$tclPathWsl = "/tmp/omx_m19_cartridge_trace.tcl"
$runCmd1 = "echo $encoded1 | base64 -d > $tclPathWsl && " +
           "timeout 120 openmsx -machine $ProbeMachine -carta '$cartRomWsl' -romtype 8kB " +
           "-command 'set renderer none' -script $tclPathWsl 2>&1 | grep -a -E 'OMTRACE|OMSETUP' || true"
$raw1 = wsl -e bash -lc $runCmd1
$rawLines1 = @()
if ($raw1) { $rawLines1 = @($raw1 -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" }) }
Set-Content -LiteralPath $traceBRaw -Value ($rawLines1 -join "`n") -Encoding ASCII
$traceBCount = @($rawLines1 | Where-Object { $_ -match "OMTRACE seq=" }).Count
Write-Output "Trace B (openMSX) instructions captured: $traceBCount"

python tools/analyze/trace-diff.py $traceA $traceBRaw $traceDiff
$subjectExit = $LASTEXITCODE
switch ($subjectExit) {
    0 { Write-Output "RESULT: ARCHITECTURAL + CONTENT PARITY (empty diff, $traceALineCount instructions)" }
    1 { Write-Output "RESULT: DIVERGENCE captured (see $traceDiff)" }
    default { Write-Output "RESULT: BLOCKED / not comparable" }
}

# =====================================================================
# Adversarial comparator self-check (every prior milestone's precedent):
# an empty-side input -> BLOCKED, a corrupted field -> DIVERGENCE.
# =====================================================================
$emptyPath = Join-Path $WorkDir "m19_cartridge_selfcheck_empty.txt"
Set-Content -LiteralPath $emptyPath -Value "" -Encoding ASCII
$selfcheckBlockedOut = Join-Path $WorkDir "m19_cartridge_selfcheck_blocked.md"
python tools/analyze/trace-diff.py $traceA $emptyPath $selfcheckBlockedOut
$selfcheckBlockedExit = $LASTEXITCODE

$corruptedPath = Join-Path $WorkDir "m19_cartridge_selfcheck_corrupted.txt"
$corruptedLines = $rawLines1 -replace "af=0000", "af=DEAD"
Set-Content -LiteralPath $corruptedPath -Value ($corruptedLines -join "`n") -Encoding ASCII
$selfcheckDivergedOut = Join-Path $WorkDir "m19_cartridge_selfcheck_diverged.md"
python tools/analyze/trace-diff.py $traceA $corruptedPath $selfcheckDivergedOut
$selfcheckDivergedExit = $LASTEXITCODE

Write-Output "Self-check: empty-side -> exit $selfcheckBlockedExit (expect 2/BLOCKED); corrupted-field -> exit $selfcheckDivergedExit (expect 1/DIVERGENCE)"

# =====================================================================
# Compose the final artifact.
# =====================================================================
$lines = @()
$lines += "# M19-S6 openMSX A/B Parity Trace Diff -- External Cartridge Slot 1, Sony HB-F1XV"
$lines += ""
$lines += "- Milestone: M19 (cartridge loading, external primary slots 1 & 2), slice S6."
$lines += "- Subject-A emulator: ``sony_msx_headless`` (this repo, Debug build)."
$lines += "- Reference-B emulator: $openmsxVersion (WSL, ``$openmsxPath``)."
$lines += "- Reference machine: ``$ProbeMachine`` -- ``references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:119`` (``<primary external=""true"" slot=""1""/>``), independently re-confirmed against the ACTUAL installed WSL openMSX 19.1 runtime config (``/usr/share/openmsx/machines/Sony_HB-F1XV.xml``, byte-identical primary-slot-1/2 declarations)."
$lines += "- Probe: ``$ProgramBin`` + synthetic cartridge ``$CartridgeRom`` (4 x 8 KB ``Generic8kB`` banks, bank N's every byte == N), generated by ``tools/gen/cartridge-probe.py``."
$lines += ""
$lines += "## Slot-lettering verification (R-M19-6, empirically confirmed, not assumed)"
$lines += ""
$lines += "openMSX's general CLI docs (``-cart``, ``-carta``..``-cartd``) do not by themselves say which lettered slot lands in THIS machine's primary slot 1 vs. 2. Verified live via a WSL Tcl probe BEFORE any slot-specific claim below: two distinguishable synthetic ``Mirrored`` images (marker bytes 0xAA / 0xBB) were mounted via ``-carta``/``-cartb``; ``debug write ioports 0xA8 0xF7`` (routing CPU page 1 to primary slot 1, this project's own SlotBus bit layout) then ``debug read memory 0x4000`` returned ``0xAA`` (the ``-carta`` file's marker); ``debug write ioports 0xA8 0xFB`` (page 1 -> primary slot 2) then the same read returned ``0xBB`` (the ``-cartb`` file's marker). **Conclusion: ``-carta`` = primary slot 1, ``-cartb`` = primary slot 2, for this machine, on this installed openMSX 19.1.** This harness therefore uses ``-carta`` for its primary-slot-1 claim below."
$lines += ""
$lines += "## Result"
$lines += ""
$lines += "Mechanism: this emulator's ``--parity-trace`` (extended M19-S5 to recognize ``--cart1``/``--cart1-type`` via the SAME parser/loader as the default run path) vs. openMSX driven by a chained single-step Tcl script that mounts the IDENTICAL cartridge file via ``-carta -romtype 8kB`` and injects the IDENTICAL driver-program bytes into RAM before single-stepping -- diffed per-instruction (PC, opcode, every register/flag, including AF where the read-back marker bytes land) via ``tools/analyze/trace-diff.py``. Because both sides load an IDENTICALLY-AUTHORED synthetic cartridge, the expected AF values at the three ``LD A,(addr)`` steps (0x00 reset-default bank0, then 0x03 after the bank-switch write, then 0x01 for the unaffected window-slot) are fully known in advance -- this is a genuine CONTENT-bearing comparison, not merely architectural."
$lines += ""
$lines += "- Emulator instructions (A): $traceALineCount"
$lines += "- openMSX instructions (B): $traceBCount"
switch ($subjectExit) {
    0 { $lines += "- **Result: ARCHITECTURAL + CONTENT PARITY -- EMPTY DIFF** over all $traceALineCount instructions (including the read-back marker bytes in AF)." }
    1 { $lines += "- **Result: DIVERGENCE captured.** See ``$traceDiff``." }
    default { $lines += "- **Result: BLOCKED / not comparable.** See ``$traceBRaw``." }
}
$lines += ""
$lines += "Raw traces: ``$traceA`` (emulator), ``$traceBRaw`` (openMSX). Diff: ``$traceDiff``."
$lines += ""
$lines += "## Adversarial comparator self-check"
$lines += ""
$lines += "- Empty-side input -> exit $selfcheckBlockedExit (expected 2 / BLOCKED): $(if ($selfcheckBlockedExit -eq 2) { 'PASS' } else { 'UNEXPECTED' })"
$lines += "- Corrupted-field input (AF forced to 0xDEAD wherever it was 0x0000) -> exit $selfcheckDivergedExit (expected 1 / DIVERGENCE): $(if ($selfcheckDivergedExit -eq 1) { 'PASS' } else { 'UNEXPECTED' })"
$lines += ""
$lines += "## Reproduce"
$lines += ""
$lines += '```powershell'
$lines += "cmake --build build --config Debug"
$lines += "python tools/gen/cartridge-probe.py"
$lines += "tools/openmsx/cartridge-parity.ps1"
$lines += '```'
$lines += ""

$diffOutFull = [System.IO.Path]::GetFullPath($DiffOut)
$diffDir = Split-Path -Parent $diffOutFull
if ($diffDir -and -not (Test-Path -LiteralPath $diffDir)) { New-Item -ItemType Directory -Path $diffDir | Out-Null }
Set-Content -LiteralPath $DiffOut -Value ($lines -join "`n") -Encoding ASCII

Write-Output ""
Write-Output "Artifact written to $DiffOut"
if ($subjectExit -eq 0) {
    exit 0
} elseif ($subjectExit -eq 2) {
    exit 2
} else {
    exit 1
}
