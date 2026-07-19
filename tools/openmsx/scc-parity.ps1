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
    [string]$ProgramBin = "tests/parity/scc_probe.bin",
    [string]$CartridgeRom = "tests/parity/scc.rom",
    [string]$BaseHex = "C000",
    [string]$Emulator = "build/Debug/sony_msx_headless.exe",
    [string]$WorkDir = "build",
    [string]$DiffOut = "docs/m29-parity-trace-diff.md",
    [string]$ProbeMachine = "Sony_HB-F1XV",
    [int]$MaxSteps = 400,
    [int]$BootSeconds = 6
)

# ---------------------------------------------------------------------------
# M29-S6 openMSX A/B parity harness -- KonamiSCC mapper + SCC register window
# (backlog G1, docs/m29-planner-package.md section 2.5).
#
# Register/window/mapper behaviour IS genuinely comparable and is compared
# here via the established M19 trace technique: BOTH sides load the IDENTICAL
# synthetic KonamiSCC image (tools/gen/scc-probe.py) and run the
# IDENTICAL Z80 driver program, whose every readback lands in AF -- an empty
# per-instruction diff (tools/analyze/trace-diff.py) is simultaneously the
# architectural AND the content-bearing proof. Probed facts (see the
# generator's annotated listing): KonamiSCC-specific mirroring (the OPPOSITE
# of plain Konami), 0x800-wide bank-register decode (mid-window write works,
# out-of-window write ignored), the masked (value&0x3F)==0x3F enable compare
# (0x3F AND 0xBF enable, 0x3E disables), the both-effects 0x9000 write, SCC
# wave write/readback, the 0x9900 window mirror (A-M29-1), write-only/dead
# register regions, and the deform-range read-as-write-0xFF side effect
# (driven by a REAL CPU read on both sides -- openMSX's own `debug read`
# would be peek-semantics and would NOT fire it).
#
# A-M29-3 (mapper-type forcing on the reference side) VERIFIED IN SOURCE
# BEFORE RUNNING, per the planner's precondition:
#   - references/openmsx-21.0/src/CartridgeSlotManager.cc:455 -- the carta/
#     cart command help: "-romtype <romtype> : specify the ROM mapper type";
#   - references/openmsx-21.0/src/config/HardwareConfig.cc:60 --
#     valueArg("-romtype", mapper) consumed when building the extension;
#   - references/openmsx-21.0/src/memory/RomInfo.cc:24 -- the canonical name
#     "KonamiSCC";
#   - and the M19 precedent already used `-carta <file> -romtype 8kB` live.
#
# AUDIO-SAMPLE comparison: honestly N/A BY DESIGN (planner section 2.5, recorded
# in advance): openMSX exposes no Tcl introspection of its resampled SCC
# sample stream, and its output chain (ResampledSoundDevice at 3579545/32 Hz
# input rate + host resampler) is architecturally different from this
# project's 81-cycle pump -- a waveform diff would compare RESAMPLER
# POLICIES, not chip semantics. The chip semantics are instead locked by the
# De Schrijder/NYYRIKKI/Pazos-derived unit oracles
# (tests/unit/devices/audio_scc_wavetable_unit_test.cpp).
#
# DEC-0028 note: no bare-`reset` memory-content comparison is performed
# anywhere here; the trace technique uses a fresh-boot session per side with
# register forcing (the M11-M28 established mechanism).
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path)) { throw "$Label not found: $Path" }
}

Require-Path -Path $Emulator -Label "emulator binary (build first)"
Require-Path -Path $ProgramBin -Label "M29 SCC probe program (run tools/gen/scc-probe.py first)"
Require-Path -Path $CartridgeRom -Label "M29 synthetic KonamiSCC image (run tools/gen/scc-probe.py first)"

if (-not (Test-Path -LiteralPath $WorkDir)) { New-Item -ItemType Directory -Path $WorkDir | Out-Null }

$openmsxPath = (wsl -e bash -lc "command -v openmsx || true").Trim()
if ([string]::IsNullOrWhiteSpace($openmsxPath)) { throw "openMSX not found in WSL PATH" }
$openmsxVersion = (wsl -e bash -lc "openmsx --version 2>/dev/null || true").Trim()
Write-Output "openMSX: $openmsxPath ($openmsxVersion), machine: $ProbeMachine"

$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $ProgramBin))
$byteList = ($bytes | ForEach-Object { "0x{0:X2}" -f $_ }) -join " "
$base = "0x$BaseHex"

# =====================================================================
# Subject A (this emulator): --parity-trace + --cart1-type KonamiSCC (the
# M29-S1 CLI value, same parser/loader as the default run path).
# =====================================================================
$traceA = Join-Path $WorkDir "m29_scc_trace_A.txt"
$traceBRaw = Join-Path $WorkDir "m29_scc_trace_B.txt"
$traceDiff = Join-Path $WorkDir "m29_scc_trace_diff.md"

& $Emulator --parity-trace $ProgramBin $BaseHex $MaxSteps $traceA --cart1 $CartridgeRom --cart1-type KonamiSCC
if ($LASTEXITCODE -ne 0) { throw "emulator parity-trace failed (exit $LASTEXITCODE)" }
$traceALineCount = (Get-Content -LiteralPath $traceA).Count
Write-Output "Trace A (emulator) instructions: $traceALineCount"

# =====================================================================
# Subject B (openMSX, WSL): mount the SAME file via -carta -romtype
# KonamiSCC (-carta = primary slot 1 for this machine, empirically confirmed
# in M19's slot-lettering probe), inject the SAME driver bytes into RAM,
# chained-single-step (the established live-Tcl technique).
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
$tclPathWsl = "/tmp/omx_m29_scc_trace.tcl"
$runCmd1 = "echo $encoded1 | base64 -d > $tclPathWsl && " +
           "timeout 180 openmsx -machine $ProbeMachine -carta '$cartRomWsl' -romtype KonamiSCC " +
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
# Adversarial comparator self-check (established precedent): an empty-side
# input -> BLOCKED, a corrupted field -> DIVERGENCE.
# =====================================================================
$emptyPath = Join-Path $WorkDir "m29_scc_selfcheck_empty.txt"
Set-Content -LiteralPath $emptyPath -Value "" -Encoding ASCII
$selfcheckBlockedOut = Join-Path $WorkDir "m29_scc_selfcheck_blocked.md"
python tools/analyze/trace-diff.py $traceA $emptyPath $selfcheckBlockedOut
$selfcheckBlockedExit = $LASTEXITCODE

$corruptedPath = Join-Path $WorkDir "m29_scc_selfcheck_corrupted.txt"
$corruptedLines = $rawLines1 -replace "af=5500", "af=DEAD"
Set-Content -LiteralPath $corruptedPath -Value ($corruptedLines -join "`n") -Encoding ASCII
$selfcheckDivergedOut = Join-Path $WorkDir "m29_scc_selfcheck_diverged.md"
python tools/analyze/trace-diff.py $traceA $corruptedPath $selfcheckDivergedOut
$selfcheckDivergedExit = $LASTEXITCODE

Write-Output "Self-check: empty-side -> exit $selfcheckBlockedExit (expect 2/BLOCKED); corrupted-field -> exit $selfcheckDivergedExit (expect 1/DIVERGENCE)"

# =====================================================================
# Compose the final artifact.
# =====================================================================
$lines = @()
$lines += "# M29-S6 openMSX A/B Parity Trace Diff -- KonamiSCC Mapper + SCC Register Window"
$lines += ""
$lines += "- Milestone: M29 (KonamiSCC mapper + SCC wavetable chip, backlog G1), slice S6."
$lines += "- Subject-A emulator: ``sony_msx_headless`` (this repo, Debug build)."
$lines += "- Reference-B emulator: $openmsxVersion (WSL, ``$openmsxPath``)."
$lines += "- Reference machine: ``$ProbeMachine``; cartridge mounted via ``-carta <file> -romtype KonamiSCC``."
$lines += "- Probe: ``$ProgramBin`` + synthetic KonamiSCC image ``$CartridgeRom`` (8 x 8 KB banks, bank N's every byte == N), generated by ``tools/gen/scc-probe.py`` (annotated instruction listing + expected readbacks in its docstring)."
$lines += ""
$lines += "## A-M29-3 precondition (mapper-type forcing) -- VERIFIED IN SOURCE FIRST"
$lines += ""
$lines += "- ``references/openmsx-21.0/src/CartridgeSlotManager.cc:455`` -- cart-command help text: ``-romtype <romtype> : specify the ROM mapper type``."
$lines += "- ``references/openmsx-21.0/src/config/HardwareConfig.cc:60`` -- ``valueArg(`"-romtype`", mapper)`` consumed when the extension config is built."
$lines += "- ``references/openmsx-21.0/src/memory/RomInfo.cc:24`` -- the canonical mapper name ``KonamiSCC``."
$lines += "- M19 precedent: ``-carta <file> -romtype 8kB`` already used live; ``-carta`` = primary slot 1 for this machine (M19's empirically-confirmed slot-lettering probe, docs/m19-parity-trace-diff.md)."
$lines += ""
$lines += "## Probed facts (each lands in AF and is covered by the trace diff)"
$lines += ""
$lines += "1. KonamiSCC-specific mirroring, BOTH regions (0x0000 tracks page 4, 0x2000 tracks page 5) -- the OPPOSITE of plain Konami (R-M29-1)."
$lines += "2. 0x800-wide bank-register decode: mid-window 0x5400 write switches; 0x5800 write ignored (fact-sheet s9.7)."
$lines += "3. Both-effects 0x9000 write: SCC enable state AND page-4 bank change on the same write."
$lines += "4. Masked enable compare (A-M29-2 / fact-sheet s9.6): 0x3F enables, 0x3E disables, 0xBF ALSO enables."
$lines += "5. SCC wave write/readback at 0x9800-0x981F; the 0x9900 window mirror (A-M29-1 / fact-sheet s9.5 -- fMSX would serve ROM there); a write via the mirror landing in the base map."
$lines += "6. Write-only freq/vol block (reads 0xFF) and the dead 0xA0-0xDF region (0xFF)."
$lines += "7. Deform-range READ acts as write 0xFF (fired via a REAL CPU read on both sides -- openMSX ``debug read`` is peek-semantics and could NOT probe this, hence the in-trace LD A,(0x9FE0)); its read-only side effect confirmed by a blocked wave write. The subsequent wave readback uses a deliberately UNIFORM waveform so it is rotation-shift-independent (timing-safe on both sides)."
$lines += ""
$lines += "BIOS-boot insensitivity: the driver normalizes all four bank registers (with a disabling 0x9000 value) BEFORE any sensitive readback, so nothing depends on reset-default bank state surviving the reference side's real BIOS boot (whose RAM search may write into the cartridge's 0x8000-0xBFFF decode windows)."
$lines += ""
$lines += "## Result"
$lines += ""
$lines += "- Emulator instructions (A): $traceALineCount"
$lines += "- openMSX instructions (B): $traceBCount"
switch ($subjectExit) {
    0 { $lines += "- **Result: ARCHITECTURAL + CONTENT PARITY -- EMPTY DIFF** over all $traceALineCount instructions (every probed fact above included via its AF-visible readback)." }
    1 { $lines += "- **Result: DIVERGENCE captured.** See ``$traceDiff``." }
    default { $lines += "- **Result: BLOCKED / not comparable.** See ``$traceBRaw``." }
}
$lines += ""
$lines += "Raw traces: ``$traceA`` (emulator), ``$traceBRaw`` (openMSX). Diff: ``$traceDiff``."
$lines += ""
$lines += "## Adversarial comparator self-check"
$lines += ""
$lines += "- Empty-side input -> exit $selfcheckBlockedExit (expected 2 / BLOCKED): $(if ($selfcheckBlockedExit -eq 2) { 'PASS' } else { 'UNEXPECTED' })"
$lines += "- Corrupted-field input (AF 0x5500 forced to 0xDEAD) -> exit $selfcheckDivergedExit (expected 1 / DIVERGENCE): $(if ($selfcheckDivergedExit -eq 1) { 'PASS' } else { 'UNEXPECTED' })"
$lines += ""
$lines += "## Audio-sample comparison: N/A by design (planner section 2.5, recorded in advance)"
$lines += ""
$lines += "openMSX exposes no Tcl introspection of its resampled SCC sample stream, and its output chain (ResampledSoundDevice at 3579545/32 Hz input rate + host resampler) is architecturally different from this project's 81-cycle pump; a waveform diff would compare resampler policies, not chip semantics. The chip semantics are instead locked by the De Schrijder/NYYRIKKI/Pazos-derived unit oracles (tests/unit/devices/audio_scc_wavetable_unit_test.cpp -- including the literal amp_out()==640 measurement reproduction). This disposition follows the E2/M28 'N/A by design, mechanism proven via cited constants' precedent."
$lines += ""
$lines += "## Reproduce"
$lines += ""
$lines += '```powershell'
$lines += "cmake --build build --config Debug"
$lines += "python tools/gen/scc-probe.py"
$lines += "tools/openmsx/scc-parity.ps1"
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
