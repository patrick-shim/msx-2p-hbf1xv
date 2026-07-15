# M19 QA Sign-off — Cartridge ROM Loading: External Primary Slots 1 & 2

- Milestone: M19 (REQ-M19-002, with a follow-up round per RESP-M19-003).
- QA Owner: MSX QA Agent.
- Date: 2026-07-07.
- Gate: M19 retains the NORMAL human-release-decision gate (per the planner package §5 item 12
  and DEC-0003's auto-close grant being M12-only). A QA Pass lets the coordinator PRESENT M19 to
  the human for the release decision + tag; it does NOT auto-close M19 and does NOT itself
  authorize release.
- Verdict: **PASS** (with one non-blocking Required Fix — a stale documentation comment; see §4/§7).

All results below were produced or independently reproduced by QA (fresh reconfigure, fresh
rebuild, fresh `ctest` run, direct reads of every changed/new source file, direct reads of the
cited `references/openmsx-21.0/` files, `git diff` inspection against the `v1.0.18` tag baseline,
an independent SHA1 + SHA256 recomputation of both `roms/aleste.rom` and `roms/metalgear.rom`, a
direct grep of `references/openmsx-21.0/share/softwaredb.xml` for the metalgear SHA1, and an
independent, from-scratch WSL openMSX 19.1 session built and driven by QA itself against the real
`Sony_HB-F1XV` machine — developer, planner, and coordinator claims were not accepted at face
value).

---

## 1. Regression Scope

Affected surface for M19 (closes backlog B7; adds new backlog rows G1-G4): one wholly new device
family (`src/devices/cartridge/`) plus additive-only wiring in `hbf1xv_machine.{h,cpp}` and
`main.cpp`. Zero edits to any existing device internals (CPU, VDP, PSG, RTC, FDC, YM2413, Kanji,
printer, cassette, PPI, joystick).

New source verified by QA directly (full read, not just diff):
- `src/devices/cartridge/cartridge_rom_window.{h,cpp}` — `CartridgeRomWindow`, the shared 8-slot
  x 8 KB bank-resolution primitive.
- `src/devices/cartridge/cartridge_mapper_type.{h,cpp}` — 6-value enum + openMSX-canonical name
  parsing.
- `src/devices/cartridge/cartridge_mapper_device.h` — family-local interface (X-pattern).
- `src/devices/cartridge/cartridge_{mirrored,generic8kb,generic16kb,ascii8kb,ascii16kb,konami}
  _rom.{h,cpp}` — the six MVP mapper devices.
- `src/devices/cartridge/cartridge_slot.{h,cpp}` — `CartridgeSlot` wrapper (load/unload/reset
  dispatch, empty-slot open-bus default).
- `src/machine/cartridge_cli.{h,cpp}` — pure argv parser.
- `src/machine/hbf1xv_machine.{h,cpp}` (modified, additive only, confirmed via full `git diff
  v1.0.18` read): two new members, a `wire_bus()` attach loop for both cartridge slots (NO
  `set_expanded` call for either), two `.reset()` calls in `cold_boot()`,
  `load_cartridge`/`unload_cartridge`/`cartridge_slot1()`/`cartridge_slot2()` accessors.
- `src/main.cpp` (modified, additive only) — `load_cartridges_from_args()` shared helper, wired
  into both the default run path and the extended `--parity-trace` mode.
- `CMakeLists.txt` / `tests/CMakeLists.txt` (modified, additive, confirmed via `git diff v1.0.18
  --stat`: +10/+89 lines respectively, 0 deletions) — 9 new source files, 13 new test targets
  (10 initial M19 + 1 metalgear follow-up test target added in the same cycle, per the diff).
- **Zero-touch confirmed by QA directly:** `git diff v1.0.18 --stat -- src/devices/cpu/ src/core/`
  returns empty output; `git diff v1.0.18 -- src/machine/hbf1xv_machine.cpp | grep -n
  "step_cpu_instruction\|run_cycles\|run_frame\|elapsed_cycles"` returns empty output (exit code
  1) — QA ran both commands itself, not merely re-quoted the developer's claim. `src/machine/
  rom_asset_loader.{h,cpp}` show no diff at all since `v1.0.18` (in fact, `git log` shows the last
  commit touching either file is the M13 commit `a9941e9`) — confirms the new cartridge-CLI
  failure policy did not leak into the BIOS/Kanji-font/disk-image graceful-degradation path.

Test/tooling additions (all read in full or spot-checked by QA): `tests/unit/devices/cartridge/
{cartridge_rom_window,cartridge_mapper_type,cartridge_mirrored_rom,cartridge_generic8kb_rom,
cartridge_generic16kb_rom,cartridge_ascii8kb_rom,cartridge_ascii16kb_rom,cartridge_konami_rom,
cartridge_slot}_unit_test.cpp`, `tests/unit/machine/cartridge_cli_unit_test.cpp`, `tests/
integration/machine/{hbf1xv_m19_cartridge_integration_test,hbf1xv_m19_aleste_smoke_integration_test,
hbf1xv_m19_metalgear_smoke_integration_test}.cpp`, `tools/gen-m19-cartridge-probe.py`, `tools/
openmsx-m19-cartridge-parity.ps1`, `tests/parity/{m19_cartridge.rom,m19_cartridge_probe.bin}`,
`docs/m19-parity-trace-diff.md`.

Regression-critical protected areas checked: X4 (CPU T-state math / M9-M12 timing oracles — no
`src/devices/cpu/` or `src/core/` files appear anywhere in the diff since `v1.0.18`), `#A8`
slot-select (untouched — `SlotBus::sub_for_page` unmodified, confirmed by reading
`slot_bus.cpp:51-56`), M11/M13 slot tests, M13-M18 boot goldens, M14 VDP (untouched), M15 device
suite (untouched), M16 FDC suite (untouched), M17 YM2413 suite (untouched), M18 peripheral I/O
suite (untouched).

## 2. Regression Matrix Status

| Area | Coverage | QA result |
|------|----------|-----------|
| Build (headless, SDL3 OFF) | fresh `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` + `cmake --build build --config Debug` | **PASS — 0 errors** (QA-executed, own tree) |
| Full deterministic suite | `ctest --test-dir build -C Debug --output-on-failure` | **PASS — 100% tests passed, 0 failed out of 92** (QA-executed, fresh, own run — not the developer's captured output) |
| `CartridgeRomWindow` bank-resolution (S1) | `devices_cartridge_rom_window_unit_test` | PASS — read source; mask-as-fallback-only confirmed byte-exact against `RomBlocks.cc:107-118` directly (§3.1) |
| `CartridgeMapperType` enum/parsing (S1) | `devices_cartridge_mapper_type_unit_test` | PASS — canonical name strings independently cross-checked against `RomInfo.cc:19-27,92` (§3.2) |
| Six MVP mapper devices (S2) | `devices_cartridge_{mirrored,generic8kb,generic16kb,ascii8kb,ascii16kb,konami}_rom_unit_test` | PASS — each read in full and compared line-by-line against its cited openMSX source (§3.3-§3.5); Konami quirk independently re-derived and confirmed correct (§3.4); Ascii16kB "both middle banks start identical" quirk confirmed against `RomAscii16kB.cc:24-27` (§3.5) |
| `CartridgeSlot` wrapper (S3) | `devices_cartridge_slot_unit_test` | PASS — read in full; empty-slot open-bus regression guard, load/unload/reset dispatch, and size-invalid-load-leaves-prior-state-untouched are all genuinely, non-tautologically exercised (§3.6) |
| Machine wiring + system integration (S4) | `machine_hbf1xv_m19_cartridge_integration_test` | PASS — QA read the full test; `slot_expanded(1)==false`/`slot_expanded(2)==false` genuinely asserted (§3.7); independence, cold-boot-preserves-cartridge, and unloaded-slot regression-guard cases all present |
| `roms/aleste.rom` mechanical smoke (S4) | `machine_hbf1xv_m19_aleste_smoke_integration_test` | PASS — QA read the full test; asserts ONLY file size/SHA256/2-byte header read-back; NO identity claim anywhere (§3.8) |
| `roms/metalgear.rom` identified-`Konami` smoke (follow-up) | `machine_hbf1xv_m19_metalgear_smoke_integration_test` | PASS — QA independently recomputed SHA1/SHA256 and grepped `softwaredb.xml`; independently reproduced the live-WSL-openMSX corroboration (§3.9) |
| CLI parser (S5) | `machine_cartridge_cli_unit_test` | PASS — order-independence, case-insensitivity, omitted-vs-unrecognized-type distinction, existing-behavior-unchanged case all present |
| CLI failure-policy isolation (S5) | direct source read + `git diff` | PASS — new loud/non-zero-exit policy confirmed scoped ONLY to `load_cartridges_from_args`; `rom_asset_loader.{h,cpp}` untouched since M13 (§3.10) |
| Two-machine / two-run determinism | every new unit + integration suite | PASS |
| X4 timing oracles (M9/M12) | full M1-M18 suite | PASS — all green in QA's own run; zero `src/devices/cpu/`/`src/core/` diff and zero `step_cpu_instruction`/`run_cycles`/`run_frame`/`elapsed_cycles` diff (QA-confirmed directly) rule out perturbation structurally, not just empirically |
| openMSX A/B — architectural + content parity | `tools/openmsx-m19-cartridge-parity.ps1` vs genuine WSL openMSX 19.1 `Sony_HB-F1XV`; QA read the raw captured trace files directly | **PASS — QA read `build/m19_cartridge_trace_{A,B}.txt` directly and confirmed byte-for-byte match across all 8 instructions**, including the AF field at the three read-back steps (§3.11) |
| openMSX A/B — slot-lettering (R-M19-6) | live WSL Tcl probe (developer) + QA's own independent metalgear WSL probe | PASS — QA independently reproduced a slot-1-routing round-trip (§3.9/§3.11) |
| Adversarial comparator self-check | developer-captured, QA read the artifact | PASS — empty-side → BLOCKED (exit 2), corrupted-field → DIVERGENCE (exit 1, 4 field mismatches), consistent with the shared `tools/trace-diff.py` comparator already proven trustworthy across M10-M18 |
| Full deferred-backlog review completeness | `docs/m19-planner-package.md` §4, `agent-protocol/state/deferred-backlog.md` | PASS — every row re-affirmed with justification; new Section F (G1-G4) added correctly, B7 marked `DONE (M19)` |

The 11 new M19 test executables (10 initial + 1 metalgear follow-up) are additive; the 81 prior
M1-M18 tests are unchanged and green (zero regression). QA counted 92/92 in its own fresh `ctest`
run, not from the developer's report.

## 3. Source-Level Verification (genuine, not stub)

### 3.1 Bank-resolution mask subtlety — the single most important correctness detail (R-M19-1)

QA read `references/openmsx-21.0/src/memory/RomBlocks.cc:107-118` (`setRom`) directly:

```cpp
block = (block < nrBlocks) ? block : block & blockMask;
if (block < nrBlocks) { setBank(region, &rom[block * BANK_SIZE], ...); }
else { setBank(region, unmappedRead.data(), 255); }
```

and compared it line-by-line against `src/devices/cartridge/cartridge_rom_window.cpp`'s
`set_bank`:

```cpp
unsigned block = requested_block;
if (block >= num_blocks_) { block &= block_mask_; }
if (block < num_blocks_) { slots_[slot] = SlotState{true, block}; }
else { slots_[slot] = SlotState{false, 0}; }
```

**Confirmed: the mask is consulted ONLY as a fallback for an already-out-of-range request, never
an unconditional AND applied to every request.** This is the exact algorithmic shape of
`RomBlocks::setRom`, not a plausible-sounding paraphrase. QA also confirmed the default mask
(`block_mask_ = num_blocks_ > 0 ? num_blocks_ - 1 : 0` in the constructor) matches `RomBlocks.cc:47`
(`blockMask = nrBlocks - 1`), and that `CartridgeKonamiRom`'s constructor calls
`window_.set_block_mask(31)`, matching `RomKonami.cc:24` (`setBlockMask(31)`) exactly. The unit test
(`cartridge_rom_window_unit_test.cpp`) explicitly exercises both branches for both a default and an
overridden mask, including the oversized (>256 KB) Konami-image case where the mask never engages —
QA independently re-derived this case by hand (`35 < 40` `nrBlocks` → direct, unmasked, even though
`35 > 31` the mask) and confirms it is asserted in `cartridge_konami_rom_unit_test.cpp`
(`OversizedImage_MaskNeverEngages_BankRequestBeyond31Works`).

### 3.2 Canonical mapper-type name strings

QA read `references/openmsx-21.0/src/memory/RomInfo.cc:19-27,92` directly and confirms
`cartridge_mapper_type.cpp`'s six string literals (`"Mirrored"`, `"8kB"`, `"16kB"`, `"ASCII8"`,
`"ASCII16"`, `"Konami"`) match openMSX's own canonical names exactly, not an invented vocabulary.

### 3.3 The Konami mirror-quirk — genuine grounding finding, correctly resolved in the SHIPPED CODE

QA read `references/openmsx-21.0/src/memory/RomKonami.cc` (all 82 lines, in full) directly, not
merely the developer's transcription. Confirmed byte-exact:
- `writeMem` (`RomKonami.cc:61-67`) only calls `bankSwitch(addr>>13, value)` for
  `0x6000 <= addr < 0xC000`, i.e. page ∈ {3,4,5} only — page 2 (`0x4000-0x5FFF`) is never a write
  target.
- `bankSwitch(2, block)` (`RomKonami.cc:38-52`) is therefore invoked exactly once, from `reset()`
  (`RomKonami.cc:54-59`, `bankSwitch(2,0)`) — window-slot 2 AND its mirror window-slot 0 (the
  `page==2` branch mirrors into `page-2=0`) are BOTH genuinely, permanently fixed at bank 0.
- `bankSwitch(3, block)` mirrors into window-slot 1 (`page-2=1`) on EVERY write to page 3
  (`0x6000-0x7FFF`) — window-slot 1 is NOT fixed; it tracks window-slot 3's LIVE value.
  Symmetrically, window-slot 6 tracks window-slot 4 (page 4) and window-slot 7 tracks window-slot 5
  (page 5).

`src/devices/cartridge/cartridge_konami_rom.cpp` is a literal, byte-exact translation of this
algorithm — confirmed correct by direct comparison, statement by statement, against
`RomKonami.cc:38-52,61-67`. `tests/unit/devices/cartridge/cartridge_konami_rom_unit_test.cpp` (read
in full by QA) correctly asserts the CORRECTED behavior: slots 0 and 2 genuinely immovable after
many writes across the full `0x6000-0xBFFF` range (`AfterManyWrites_Slot0_StillFixedAtBank0`,
`AfterManyWrites_Slot2_StillFixedAtBank0`), while slot 1 is explicitly asserted to track slot 3's
live value (`Write_Page3_MirrorSlot1_TracksLiveValue`) — a genuinely non-tautological, corrected
test, not a restatement of the imprecise "0/1/2 all fixed" shorthand.

**Finding (non-blocking, documentation-only — see §4/§7 for full detail): the corresponding
class-level doc comment in `src/devices/cartridge/cartridge_konami_rom.h` (lines 38-44) was NOT
actually corrected**, and still reads "...slots 0/1 too, slots 0/1/2 ALL stay permanently at bank 0
for the entire session" — the exact overstatement the `.cpp` file's own (correctly fixed) inline
comment and the unit test both explicitly debunk. `RESP-M19-003`'s claim that the fix "points to the
fuller derivation already present in `cartridge_konami_rom.h`'s class-level doc comment" is
therefore inaccurate — that derivation, as it stands in the `.h` file today, restates the disproven
claim rather than the corrected one. QA grepped the whole `src/` tree for the string
`"slots 0/1/2 ALL stay permanently"` and confirms this is the ONLY remaining occurrence (i.e. this
is an isolated, narrow miss — not evidence of a broader, unaddressed misunderstanding elsewhere).
**This does not affect shipped behavior**: the `.cpp` implementation (§ above), the unit test, and
the implementation report all state the correct, grounded claim; only this one header docstring is
stale.

### 3.4 Other five mapper devices — spot-verified line-by-line against their cited openMSX sources

QA read `RomGeneric8kB.cc`, `RomGeneric16kB.cc`, `RomAscii8kB.cc`, `RomAscii16kB.cc`, and
`RomPlain.cc` (MIRRORED branch, lines 85-98) directly and compared each against its corresponding
`src/devices/cartridge/cartridge_*_rom.cpp`:
- `Generic8kB`: `reset()` (slots 0/1/6/7 unmapped, slots 2-5 = banks 0-3) and `mem_write` (`slot =
  addr>>13`, writable at any address) match `RomGeneric8kB.cc:13-27` exactly.
- `Generic16kB`: `reset()` (logical bank 0 unmapped, bank1=image-bank0, bank2=image-bank1, bank3
  unmapped) and `mem_write` (`bank = addr>>14`) match `RomGeneric16kB.cc:12-23` exactly.
- `Ascii8kB`: `reset()` (slots 2-5 all = bank 0) and `mem_write` (`0x6000<=addr<0x8000`,
  `region=((addr>>11)&3)+2`) match `RomAscii8kB.cc:24-41` exactly.
- `Ascii16kB`: `reset()`'s genuine quirk — **both middle logical banks (1 and 2) start at image
  bank 0, not sequential 0/1** — and `mem_write`'s address-range exclusion (`0x6000<=addr<0x7800`
  AND `(addr&0x0800)==0`, excluding `0x6800-6FFF`/`0x7800-7FFF`) match `RomAscii16kB.cc:22-36`
  exactly; the unit test explicitly asserts both middle banks are identical at reset (R-M19-2).
- `Mirrored`: `reset()` calls `window_.set_bank(s, s)` for every slot `s`, letting the shared
  window's own mask-fallback resolve the mirror via `s & (nrBlocks-1)` — QA confirms this is the
  literal bitwise-AND `RomPlain.cc:92-93` uses (`romPage & (numPages - 1)`), NOT a true modulo,
  which the implementation report correctly flags as a deliberate, non-obvious equivalence (they
  coincide only when `nrBlocks` is a power of two).

No discrepancy found in any of the five; each is a genuinely correct, verified-by-QA-from-
primary-source implementation.

### 3.5 (folded into 3.3/3.4 above — no separate content)

### 3.6 `CartridgeSlot` wrapper — genuine, non-tautological regression guard

QA read `tests/unit/devices/cartridge/cartridge_slot_unit_test.cpp` in full (159 lines). The
empty-slot case exercises `mem_read` at three distinct addresses plus a no-op `mem_write` plus a
no-op `reset()`; the size-invalid-load case genuinely perturbs bank state away from its reset
default BEFORE the invalid load attempt, then asserts BOTH the mapper type AND the perturbed state
survive untouched — a real regression guard, not a tautology. QA also read `cartridge_slot.cpp`'s
`load()` directly and confirms validation happens strictly BEFORE `std::make_unique` construction
for every one of the 6 `case` branches (no code path constructs-then-rejects).

### 3.7 Machine wiring — `slot_expanded` regression guard (R-M19-8) and additive-only diff

QA ran `git diff v1.0.18 -- src/machine/hbf1xv_machine.cpp src/machine/hbf1xv_machine.h` itself and
confirms the diff is purely additive (two new members, one `wire_bus()` attach loop, two
`cold_boot()` reset calls, and a self-contained new API block) — zero lines removed or altered in
any pre-existing function body. QA confirms `wire_bus()`'s two `slot_bus_.set_expanded(...)` calls
(lines 39-40, for primary slots 0 and 3 only) are unchanged and that the new cartridge-slot attach
loop calls only `slot_bus_.attach(1, 0, page, ...)` / `slot_bus_.attach(2, 0, page, ...)` — no
`set_expanded(1, ...)`/`set_expanded(2, ...)` anywhere. QA read
`tests/integration/machine/hbf1xv_m19_cartridge_integration_test.cpp` and confirms
`expect(!machine.slot_expanded(1), ...)` / `expect(!machine.slot_expanded(2), ...)` are genuinely
present and executed (not merely mentioned in a comment).

### 3.8 `roms/aleste.rom` disposition discipline — confirmed non-claiming

QA read `tests/integration/machine/hbf1xv_m19_aleste_smoke_integration_test.cpp` in full. The file's
own header comment states, in capitals, "MECHANICAL SMOKE CHECK ONLY — NO GAMEPLAY/HARDWARE-
IDENTITY CLAIM." The `main()` body asserts exactly three things: file size (2,097,152 bytes), an
independently-computed SHA256 (QA re-derived the same digest independently below, §3.11), and two
header bytes (`0x41`/`0x42`) after routing primary slot 1 into CPU page 1 via `debug_io_write(0xA8,
0xF7)`. No mapper-identity or gameplay-correctness claim appears anywhere in the file. QA grepped
`src/` for `"Aleste"`/`"aleste"` and found zero occurrences outside this test and its own comments —
confirming the discipline holds project-wide, not just in this one file.

### 3.9 `roms/metalgear.rom` disposition discipline — independently re-verified, not merely re-read

This is the genuinely NEW claim this round (an IDENTIFIED `Konami` dump, not a disclaimed
mechanical-only fixture), so QA verified it from first principles rather than trusting the
developer's or coordinator's transcription:

1. **Independent SHA1/SHA256 recomputation.** QA computed both hashes of `roms/metalgear.rom`
   directly (`Get-FileHash`/Python `hashlib`, cross-checked both ways):
   `SHA1 = 919fa773e1f239dc90fa47e2770f3f5eca7f9bfe`,
   `SHA256 = 399447d8012035b5a97dd3aec235a8e7d03b8da499196b6f047e1c7290a35760`, file size
   `139,264` bytes — all three match the test file's asserted constants and
   `docs/asset-checksums.txt`'s recorded value exactly.
2. **Independent `softwaredb.xml` grep.** QA ran `grep -n
   "919fa773e1f239dc90fa47e2770f3f5eca7f9bfe" references/openmsx-21.0/share/softwaredb.xml`
   directly and located the exact line (`<dump><megarom><type>Konami</type><hash>
   919fa773e1f239dc90fa47e2770f3f5eca7f9bfe</hash></megarom></dump>`), then read upward to confirm
   it falls under `<title>Metal Gear</title>`, `<genmsxid>1028</genmsxid>`, `<system>MSX2</system>`,
   `<company>Konami</company>`, `<year>1987</year>`, `<country>JP</country>` — genuinely a
   "Metal Gear" / Konami / 1987 title block, not merely "some Konami-typed hash somewhere." QA also
   confirms (by reading the surrounding block, lines 7799-7834) that the SAME title records several
   `ASCII8`/`KonamiSCC`/`ReproCartridgeV1`-typed dumps under DIFFERENT hashes, corroborating that
   this file's specific hash is tied to the `Konami` type claim, not a genre generalization.
3. **Independent live-WSL-openMSX reproduction.** QA has WSL access (`Ubuntu-24.04`, openMSX 19.1
   confirmed via `openmsx -v`) and independently drove a fresh Tcl probe (written by QA, not reusing
   the developer's harness) mounting the identical file: `openmsx -machine Sony_HB-F1XV -carta
   roms/metalgear.rom -romtype Konami -command 'set renderer none' -script <qa's own script>` —
   loaded without any rejection/fatal error, and after `debug write ioports 0xA8 0xF7` (routing CPU
   page 1 to primary slot 1), `debug read memory 0x4000`/`0x4001` returned `0x41`/`0x42` ('A'/'B'),
   matching this repo's own emulator result byte-for-byte. **This is a genuine, independent,
   first-hand reproduction, not a re-statement of the developer's or coordinator's captured
   session.** (This also independently corroborates R-M19-6's slot-lettering claim: if `-carta` had
   NOT landed in primary slot 1, routing page 1 to slot 1 via `0xF7` would have read back open-bus
   `0xFF`/`0xFF`, not the cartridge's real header bytes.)
4. QA read `tests/integration/machine/hbf1xv_m19_metalgear_smoke_integration_test.cpp` in full and
   confirms it asserts ONLY: file size, SHA1-vs-softwaredb-cited-hash match, SHA256-vs-
   asset-checksums match, successful `Konami` load + `mapper_type()==Konami`, and the two header
   bytes — no byte-exact bank-switch/protocol correctness claim is smuggled in; those remain
   exclusively on the synthetic fixtures used by the S1/S2 unit tests and the S6 A/B probe.

### 3.10 CLI failure-policy isolation

QA read `src/machine/rom_asset_loader.{h,cpp}` in full and confirms neither file references
`CartridgeLoadResult`, `cartridge_cli`, or any M19 symbol at all; `RomAssetLoader::load`'s
missing/unreadable/wrong-size handling is byte-for-byte the same non-fatal 0xFF-fill +
diagnostic-append policy it has had since M13 (`git log` shows the last commit touching either file
is `a9941e9`, the M13 close commit — zero M19-cycle edits). QA read `src/main.cpp`'s
`load_cartridges_from_args` and confirms the loud-stderr/non-zero-exit policy is entirely local to
this one function and its two call sites (default run path, `--parity-trace` mode); it never calls
into `RomAssetLoader` or `Hbf1xvMachine::load_rom_assets()`.

### 3.11 openMSX A/B evidence — QA read the raw captured artifacts directly, not just the summary doc

QA opened `build/m19_cartridge_trace_A.txt` (this emulator's trace) and
`build/m19_cartridge_trace_B.txt` (openMSX 19.1's trace) directly and confirmed, instruction by
instruction, that all 8 lines match on PC, opcode, and every register field, including the AF field
at the three `LD A,(addr)` steps: `AF=0000` (reset-default bank 0), `AF=0300` (after the bank-switch
write to bank 3), `AF=0100` (the unaffected window-slot, still bank 1) — on BOTH sides, identically.
`build/m19_cartridge_trace_diff.md` independently states "ARCHITECTURAL PARITY -- EMPTY DIFF,"
consistent with QA's own direct comparison. This is a genuine, content-bearing A/B result, not
merely architectural, and QA did not simply trust the summary — the raw per-instruction traces were
read and compared directly.

Additionally, QA independently confirmed the ACTUAL installed WSL openMSX 19.1 machine XML
(`/usr/share/openmsx/machines/Sony_HB-F1XV.xml`) declares `<primary external="true" slot="1"/>` and
`<primary external="true" slot="2"/>` at lines 117/119 — matching this repo's reference copy
(`references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:119,121`) exactly, independently
reproducing the developer's own cross-check of this fact.

## 4. Failures and Risk Ranking

No functional/behavioral failures found. No test was weakened, deleted, or rewritten to
accommodate incorrect behavior. No M9/M12 CPU-timing-oracle test file was touched at all
(confirmed via `git diff v1.0.18 --stat -- tests/`: only new files and additive edits to
`tests/CMakeLists.txt` appear).

### Risk ranking (residual)

- **Low, non-blocking — stale documentation comment in `src/devices/cartridge/
  cartridge_konami_rom.h` (§3.3).** The class-level doc comment (lines 38-44) still states the
  disproven "slots 0/1/2 ALL stay permanently at bank 0" claim, contradicting the correctly-fixed
  `.cpp` inline comment, the unit test, and the implementation report's own §2.1/§9.1. This is
  purely a documentation/comment defect — the shipped device behavior is correct and thoroughly
  tested (§3.3) — but it is a genuine gap between what `RESP-M19-003` claimed was fixed ("points to
  the fuller derivation already present in the header") and what the header actually contains
  today. Left uncorrected, a future maintainer reading only the header (a very likely first stop)
  would be misled by the exact overstatement this milestone went out of its way to debunk elsewhere.
  See §7 for the recommended fix.
- **Low — `KonamiSCC` + embedded SCC sound chip (backlog G1), ROM-database/heuristic
  auto-detection (backlog G2), full runtime hot-plug (backlog G3), and the ~80-type long tail
  (backlog G4) are all explicitly out of scope.** Correctly disclosed and tracked as new OPEN rows,
  consistent with the M14/M17/M18 contract-vs-depth precedent.
- **Low — `roms/aleste.rom`'s real-world identity remains genuinely unconfirmed.** Correctly
  disclosed, used only as a mechanical smoke fixture under the non-hardware-claiming `Generic8kB`
  type; QA independently confirmed no identity claim appears anywhere (§3.8).
- **None — CPU-stepping loop / X4 timing oracles untouched.** Confirmed by direct `git diff v1.0.18`
  inspection (empty for `src/devices/cpu/`/`src/core/`; zero `step_cpu_instruction`/`run_cycles`/
  `run_frame`/`elapsed_cycles` occurrences in the machine-file diff) — structurally, not just
  empirically, ruled out.
- **None — `RomAssetLoader`/BIOS graceful-degradation path untouched.** Confirmed via `git log`
  (last touching commit is the M13 close) and direct full-file reads (§3.10).

No new risk was introduced that is not already disclosed in `docs/m19-implementation-report.md` §7
(and its §9 addendum) or the planner package §6, aside from the one documentation-staleness finding
QA itself surfaced in §3.3 (which the developer's own follow-up round did not fully complete, despite
believing it had).

## 5. Full Deferred-Backlog Review Completeness

QA cross-checked `docs/m19-planner-package.md` §4 against the current
`agent-protocol/state/deferred-backlog.md` and confirms both re-affirm **every** row (A.B1-B9,
B.C1-C10, C.D1-D7, D.E1-E2, E.F1-F2), each with a one-line justification:

- **B7 → DONE (M19)**: correctly and specifically justified (shared `CartridgeRomWindow`, 6 MVP
  mapper devices, `CartridgeSlot` wrapper, CLI/API loading, real A/B evidence). QA confirms the
  `deferred-backlog.md` B7 row text also correctly reflects the follow-up `roms/metalgear.rom`
  round via the file's footer entry (dated 2026-07-07).
- **New Section F (G1-G4)**: QA confirms `deferred-backlog.md` contains a genuine new
  `## F. M19 cartridge-mapper-depth deferrals` heading (mirroring the existing `##
  C./D./E.` sectioning convention) with G1 (KonamiSCC+SCC chip), G2 (ROM-database/heuristic
  auto-detection), G3 (runtime hot-plug), and G4 (~80-type long tail) correctly scoped and
  grounded.
- **All other rows (B1-B6, B8-B9, C1-C10, D1-D7, E1-E2, F1-F2 from prior milestones)**: present,
  statuses unchanged from their last accurate disposition, each with a named candidate owner — no
  row silently dropped or renumbered away.

No backlog item was silently addressed, silently dropped, or force-closed without justification.

## 6. Boundary + Backlog Check

- No DEFERRED item was implemented ahead of schedule. Verified absent/untouched this cycle:
  KonamiSCC/embedded-SCC chip (G1), ROM-database auto-detection (G2), runtime hot-plug (G3), the
  ~80-type long tail (G4), Halnote firmware + slot-0-3 SRAM (B6), VDP rendering depth (D1-D7), Sony
  speed/pause (C8), SDL3 frontend (C9), exact cycle/write-timing (C1/C2/E2), ZEXALL (C3), YM2413 DSP
  audio synthesis (E1), FDC flux/DMK fidelity (C10), cassette tape-format fidelity (F1), printer
  image rendering (F2).
- `deferred-backlog.md` correctly records **B7 → DONE (M19)** and the four new **G1-G4 → OPEN**
  rows under a genuine new Section F; all prior rows re-affirmed with no silent drops. Footer
  updated same-cycle, including the `roms/metalgear.rom` follow-up round.

## 7. Required Fixes

**Not blocking this Pass**, but should be corrected before or immediately alongside the human
release decision (a one-line, comment-only change with zero behavioral impact, no rebuild-of-logic
required):

1. **Fix the stale class-level doc comment in `src/devices/cartridge/cartridge_konami_rom.h`
   (lines 38-44).** Replace "...slots 0/1 too, slots 0/1/2 ALL stay permanently at bank 0 for the
   entire session" with the corrected claim already used elsewhere in this milestone (e.g. mirror
   the `.cpp` file's own, already-correct `mem_write` comment): only window-slots 0 and 2 are
   genuinely, permanently fixed at bank 0; window-slot 1 mirrors window-slot 3's LIVE value (and
   symmetrically 4↔6, 5↔7). This is a documentation-only correction (the shipped algorithm has
   always been correct, per §3.3); no test or behavior changes are required.

No other blocking or non-blocking code changes are required before the human release decision.

## 8. Sign-off Decision

**PASS** (with one non-blocking Required Fix recorded in §7, to be applied before or alongside the
human release decision at the coordinator's discretion — it does not gate this Pass).

QA-executed evidence: fresh build clean (0 errors); **ctest 92/92 (0 failed)**, independently
re-run from a clean tree; the bank-resolution mask-as-fallback-only algorithm independently
verified byte-exact against `RomBlocks.cc:107-118`; the Konami mirror-quirk correction independently
re-derived from `RomKonami.cc:38-52,61-67` and confirmed correct in the SHIPPED device code and its
unit test (a genuine, narrow documentation-only staleness was found in the header's class-doc
comment and is recorded as a non-blocking Required Fix, §7); the other five mapper devices
independently spot-verified line-by-line against their cited openMSX sources; `roms/aleste.rom`'s
non-identity-claiming discipline confirmed by direct file read and a project-wide grep;
`roms/metalgear.rom`'s NEW, stronger `Konami`-identified disposition independently re-verified three
ways (own SHA1/SHA256 recomputation, direct `softwaredb.xml` grep confirming the "Metal Gear" /
Konami / 1987 title block, and a from-scratch WSL openMSX 19.1 Tcl probe reproducing the identical
load-and-read-back result) — going beyond the developer's own verification; the cartridge-CLI
loud/non-zero-exit failure policy confirmed structurally isolated from `RomAssetLoader` (zero diff
to `rom_asset_loader.{h,cpp}` since the M13 close commit); `machine.slot_expanded(1)==false`/
`slot_expanded(2)==false` confirmed genuinely asserted and holding; the openMSX A/B evidence's raw
trace files read directly by QA and confirmed to match instruction-for-instruction, including the
content-bearing AF read-back values; the full deferred-backlog review confirmed genuinely complete
(every row re-affirmed, B7 correctly closed, G1-G4 correctly added under a genuine new Section F,
none silently dropped); zero regression confirmed structurally (`git diff v1.0.18` shows no changes
to `src/devices/cpu/`, `src/core/`, or `RomAssetLoader`, and zero `step_cpu_instruction`/
`run_cycles`/`run_frame`/`elapsed_cycles` occurrences in the machine-file diff).

No blocker-level gaps remain. No fabricated hardware behavior found. No test was weakened or
tautological. The one residual finding (§3.3/§7) is a narrow, isolated, comment-only staleness with
zero effect on shipped behavior, evidence, or test correctness — it does not gate this Pass, but
should be swept up promptly since it directly contradicts a specific claim made in `RESP-M19-003`.

Per the milestone rule, this PASS authorizes the coordinator to PRESENT M19 for the human release
decision (normal gate); it does not auto-close M19 and does not itself authorize release. The
Required Fix in §7 is a recommended pre-tag cleanup, not a blocking condition.
