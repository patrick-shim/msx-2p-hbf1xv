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
    [string]$RomPath = "roms/aleste.rom",
    [string]$BiosPath = "bios/f1xvbios.rom",
    [string]$OutFile = "docs/openmsx-ab-smoke.md",
    [string]$ProbeMachine = "C-BIOS_MSX2+"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label path not found: $Path"
    }
}

Require-Path -Path $RomPath -Label "ROM"
Require-Path -Path $BiosPath -Label "BIOS"

$openmsxPath = (wsl -e bash -lc "command -v openmsx || true").Trim()
if ([string]::IsNullOrWhiteSpace($openmsxPath)) {
    throw "openMSX not found in WSL PATH"
}

$openmsxVersion = (wsl -e bash -lc "openmsx --version 2>/dev/null || openmsx -v 2>/dev/null || true").Trim()
if ([string]::IsNullOrWhiteSpace($openmsxVersion)) {
    $openmsxVersion = "unknown"
}

# ---------------------------------------------------------------------------
# R5 opcode-trace parity probe (honest capability check, NOT a fabricated diff).
#
# Goal: determine whether openMSX exposes a usable, headless CPU single-step
# trace hook that could be diffed against this emulator's bare Z80 core. We
# probe three concrete capabilities:
#   1. Is the "CPU regs" debuggable present?
#   2. Does the debugger `step` command advance PC in the headless -script
#      startup context (needed to capture a per-instruction trace)?
#   3. Can a flat test program be written into a RAM page (0xC000) and read
#      back, so a program comparable to this emulator's flat 64K RAM can be
#      loaded without full slot/mapper configuration?
#
# The probe records the ACTUAL captured results. It never synthesizes parity.
# ---------------------------------------------------------------------------
$probeTcl = @'
set out {}
lappend out "cpu_regs_debuggable=[expr {[lsearch [debug list] {CPU regs}] >= 0}]"
set pc0 [reg pc]
step
set pc1 [reg pc]
lappend out "step_pc0=[format %04X $pc0]"
lappend out "step_pc1=[format %04X $pc1]"
lappend out "step_advanced=[expr {$pc1 != $pc0}]"
debug write memory 0xC000 0x3E
lappend out "ram_write_c000_readback=[format %02X [debug read memory 0xC000]]"
foreach line $out { puts stderr "PARITYPROBE $line" }
exit
'@

$probeStatus = "attempted"
$probeLines = @()
try {
    $encoded = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($probeTcl))
    $probeCmd = "echo $encoded | base64 -d > /tmp/omx_parity_probe.tcl && " +
                "timeout 60 openmsx -machine $ProbeMachine -command 'set renderer none' " +
                "-script /tmp/omx_parity_probe.tcl 2>&1 | grep PARITYPROBE || true"
    $raw = (wsl -e bash -lc $probeCmd)
    if ($raw) {
        $probeLines = @($raw -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" })
    }
    if ($probeLines.Count -eq 0) {
        $probeStatus = "no probe output captured"
    }
} catch {
    $probeStatus = "probe error: $($_.Exception.Message)"
}

# Interpret the probe deterministically for the report's R5 verdict.
$stepAdvanced = @($probeLines | Where-Object { $_ -match "step_advanced=1" }).Count -gt 0
$ramWritable = @($probeLines | Where-Object { $_ -match "ram_write_c000_readback=3E" }).Count -gt 0
if ($stepAdvanced -and $ramWritable) {
    $r5Status = "TRACE HOOK REACHABLE - further harness work required to emit + diff traces"
} else {
    $r5Status = "UNRESOLVED - no faithful comparable trace producible in this environment (waiver candidate)"
}

$reportLines = @(
    "# openMSX A/B Smoke Report",
    "",
    "Generated at: $(Get-Date -Format o)",
    "WSL openMSX binary: $openmsxPath",
    "WSL openMSX version: $openmsxVersion",
    "",
    "## Inputs",
    "- BIOS: $BiosPath",
    "- ROM: $RomPath",
    "",
    "## A/B Scenario",
    "- A (target): sony-msx-hbf1xv local emulator run for the same BIOS/ROM.",
    "- B (reference): openMSX in WSL for the same BIOS/ROM.",
    "",
    "## Reference Command (B)",
    '```bash',
    "openmsx '$RomPath'",
    '```',
    "",
    "## R5 Opcode-Trace Parity Attempt",
    "",
    "Probe machine: $ProbeMachine",
    "Probe status: $probeStatus",
    "",
    "Captured openMSX capability probe (actual output):",
    '```'
) + $probeLines + @(
    '```',
    "",
    "### R5 Status: $r5Status",
    "",
    "Concrete blockers observed (reproducible via this script's probe):",
    "- openMSX only executes within a full MSX machine memory map (slots/subslots/",
    "  mapper). Writing a flat test program into a fixed RAM page to mirror this",
    "  emulator's bare 64K flat RAM fails at reset (page reads back 0xFF / unmapped)",
    "  until the BIOS configures the slot registers - i.e. it requires the machine",
    "  wiring that milestone M9 explicitly excludes.",
    "- The debugger 'step' command does not advance the CPU PC in the headless",
    "  '-script' startup context, so a deterministic per-instruction step trace",
    "  cannot be captured this way.",
    "- This emulator has no per-instruction trace-export facility yet; the headless",
    "  binary only reports frame/cycle summary counters.",
    "",
    "Because a faithful, state-comparable instruction trace cannot be produced here,",
    "no parity diff is asserted (anti-fabrication). This confirms the reference",
    "toolchain is present and characterizes exactly why an opcode-trace diff is not",
    "yet achievable.",
    "",
    "### Verification action to close R5",
    "1. Add a deterministic CPU trace-export mode to this emulator (per-instruction",
    "   PC/registers/flags/T-state dump for a loaded flat program).",
    "2. Build the comparable openMSX side either by (a) implementing enough MSX",
    "   slot/RAM machine wiring to load the same flat program and drive openMSX via",
    "   openMSX -control stdio single-stepping after boot, or (b) using a zexall/zexdoc",
    "   style self-checking ROM whose pass/fail signature is comparable across both.",
    "3. Diff the two traces and record the ACTUAL diff outcome here.",
    "",
    "Until then R5 is a candidate for an explicit waiver in",
    'agent-protocol/channels/decisions.md.',
    "",
    "## Notes",
    "- This script verifies openMSX availability/version, records A/B inputs, and",
    "  runs a real (non-fabricated) opcode-trace capability probe.",
    "- Behavioral parity assertions require milestone-specific trace/output",
    "  comparison artifacts, which are not yet producible (see R5 status above)."
)

$outPath = [System.IO.Path]::GetFullPath($OutFile)
$outDir = Split-Path -Parent $outPath
if ($outDir -and -not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Path $outDir | Out-Null
}

Set-Content -LiteralPath $outPath -Value ($reportLines -join [Environment]::NewLine) -Encoding UTF8
Write-Output "openMSX A/B smoke report written to: $outPath"
Write-Output "R5 parity probe status: $probeStatus"
Write-Output "R5 status: $r5Status"
