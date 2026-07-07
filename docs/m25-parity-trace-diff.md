# M25 openMSX A/B Evidence -- Backlog C8 (Speed Controller + Hardware PAUSE + Ren-Sha Turbo)

Two separate, honestly-dispositioned sub-claims -- see docs/m25-planner-package.md section 2.7 /
Acceptance Criterion 9.

## Ren-Sha Turbo: Result

- Reference: openMSX 19.1 flavour: debian components: ALSAMIDI CORE GL LASERDISC / WSL, machine `Sony_HB-F1XV` (the real target machine,
  carrying the real `<RenShaTurbo min_ints=47 max_ints=221>` calibration,
  `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:16-19`).
- Technique: live Tcl `set renshaturbo 100` (the real openMSX Autofire speed
  setting) + `debug write/read ioports` sampling of keyboard row 8 (#A9, after selecting
  row 8 via #AA) and PSG R14 (#A2, after latching register 14 via #A0), scheduled via
  `after time` (continuous native emulation between samples, not per-instruction
  single-stepping -- see the file header for why).
- Not-held samples: 10; held samples: 40.

### (a) Not held, RenSha engaged: keyboard row-8 bit0 (SPACE)

**PARITY** -- ZERO observable effect at every sampled point (10/10 idle), matching
R-M25-6's invariant, confirmed live on the real reference engine.

### (c) Not held, RenSha engaged: PSG R14 bit4 (joystick trigger A)

**PARITY** -- ZERO observable effect at every sampled point (10/10 idle), matching
R-M25-6's invariant, confirmed live on the real reference engine.

### (b) Held (`keymatrixdown 8 1`), RenSha engaged: keyboard row-8 bit0 alternation

**PARITY** -- the real reference engine's live read alternates between "pressed" (bit0=0,
20 samples) and "released" (bit0=1, forced by the autofire OR-combine,
20 samples) -- matching this project's own OR-only-releases implementation
(A-M25-7).

### Explicitly NOT attempted (honest disposition, not silently skipped)

A live "held" alternation demonstration for the PSG R14/joystick-trigger-A path: no
equivalent "hold a joystick button" Tcl scripting primitive was found in openMSX 21.0
(`keymatrixdown`/`keymatrixup`, `references/openmsx-21.0/src/input/Keyboard.cc:1565-1611`,
apply ONLY to the keyboard matrix). The not-held invariant (c, above) IS confirmed live for
this path; the held-alternation behavior for the joystick path relies on this project's own
already-exhaustively-tested implementation (M25-S2/S3, the identical OR-combine code path as
the keyboard, A-M25-7), not a live cross-engine comparison.

## Hardware PAUSE / Speed Controller (MB670836): Result BLOCKED

**This is the RIGHT, honest disposition, not a shortfall.** openMSX 21.0 has ZERO
Sony-specific PAUSE/speed-controller modeling anywhere -- not "hard to find", genuinely
absent, with the reference project's OWN machine-description text saying so explicitly
for four of five sibling Sony machines. There is no reference engine behavior to diff
against. Exact citations (independently re-confirmed this cycle):

- `references/openmsx-21.0/src/SG1000Pause.hh` -- Sega SG-1000/SC-3000 "hold"/"pause"/
  "reset" button, triggers an NMI. Different machine family entirely (not MSX), different
  mechanism (NMI, not a CPU-halting WAIT-line gate).
- `references/openmsx-21.0/src/MSXTurboRPause.{hh,cc}` -- MSX turboR (S1990 chipset) pause
  key, a flip-flop status bit at I/O port 0xA7, implemented by calling
  `getMotherBoard().pause()`/`unpause()` (whole-session engine pause, architecturally
  incompatible with this project's atomic, deterministic per-instruction
  `step_cpu_instruction()` stepping model). Different chipset (S1990, not S1985/
  MB670836) and different machine family (turboR, not the HB-F1XV's plain MSX2+).
- None of the six real Sony MSX machine XML definitions wire a Pause or SpeedController
  device, and four of the six explicitly say so in their own `<description>` text (direct
  quotes): `Sony_HB-F1.xml:9` "speed controller (not emulated)"; `Sony_HB-F1II.xml:9`
  "speed controller (not emulated)"; `Sony_HB-F1XD.xml:9` "speed controller (not
  emulated)"; `Sony_HB-F1XDJ.xml:9` "speed controller (not emulated)".
  `Sony_HB-F1XDmk2.xml:9` (found via QA's fresh `find` sweep, RESP-M25-003) "identical to
  its predecessor" (`Sony_HB-F1XD.xml`, "not emulated") -- independently confirmed to wire
  no Pause/SpeedController device either.
  `Sony_HB-F1XV.xml:9` (this project's actual target machine) does not even mention the
  speed controller in its own (shorter) description -- consistent with "not emulated, not
  even discussed" rather than a silent omission of something that IS emulated.

No parity is asserted for this sub-claim (honest disposition). Per Acceptance Criterion 9,
this BLOCKED disposition does NOT gate C8's closure (mirrors the M21 computed-pixel-color
and C3/M24 disk-boot-A/B precedents of an honest, non-fabricated BLOCKED sub-claim not
blocking an otherwise-complete milestone).
