# M19 Implementation Report — Cartridge Loading, External Primary Slots 1 & 2

- Milestone ID: M19
- Request: REQ-M19-002 (implementation)
- Developer: MSX Developer Agent
- Planner package: `docs/m19-planner-package.md` (RESP-M19-001)
- Gate: normal human-release-decision gate (no auto-close) — this report covers
  implementation only; QA sign-off is a separate, subsequent gate.

---

## 1. Milestone Target

Deliver the six-MVP-mapper-type cartridge-loading contract for the HB-F1XV's two external,
unexpanded primary slots (1 and 2): a shared 8-slot × 8 KB `CartridgeRomWindow` bank-resolution
primitive, six concrete mapper devices (`Mirrored`, `Generic8kB`, `Generic16kB`, `Ascii8kB`,
`Ascii16kB`, `Konami` — no SCC), a `CartridgeSlot` wrapper (the one device actually attached to
`slot_bus_`), machine wiring + a `load_cartridge`/`unload_cartridge` API, a pure `cartridge_cli`
argv parser wired into `src/main.cpp`, deterministic unit/integration tests, and real openMSX A/B
evidence — closing deferred-backlog row B7.

## 2. Code Changes

### 2.1 New device family — `src/devices/cartridge/`

| File | Grounding |
|---|---|
| `cartridge_mapper_type.{h,cpp}` | `RomInfo.cc:19-20,23,26-27,92` (canonical name strings) |
| `cartridge_rom_window.{h,cpp}` | `RomBlocks.hh`, `RomBlocks.cc:28-51,107-118` (`setRom` bank-resolution algorithm, byte-exact) |
| `cartridge_mapper_device.h` | Family-local interface, mirrors the existing `RtcClockSource`/`FdcClockSource`/`CassetteClockSource` X-pattern |
| `cartridge_mirrored_rom.{h,cpp}` | `RomPlain.cc` (MIRRORED case; lines 38-98) |
| `cartridge_generic8kb_rom.{h,cpp}` | `RomGeneric8kB.cc:7-36` |
| `cartridge_generic16kb_rom.{h,cpp}` | `RomGeneric16kB.cc:6-24` |
| `cartridge_ascii8kb_rom.{h,cpp}` | `RomAscii8kB.cc` (header comment + lines 18-52) |
| `cartridge_ascii16kb_rom.{h,cpp}` | `RomAscii16kB.cc` (header comment + lines 16-45) |
| `cartridge_konami_rom.{h,cpp}` | `RomKonami.cc` (whole file, no SCC) |
| `cartridge_slot.{h,cpp}` | Mirrors the M13 `RomAssetLoader`/device split + the `RomFactory::create` dispatch pattern, scoped to 6 types |

Every openMSX citation above is a behaviour reference read for understanding; no openMSX source
was copied into `src/` (GPL isolation, guardrails).

**`CartridgeRomWindow::set_bank`** implements `RomBlocks<BANK_SIZE>::setRom` (`RomBlocks.cc:107-118`)
literally:

```
block = (block < nrBlocks) ? block : block & blockMask;
if (block < nrBlocks) { mapped } else { unmapped }
```

The mask is consulted **only** when the requested block is already out of range — never an
unconditional AND. Unit tests (`cartridge_rom_window_unit_test.cpp`) assert both branches for the
default mask (`nrBlocks-1`) and an overridden (Konami-style, 31) mask, including the "oversized
(>256 KB) Konami image, mask never engages beyond the image's own bank count" subtlety
(`RomKonami.cc:27-31`).

**Mirrored placement** (`cartridge_mirrored_rom.cpp`): rather than computing `s % nrBlocks` by
hand, `reset()` calls `window_.set_bank(s, s)` for every window-slot `s`, letting the shared
window's own mask-fallback resolve the mirror. This is a deliberate, literal match of
`RomPlain.cc:85-98`'s own mirrored branch, which uses `romPage & (numPages - 1)` (a bitwise AND,
**not** a true modulo) — the two coincide only when `numPages` is a power of two. Computing
`s % nrBlocks` directly would have silently diverged from real openMSX behaviour for a
non-power-of-two bank count.

**16 KB-granularity types** (`Generic16kB`, `Ascii16kB`) compose the SAME shared 8 KB window via
window-slot pairs (`{2n, 2n+1}` = logical bank `n`'s low/high half), per planner §2.2, rather than
a second, separately-templated 16 KB-bank-size window class — an explicit, planner-approved
architectural simplification (one shared primitive for all six types).

**Konami quirk — a grounding finding surfaced and corrected, not silently trusted from the task
brief's shorthand.** The task brief's own summary states "window-slot 2 ... is never targeted by
any write address ... so slots 0/1/2 stay PERMANENTLY fixed at bank 0." A literal read of
`RomKonami.cc:38-52,61-67` shows this is imprecise:

- `writeMem` only ever calls `bankSwitch(addr>>13, value)` for `addr` in `[0x6000, 0xC000)`, i.e.
  `addr>>13 ∈ {3,4,5}` — page 2 (`0x4000-0x5FFF`) is **never** reachable from any write address.
- `bankSwitch(2, block)` is therefore called **exactly once**, from `reset()` (`block=0`) — window-
  slot 2 AND its mirror window-slot 0 (`bankSwitch`'s `page==2` branch mirrors into `page-2=0`) are
  both genuinely fixed at bank 0 for the entire session.
- `bankSwitch(3, block)` mirrors into window-slot 1 (`page-2=1`) on **every** write to page 3
  (`0x6000-0x7FFF`, directly reachable) — so window-slot 1 is **not** fixed; it tracks window-slot
  3's live value. Symmetrically, window-slot 6 tracks window-slot 4 (page 4), and window-slot 7
  tracks window-slot 5 (page 5).

`cartridge_konami_rom.cpp` is a literal, byte-exact translation of `RomKonami.cc`'s actual
algorithm (not the task brief's paraphrase), so the code was always correct; the unit test
(`cartridge_konami_rom_unit_test.cpp`) was written to assert the CORRECT, literally-grounded
behaviour (only slots 0 and 2 permanently fixed; slots 1/3, 4/6, 5/7 each paired-but-live) —
documented explicitly in the test file's own header comment so this correction is not silently
buried. This mirrors the DEC-0012 "verify against the primary source before trusting a summary"
precedent the planner package itself established (§2.8).

### 2.2 `CartridgeSlot` — the wrapper actually wired onto the bus

`load(type, image)` validates the image size against the concrete type's own
`is_valid_image_size()` **before** constructing anything; on success it constructs the mapper,
calls its `reset()` once (establishing a well-defined power-up bank layout uniformly across all
six types, including Konami, whose own constructor deliberately does **not** self-reset per
`RomKonami.cc:33-35` — `CartridgeSlot::load()` supplies that reset explicitly instead), and only
then swaps it into the active `mapper_`. A size-invalid load returns
`CartridgeLoadResult::ImageSizeInvalidForMapperType` and leaves the slot's prior state completely
untouched (unit-tested directly: load a valid cartridge, perturb its bank state, attempt an
invalid load, assert both the mapper type AND the perturbed bank state are unchanged).

An empty `CartridgeSlot` (`mapper_ == nullptr`) is byte-for-byte identical to the M13-M18 open-bus
default: `mem_read` returns `0xFF`, `mem_write` is a no-op. `reset()` calls the active mapper's
`reset()` (no-op when empty) and **never** unloads.

### 2.3 Machine wiring — `src/machine/hbf1xv_machine.{h,cpp}` (additive only)

- Two new members: `devices::cartridge::CartridgeSlot cartridge_slot1_{1};` /
  `cartridge_slot2_{2};`.
- `wire_bus()`: `for (page 0..3) { slot_bus_.attach(1, 0, page, &cartridge_slot1_);
  slot_bus_.attach(2, 0, page, &cartridge_slot2_); }` — **no** `set_expanded(1, ...)` /
  `set_expanded(2, ...)` call, per A-M19-1 (confirmed directly against
  `Sony_HB-F1XV.xml:119,121`: both are bare `<primary external="true">` elements).
  `SlotBus::sub_for_page` (`slot_bus.cpp:51-56`) already returns 0 unconditionally for a
  non-expanded primary — zero change to `SlotBus` itself, confirmed by direct read.
- `cold_boot()`: `cartridge_slot1_.reset(); cartridge_slot2_.reset();` — reinitializes bank state,
  never unloads.
- New API: `load_cartridge(int slot_number, CartridgeMapperType, std::vector<uint8_t> image) ->
  CartridgeLoadResult`; `unload_cartridge(int slot_number)`; `cartridge_slot1()`/`cartridge_slot2()`
  accessors (const + non-const), mirroring the existing `kanji()`/`printer()`/`cassette()` pattern.

### 2.4 CLI — `src/machine/cartridge_cli.{h,cpp}` (new) + `src/main.cpp` (additive)

`parse_cartridge_cli(args)` is a pure function (no file I/O, no `Hbf1xvMachine` dependency):
recognizes `--cart1`/`--cart1-type`/`--cart2`/`--cart2-type` anywhere in `args`, order-independent.
An omitted `-type` flag defaults to `Mirrored` (A-M19-5, matches openMSX's own default,
`RomFactory.cc:178-179`); an **unrecognized** type value is always a parse error, never silently
defaulted.

`src/main.cpp` gained `load_cartridges_from_args()` (shared helper), wired into **both** the
default/normal run path and the existing `--parity-trace` mode (which now takes an additional
`const std::vector<std::string>& cli_args` parameter). This is a **new, deliberately stricter**
policy than `RomAssetLoader`'s existing BIOS/Kanji-font/disk-image graceful-degradation policy: an
unreadable file OR any non-`Ok` `CartridgeLoadResult` prints a specific diagnostic to `stderr` and
returns a non-zero exit code — never a silent fallback to "no cartridge." This policy lives **only**
at the new cartridge-CLI call sites; `Hbf1xvMachine::load_rom_assets()` (the BIOS/Kanji-font/
disk-image path) was not touched — confirmed by direct diff review and by a dedicated regression
test case (`cartridge_cli_unit_test.cpp`'s "absent --cart1/--cart2 entirely yields no per-slot
request" case, plus the existing `RomAssetLoader`/boot-checkpoint tests, all of which remain green
unmodified).

### 2.5 Build files

`CMakeLists.txt`: 8 new `.cpp` files under `src/devices/cartridge/` + `src/machine/cartridge_cli.cpp`
added to `add_library(sony_msx_core ...)` (`cartridge_mapper_device.h` is header-only, no `.cpp`).
`tests/CMakeLists.txt`: 10 new test executables (8 unit + 2 integration), plus a new
`SONY_MSX_ROMS_DIR_ABS` compile-definition variable (mirrors the existing `SONY_MSX_BIOS_DIR_ABS`
pattern) for the aleste-smoke test.

### 2.6 The #A8 byte-value derivation (a documentation clarification, not a behaviour change)

The task brief's own illustrative example states `#A8 = 0b00_00_01_00 = 0x04` for "page1 -> slot 1,
pages 0/2/3 -> slot 3." Per this project's own, already-tested `SlotBus` bit layout
(`slot_bus.cpp:47-49`, `primary_for_page(page) = (primary_select_ >> (2*page)) & 0x03`), `0x04`
actually routes pages 0/2/3 to primary slot **0** (BIOS ROM), not slot 3 (RAM) — inconsistent with
its own stated intent, and unusable for a CPU-executed test program that must keep running from
RAM. For a program that needs to KEEP EXECUTING from RAM while only page 1 is repointed at a
cartridge slot, the correct byte (derived directly from `SlotBus`, not assumed) is:

```
page0=3, page1=N, page2=3, page3=3
value = page0 | (page1<<2) | (page2<<4) | (page3<<6)
slot 1 (N=1): 0xF7      slot 2 (N=2): 0xFB
```

This value is used throughout the M19 CPU-driven integration test and the openMSX A/B harness. The
non-CPU-executing `aleste` mechanical-smoke test (which only uses `debug_bus_read`, never runs the
CPU) does not need pages 0/2/3 to be anything in particular, so it legitimately uses the task
brief's literal `0x04` value where it is harmless — both values are documented explicitly in the
relevant test files so this distinction is never silently glossed over.

## 3. Unit Test Results

New unit test executables (all pass, `ctest` output below): `devices_cartridge_rom_window_unit_test`,
`devices_cartridge_mapper_type_unit_test`, `devices_cartridge_mirrored_rom_unit_test`,
`devices_cartridge_generic8kb_rom_unit_test`, `devices_cartridge_generic16kb_rom_unit_test`,
`devices_cartridge_ascii8kb_rom_unit_test`, `devices_cartridge_ascii16kb_rom_unit_test`,
`devices_cartridge_konami_rom_unit_test`, `devices_cartridge_slot_unit_test`,
`machine_cartridge_cli_unit_test`.

Coverage highlights: bank-resolution mask-as-fallback-only algorithm (both branches, default and
overridden mask, including the oversized-Konami-image subtlety); the Mirrored full-mirror
placement; each of the other five mappers' documented reset()/write dispatch, including the
Ascii16kB "both middle banks start identical" quirk and the corrected Konami fixed-slot finding;
`CartridgeSlot`'s empty-slot regression guard, load/unload/reset dispatch, and
size-invalid-load-leaves-prior-state-untouched contract; the CLI parser's order-independence,
case-insensitivity, omitted-vs-unrecognized-type distinction, and existing-behavior-unchanged
(zero-flags) case. Every new test file also asserts two-run determinism.

## 4. Integration Test Results

- `machine_hbf1xv_m19_cartridge_integration_test`: a real CPU program (Prog-assembled Z80 bytes,
  executed via `step_cpu_instruction()`) issues `OUT (#A8),A` to repoint CPU page 1 at primary
  slot 1, then performs each of the six MVP mappers' documented bank-switch sequence and reads
  back the expected marker bytes — repeated independently for primary slot 2. Additional cases:
  independence (two different cartridges in slot 1/2 simultaneously, never cross-contaminated);
  an unloaded/never-loaded slot (byte-for-byte M13-M18 open-bus regression guard); a
  `cold_boot()`-preserves-cartridge case (bank-switch away from default, `cold_boot()` again,
  bank state reverts but the cartridge stays loaded); explicit `slot_expanded(1) == false` /
  `slot_expanded(2) == false` assertions (R-M19-8).
- `machine_hbf1xv_m19_aleste_smoke_integration_test`: loads the real `roms/aleste.rom`
  (2,097,152 bytes) as `Generic8kB`; asserts its size, an independently-computed SHA256 (a
  from-spec, self-contained SHA-256 implementation written for this test only) matches
  `docs/asset-checksums.txt`'s recorded value, and after routing primary slot 1 into CPU page 1,
  `debug_bus_read(0x4000) == 0x41` / `debug_bus_read(0x4001) == 0x42`. The test file's own header
  comment states explicitly, in capital letters, that this is a MECHANICAL smoke check only — no
  claim whatsoever about the file's real-world game/mapper identity.

## 5. Evidence Gates (exact captured output)

```
tools/validate-assets.ps1        -> Asset validation result: True (7 BIOS files, 2 ROM files)
tools/checksum-assets.ps1        -> docs/asset-checksums.txt refreshed (aleste.rom SHA256 unchanged:
                                     4ea1e183467abe094df88901fc5dc70f0df5c1fc424de19f212692001bd5ad43)
cmake --build build --config Debug -> 0 errors (only pre-existing C4819 codepage warnings, unrelated)
ctest --test-dir build -C Debug --output-on-failure
  -> 100% tests passed, 0 tests failed out of 91
     (81 prior M1-M18 + 10 new M19 test executables)
```

Zero regression: every prior M1-M18 test remained green, in particular the M9/M12 CPU-timing
oracles (`devices_z80a_*`, `machine_hbf1xv_cpu_parity_integration_test`,
`machine_hbf1xv_m11_parity_checkpoint_integration_test`) and the M13-M18 slot-map/device-accessor
goldens (`machine_hbf1xv_slot_map_unit_test`, `machine_hbf1xv_m1{5,6,7,8}_*`) — confirming no new
clock consumer was introduced (every cartridge mapper device is pure/combinational, per §2.5 of
the planner package).

One test-authoring bug was found and fixed during development (not a device defect): the
`cold_boot()`-preserves-cartridge integration case initially bank-switched to value `7` against a
4-bank synthetic image, which — per the CORRECT, byte-exact mask-fallback algorithm (`7 >= 4` →
`7 & 3 = 3`) — resolves to bank 3, not bank 7. The test's own expectation was wrong, not the
device; fixed to use an in-range value (`2`) so the assertion is unambiguous.

## 6. openMSX A/B Evidence

Full artifact: `docs/m19-parity-trace-diff.md`.

**Slot-lettering (R-M19-6), empirically verified, not assumed.** A live WSL Tcl probe (two
distinguishable synthetic `Mirrored` images, marker bytes `0xAA`/`0xBB`, mounted via
`-carta`/`-cartb`) confirmed: `debug write ioports 0xA8 0xF7` (page 1 → primary slot 1) then
`debug read memory 0x4000` returns `0xAA` (the `-carta` file); `debug write ioports 0xA8 0xFB`
(page 1 → primary slot 2) then the same read returns `0xBB` (the `-cartb` file). **Conclusion:
`-carta` = primary slot 1, `-cartb` = primary slot 2**, for this machine on this installed
openMSX 19.1 (WSL). The actual installed openMSX runtime machine XML
(`/usr/share/openmsx/machines/Sony_HB-F1XV.xml`) was independently read and confirmed
byte-identical to this repo's reference copy's primary-slot-1/2 declarations before this claim
was made.

**Result: genuine EMPTY DIFF, not BLOCKED.** The extended `--parity-trace` mode (now recognizing
`--cart1`/`--cart1-type` via the shared S5 parser/loader) ran the SAME synthetic `Generic8kB`
cartridge (`tests/parity/m19_cartridge.rom`, 4 × 8 KB banks, bank N's every byte == N) and driver
program (`tests/parity/m19_cartridge_probe.bin`, generated by `tools/gen-m19-cartridge-probe.py`)
against real openMSX 19.1 (WSL) via `tools/openmsx-m19-cartridge-parity.ps1`. Both sides produced
**8 identical instructions** (PC, opcode, every register/flag) — including the AF register at the
three `LD A,(addr)` steps, which is where the read-back marker bytes land: `0x00` (reset-default
bank 0), then `0x03` (after the bank-switch write), then `0x01` (the unaffected window-slot,
confirming bank 1 was never touched). Because both sides load an identically-authored file, this
single per-instruction trace diff is simultaneously the architectural parity proof AND the
content-bearing proof — no second subject/mechanism was needed (unlike M17's YM2413 harness, which
needed a second internal-register-file subject because the OPLL is write-only; here, the
cartridge's own read-back path already surfaces content through the CPU-visible register trace).

The adversarial comparator self-check passed both ways: an empty-side input produced exit code 2
(BLOCKED, as expected); a corrupted `af=` field (forced to `0xDEAD` wherever it was `0x0000`)
produced exit code 1 (DIVERGENCE, 4 field mismatches correctly flagged), so the empty diff above is
trustworthy and not an artifact of a broken comparator.

## 7. Known Issues / Residual Risks (honestly recorded, none blocking)

1. **KonamiSCC + embedded SCC sound chip** (backlog G1) is explicitly out of scope — a genuinely
   new audio device, not an incremental mapper.
2. **ROM-database/heuristic auto-mappertype-detection** (backlog G2) is explicitly out of scope —
   this milestone requires an explicit `--cartN-type`.
3. **Full `CartridgeSlotManager`-style runtime hot-plug** (backlog G3) is explicitly out of scope —
   cartridge composition is fixed at construction/cold-boot time, matching every other device in
   this project.
4. **The long tail of ~80 other specialized/vendor-specific mapper types** (backlog G4) is
   explicitly out of scope.
5. `roms/aleste.rom`'s real-world mapper/game identity remains genuinely unconfirmed (A-M19-10) —
   used ONLY as a mechanical smoke fixture, never asserted as a real, identified cartridge,
   anywhere in this milestone's code, tests, or documentation.
6. The Konami "fixed slots" grounding finding (§2.1 above) is a documentation/test correction, not
   a behaviour risk — the shipped code was always a literal, correct translation of
   `RomKonami.cc`; only the task brief's own prose summary was imprecise, and this report/tests now
   state the corrected, grounded claim explicitly.

## 8. QA Handoff

- Evidence gates: all four executed and captured above (validate-assets, checksum-assets, Debug
  build, ctest 91/91).
- A/B evidence: `docs/m19-parity-trace-diff.md` — genuine EMPTY DIFF (architectural + content),
  slot-lettering empirically confirmed (not BLOCKED), adversarial self-check passed both ways.
- Ledger: `agent-protocol/state/deferred-backlog.md` updated (B7 → DONE (M19); new Section F,
  rows G1-G4).
- Please independently verify (do not merely re-trust this report):
  1. The bank-resolution mask-as-fallback-only algorithm (`cartridge_rom_window.cpp`) against
     `RomBlocks.cc:107-118` directly.
  2. The corrected Konami "fixed slots 0/2 only, not 0/1/2" finding against `RomKonami.cc:38-52,
     61-67` directly (this report's §2.1 explains the derivation in full).
  3. That `roms/aleste.rom` is never asserted as a real, identified cartridge anywhere (grep for
     "Aleste"/"aleste" claims outside the disclosed mechanical-smoke-only framing).
  4. That the new cartridge-CLI loud/non-zero-exit failure policy did not leak into
     `RomAssetLoader`'s existing graceful-degradation call sites (`git diff` on
     `rom_asset_loader.{h,cpp}` should be empty; it was not touched this milestone).
  5. `machine.slot_expanded(1) == false` / `slot_expanded(2) == false` hold after `cold_boot()`
     (R-M19-8 regression guard).
  6. Independently reproduce the openMSX A/B slot-lettering probe and/or the full harness
     (`tools/openmsx-m19-cartridge-parity.ps1`) against the WSL openMSX installation.
- QA sign-off is a separate, subsequent gate (`docs/m19-qa-signoff.md`), not attempted here.

---

## 9. Addendum (coordinator follow-up, same milestone) — `roms/metalgear.rom` second smoke fixture

Two small, additive follow-ups were made after initial delivery, per the coordinator's review
feedback (RESP-M19-003):

### 9.1 Comment-precision fix (`cartridge_konami_rom.cpp`)

The coordinator flagged that the inline comment directly above `mem_write` still said "window-slot
2 (and, via the mirror, slots 0/1) never move again after reset()" — the same imprecision the
Konami unit test already correctly debunks (window-slot 1 is NOT fixed; it mirrors window-slot 3's
LIVE value). Fixed: the comment now states the corrected claim (only window-slots 0 and 2 are
permanently fixed; window-slot 1 tracks window-slot 3 live) and points to the fuller derivation
already present in `cartridge_konami_rom.h`'s class-level doc comment. No behavioural change — this
was a comment-only edit; the shipped algorithm was always correct.

### 9.2 `roms/metalgear.rom` — second real-file smoke fixture, disposition: **identified `Konami`**

Unlike `roms/aleste.rom` (genuinely unconfirmable, A-M19-10), `roms/metalgear.rom`'s real-world
identity as a `Konami`-mapper "Metal Gear" (Konami, 1987, MSX2) dump **is** directly confirmed by a
concrete, citable source already in this repo:

- **Exact SHA1 hash match, not a genre/heuristic inference.** This file's SHA1
  (`919fa773e1f239dc90fa47e2770f3f5eca7f9bfe`, computed independently both by hand during
  development and inside the new test itself) matches, byte-for-byte, one of the
  `<dump><megarom><type>Konami</type><hash>919fa773e1f239dc90fa47e2770f3f5eca7f9bfe</hash>
  </megarom></dump>` entries recorded under `<title>Metal Gear</title>`
  (`<genmsxid>1028</genmsxid>`, `<company>Konami</company>`, `<year>1987</year>`,
  `<system>MSX2</system>`, `<country>JP</country>`) in
  `references/openmsx-21.0/share/softwaredb.xml`. That same title entry ALSO records a few
  `ASCII8`/`KonamiSCC`/`ReproCartridgeV1` dumps under **different** hashes — this specific file's
  hash is recorded ONLY under `Konami`, so the type claim is tied to the exact byte content, not
  merely "Metal Gear games are usually Konami-mapper."
- **Independent live-openMSX corroboration.** Mounted the identical file in real WSL openMSX 19.1
  via `-carta roms/metalgear.rom -romtype Konami`: it loaded without any rejection/fatal error
  (`carta` reported `roms/metalgear.rom` mounted), and reading CPU memory 0x4000/0x4001 (after
  `debug write ioports 0xA8 0xF7`, routing primary slot 1 into CPU page 1 — the identical mechanism
  this project's own test uses) returned `0x41`/`0x42` ('A'/'B'), matching this emulator's own
  result byte-for-byte.

Given this — an exact hash match to a specifically `Konami`-typed catalogued dump, PLUS
independent acceptance by a mature, independent reference emulator under the same type — the
disposition chosen is to load `roms/metalgear.rom` as `Konami` (not the disclaimed `Generic8kB`
pattern used for `aleste.rom`).

**Scope discipline preserved, per the coordinator's explicit instruction:** this file is still
NEVER used for any byte-exact bank-switch/protocol correctness claim — those remain exclusively on
synthetic, hand-authored fixtures (unchanged from the original M19 discipline). The new test
(`tests/integration/machine/hbf1xv_m19_metalgear_smoke_integration_test.cpp`, same
`SONY_MSX_ROMS_DIR` compile-definition mechanism as the aleste smoke test) asserts ONLY: file size
(139,264 bytes = 17 × 8 KB banks); an independently-computed SHA1 matching the softwaredb.xml-cited
hash; an independently-computed SHA256 matching `docs/asset-checksums.txt`'s recorded value; the
file loads without error as `Konami` and the active mapper reports `Konami`; and, after
`cold_boot()` + routing primary slot 1 into CPU page 1, `debug_bus_read(0x4000) == 0x41` /
`debug_bus_read(0x4001) == 0x42`. No claim is made about actual gameplay correctness or about any
bank beyond the two header bytes directly observed.

### 9.3 Updated evidence gates (fresh, full re-run after both changes)

```
tools/validate-assets.ps1        -> Asset validation result: True (7 BIOS files, 2 ROM files, unchanged)
tools/checksum-assets.ps1        -> docs/asset-checksums.txt refreshed (both roms/ SHA256 values unchanged:
                                     aleste.rom    4ea1e183467abe094df88901fc5dc70f0df5c1fc424de19f212692001bd5ad43
                                     metalgear.rom 399447d8012035b5a97dd3aec235a8e7d03b8da499196b6f047e1c7290a35760)
cmake --build build --config Debug -> 0 errors (only pre-existing C4819 codepage warnings, unrelated)
ctest --test-dir build -C Debug --output-on-failure
  -> 100% tests passed, 0 tests failed out of 92
     (91 prior M1-M19-initial + 1 new metalgear smoke test)
```

Zero regression: all 91 previously-green tests (including all 10 original M19 test executables)
remained green; the one new test executable
(`machine_hbf1xv_m19_metalgear_smoke_integration_test`) passed on first run after the mask-fallback
value derivation above was confirmed correct against the actual file bytes.

### 9.4 Files added/changed this addendum

- New: `tests/integration/machine/hbf1xv_m19_metalgear_smoke_integration_test.cpp`.
- Modified (additive): `tests/CMakeLists.txt` (+1 test target, reusing the existing
  `SONY_MSX_ROMS_DIR_ABS` variable); `src/devices/cartridge/cartridge_konami_rom.cpp`
  (comment-only fix, §9.1); `docs/asset-checksums.txt` (refreshed, byte-identical content);
  `docs/m19-implementation-report.md` (this addendum); `agent-protocol/channels/responses.md`
  (RESP-M19-003).
