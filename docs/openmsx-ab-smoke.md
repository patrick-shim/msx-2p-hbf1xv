# openMSX A/B Smoke Report

> R5 UPDATE (M10-S4, 2026-07-06): The opcode-trace parity blocker described in the
> R5 section below is now RESOLVED by the dedicated harness
> `tools/openmsx-trace-parity.ps1`. It boots openMSX to a RAM-ready state, forces
> the documented initial CPU vector, writes the committed RAM-only parity program
> into mapped page-3 RAM, and single-steps via chained `debug break` / `after break`
> callbacks (the mechanism that makes `step` actually advance PC — the old blocker
> was stepping in the un-broken `-script` startup context). The REAL captured
> per-instruction architectural-state diff (EMPTY over all 48 instructions to the
> HALT checkpoint) is the authoritative artifact: `docs/m10-parity-trace-diff.md`.
> The capability probe below remains as the honest record of the original blocker.

Generated at: 2026-07-09T19:02:15.5824614+09:00
WSL openMSX binary: /usr/bin/openmsx
WSL openMSX version: openMSX 19.1 flavour: debian components: ALSAMIDI CORE GL LASERDISC

## Inputs
- BIOS: bios/f1xvbios.rom
- ROM: roms/aleste.rom

## A/B Scenario
- A (target): sony-msx-hbf1xv local emulator run for the same BIOS/ROM.
- B (reference): openMSX in WSL for the same BIOS/ROM.

## Reference Command (B)
```bash
openmsx 'roms/aleste.rom'
```

## R5 Opcode-Trace Parity Attempt

Probe machine: C-BIOS_MSX2+
Probe status: attempted

Captured openMSX capability probe (actual output):
```
PARITYPROBE cpu_regs_debuggable=1
PARITYPROBE step_pc0=0000
PARITYPROBE step_pc1=0000
PARITYPROBE step_advanced=0
PARITYPROBE ram_write_c000_readback=FF
```

### R5 Status: UNRESOLVED - no faithful comparable trace producible in this environment (waiver candidate)

Concrete blockers observed (reproducible via this script's probe):
- openMSX only executes within a full MSX machine memory map (slots/subslots/
  mapper). Writing a flat test program into a fixed RAM page to mirror this
  emulator's bare 64K flat RAM fails at reset (page reads back 0xFF / unmapped)
  until the BIOS configures the slot registers - i.e. it requires the machine
  wiring that milestone M9 explicitly excludes.
- The debugger 'step' command does not advance the CPU PC in the headless
  '-script' startup context, so a deterministic per-instruction step trace
  cannot be captured this way.
- This emulator has no per-instruction trace-export facility yet; the headless
  binary only reports frame/cycle summary counters.

Because a faithful, state-comparable instruction trace cannot be produced here,
no parity diff is asserted (anti-fabrication). This confirms the reference
toolchain is present and characterizes exactly why an opcode-trace diff is not
yet achievable.

### Verification action to close R5
1. Add a deterministic CPU trace-export mode to this emulator (per-instruction
   PC/registers/flags/T-state dump for a loaded flat program).
2. Build the comparable openMSX side either by (a) implementing enough MSX
   slot/RAM machine wiring to load the same flat program and drive openMSX via
   openMSX -control stdio single-stepping after boot, or (b) using a zexall/zexdoc
   style self-checking ROM whose pass/fail signature is comparable across both.
3. Diff the two traces and record the ACTUAL diff outcome here.

Until then R5 is a candidate for an explicit waiver in
agent-protocol/channels/decisions.md.

## Notes
- This script verifies openMSX availability/version, records A/B inputs, and
  runs a real (non-fabricated) opcode-trace capability probe.
- Behavioral parity assertions require milestone-specific trace/output
  comparison artifacts, which are not yet producible (see R5 status above).
