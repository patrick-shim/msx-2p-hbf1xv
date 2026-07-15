# M41 — Final System-Wide A/B Production-QA Suite (Planner Package)

Status: PLANNING (read-only spec). No production code implemented by this package.
Spec owner: MSX Planner Agent. Target tag: production-QA release gate (post-v1.0.38).
Deliverable class: durable planner package (survives handoff). Local artifact; `docs/` is
gitignored on the export remote (DEC-0044), so this file may stay local.

This is the release-gate build brief the human asked for: exercise the WHOLE Sony HB-F1XV
through its real ROM/BIOS paths (BIOS + SUB-ROM + DISK ROM + FM-BIOS + VDP + PSG + OPLL + FDC)
and A/B it against the genuine `Sony_HB-F1XV` openMSX machine for a per-subsystem parity matrix.

---

## 1. Scope, Assumptions, Non-Goals

### 1.1 In scope
- A deterministic, regression-able A/B suite that drives the emulator through **guest programs**
  (MSX-BASIC / Disk-BASIC self-running from disk images; cart-ROM fallback where BASIC can't
  reach) and compares the settled result against openMSX for eight subsystems: keyboard,
  joystick, PSG, FM/OPLL, BASIC execution, screen modes, sprites, disk write/read via BASIC.
- Four small **enabler slices** (§5): the `JOY=` input-script verb, a deterministic FAT12
  BASIC-file injector, a spectral/RMS audio A/B comparator, and a generalized dual-emulator
  capture/compare harness.
- A per-behaviour **PASS taxonomy** that measures and reports the known deltas rather than hiding
  them (§4, §6).

### 1.2 Out of scope (unless a `decisions.md` entry approves it)
- Any `src/devices/cpu/` or `src/core/` change (→ ZEXALL/ZEXDOC withheld per
  `feedback_slow_test_cadence.md`; git-diff-proven empty is an acceptance item).
- Any device-behaviour change. M41 is a **measurement** milestone. If an A/B reveals a real
  defect, that defect is triaged into a *separate* fix milestone (planner-first), not patched
  inside M41 — mirrors the M32/M34 "escalate, never rebaseline" discipline.
- New game-specific fixtures. Every guest program is a generic MSX-BASIC/assembler probe.
- Closing the license-sensitive deltas (FM attack table, VDP slot-timing tables). Those are
  reported as named, permanent known-deltas (§6), never fabricated to bit-exact.

### 1.3 The only production-code touch
The **`JOY=` input-script verb** (§5.1) is the single production-code change: additive to
`src/machine/input_script.{h,cpp}` + threading through `src/main.cpp` `--debug-session` (and,
optionally, the SDL3 live path). It is additive/default-off (scripts with no `JOY=` line parse
byte-identically), touches zero cpu/core/devices files, and follows the M27 `--input-script`
additive precedent. Everything else is `tools/` (Python/PowerShell) + disk/cart assets.
**Kickoff requires a `agent-protocol/channels/decisions.md` entry** authorizing the M41
milestone and the JOY-verb src touch.

### 1.4 Assumptions (each with a verification action)
- **A-1 (openMSX runtime version).** The A/B oracle is openMSX **19.1** on WSL
  (`/usr/bin/openmsx`), machine `Sony_HB-F1XV`. Source grounding is `references/openmsx-21.0/`.
  *Verify:* `wsl -- openmsx -v` at Phase-0; record the exact version string in the report
  header. Flag any behaviour where 19.1↔21.0 could differ (e.g. VDP timing edge cases).
- **A-2 (AUTOEXEC.BAS auto-run).** Disk-BASIC auto-loads and runs `AUTOEXEC.BAS` at cold boot
  when the inserted disk is non-DOS (no `MSXDOS.SYS`). *Verify:* Phase-0 smoke — inject a
  one-line `AUTOEXEC.BAS` (`10 SCREEN2:CIRCLE(128,96),80`) and confirm both emulators reach the
  drawn circle unattended. If it does NOT auto-run, fall back to the typed `RUN"PROG.BAS"`
  path (§3.2, fallback B).
- **A-3 (`screenshot -raw` geometry).** openMSX `screenshot -raw` yields 320×240 for 256-wide
  modes (active crop offset 32,24 @192 / 32,14 @212, already established by M38) and 640×480 for
  512-wide modes. *Verify:* Phase-0 — dump one frame per width class and confirm dimensions +
  the per-mode crop offset that lands `pct_mismatch≈0` on a baseline.
- **A-4 (openMSX headless audio).** `record start -audioonly` produces a mono 16-bit 44.1 kHz
  WAV with no host audio device (VERIFIED for 19.1/WSL in `tools/openmsx-m34-aleste-audio-ab.ps1`).
  *Verify:* re-confirm at Phase-0 with one PSG tone.
- **A-5 (openMSX scriptable joystick injection).** openMSX can inject joystick direction/trigger
  state headlessly (candidate: `plug joyporta keyjoystick` + `keymatrixdown/up` on the mapped
  keys, or a debugger surface). *Verify:* Phase-0 — probe `plug`, `keyjoystick`, and any
  `joystick`-related Tcl on 19.1. If unscriptable, the joystick side-B is honestly **BLOCKED**
  (our-side determinism + real-hardware-doc grounding only), an accepted disposition mirroring
  M25 hardware-PAUSE.
- **A-6 (disk writes persist).** Our side persists SAVE via `--disk-writable`
  (M36-S-c / M37); openMSX writes back to `-diska` images by default. *Verify:* Phase-0 —
  SAVE then re-read the host `.dsk` with the injector's reader on both sides.

---

## 2. Grounding & environment

| Concern | Grounding (read, not assumed) |
|---|---|
| Input-script format + player | `src/machine/input_script.h` (format tag `HBF1XV-INPUT-SCRIPT v1`, `T=<tstate> KEY=<name> DOWN/UP`, `[END]`; `InputScriptPlayer::apply_due(tstate, KeyboardMatrix&)`) |
| Key-name ↔ (row,col) table | `src/peripherals/msx_key_names.cpp` (72 entries; **RCTRL at row2,col0 = the `:`/`*` key**, msx_key_names.cpp:57-59 — the "new Right-Ctrl :/*" the brief calls out) |
| Joystick device API | `src/peripherals/joystick.h` (`JoystickPorts::set_port(int index, PortState{up,down,left,right,trigger_a,trigger_b})`; read via PSG R14, selected via R15 bit6) |
| Headless CLI | `src/main.cpp` `--debug-session <bios> <max> [--disk <p>]* [--cart1 <p>] [--input-script <p>] [--frames <N>] [--dump-frame <name>] [--disk-writable] [--fmpac-sram <p>] [--audio-dump <name>] [--audio-sync] [--audio-dump-psg-only]` (main.cpp:826-905,1542-1566) |
| Frame → PNG | `tools/frame-to-png.py` (decodes `HBF1XV-FRAME-DUMP v1` RGB555→888) |
| Audio dump → WAV | `tools/audio-dump-to-wav.py` (decodes `HBF1XV-AUDIO-DUMP v1`); `--audio-dump` mixes the FULL machine (PSG+SCC+FM+FM-PAC+click), `--audio-sync` = sub-frame accuracy, `--audio-dump-psg-only` isolates PSG |
| Video A/B differ | `tools/m38-ab-diff.py` (crop openMSX active area, mean_dist/ch + pct_mismatch, tol 40/ch absorbs RGB555→888 ~1.6/ch; verdict MATCH if pct<0.5%) |
| openMSX video capture pattern | `tools/m38-run.ps1` (`after time N { screenshot -raw ...; exit }`, WSL, `-machine Sony_HB-F1XV -cart <rom>`) |
| openMSX audio + matrix injection | `tools/openmsx-m34-aleste-audio-ab.ps1` (`set throttle off`, `keymatrixdown/up <row> <mask>`, `record start -audioonly` … `record stop`; openMSX 19.1/WSL) |
| Cart-ROM guest pattern | `tools/gen-m38-vdp-scroll-cart.py` (hand-encoded Z80 `Asm`, `AB` header + INIT autostart, deterministic `--self-check`) |
| openMSX replay → input-script | `tools/omr-to-input-script.py` (proves the (row,col)↔key table is shared with openMSX's `KeyMatrixState`; `T_state = emutime/960`, MAIN_FREQ = 3579545×960) |
| Formatted-blank disk | `tools/format-blank-disk.ps1` (720 KB FAT12, media 0xF9, empty root+data; byte-deterministic) |
| FM BASIC drivers | `references/music_in_basic.md` (`CALL MUSIC`/`CALL MUSIC(1)`, `PLAY#n` MML, `@n` presets, rhythm on `#10`) |

---

## 3. Methodology

### 3.1 Guest delivery (in priority order)
1. **Disk AUTOEXEC.BAS (primary).** The FAT12 injector (§5.2) writes an ASCII `AUTOEXEC.BAS`
   (the whole guest program) onto a formatted-blank 720 KB image. Both emulators boot the disk;
   Disk-BASIC auto-runs it. **Zero keyboard dependency ⇒ maximum determinism.** Used for PSG,
   FM, BASIC-exec, screen-mode, sprite, and file-I/O driver scenarios. (Gated on A-2.)
2. **Typed program (keyboard scenario + SAVE scenario).** The `--input-script` (our side) and
   openMSX `keymatrixdown/up` (side B) drive the **identical (row,col) matrix-event sequence**
   (never `type` — that would add a layout-translation variable). This is the *purpose* of the
   keyboard scenario, and the only honest way to test the matrix/BIOS decode incl. RCTRL `:`/`*`.
3. **Cart-ROM (fallback, "where BASIC can't reach").** The `gen-m38-*-cart.py` hand-assembled
   pattern for exact VDP register sequences (e.g. a specific YJK layout) that BASIC cannot
   express. Loads identically via our `--cart1` and openMSX `-cart`.

### 3.2 Draw-then-settle determinism (mirrors M38 static-settled)
Exact frame alignment across two emulators is fragile. Every VIDEO scenario is authored to
**DO the thing, then SETTLE into a static scene** — the drawing program ends with either `END`
(BASIC leaves a stable screen) or a tight spin (cart `DI:JR $`). Because the scene is stable,
the exact capture frame is irrelevant: any frame inside the settled window is byte-stable.
- Our side: `--frames N` lands inside the settled window; `--dump-frame` captures the final frame.
- openMSX side: `after time <sec> { screenshot -raw <p>; exit }` at a time inside the settled
  window.
- Fallback B (if A-2 fails): boot to the Disk-BASIC `Ok` prompt (settle), inject a fixed short
  `RUN"PROG.BAS"<RET>` (SHIFT for `"`) via matrix events on both sides, settle, capture.

For AUDIO scenarios the guest plays a **sustained / looping** tone or phrase; capture a **fixed
time window** during steady-state on both sides and compare spectrum + RMS envelope, never
sample-for-sample (§3.4).

### 3.3 Capture (both sides)
| | Our side (headless) | openMSX side (WSL Tcl) |
|---|---|---|
| Video | `sony_msx_headless --debug-session bios <max> --disk <img> [--input-script <s>] --frames <N> --dump-frame frame --debug-root <d>` → `tools/frame-to-png.py <d>/frames/frame -o ours.png` | `-machine Sony_HB-F1XV -diska <img>`; `after time <s> { screenshot -raw omx_raw.png; exit }` |
| Audio | add `--audio-dump snd --audio-sync` (or `--audio-dump-psg-only` for PSG isolation) → `tools/audio-dump-to-wav.py` → ours.wav | `set throttle off`; `after time <t0> { record start -audioonly omx }`; `after time <t1> { record stop; exit }` → omx.wav |

### 3.4 Compare
- **Video pixel:** `tools/m38-ab-diff.py --ours ours.png --omx omx_raw.png --ox <ox> --oy <oy>
  --out comp.png --label <id>`. Per-mode `(ox,oy)`: 256-wide @192 = (32,24); 256-wide @212 =
  (32,14); 512-wide = (64,14) *(confirm A-3)*; text modes per Phase-0 geometry probe. Metric:
  `mean_dist/ch`, `pct_mismatch`; MATCH if `pct_mismatch < 0.5%` (tol 40/ch absorbs the
  RGB555→888 ~1.6/ch rounding). Known-delta modes report the raw metric + a structural check.
- **Audio spectral/RMS:** `tools/m41-audio-ab-analyze.py` (NEW, §5.3): onset-align, windowed FFT
  → dominant-peak Hz + coarse band-energy vector, per-~10 ms RMS envelope. Reports
  peak-freq delta (cents), band-energy cosine similarity, envelope correlation. PASS thresholds
  per subsystem (§4). Reuses `m34-audio-ab-analyze.py`'s WAV-read + block structure and
  `m39-voice-ab-analyze.py`'s envelope logic; deterministic (FFT over a fixed payload).

### 3.5 openMSX-side scripting (canonical skeleton)
```tcl
set throttle off                       ;# run fast, headless
# (disk scenarios) machine already booted with -diska <img>; AUTOEXEC auto-runs
# (typed scenarios) matrix-event injection, identical (row,col) to our input-script:
#   keymatrixdown <row> <mask> ; after time 0.05 { keymatrixup <row> <mask> }
after time <settle_s> { screenshot -raw <out>; exit }         ;# VIDEO
# or, AUDIO:
after time <t0> { record start -audioonly <out> }
after time <t1> { record stop; exit }
```
Launched via `wsl -- openmsx -machine Sony_HB-F1XV -diska <wslimg> -script <wsltcl>` inside the
generalized harness (§5.4), which reuses `m38-run.ps1`'s WSL-path + base64-Tcl plumbing.

---

## 4. Per-behaviour PASS taxonomy (honest by construction)

For every scenario, PASS is exactly one of:
- **MATCH** — video: `m38-ab-diff` verdict MATCH (`pct_mismatch < 0.5%`, i.e. bit-exact within
  the RGB555→888 ~1.6/ch rounding); audio: peak-freq delta ≤ 5 cents AND band cosine ≥ 0.98 AND
  envelope correlation ≥ 0.95.
- **KNOWN-DELTA (named + quantified)** — the result differs from bit-exact by a specific,
  pre-documented, measured amount tied to a registered delta (§6). PASS requires the measured
  delta to stay **within** the documented band AND remain structurally correct (bands/pitches/
  onsets align). A delta that grows beyond its band is a FAIL and escalates.
- **BLOCKED (accepted, disclosed)** — the openMSX side cannot produce a comparable capture
  (e.g. A-5 joystick injection unavailable). Recorded as BLOCKED with the reason; our side's
  determinism + real-hardware/reference grounding stands in, mirroring M25 hardware-PAUSE and
  M31 audio-sample-N/A dispositions. BLOCKED does not gate closure but is listed, never hidden.

The suite is designed to **measure and report** deltas, not to be tuned until it reads MATCH.
The tolerance (40/ch video; the audio thresholds) is a rounding-absorber, calibrated on a
baseline scene to read ≈0, exactly as documented in `m38-ab-diff.py:31-32`.

---

## 5. Enabler slices (build these first — Phase 0)

### 5.1 `JOY=` input-script verb (the one src touch)
- **Grammar (additive):** `T=<tstate-dec> JOY=<port> <UP|DOWN|LEFT|RIGHT|A|B> DOWN|UP`, where
  `<port>` ∈ {1,2}. Coexists with the existing `KEY=` line kind; unchanged files still parse.
- **`src/machine/input_script.h`:** extend `InputScriptEvent` with a discriminator (kind =
  KEY | JOY) + joystick fields (`int joy_port`, `enum JoyControl {Up,Down,Left,Right,A,B}`). Keep
  the KEY path byte-identical. `InputScriptPlayer` gains an optional
  `peripherals::JoystickPorts*` and its `apply_due` overload dispatches JOY events by mutating a
  per-port `PortState` field and calling `JoystickPorts::set_port(port-1, state)`
  (joystick.h:79-81). Keyboard-only construction/behaviour is preserved (default-null joystick).
- **`src/machine/input_script.cpp`:** the parser (input_script.cpp:52-88) branches on the second
  token: `KEY=` (as today) vs `JOY=` (parse port + control + DOWN/UP; validate port∈{1,2} and a
  known control, throwing on malformed, mirroring the existing discipline). `serialize_input_script`
  round-trips both kinds.
- **`src/main.cpp` `--debug-session`:** thread the machine's `JoystickPorts` into the player so
  `--frames` mode applies JOY events (main.cpp ~1029-1231 input-script apply site). SDL3 live
  path wiring is OPTIONAL (not required by M41).
- **Deterministic tests (ctest):** `input_script_unit_test` gains JOY cases —
  parse→serialize→parse round-trip; malformed port/control/state throws; a KEY-only script is
  byte-identical pre/post change (regression guard); an integration case asserting an injected
  `JOY=1 LEFT DOWN` flips PSG R14 bit2 to 0 through `JoystickPorts::read_port_a()` after port
  select. Grounds STICK/STRIG reachability without a frontend.

### 5.2 FAT12 ASCII-BASIC injector — `tools/msx-basic-to-disk.py` (NEW)
- **Purpose:** deterministically place an ASCII BASIC program (and/or `AUTOEXEC.BAS`, and/or a
  binary for BSAVE/BLOAD) onto a formatted-blank 720 KB image, so both emulators boot an
  identical self-running disk. Removes all keyboard dependency from non-keyboard scenarios.
- **Behaviour:** start from `format-blank-disk.ps1`'s BPB geometry (80×2×9×512, FAT12, 2
  sectors/cluster, 112 root entries, 2 FATs @ LBA 1 and 4, root dir @ LBA ~14, data area
  after). Given `(name.ext, bytes)`: allocate a FAT cluster chain, write an 8.3 root-dir entry
  (name, ext, attr, size, first cluster; **fixed zero date/time for determinism** — not
  wall-clock), write the data into the data clusters, terminate the chain (0xFFF). ASCII BASIC =
  plain text, CRLF line endings, trailing `0x1A` EOF byte (the MSX ASCII-BASIC convention).
- **Reader (for verification):** a `--read <name>` mode that parses the FAT12 and dumps a file's
  bytes — used to verify SAVE round-trips (scenario H, A-6) on both the our-side and openMSX-side
  written images.
- **Determinism / test:** `--self-check` (two injections byte-identical; SHA256 stable) + a
  ctest-free tool self-test; plus one integration proof that our FDC can LOAD an injected file
  (boot the disk, `RUN"PROG.BAS"`, capture the expected drawing).

### 5.3 Spectral/RMS audio A/B comparator — `tools/m41-audio-ab-analyze.py` (NEW)
- Reads two WAVs (ours: `audio-dump-to-wav.py` output; openMSX: `record` output), handles
  differing sample rates (compare in Hz), onset-aligns (first sample past a threshold), computes:
  (a) dominant-peak frequency via windowed FFT (report delta in cents), (b) a coarse
  log-spaced band-energy vector (report cosine similarity), (c) a per-~10 ms RMS envelope
  (report Pearson correlation). Emits one deterministic verdict line + a metric block. Reuses
  `m34-audio-ab-analyze.py` WAV-read/block code and `m39-voice-ab-analyze.py` envelope logic.
- **PASS thresholds** are subsystem-specific (§4): PSG tone MATCH = ≤5 cents / cosine ≥0.98 /
  env ≥0.95; PSG noise = broadband band-cosine ≥0.90 (no pitch); FM MATCH-within-known-delta =
  peak ≤10 cents / env correlation ≥0.85 (envelope-accurate, not sample-exact — M31).
- **Determinism / test:** `--self-check` compares a WAV to itself (perfect) and to a
  pitch-shifted copy (detects the shift), proving the metric is non-vacuous.

### 5.4 Generalized dual-emulator harness — `tools/m41-run.ps1` (NEW)
- Generalizes `m38-run.ps1` (video) + `openmsx-m34-aleste-audio-ab.ps1` (audio + injection).
  Driven by a per-scenario manifest (`tools/m41-scenarios.psd1` or a small JSON): each entry
  names the guest (disk image / cart / input-script), capture kind (video|audio), settle timing,
  crop `(ox,oy)`, and expected disposition (MATCH | KNOWN-DELTA:<id> | BLOCKED). For each: run our
  side, run openMSX side (base64-Tcl over WSL, reusing m38's `To-WslPath`/`Invoke-Openmsx`),
  diff, and append a row to `debug/m41/ab/m41-ab-report.md`. Idempotent + re-runnable.

---

## 6. Known-deltas register (measure, don't hide)

| ID | Delta | Magnitude / band | Origin | Bit-exact blocked? |
|---|---|---|---|---|
| KD-VIDEO-RGB | RGB555→RGB888 rounding | ~1.6/ch (absorbed by tol 40) | universal decode | No — inherent, absorbed |
| ~~KD-YJK~~ **SUPERSEDED → CLOSED** | SCREEN 10/11/12 — the "~6% YJK luma decode" delta was a **MISDIAGNOSIS**: it was a **4-px horizontal registration bug** (DEF-M41-YJKOFFSET), NOT a luma approximation. The YJK/YAE luma/colour decode is **bit-exact** (residual ≤5/ch, sub-tol). Root cause: `render_yjk`/`render_yjk_yae` registered the decoded page at the GRAPHIC7 base with no display lead; hardware trails YJK by one 4-pixel group (chroma J lives in the group's 3rd/4th bytes). Fixed via `kYjkDisplayLead=4` in `compose_bitmap_scroll` (corroborated openMSX-19.1 A/B + fMSX `Common.h:732-737/778-783` + fact-sheet §5). Post-fix A/B: m10/m11/m12/probes = **0.000% MATCH** at the SAME crop as the G7/G4 controls (which stay 0.000%). | `vdp` YJK path registration | **No — now bit-exact MATCH** |
| KD-RASTER | sub-line raster / mid-frame register latch | ±1 line | M32 raster-truth (bios-f150, MG/mg2 bands) | No — raster-truth, escalate if grows |
| KD-SPRITE | sprite band / previous-frame lag | ≤1 frame on animated sprites | M22/M32 D9 | No — flag on animated sprite scenes |
| KD-FM-SYNTH | OPLL FM synthesis | spectral/envelope-accurate, NOT sample-exact | M31 §2.4 attack approximation | **YES — `YM2413NukeYktTables.ii` never opened (license-sensitive-table ban).** Bit-exact permanently blocked |
| KD-FM-RHYTHM | OPLL rhythm SD/HH/T-CY | disclosed approximation (BD/TOM full) | M31 §6 | Partially (same table ban) |
| KD-PSG-RESAMP | PSG/SCC band-limited box-average vs openMSX true resampler | residual ~500 RMS @ ~20.7 kHz sub-threshold (E4 backlog) | M34 | No — approximation, spectral MATCH holds mid-band |
| KD-FDC-TIMING | FDC read rotational latency | sector-granularity approximation (not track) | M37 Slice C | No — timing approximation; irrelevant to static video/audio A/B |

License-sensitive deltas that **cannot** be closed to bit-exact: **KD-FM-SYNTH / KD-FM-RHYTHM**
(the OPLL attack/waveform tables — standing zero-license-sensitive-work directive,
`feedback_license_sensitive_scope.md`) and the **C1/D4 VDP slot-timing tables** (no independent
non-GPL source; not exercised by these static-scene A/Bs, so they do not affect M41 verdicts but
are named for completeness).

---

## 7. Scenario matrix (exhaustive, grouped by subsystem)

Guest = A(utoexec disk) / T(yped matrix events) / C(art). Capture = V(ideo) / Aud(io) /
F(ile-content). Expected = MATCH / KD:<id> / (BLOCKED where noted).

### Subsystem 1 — Keyboard (matrix + BIOS decode)
| ID | Guest | What it exercises | Capture pt | Compare | Expected |
|---|---|---|---|---|---|
| K1 | T | Echo the full alpha set A–Z at the `Ok` prompt | after settle | V (text mode) | MATCH |
| K2 | T | Digits 0–9 + unshifted symbols `- = \ [ ] ; ' , . / \`` | settle | V | MATCH |
| K3 | T | SHIFTed symbols `! " # $ % & ' ( ) *`… (SHIFT down/up bracketing) | settle | V | MATCH |
| K4 | T | **RCTRL `:` and `*`** (row2,col0) + a typed line using them | settle | V | MATCH |
| K5 | T | Type a 3-line program then `LIST` (RETURN, cursor, editing keys) | settle | V | MATCH |

### Subsystem 2 — Joystick (STICK/STRIG) — needs the JOY verb (§5.1)
| ID | Guest | What it exercises | Capture pt | Compare | Expected |
|---|---|---|---|---|---|
| J1 | A + JOY | `10 SCREEN0:LOCATE0,0:PRINT STICK(1);STRIG(1):GOTO10`; inject each of 8 directions + trig A/B via `JOY=1 …` | settle per injected state | V (printed value) | MATCH *(side-B gated on A-5; else BLOCKED)* |
| J2 | A + JOY | `PUT SPRITE` moved by `STICK(1)`; inject LEFT then capture sprite position | settle | V | MATCH *(A-5)* |
| J3 | A + JOY | Port-2 select (R15 bit6) + `STICK(2)` read | settle | V | MATCH *(A-5)* |

### Subsystem 3 — PSG sound
| ID | Guest | What it exercises | Capture | Compare | Expected |
|---|---|---|---|---|---|
| P1 | A | `SOUND` direct: ch A tone at a fixed period+volume, sustained | 2 s window | Aud (`--audio-dump-psg-only`) | MATCH (peak/cents) |
| P2 | A | `PLAY"CDEFGAB>C"` (MML scale) single channel | window per note or whole phrase | Aud | MATCH (peak+env); KD-PSG-RESAMP on hi-freq |
| P3 | A | Noise: `SOUND` noise-enable + noise period | 2 s window | Aud | MATCH (band cosine ≥0.90) |
| P4 | A | Envelope: `SOUND` R13 shape + R11/R12 period, one channel enveloped | 2 s window | Aud (env) | MATCH (envelope correlation) |
| P5 | A | Volume steps + 3-channel chord | window | Aud | MATCH; KD-PSG-RESAMP |

### Subsystem 4 — FM / OPLL (MSX-MUSIC)
| ID | Guest | What it exercises | Capture | Compare | Expected |
|---|---|---|---|---|---|
| F1 | A | `CALL MUSIC:PLAY#1,"@3 O4 L4 CEGC"` (piano preset, sustained scale) | 2–3 s window | Aud | **KD-FM-SYNTH** (peak ≤10 cents, env ≥0.85) |
| F2 | A | Preset sweep: `@8` organ, `@14` bass on separate `PLAY#` channels | window | Aud | KD-FM-SYNTH |
| F3 | A | `CALL MUSIC(1):PLAY#10,"BD SD BD SD"` rhythm-mode drums | window | Aud | **KD-FM-RHYTHM** (BD/TOM closer; SD/HH approx) |
| F4 | A | Multi-channel (`PLAY#1,#2,#3`) melody+harmony+bass together | window | Aud | KD-FM-SYNTH |

*(F1–F4 grounded in `references/music_in_basic.md`; validate each MML statement against real
`f1xvmus.rom` behaviour + an openMSX A/B before trusting it, per the CLAUDE.md caveat.)*

### Subsystem 5 — BASIC execution (deterministic text output)
| ID | Guest | What it exercises | Capture | Compare | Expected |
|---|---|---|---|---|---|
| B1 | A | Arithmetic + float (`PRINT 22/7`, `SIN`, `^`) → fixed lines | settle | V (text) | MATCH |
| B2 | A | String ops (`MID$`,`STR$`,`VAL`,`+`) → fixed lines | settle | V | MATCH |
| B3 | A | `PRINT USING` numeric/string formatting | settle | V | MATCH |
| B4 | A | Control flow: `FOR`/`WHILE`/`IF`/`GOSUB` producing a table | settle | V | MATCH |

### Subsystem 6 — Screen modes (deterministic LINE/CIRCLE/PAINT/PSET + text)
| ID | Guest (SCREEN n + drawing) | Width class / crop | Compare | Expected |
|---|---|---|---|---|
| M0 | SCREEN0 40×24 text pattern | text (Phase-0 geometry) | V | MATCH |
| M1 | SCREEN1 32×24 + `COLOR` | text | V | MATCH |
| M2 | SCREEN2 `LINE`/`CIRCLE`/`PSET` | 256@192 (32,24) | V | MATCH |
| M4 | SCREEN4 `LINE`/`CIRCLE`/`PAINT` | 256@212 (32,14) | V | MATCH |
| M5 | SCREEN5 `LINE`/`CIRCLE`/`PAINT`/`PSET` + text | 256@212 | V | MATCH |
| M6 | SCREEN6 (512×212, 4c) pattern | 512@212 (64,14) A-3 | V | MATCH |
| M7 | SCREEN7 (512×212, 16c) pattern | 512@212 | V | MATCH |
| M8 | SCREEN8 (256×212, 256c) pattern | 256@212 | V | MATCH |
| M10 | SCREEN10 (YJK+YAE) deterministic ramp | 256@212 | V | **MATCH** (0.000%; DEF-M41-YJKOFFSET fixed — was "KD-YJK ~6%", actually a 4-px offset, now bit-exact) |
| M11 | SCREEN11 (YJK+YAE) | 256@212 | V | **MATCH** (0.000%; DEF-M41-YJKOFFSET fixed) |
| M12 | SCREEN12 (YJK) grayscale ramp (see `fill_yjk_yramp` recipe) | 256@212 | V | **MATCH** (0.000%; DEF-M41-YJKOFFSET fixed) |

*(YJK modes may need the cart-ROM guest (§3.1.3, `gen-m38`'s `s09/s12` YJK recipe) if BASIC
`SCREEN 10/11/12` + draw can't produce a clean deterministic ramp.)*

### Subsystem 7 — Sprites
| ID | Guest | What it exercises | Compare | Expected |
|---|---|---|---|---|
| SP1 | A | SCREEN2: `SPRITE$(0)=` 8px pattern, `PUT SPRITE` single-size, 2 colours | V | MATCH |
| SP2 | A | SCREEN5: 16px sprite, double-size (`SCREEN5,,,,` size), 4 sprites, distinct colours | V | MATCH |
| SP3 | A | Deliberate overlap → collision (`STRIG`? no — read collision via `VDP` status/`ON SPRITE`) | V + state | MATCH (KD-SPRITE if animated) |

### Subsystem 8 — Disk write/read via BASIC (uses `--disk-writable`, A-6)
| ID | Guest | What it exercises | Capture | Compare | Expected |
|---|---|---|---|---|---|
| H1 | T | Type a program, `SAVE"TEST.BAS"`, `FILES` | settle (FILES on screen) | V + F (re-read `.dsk` via injector reader on both sides) | MATCH + file-content equal |
| H2 | T | `NEW` then `LOAD"TEST.BAS"`, `LIST` == original | settle | V | MATCH |
| H3 | A | `BSAVE`/`BLOAD` a data block; `PEEK` verify → PRINT | settle | V | MATCH |
| H4 | A | Sequential: `OPEN"O"`,`PRINT#`,`CLOSE`,`OPEN"I"`,`INPUT#` → PRINT round-trip | settle | V + F | MATCH |
| H5 | A | Random: `OPEN"R"`,`FIELD`,`LSET`,`PUT`,`GET` → PRINT round-trip | settle | V + F | MATCH |

Total: ~35 scenarios across 8 subsystems.

---

## 8. Phasing (batches — avoid one giant fragile program)

- **Phase 0 — Enablers + smoke (blocking).** Build §5.1–§5.4. Verify A-1..A-6. One end-to-end
  smoke per capture kind: a trivial AUTOEXEC circle (video), one PSG tone (audio), one typed
  line (keyboard). Prove the whole loop (inject→boot→settle→capture→diff→report) works and reads
  MATCH on the baselines BEFORE scaling. Gate: enablers' ctest green + Phase-0 smoke MATCH.
- **Phase 1 — Video-static, exact modes.** K1–K5, B1–B4, M0–M8, SP1–SP3. All MATCH-expected
  video. Grouped by capture geometry (text / 256@192 / 256@212 / 512).
- **Phase 2 — YJK known-delta.** M10–M12. Measure and report KD-YJK; PASS = within band + bands
  align.
- **Phase 3 — Audio.** P1–P5 (PSG, MATCH), then F1–F4 (FM, KD-FM-SYNTH/RHYTHM). Fixed-window
  spectral/RMS.
- **Phase 4 — Disk I/O via BASIC.** H1–H5 (SAVE/FILES/LOAD/BSAVE/BLOAD/seq/random). Video +
  file-content verification.
- **Phase 5 — Joystick.** J1–J3 (needs the JOY verb + A-5 side-B path; BLOCKED-honest if A-5
  fails).

Each phase is an independent build/run batch appending to `debug/m41/ab/m41-ab-report.md`, so a
single flaky scenario never blocks the rest, and a re-run touches only its phase.

---

## 9. Milestones (M41), acceptance criteria, test obligations, risks

### M41-S1 — Enablers (JOY verb + FAT12 injector + audio comparator + harness)
- **Acceptance:** JOY verb parses/serializes/round-trips + drives `JoystickPorts` (integration
  proof: R14 bit flips); KEY-only scripts byte-identical (regression guard); injector
  `--self-check` SHA-stable + FDC-LOAD proof; audio comparator `--self-check` non-vacuous
  (self=perfect, shifted=detected); harness produces a report row for the Phase-0 smoke.
- **Deterministic tests (ctest):** `input_script_unit_test` JOY cases + KEY regression guard; a
  new `machine_input_script_joystick_integration_test`. Tool self-checks run in Phase-0.
- **Evidence gates:** `tools/validate-assets.ps1`; `tools/checksum-assets.ps1 -OutFile
  docs/asset-checksums.txt`; `cmake --build build --config Debug`; `ctest --test-dir build -C
  Debug --output-on-failure` (full fast subset green, incl. the new tests); `git diff` proving
  zero `src/devices/cpu/` + `src/core/` edits ⇒ ZEXALL withheld.
- **Risks:** JOY event model must not perturb the KEY path (byte-identity guard); openMSX-side
  joystick injection (A-5) may be unscriptable → J* side-B BLOCKED.

### M41-S2 — Video parity phases (Phase 1 + Phase 2)
- **Acceptance:** every Phase-1 scenario verdict MATCH; every Phase-2 (YJK) scenario within its
  KD-YJK band with bands structurally aligned; each row (metric + composite PNG) recorded in
  `debug/m41/ab/m41-ab-report.md`; per-mode crop offsets confirmed (A-3). Any true MISMATCH is
  **escalated as a candidate defect** (root-caused, evidence committed), never rebaselined.
- **Deterministic test obligation:** the guest disk images + input-scripts are checked in under
  `debug/m41/` (or `disks/m41/`) with a generator `--self-check` (byte-stable), so a rebuild
  reproduces identical guests.
- **Evidence gates:** build + ctest green; the A/B report + composites are the milestone evidence
  (the openMSX A/B gate, conditional-applicable here = mandatory since M41 is an A/B milestone).
- **Risks:** BASIC-vs-openMSX timing on non-settling scenes (mitigated by draw-then-settle);
  text-mode geometry unknown until Phase-0; 512-wide crop unconfirmed (A-3).

### M41-S3 — Audio parity phase (Phase 3)
- **Acceptance:** P1–P5 MATCH (spectral thresholds §4); F1–F4 within KD-FM-SYNTH/RHYTHM bands;
  rows recorded with metric blocks. FM sample-exactness explicitly NOT claimed (KD-FM-SYNTH,
  license-blocked) — the report states this in-line.
- **Deterministic test obligation:** fixed capture windows + fixed guest MMLs; comparator
  deterministic over the WAV payload.
- **Evidence gates:** build + ctest green; WAV artifacts + comparator logs committed under
  `debug/m41/`; A/B report rows.
- **Risks:** audio window alignment (mitigated by onset-align + steady-state window); PSG
  resampler residual (KD-PSG-RESAMP); openMSX `record` availability (A-4).

### M41-S4 — Disk-I/O + Joystick phases (Phase 4 + Phase 5) and final gate
- **Acceptance:** H1–H5 MATCH incl. file-content round-trip verification on both host images
  (A-6); J1–J3 MATCH or honest BLOCKED (A-5); the consolidated `m41-ab-report.md` carries EVERY
  scenario with its disposition and no undisclosed delta; a **closure summary** tabulates
  MATCH / KNOWN-DELTA / BLOCKED counts and lists each named delta with its measured magnitude.
- **Evidence gates:** validate-assets; checksum refresh; full fast-subset ctest green; the
  end-to-end A/B report is the release-gate artifact. Any defect found → separate fix milestone
  (planner-first), not patched in M41.
- **Risks:** disk write-back parity (A-6); random-file `FIELD`/`GET`/`PUT` edge behaviour;
  joystick side-B (A-5).

**QA sign-off** (msx-qa) is required before the production-QA gate is declared met: independent
re-run of a representative subset of the A/B matrix from a clean build, confirmation that no
oracle was weakened, and confirmation that every KNOWN-DELTA is measured (not asserted) and
every BLOCKED is genuinely unscriptable (not a masked failure).

---

## 10. Developer handoff (do this in order)

1. **Get the decision entry.** Coordinator adds a `agent-protocol/channels/decisions.md` entry
   authorizing M41 + the additive JOY-verb src touch. Do not start src edits before it exists.
2. **Phase 0 first, always.** Implement §5.1–§5.4; verify A-1..A-6 empirically and record the
   findings (openMSX version string, AUTOEXEC behaviour, capture geometries, joystick
   scriptability) at the top of `debug/m41/ab/m41-ab-report.md`. Do NOT scale scenarios until the
   smoke loop reads MATCH on baselines.
3. **JOY verb is the only src change.** Keep it additive/default-off; prove KEY-only byte
   identity; thread `JoystickPorts` through `--debug-session` `--frames`. Zero cpu/core/devices
   edits (git-diff-proven) ⇒ ZEXALL withheld.
4. **Author guests as disk AUTOEXEC.BAS where possible** (injector), typed matrix-events only for
   keyboard + SAVE, cart-ROM only where BASIC can't reach. Every guest ends in a stable settled
   state.
5. **Drive both sides through the harness**, append rows, keep composites. Treat any true
   MISMATCH as a defect to escalate (root-cause + evidence), never a tolerance to loosen.
6. **Report honestly.** Every row is MATCH, KNOWN-DELTA:<id> (+ measured magnitude), or BLOCKED
   (+ reason). The closure summary must name every delta in §6 that fired and confirm the
   license-blocked ones (KD-FM-SYNTH/RHYTHM) were reported, not fabricated.
7. **Run the evidence gates** each phase: validate-assets, checksum refresh, Debug build, fast
   ctest. Hand to msx-qa with the consolidated report.

Key files the developer will touch or drive (all absolute):
- `c:\Users\pashim\source\sony-msx-hbf1xv\src\machine\input_script.h` / `input_script.cpp` (JOY verb)
- `c:\Users\pashim\source\sony-msx-hbf1xv\src\main.cpp` (`--debug-session` JOY threading)
- `c:\Users\pashim\source\sony-msx-hbf1xv\src\peripherals\joystick.h` (consumer API — read-only)
- NEW `c:\Users\pashim\source\sony-msx-hbf1xv\tools\msx-basic-to-disk.py`
- NEW `c:\Users\pashim\source\sony-msx-hbf1xv\tools\m41-audio-ab-analyze.py`
- NEW `c:\Users\pashim\source\sony-msx-hbf1xv\tools\m41-run.ps1` (+ scenario manifest)
- Reuse: `tools\frame-to-png.py`, `tools\audio-dump-to-wav.py`, `tools\m38-ab-diff.py`,
  `tools\format-blank-disk.ps1`, `tools\omr-to-input-script.py`, `tools\gen-m38-vdp-scroll-cart.py`
- Report sink: `c:\Users\pashim\source\sony-msx-hbf1xv\debug\m41\ab\m41-ab-report.md`
