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
    [string]$ProgramBin = "tests/parity/z80_parity_checkpoint.bin",
    [string]$BaseHex = "C000",
    [string]$Emulator = "build/Debug/sony_msx_headless.exe",
    [string]$TraceA = "build/m12_cpu_parity_trace_A.txt",
    [string]$TraceBRaw = "build/m12_cpu_parity_trace_B.txt",
    [string]$DiffOut = "docs/m12-parity-trace-diff.md",
    [string]$ProbeMachine = "C-BIOS_MSX2+",
    [int]$MaxSteps = 200,
    [int]$BootSeconds = 6
)

# ---------------------------------------------------------------------------
# M12-S6 openMSX per-instruction CPU-parity harness (extends
# tools/openmsx-trace-parity.ps1). Produces a REAL captured architectural-state
# trace diff between:
#   A = this emulator (M10-S1 trace-export, --parity-trace mode), and
#   B = openMSX 19.1 on WSL, single-stepped over the SAME RAM-only program.
#
# It NEVER fabricates parity. It reuses the proven trace-parity mechanism and
# then annotates the resulting artifact with the M12 benign-boundary notes that
# are INTENTIONALLY excluded from the architectural field gate:
#
#   A-2 (openMSX version): B is openMSX 19.1 while the grounding tree is 21.0;
#        documented Z80 behavior is stable across these. Recorded at run.
#   A-3 (WZ / MEMPTR): openMSX's `reg` interface does not expose WZ, and this
#        emulator's trace does not export it either, so WZ is structurally
#        absent from the diff. WZ correctness is proven by the S3 unit tests
#        (z80a_wz_memptr) + the S6 integration test (BIT n,(HL) X/Y from WZ hi).
#   A-4 (SCF/CCF X/Y): a CORRECT genuine-Zilog NMOS Q-latch (X/Y = ((Q^F)|A))
#        legitimately DIVERGES from openMSX 21.0/19.1's (F|A) OR-form in the
#        "previous instruction modified flags" case. This is gated on the
#        fact-sheet Q rule + the S4 unit tests (z80a_scf_ccf_q), NOT on this
#        trace-diff. The parity program deliberately avoids SCF/CCF so the
#        architectural gate stays meaningful; do NOT "fix" the Q-latch to match
#        openMSX.
#   Cycle/T-state (DP-4 precedent): openMSX inserts MSX M1 waits; this core
#        reports canonical Z80 T-states. Timing parity is proven by the machine
#        oracles (M11 §4 + the S6 integration cycle assertions), not the diff.
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Delegate the actual A/B capture to the proven M10 harness.
& "$PSScriptRoot/openmsx-trace-parity.ps1" `
    -ProgramBin $ProgramBin `
    -BaseHex $BaseHex `
    -Emulator $Emulator `
    -TraceA $TraceA `
    -TraceBRaw $TraceBRaw `
    -DiffOut $DiffOut `
    -ProbeMachine $ProbeMachine `
    -MaxSteps $MaxSteps `
    -BootSeconds $BootSeconds
$harnessExit = $LASTEXITCODE

# Append the M12 benign-boundary annotations to the captured artifact so the
# excluded boundaries are stated in the artifact (never silently dropped).
if (Test-Path -LiteralPath $DiffOut) {
    $note = @"

---

## M12-S6 CPU-parity boundaries (excluded from the architectural field gate)

This diff was produced by ``tools/openmsx-cpu-parity.ps1`` (extends
``tools/openmsx-trace-parity.ps1``) over ``$ProgramBin`` at base 0x$BaseHex.

The following boundaries are INTENTIONALLY out of the field-diff and are proven
elsewhere (planner A-2/A-3/A-4, R-2):

- **WZ / MEMPTR (A-3):** not exposed by openMSX ``reg`` and not exported by this
  emulator's trace; structurally absent. Proven by ``z80a_wz_memptr_unit_test``
  and the ``BIT n,(HL)`` X/Y path in ``hbf1xv_cpu_parity_integration_test``.
- **SCF/CCF undocumented X/Y (A-4):** genuine-Zilog NMOS Q-latch
  ``X/Y = ((Q^F)|A)`` legitimately diverges from openMSX's ``(F|A)`` OR-form.
  Gated on the fact-sheet Q rule + ``z80a_scf_ccf_q_unit_test``, NOT this diff.
  The parity program avoids SCF/CCF so the gate stays meaningful.
- **Cycle / T-states (DP-4):** openMSX inserts MSX M1 waits; this core reports
  canonical Z80 T-states. Timing parity is proven by the machine oracles
  (M11 §4 and ``hbf1xv_cpu_parity_integration_test`` cycle assertions).

If openMSX could not be driven, the section above records BLOCKED (empty B side)
and no parity is asserted — an honest outcome, never a fabricated pass.
"@
    Add-Content -LiteralPath $DiffOut -Value $note -Encoding ASCII
}

exit $harnessExit
