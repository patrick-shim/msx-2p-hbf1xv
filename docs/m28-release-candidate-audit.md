# M28 Release-Candidate Health Audit

- Milestone: M28 (Release Candidate — Backlog Closure Sweep + Full-Project Health Audit), slice S3.
- Scope: `docs/m28-planner-package.md` §2.4 — four parts, all mandatory, run with concrete command
  output (not restated planning text). Part 4 (backlog-accuracy pass) run LAST, after E2 (S1) and C5
  (S2) landed, so it reflects the milestone's final state.
- Capture date: 2026-07-08. Repository state: HEAD past `v1.0.27`, this cycle's E2/C5 changes
  applied (see `docs/m28-implementation-report.md`).

---

## Part 1 — Component-by-component source-code health

### 1(a) Source inventory (re-enumerated fresh this cycle, not reused from planning)

```
Glob "src/**/*.h"   -> 75 headers   (77 before this cycle's 2-file removal, §1(c))
Glob "src/**/*.cpp" -> 65 sources
```

(The planner package's own §2.4 estimate — "78 headers / 68 sources" — was a planning-time
approximation; this is the actual, re-enumerated count at implementation time, per A-M28-4.)

### 1(b) Every file exercised by at least one test

Cross-checked via a mechanical script: for every `src/**/*.h`, search all `tests/**/*.cpp` for a
literal `#include "<path-relative-to-src>"`. This is a conservative, direct-inclusion proxy — some
headers are legitimately exercised only *indirectly* (composed inside another device that itself has
a dedicated test), so every "not directly included" hit was individually verified below, not assumed
to be a gap.

**Result: 70/75 headers directly included by at least one test file; 5 more verified as legitimately,
indirectly exercised (composition + a dedicated test of the composing device); 0 genuine gaps.**

| Header not directly `#include`d by a test | How it is actually exercised |
|---|---|
| `src/devices/cartridge/cartridge_mapper_device.h` | Pure abstract base (`CartridgeMapperDevice : core::MemoryDevice`); every concrete subclass (`CartridgeGeneric8kbRom`, `CartridgeAscii8kbRom`, `CartridgeKonamiRom`, `CartridgeMirroredRom`, `CartridgeGeneric16kbRom`, `CartridgeAscii16kbRom`) has its own dedicated unit test under `tests/unit/devices/cartridge/`. |
| `src/devices/chipset/system_bus.h` | `SystemBus bus_{slot_bus_, io_bus_};` composed directly inside `Hbf1xvMachine` (`src/machine/hbf1xv_machine.h:551`); exercised by every machine-level test that calls `step_cpu_instruction()`/`debug_bus_read`/`debug_bus_write` — including the dedicated `tests/integration/machine/hbf1xv_system_bus_integration_test.cpp`, `tests/integration/devices/cpu_bus_contract_integration_test.cpp` (via `core/bus.h` + `cpu_bus_client.h` directly, not `system_bus.h`, since `SystemBus` implements the `core::Bus` contract). |
| `src/devices/video/sprite_engine.h` | Composed inside `V9958Vdp` (`src/devices/video/v9958_vdp.h:8` includes it); `tests/unit/devices/video_sprite_engine_mode1_unit_test.cpp` / `_mode2_unit_test.cpp` exercise it through `V9958Vdp`'s own public API (confirmed: both files `#include "devices/video/v9958_vdp.h"`, not `sprite_engine.h` directly). |
| `src/machine/debug_event_log.h` | `DebugEventLog debug_event_log_;` composed inside `Hbf1xvMachine`; exercised via `machine.set_event_logging_enabled()`/`write_event_log()` in `tests/integration/machine/hbf1xv_debug_event_log_integration_test.cpp` and the M27 replay-determinism system test, both going through the machine's public API rather than including the header directly. |
| `src/machine/debug_format.h` | A shared internal formatting utility (`to_hex`/`to_dec`/`flag_string`) consumed by `debug_dump.cpp`, `debug_event_log.cpp`, `cpu_trace_sink.cpp`, `frame_dump.cpp`, `input_script.cpp`, `psg_audio_dump.cpp`, `hbf1xv_machine.cpp` — every one of those output formats is asserted byte-for-byte by its own dedicated test (state-dump, event-log, CPU-trace, frame-dump, input-script, audio-dump tests), which transitively exercises every `debug_format.h` function on every assertion. |

Zero headers remain unaccounted for. (`src/devices/video/vdp_command_engine.h`, `vdp_mode.h`,
`vdp_palette.h`, `cpu/z80a_state.h`, `cpu/z80a_trace.h`, `chipset/ppi_slot_select.h` show the same
"composed-inside-a-tested-device" shape and were spot-checked identically.)

### 1(c) Seeded finding (A-M28-5) resolved: the two M0-era placeholder files

Re-ran the exact cited command this cycle:

```
$ rg "device_placeholder|peripheral_placeholder|DevicePlaceholder|PeripheralPlaceholder"
README.md:59:|   |   |-- device_placeholder.h        (pre-removal)
README.md:63:|   |   `-- peripheral_placeholder.h     (pre-removal)
src/devices/device_placeholder.h:5:struct DevicePlaceholder { ... }
src/peripherals/peripheral_placeholder.h:5:struct PeripheralPlaceholder { ... }
```

Confirmed exactly the seeded 3 hits (finding independently re-verified, not assumed). Cross-checked
per R-M28-7 before removal:

- `grep -n "device_placeholder\|peripheral_placeholder" CMakeLists.txt tests/CMakeLists.txt` → **zero
  hits** (neither file is referenced by any build target).
- `grep -n "GLOB" CMakeLists.txt tests/CMakeLists.txt` → **zero hits** — this project uses fully
  explicit `add_executable(... src/file1.cpp src/file2.cpp ...)` source lists throughout, never
  `file(GLOB_RECURSE ...)`, so removing these two header-only (no `.cpp`) files cannot silently
  change any target's compiled-file count.
- Not wired into `Hbf1xvMachine`'s construction (`grep -n "Placeholder" src/machine/hbf1xv_machine.h`
  → zero hits).

**Disposition: removed** (`src/devices/device_placeholder.h`, `src/peripherals/peripheral_placeholder.h`
deleted; `README.md`'s stale project-scaffold tree snippet updated to drop the two now-nonexistent
lines — a minimal, mechanical edit, not a full README refresh, since the rest of that snippet is a
pre-existing, already-stale M0-era artifact out of this audit's small-fix scope). Verified the build
still configures and compiles cleanly after removal:

```
$ cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF
-- Configuring done
-- Generating done
$ cmake --build build --config Debug --target sony_msx_core sony_msx_headless
(zero errors)
$ rg "device_placeholder|peripheral_placeholder|DevicePlaceholder|PeripheralPlaceholder"
(only historical mentions remain, in agent-protocol/channels/{requests,responses}.md and
docs/m28-planner-package.md — append-only ledgers/planning artifacts documenting the finding, which
correctly stay unchanged)
```

### 1(d) No untracked TODO/FIXME/STUB/"not implemented" markers

```
$ rg -i "TODO|FIXME|STUB|not implemented|NotImplemented" src/
```

Returned 7 hits, each individually checked — **all 7 are either (i) historical prose describing a
PAST stub replacement (`joystick.h`, `hbf1xv_machine.cpp`), (ii) citations of openMSX's/real
hardware's OWN documented behaviour/uncertainty used as grounding (`printer_port.{h,cpp}`'s "PDIR
not implemented, matches openMSX's own scope limit"; `wd2793.h`'s "HLT is just stubbed to +5V" —
a real-hardware wiring fact; `vdp_command_engine.h`'s "existing M14 stub mask"), or (iii) an
already-backlog-tracked hedge (`vdp_frame_renderer.cpp`'s citation of openMSX's own "TODO: verify"
comment on interlace/EO addressing, already disclosed and QA-accepted under backlog row D6, DONE
M21).** Zero untracked, unimplemented-and-unflagged gaps found.

---

## Part 2 — Integration-flow coherence, confirmed END-TO-END

### 2(a) Machine composition

Fresh read of `Hbf1xvMachine::wire_bus()` (`src/machine/hbf1xv_machine.cpp:33-`) cross-referenced
against the Target Machine Specification's I/O port list (below, Part 3) — every DONE backlog device
is attached either on `slot_bus_` (memory-mapped) or `io_bus_` (port-mapped), with zero devices
declared-but-unattached. 31 `io_bus_.attach`/`register_mirror` calls counted (`grep -c` in
`hbf1xv_machine.cpp`). The M28-S1 addition (`ym2413_.attach_clock_source(&ym2413_clock_)`) is the
only new wiring statement this cycle; it is a no-op behaviourally since the gate defaults off (see
`docs/m28-implementation-report.md`).

### 2(b) Bus/slot wiring vs. the machine XML

Re-confirmed the primary/secondary slot decode against `references/openmsx-21.0/share/machines/
Sony_HB-F1XV.xml`: both primary slots 0 and 3 expanded (`XML:85-116`, `123-199`), external slots 1/2
unexpanded (`XML:119,121`) — matches `slot_bus_.set_expanded(0, true)`/`set_expanded(3, true)` in
`wire_bus()` with no corresponding calls for slots 1/2. Unchanged since M11/M13/M19 (this cycle
touched zero slot-bus wiring).

### 2(c) CLI — both executables actually LAUNCHED (not merely `ctest`)

`sony_msx_headless` (Debug, `build/Debug/sony_msx_headless.exe`), covering every flag named in the
request:

```
$ ./build/Debug/sony_msx_headless.exe
sony-msx-hbf1xv headless scaffold
elapsed_cycles=59736 frame_count=1 frame_cycles_per_frame=59736

$ ./build/Debug/sony_msx_headless.exe --debug-session bios 2000 --disk disks/msxdos22.dsk \
    --dump-state audit_state.txt --trace-cpu audit_trace.txt \
    --input-script build/audit_input_script.txt --debug-root build/audit_debug
debug-session: steps=2000 halted=0 final_pc=457 elapsed_cycles=15274
(exit 0; build/audit_debug/traces/{audit_state.txt,audit_trace.txt} written)

$ ./build/Debug/sony_msx_headless.exe --debug-session bios 5000 --cart1 roms/metalgear.rom \
    --cart1-type Konami --cart2 roms/aleste.rom --cart2-type 8kB --debug-root build/audit_debug3
cartridge: --cart1 loaded (roms/metalgear.rom, Konami)
cartridge: --cart2 loaded (roms/aleste.rom, 8kB)
debug-session: steps=3000 halted=0 final_pc=456 elapsed_cycles=22825
(exit 0 -- both cartridge slots simultaneously populated and booted)
```

(One real usage finding, immediately resolved by re-reading the CLI's own accepted-value list rather
than guessing: `--cart1-type`/`--cart2-type` take the mapper's CLI *display* string, e.g. `8kB`/
`Konami`/`ASCII8`, not the C++ enum identifier `Generic8kB` — confirmed in
`src/devices/cartridge/cartridge_mapper_type.cpp`'s `parse_cartridge_mapper_type`. Not a defect, no
fix needed; noted here only because it took two attempts to launch correctly.)

`sony_msx_sdl3` (Debug, SDL3-ON build directory `build/sdl3-on/Debug/sony_msx_sdl3.exe`, rebuilt
fresh this cycle — `cmake -S . -B build/sdl3-on -DSONY_MSX_ENABLE_SDL3=ON
-DCMAKE_PREFIX_PATH=<repo>/build/_sdl3_install` then `cmake --build build/sdl3-on --config Debug`,
reusing the pre-existing vendored SDL3 install per the request's explicit instruction — not
reinstalled from scratch):

```
$ SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/sdl3-on/Debug/sony_msx_sdl3.exe \
    --bios-dir bios --disk disks/msxdos22.dsk --hidden-window --max-frames 30 \
    --dump-state audit_sdl3_state.txt --trace-cpu audit_sdl3_trace.txt \
    --input-script build/audit_input_script.txt
sony-msx-hbf1xv SDL3 frontend: window opened, entering real-time loop (PSG audio live;
YM2413/FM-PAC intentionally silent -- backlog E1, still open)
(exit 0; debug/traces/audit_sdl3_{state,trace}.txt written)
```

Both executables launch, drive a real cold boot with real BIOS/disk/cartridge assets, and produce
real, inspectable output artifacts — confirmed end-to-end, not merely "it compiles."

### 2(d) Asset loading

```
$ tools/validate-assets.ps1
Asset validation result: True
Present BIOS files: f1xvbios.rom, f1xvdisk.rom, f1xvext.rom, f1xvfirm.rom, f1xvkdr.rom,
                     f1xvkfn.rom, f1xvmus.rom
ROM file count: 2 (aleste.rom, metalgear.rom)
```

---

## Part 3 — Target Machine Specification feature-completeness cross-check

Cross-checked row-by-row against `CLAUDE.md`'s Target Machine Specification table.

| Spec field | Value | Implementation | Status |
|---|---|---|---|
| NAME/MANUFACTURER/ORIGIN/YEAR | Sony HitBit F1XV, Japan, 1988 | Machine identity, not code-mapped (grounds asset/XML selection) | N/A (identity, not a feature) |
| KEYBOARD | Full-stroke, 5 function keys, numeric keypad, arrow keys | `src/peripherals/keyboard_matrix.{h,cpp}` — 11×8 row/col matrix, PPI-driven | Built |
| CPU/SPEED | Zilog Z80A, 3.58 MHz | `src/devices/cpu/z80a_cpu.{h,cpp}` — full opcode set, ZEXALL/ZEXDOC-verified (backlog C3, DONE M24) | Built |
| CO-PROCESSOR | Yamaha V9958 VDP | `src/devices/video/v9958_vdp.{h,cpp}` — register/VRAM/status/interrupt contract (M14) + full rendering depth (D1-D7, all DONE) | Built |
| RAM | 64 KB | `src/devices/memory/memory_mapper_ram.{h,cpp}` over a 64 KB `MemoryRegion` (`kDramBytes = 64*1024`, `hbf1xv_machine.h:172`) | Built |
| VRAM | 128 KB | `src/devices/video/vdp_vram.h` (`kVramBytes = 128*1024`) | Built |
| ROM (32+16+16+16 KB) | BASIC&BIOS / SUB / Kanji-driver+font / DISK | `bios_rom_` (32 KB, slot 0-0), `sub_rom_` (16 KB, slot 3-1 p0), `kanji_rom_` (16 KB driver, slot 3-1 p1-2) + `kanji_font_rom_` (256 KB JIS1+JIS2 font ROM, I/O `#D8-#DB`, separate from the 16 KB driver window — matches real hardware's split between the memory-mapped Kanji BASIC driver and the I/O-bank-switched font ROM), `disk_rom_` (16 KB, slot 3-2 p1, wrapped by `sony_fdc_`) | Built (the internal MSX-MUSIC 16 KB ROM, `fmmusic_rom_`, is real HB-F1XV hardware not itemized in this CLAUDE.md row — it is covered under SOUND below, and IS implemented) |
| BUILT-IN MEDIA | 720 KB 3.5" FDD | `src/devices/fdc/{wd2793,disk_drive,disk_image,sony_fdc,fdc_clock_source}.*` — WD2793 core, Sony connection-style decode, deterministic 720 KB image | Built (backlog B8, DONE M16; flux/DMK-level depth is C10, DEFERRED → M31) |
| TEXT MODES (40×24/32×24) | TEXT1/TEXT2 | `src/devices/video/vdp_frame_renderer.{h,cpp}` | Built (D1, DONE M21) |
| GRAPHIC MODES | 64×48…256×424 | `vdp_frame_renderer.*` (all Target-Spec modes incl. MULTICOLOR's real 256×192 canvas) | Built (D1, DONE M21) |
| COLORS | 512-color palette (16 selectable) + YJK/YAE up to 19,268 on-screen | `src/devices/video/vdp_palette.h` (palette + 3-bit→5-bit expansion) + `vdp_frame_renderer.cpp` (`render_yjk`/`render_yjk_yae`) | Built (D5, DONE M21) |
| SOUND | FM-PAC (OPLL YM2413) 9-ch FM | `src/devices/audio/{psg_ym2149,ym2413_opll}.*` — PSG register-accurate + real synthesis (B1, DONE M15); YM2413 register/channel/rhythm CONTRACT + the M28 write-timing gate (B3/E2, DONE M17/M28) | **Partial-with-tracked-backlog-row**: YM2413 waveform-synthesis DSP (E1) is OPEN/DEFERRED → M30 — audio is SILENT for FM-PAC in the SDL3 mix (disclosed, not a hidden gap) |
| I/O PORTS: 2 cartridge slots | primary slots 1/2 | `src/devices/cartridge/*` + `CartridgeSlot` wrapper, wired all 4 pages each | Built (B7, DONE M19; KonamiSCC/auto-detect/hot-plug/long-tail mapper depth = G1/G2/G3/G4) |
| RGB/Scart, NTSC, RF video output | analog output paths | N/A — correctly out of a software emulator's scope (no code-mapped equivalent; the *decoded pixel data* these outputs would carry is what `FrameBuffer`/`Sdl3VideoPresenter` present) | N/A (by design) |
| 2 joystick ports | digital joystick | `src/peripherals/joystick.h` | Built (part of C6, DONE M15) |
| Printer port | Centronics | `src/peripherals/printer_port.{h,cpp}` | Built (C7, DONE M18; ESC/image-rendering depth = F2, DEFERRED → M33) |
| Mono audio output | analog mix output | Modeled as the PSG+YM2413 mix path in `Sdl3AudioPresenter`/`PsgAudioPump` | Built (mix exists; YM2413 contribution silent per E1, disclosed) |
| Cassette interface | motor/output/input | `src/peripherals/cassette_interface.{h,cpp}` | Built (C7, DONE M18; real `.CAS`/`.WAV`/`.TSX` format fidelity = F1, DEFERRED → M32) |

**No new, untracked "Gap" found this cycle** — every row is either fully Built or
Partial-with-an-already-tracked-backlog-row (E1/F1/F2/G1/G2/G3/G4/C10, all present in
`deferred-backlog.md` with M28-confirmed dispositions, §Part 4 below), matching Acceptance Criterion
6's requirement that any untracked Gap would need a new backlog/decision entry — none was needed.

---

## Part 4 — Full 34-row backlog-accuracy audit (run LAST, after E2/C5/S4 landed)

### 4(a) Every cited `references/...`/`src/...` path still exists

Mechanical sweep (handles this file's own brace-expansion/wildcard/dual-extension citation
conventions, e.g. `foo.{h,cpp}`, `foo.*`, `foo.hh/.cc`) over all 263 backtick-quoted path-like spans
in `agent-protocol/state/deferred-backlog.md`:

```
Total distinct repo-relative path citations checked: 123
OK: 123   MISSING: 0   (after one fix, see below)
```

**One genuine stale citation found and fixed**: the C7 row cited
`references/openmsx-21.0/src/CassettePort.hh/.cc`, but the real vendored path is
`references/openmsx-21.0/src/cassette/CassettePort.hh/.cc` (confirmed via `find references/
openmsx-21.0/src -iname "CassettePort*"` → both files exist only under the `cassette/`
subdirectory). Fixed directly in `deferred-backlog.md` (a small, safe, mechanically-verified
citation-path correction, no runtime/behavior change — within the §2.4 fix-scope rule). Re-ran the
sweep after the fix: 0 missing.

### 4(b) DONE rows' named files still exist and are wired (spot-check)

Spot-checked a representative sample spanning every section: `psg_ym2149.*`/`ym2413_opll.*` (B1/B3),
`rp5c01.*` (B2), `halnote_rom.*` (B4/B6), `kanji_font_rom.*` (B5), `{wd2793,disk_drive,disk_image,
sony_fdc,fdc_clock_source}.*` (B8), `cartridge/*` (B7), `vdp_frame_renderer.*`/`sprite_engine.*`/
`vdp_command_engine.*`/`vdp_command_address.h` (D1-D3/D5-D7), `z80a_cpu.cpp` (C2),
`cpm_bdos_harness.*` (C3), `printer_port.*`/`cassette_interface.*` (C7),
`mb670836_pause.*`/`rensha_turbo.*` (C8), `sdl3_{app,video_presenter,audio_presenter,input_mapper,
cli}.*` (C9) — all present (already covered by the Part 4(a) mechanical sweep above, which checked
every one of these) and all confirmed wired into `hbf1xv_machine.{h,cpp}`'s construction (Part 1(b)/
2(a) above), consistent with each row's own "wired into..." claim.

### 4(c) `milestones.md`/`definition-of-done.yaml` vs. `deferred-backlog.md` — drift check

- Every milestone M11-M27's `definition-of-done.yaml` entry shows `status: done`/`overall_done:
  true`, consistent with `deferred-backlog.md`'s own DONE-row milestone tags (M11 through M27, all
  present and consistent) and with the git tags `v1.0.11`..`v1.0.27` (`git tag -l` confirmed all
  present). No contradiction found between the two files' CLOSED-milestone claims.
- **One historical (pre-existing, not introduced this cycle) minor drift found**: `definition-of-
  done.yaml`'s `M10` entry still shows `status: in_progress` even though the same entry's
  `overall_done: true` and its own `notes: "CLOSED 2026-07-06 (REQ-M10-009)..."` clearly show M10 is
  fully closed — the `status:` scalar was evidently never updated to `done` at M10's own closure
  time, back near the start of this project (`M11`'s entry onward all correctly show `status:
  done`). This is a genuinely stale field on an ancient, fully QA-signed, already-tagged (`v1.0.10`
  is a pre-tagging-era milestone; formal tags start at `v1.0.11`) milestone — reported here as a
  finding, NOT fixed ad hoc: editing a QA-signed historical milestone's ledger entry is a documentation-
  history question, not a "small, safe, mechanically-verifiable" fix in the same sense as a stale
  path citation, so it is left for a future decision if the coordinator/QA judges it worth a
  dedicated one-line correction.
- **One documentation-completeness gap found (not a contradiction)**: `deferred-backlog.md`'s
  reverse-chronological trailer-block section (`Last updated:` / `Prior status:` summaries) jumps
  directly from the M26 entry to the M25 entry — **no M27 summary trailer block exists**, even
  though M27 is fully CLOSED (tag `v1.0.27`, `definition-of-done.yaml` `status: done`) and M27's own
  row-level edit (a factual note appended to the C5 row) IS present and correct. The per-row content
  is accurate; only the top-level narrative trailer summary for M27 is missing. Reported as a
  finding, not backfilled ad hoc (adding a full historical narrative paragraph for an already-closed
  milestone is more editorial than mechanical, and out of this audit's small-fix scope) — a future
  cycle may choose to add it for narrative completeness.
- `current-phase.md`'s "Active Phase: M28" description and `milestones.md`'s M28 section both
  correctly describe the planner-approved scope (E2/C5/health-audit IN; G1/E1/C10/F1/F2 DEFERRED
  with named owners; G2/G3/G4 indefinite; C1/D4 UNBUILDABLE-WITHOUT-FABRICATION) consistently with
  each other and with `docs/m28-planner-package.md` — no drift found. Both are updated by this
  slice's S4 ledger closure to reflect developer-implementation-complete status (see
  `docs/m28-implementation-report.md`).

### 4(d) Full 34-row disposition confirmed current as of this audit

All 34 rows re-verified present, and every row touched this milestone (E2, C5, C1, D4, E1, C10, F1,
F2, G1) carries its M28 disposition **directly in the row itself** (not just in this audit doc) —
see `agent-protocol/state/deferred-backlog.md`. G2/G3/G4 re-affirmed indefinite/on-demand, rows left
byte-for-byte unchanged (no edit applied, matching "unchanged" instruction). 23 previously-DONE rows
re-confirmed unchanged (B1-B9, C2-C4, C6-C9, D1-D3, D5-D7) — none touched by M28.

---

## Fix-scope compliance summary

Applied this cycle (all small, safe, mechanically-verified, no behavior change to any already-QA-
signed device):
1. Removed `src/devices/device_placeholder.h` / `src/peripherals/peripheral_placeholder.h`
   (dead M0-era scaffolding, zero consumers, safe per the R-M28-7 pre-check) + the corresponding
   two lines in `README.md`'s stale scaffold tree snippet.
2. Fixed one stale path citation in `deferred-backlog.md`'s C7 row
   (`CassettePort.hh/.cc` → `cassette/CassettePort.hh/.cc`).

Reported as findings, NOT fixed ad hoc (each would be a bigger, more editorial change than this
audit's small-fix bar permits):
- `definition-of-done.yaml`'s M10 `status: in_progress` field (stale, contradicts its own
  `overall_done: true`/notes; pre-existing, not introduced this cycle).
- `deferred-backlog.md`'s missing M27 trailer-block summary (row-level content is correct; only the
  top-level narrative recap is absent).

No Target-Spec "Gap" requiring a new backlog/decision row was found (Part 3).
