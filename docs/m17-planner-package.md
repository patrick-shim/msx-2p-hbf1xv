# M17 Planner Package — FM-PAC (Yamaha YM2413 OPLL) 9-Channel FM Synthesizer + MSX-JE 16 KB SRAM

- Milestone ID: M17
- Title: FM-PAC (Yamaha YM2413/OPLL) Register-Accurate Device + MSX-JE 16 KB Battery-Backed SRAM
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Request: REQ-M17-001 (planner-first, no production code)
- Decisions in force: DEC-0005 (backlog disposition discipline), DEC-0009 (Q4-seq: B4 "pairs with the
  FM-PAC/MSX-MUSIC milestone (now M17)" — **re-examined below with concrete grounding; see §3.3**)
- Backlog effect: **closes B3** (YM2413/OPLL register-accurate device); **B4 (MSX-JE 16 KB SRAM)
  re-grounded and NOT closed here** — see the dedicated finding in §3.3 and the full review in §4.
  All other rows re-affirmed (§4).
- Gate: **NORMAL human-release-decision gate (no auto-close)** — explicitly stated per the task
  brief and consistent with DEC-0003's auto-close grant being M12-only. Autopilot pauses at QA
  sign-off for the human release decision + tag, matching the M13/M14/M15/M16 pattern.

> Grounding rule: every behaviour claim below cites a concrete `references/...` path or a
> current-repo `src/...`/`docs/...` path. openMSX sources are the BEHAVIOUR reference only and are
> GPL — **never copy openMSX code into `src/`** (CLAUDE.md, `agent-protocol/guardrails.md`).

---

## 1. Scope and Assumptions

### 1.1 In scope

- **(a) YM2413 (OPLL) register-accurate device** — the 9-channel (or 6-melody + 5-rhythm) FM
  synthesizer named in the Target Machine Specification SOUND field, wired to the **real HB-F1XV
  I/O ports `#7C`/`#7D`** (write-only, per `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:190-196`)
  alongside the M13 FM-MUSIC ROM already mapped at slot 3-3 page 1.
- **(b) Register/channel/rhythm decode model** — full 64-register file, per-channel (0-8) decode
  (F-Number/Block/Key-on/Sustain/Instrument/Volume), rhythm-mode decode (register `$0E` + rhythm
  volumes `$36-$38`), and the 15-entry ROM melody instrument patch table + 3 rhythm patches.
  **Numeric/register-accurate only** — no audio waveform synthesis/DSP/output sample generation
  (see §3.2 for the precise operational definition, and §4/new-row E1 for what is explicitly
  deferred).
- **(c) MSX-JE 16 KB battery-backed SRAM** — addressed per the human directive, but **re-grounded**:
  §3.3 shows this store, on the real HB-F1XV, is NOT part of the YM2413/FM-PAC device; it belongs to
  the Halnote-mapped MSX-JE firmware ROM at **slot 0-3** (backlog B6). M17 extracts a reusable,
  deterministic, parametric-size battery-backed-store primitive (generalizing the M15 S1985 `.sram`
  pattern) so B6/Halnote can wire it directly later — standalone and unit-tested, **not attached to
  any slot** in M17 (no real consumer exists yet). This is presented as a recommendation requiring
  explicit human confirmation (§3.3, open question).
- **(d) Deterministic unit + integration tests**, zero regression M1-M16.
- **(e) Real openMSX A/B evidence** for the register-write path (§3.5) — no parity claim without a
  genuine capture.
- **(f) Full deferred-backlog review** — every row A.B1-B9 and B.C1-C10 re-affirmed with a
  justification (§4), per the human's explicit instruction.

### 1.2 Out of scope (named explicitly, each with justification)

| Deferred item | Why OUT of M17 | Owning milestone |
|---|---|---|
| **YM2413 FM-synthesis DSP/audio depth** (18-slot TDM pipeline, log-sin/exp operator, EG global-counter rate mechanism, phase generator, AM/VIB LFOs, feedback, rhythm noise/DAC, 49716 Hz sample generation, DAC nonlinearity) | Explicitly out per the task brief ("no audio waveform synthesis/presentation required this milestone"); mirrors the M14 VDP contract-vs-depth split (D1-D7). Proposed as **new backlog row E1** (§4). | Future audio-rendering milestone, paired with C9 (SDL3 frontend) |
| **YM2413 write-timing constraint** (≥12/84 master-cycle address/data write spacing) | Fact-sheet §8 explicitly notes "openMSX currently has the too-fast-access-timing emulation disabled" — M17 matches openMSX's default (unenforced) rather than inventing a timing model with no A/B reference behaviour to validate against. Proposed as **new backlog row E2**, grouped with C1 (exact cycle/timing parity). | Timing-fidelity milestone (with C1) |
| **MSX-JE Halnote firmware mapping** (slot 0-3, 1 MB `bios/f1xvfirm.rom`, `mappertype=Halnote`) | Backlog B6, unchanged scope from M13's D-3 deferral; a large, independent banked-ROM mapper. The *real* 16 KB SRAM belongs here (§3.3) — M17 does not wire it. | Halnote/MSX-JE milestone (B6) |
| **Kanji-font I/O `#D8-DB`, printer `#90/91`, cassette** | Backlog B5/C7 — unrelated peripheral I/O surfaces; explicitly the next milestone per the human's bundled M17+M18 directive. | M18 (peripheral I/O) |
| **Cartridge loading, FDC flux/DMK depth, VDP rendering depth, exact cycle timing, ZEXALL, Sony speed/pause, SDL3 frontend** | Unchanged from prior milestones' deferrals (§4). | Per §4 candidate owners |

### 1.3 Assumptions (each with a verification action)

- **A-M17-1 (device identity: `MSX-Music`, not `FM-PAC` cartridge).** The HB-F1XV's built-in sound
  device is openMSX class `MSXMusic` (XML `<MSX-MUSIC id="MSX Music">`,
  `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:180-196`), **not** `MSXFmPac` (the external
  Panasonic SW-M004 cartridge class, `references/openmsx-21.0/src/sound/MSXFmPac.{hh,cc}`). The two
  differ materially: `MSXMusicBase` (`references/openmsx-21.0/src/sound/MSXMusic.cc:9-50`) is a fixed
  16 KB ROM + YM2413 with I/O-port-only access and **no SRAM, no bank register, no memory-mapped
  register overlay**; `MSXFmPac` (`MSXFmPac.cc`) adds a 4-bank ROM, an 8 KB SRAM with a magic-byte
  handshake, and memory-mapped register access at `0x3FF4-0x3FF7` inside the ROM window. The YM2413
  fact-sheet's FM-PAC cartridge details (bank register `7FF7h`, SRAM handshake `4Dh/69h` at
  `5FFEh/5FFFh`, ID strings `PAC2OPLL`/`APRLOPLL`, memory-mapped `7FF4h/7FF5h`) describe the **external
  cartridge**, which is **not what ships in this machine**. *Verify:* re-read
  `Sony_HB-F1XV.xml:179-197` (already done, cited) — no `<sramname>` tag on `<MSX-MUSIC>`, only
  `<io base="0x7C" num="2" type="O"/>`.
- **A-M17-2 (no memory-space register overlay — no "wrap" needed).** Because `MSXMusicBase::peekMem`
  is a plain masked ROM read (`MSXMusic.cc:37-50`, `&rom[start & (rom.size()-1)]`) with no register
  overlay, the M13 `fmmusic_rom_` `RomDevice` at slot 3-3 page 1 needs **no wrapping device** (unlike
  M16's `SonyFdc`, which had to wrap `disk_rom_` because the WD2793 registers *are* memory-mapped
  inside the ROM window). The YM2413 device is a **separate `IoDevice`** attached only at `#7C/#7D`,
  alongside the unmodified `fmmusic_rom_`. *Verify:* `src/machine/hbf1xv_machine.cpp:71`
  (`slot_bus_.attach(3, 3, 1, &fmmusic_rom_)`) stays untouched; only `io_bus_` gains two new
  attachments (mirrors the PSG precedent at `hbf1xv_machine.cpp:90-92`).
- **A-M17-3 (two-port write protocol, address-latch masking).** Port `#7C` (address) stores the
  latched register index; port `#7D` (data) writes `regs_[latch & 0x3F] = value`. Grounded exactly:
  `references/openmsx-21.0/src/sound/YM2413Okazaki.cc:1368-1374`
  (`writePort`: `port==0` → `registerLatch = value` unmasked; `port==1` → `writeReg(registerLatch &
  0x3f, value)`). *Verify:* unit test writes `#7C=0x30+i`, `#7D=v` for each channel and asserts
  `register_value(0x30+i)==v`; also assert latch values `>0x3F` still mask correctly on the
  subsequent data write (e.g. latch `0xFF` → effective register `0x3F`).
- **A-M17-4 (reset zeroes all 64 registers).** Grounded:
  `references/openmsx-21.0/src/sound/YM2413Okazaki.cc:696,707-720` (`std::ranges::fill(reg, 0)` at
  construction; `reset()` calls `writeReg(i, 0)` for `i` in `[0, 0x40)`). *Verify:* unit test writes
  non-zero values across the register space, calls `reset()`, asserts all 64 bytes read back zero.
- **A-M17-5 (read port behaviour: open-bus).** The XML declares `#7C/#7D` **write-only**
  (`type="O"`, no `type="I"` entry), and `MSXMusicBase` overrides only `writeIO`
  (`references/openmsx-21.0/src/sound/MSXMusic.hh:14-15`), implying reads fall through to the
  device-default (open-bus) behaviour — consistent with the real chip having **no status/busy
  register and no readback** (fact-sheet §8 "No busy flag / no readback"). The M17 device's
  `io_read` therefore returns the machine's established open-bus value `0xFF` (same convention as
  `IoBus`'s unmapped-port default, `src/devices/chipset/io_bus.h:15`), making an `IN A,(#7C)` /
  `IN A,(#7D)` observably identical to the port being unattached. *Verify:* unit test asserts
  `io_read(0x7C)==io_read(0x7D)==0xFF` regardless of prior writes.
- **A-M17-6 (debug register readback for tests/A-B, distinct from the real chip's write-only
  nature).** Mirroring openMSX's own `YM2413::peekRegs()`/`Debuggable` pattern
  (`references/openmsx-21.0/src/sound/YM2413.{hh:26,40-44}`, a `SimpleDebuggable` named
  `getName() + " regs"`, size `0x40`), the device exposes a **debug-only** `register_value(addr)`
  accessor (not CPU-bus-reachable) so unit tests and the A/B harness can assert exact register state
  without needing audio synthesis. *Verify:* mirrors the existing `PsgYm2149::register_value` /
  `S1985Engine::backup_byte` precedent (`src/devices/audio/psg_ym2149.h:72`).
  For A/B, the corresponding openMSX-side introspection is its own `"<id> regs"` debuggable
  (device id "MSX Music" per the XML → debuggable name `"MSX Music regs"`), reachable from the WSL
  Tcl console as `debug read {MSX Music regs} <addr>` (same mechanism class as `debug read memory
  <addr>` already used by `tools/openmsx-io-parity.ps1:87-95`, generalized to non-memory
  debuggables — openMSX's `debug read` command dispatches by debuggable name, not just "memory").
  *Verify (developer, before claiming A/B):* confirm this Tcl invocation actually returns register
  bytes against a real WSL openMSX run before relying on it (§3.5 / R-M17-6).
- **A-M17-7 (instrument ROM patch table provenance).** The fact-sheet §4 table is explicitly caveated
  as "community-standard... ~99% but not datasheet-certain," and separately notes the HB-F1XV XML
  selects `<ym2413-core>NukeYKT</ym2413-core>` (`Sony_HB-F1XV.xml:193`) — the die-shot-derived core
  openMSX adopted for accuracy (`references/openmsx-21.0/src/sound/YM2413NukeYKT.hh:1-40`, "close to
  how the real hardware is implemented"; contrast `YM2413Burczynski.hh` — an older MAME-derived
  table/fixed-point core). M17 encodes the fact-sheet's canonical table (verbatim, cited) for the
  register-accurate contract; reconciling it against NukeYKT's exact decoded per-field patches is
  deferred to whichever milestone implements DSP depth (E1), since only real audio synthesis would
  make the byte-exact difference observable. *Verify (E1 owner, later):* read
  `references/openmsx-21.0/src/sound/YM2413NukeYKT.cc` patch table before any audio-affecting claim.
- **A-M17-8 (assets present).** `bios/f1xvmus.rom` (16 KB FM-MUSIC) is already loaded and mapped by
  M13 (`src/machine/hbf1xv_machine.cpp:233`); M17 adds no new asset requirement. *Verify:*
  `tools/validate-assets.ps1`.

---

## 2. Spec Summary

### 2.1 src/ placement (per `src/CLAUDE.md`)

| File | Responsibility | Grounding |
|---|---|---|
| `src/devices/audio/ym2413_opll.h` / `.cpp` (**new**, alongside `psg_ym2149.{h,cpp}`) | The YM2413 register-accurate model: 64-byte register file, two-port write protocol (`#7C`/`#7D`), per-channel decode, rhythm decode, ROM instrument-patch table, `reset()`, debug `register_value()`. Implements `core::IoDevice`. **No audio synthesis.** | Fact-sheet §3-§6; `references/openmsx-21.0/src/sound/YM2413Core.hh` (interface shape); `YM2413Okazaki.cc` (register/latch semantics, cited per-claim above) |
| `src/devices/memory/battery_backed_sram.h` / `.cpp` (**new**, alongside `rom_device.{h,cpp}`) | Generalization of `S1985Engine::load_backup_ram/save_backup_ram` (`src/devices/chipset/s1985_engine.h:63-64`) into a reusable, parametric-size, deterministic byte-store with `.sram`-style file load/save. **Not wired to any slot in M17** — see §3.3. | M15 `.sram` precedent; S1985 fact-sheet §6; RomHalnote SRAM size grounding (§3.3) |

Machine wiring (in `src/machine/`, existing files, additive only):
- `src/machine/hbf1xv_machine.{h,cpp}` — add member `devices::audio::Ym2413Opll ym2413_;` and, in
  `wire_bus()`, `io_bus_.attach(0x7C, &ym2413_); io_bus_.attach(0x7D, &ym2413_);` (mirrors the PSG
  attachment style at `hbf1xv_machine.cpp:90-92`). Add `ym2413_.reset()` to the `cold_boot()` device
  reset sequence (alongside psg_/rtc_/ppi_ resets). Add accessors `ym2413()`
  (const/non-const), mirroring `psg()`/`rtc()`. **No change** to `slot_bus_.attach(3, 3, 1,
  &fmmusic_rom_)` (A-M17-2). The new `battery_backed_sram.*` primitive is added to
  `CMakeLists.txt`'s source list but is **not instantiated** in `Hbf1xvMachine` this milestone
  (standalone, unit-tested only) — see §3.3.
- No new `src/core/` types required — `core::IoDevice` already covers this device's bus
  participation.

Boundary compliance: the YM2413 model carries zero slot/PPI/CPU knowledge; the machine does all
composition, matching the M15 `PsgYm2149`/`Rp5c01` precedent.

### 2.2 YM2413 register/command model — what "register-accurate" means operationally

**Storage.** A `std::array<std::uint8_t, 64> regs_` (indices `$00-$3F`), written via the two-port
protocol (A-M17-3) and read back only via the debug accessor `register_value(addr)` (A-M17-6) — the
real chip is write-only over the CPU bus (A-M17-5).

**Per-channel decode** (9 channels, indices 0-8), computed on demand from `regs_` (fact-sheet §3
master register-map table):
- `f_number(c)` — 9-bit: `regs_[0x10+c] | ((regs_[0x20+c] & 1) << 8)`.
- `block(c)` — 3-bit: `(regs_[0x20+c] >> 1) & 0x7`.
- `key_on(c)` — bool: `regs_[0x20+c]` bit 4.
- `sustain(c)` — bool: `regs_[0x20+c]` bit 5 (release rate forced to 5 at key-off when set).
- `instrument(c)` — 4-bit: `regs_[0x30+c] >> 4` (0 = user patch, 1-15 = ROM patch).
- `volume(c)` — 4-bit attenuation: `regs_[0x30+c] & 0xF`.
- `patch(c)` — the resolved 2-operator parameter set: if `instrument(c)==0`, decoded LIVE from
  `regs_[0x00..0x07]` (user patch, mutable); else the fixed ROM patch entry `instrument(c)` from the
  static table below. Decoded fields per operator (mod/car): `AM, VIB, EG-TYP (sustained/percussive),
  KSR, MUL[3:0]` (reg `$00`/`$01`); `KSL[1:0], TL[5:0]` (mod only, reg `$02`); `waveform (DC/DM),
  FB[2:0]` (reg `$03`, feedback applies to the modulator only); `AR[3:0], DR[3:0]` (reg `$04`/`$05`);
  `SL[3:0], RR[3:0]` (reg `$06`/`$07`).

**Rhythm-mode decode** (register `$0E`, fact-sheet §3 table + §6):
- `rhythm_enabled()` — bit 5 (`R`).
- `bd_key()/sd_key()/tom_key()/cym_key()/hh_key()` — bits 4/3/2/1/0 respectively (per the fact-sheet
  §3 table column order `BD SD TOM T-CY HH` at bits `4 3 2 1 0`).
- Rhythm volumes (fact-sheet §6): `bd_volume() = regs_[0x36] & 0xF`; `hh_volume() = (regs_[0x37] >>
  4) & 0xF`; `sd_volume() = regs_[0x37] & 0xF`; `tom_volume() = (regs_[0x38] >> 4) & 0xF`;
  `cym_volume() = regs_[0x38] & 0xF`.
- Note (documented, not enforced as a side effect): when `rhythm_enabled()`, channels 6-8's
  `$16-$18`/`$26-$28` registers are conventionally fixed by firmware (Yamaha's recommended values,
  fact-sheet §6) and `$36-$38` are reinterpreted as rhythm volumes rather than channel 6-8
  instrument/volume — the decode functions are pure register views; callers/tests select the
  melody-channel view or the rhythm view based on `rhythm_enabled()`, exactly as real firmware would.

**ROM instrument patch table** (fact-sheet §4, reproduced verbatim as the canonical/community table;
caveat per A-M17-7 preserved in source comments) — a `static constexpr` 15×8 table (melody, 1-indexed
1-15) plus 3 rhythm patches (BD; SD/HH; TOM/T-CY), each row = raw `$00-$07` bytes:

```
 1 Violin          71 61 1E 17 D0 78 00 17
 2 Guitar          13 41 1A 0D D8 F7 23 13
 3 Piano           13 01 99 00 F2 C4 11 23
 4 Flute           31 61 0E 07 A8 64 70 27
 5 Clarinet        32 21 1E 06 E0 76 00 28
 6 Oboe            31 22 16 05 E0 71 00 18
 7 Trumpet         21 61 1D 07 82 81 10 07
 8 Organ           23 21 2D 14 A2 72 00 07
 9 Horn            61 61 1B 06 64 65 10 17
10 Synthesizer     41 61 0B 18 85 F7 71 07
11 Harpsichord     13 01 83 11 FA E4 10 04
12 Vibraphone      17 C1 24 07 F8 F8 22 12
13 Synth Bass      61 50 0C 05 C2 F5 20 42
14 Acoustic Bass   01 01 55 03 C9 95 03 02
15 Electric Guitar 61 41 89 03 F1 E4 40 13
Rhythm BD          01 01 18 0F DF F8 6A 6D
Rhythm SD/HH        01 01 00 00 C8 D8 A7 48
Rhythm TOM/T-CY     05 01 00 00 F8 AA 59 55
```

An accessor `rom_patch(int number)` (1-15) returns the decoded struct (same field shape as
`patch(c)`) for direct assertion in tests, e.g. `rom_patch(3).modulator.mul == 3`.

**Operational test contract (no audio sink needed):**
1. Write via the bus (`io_write(0x7C, reg); io_write(0x7D, value);`) then assert exact byte state via
   `register_value(reg)` — proves the two-port latch/mask protocol (A-M17-3).
2. Assert derived per-channel fields (`f_number`, `block`, `key_on`, `sustain`, `instrument`,
   `volume`) after a representative write sequence covering `$10-$18`/`$20-$28`/`$30-$38`.
3. Assert rhythm decode fields after writing `$0E` and `$36-$38`.
4. Assert `rom_patch(n)` for all 15 melody + BD/SD-HH/TOM-TC entries equal the table above exactly.
5. Assert `reset()` zeroes all 64 registers (A-M17-4) and that `io_read` is always `0xFF`
   (A-M17-5) regardless of prior writes.
This is deterministic, requires no clock advance, no DAC, and no waveform buffer — matching the task
brief's explicit "no audio waveform synthesis/presentation required this milestone."

### 2.3 MSX-JE 16 KB SRAM — corrected grounding, design, and an open question

**Finding (concrete, re-grounded — supersedes the informal DEC-0009 Q4-seq note).** Re-reading the
authoritative sources shows the 16 KB MSX-JE SRAM is **not** part of the YM2413/MSX-MUSIC device:

- `Sony_HB-F1XV.xml:105-115` — primary slot **0**, secondary slot **3**: `<ROM id="HB-F1XV
  MSX-JE"><rom>hb-f1xv.rom</rom><mem base="0x0000" size="0x10000"/><sramname>hb-f1xv_msx-je.sram</sramname><mappertype>Halnote</mappertype></ROM>`.
  This is backlog row **B6** (Halnote/MSX-JE firmware), already tracked separately since M13 (M13
  planner package §1.2 "D-3", `docs/m13-planner-package.md` line 53/121).
- `references/openmsx-21.0/src/memory/RomHalnote.cc:37-46` — `RomHalnote`'s constructor:
  `sram = std::make_unique<SRAM>(getName() + " SRAM", 0x4000, config);` — **`0x4000` = 16384 bytes =
  16 KB**, confirming the S1985 fact-sheet §9 "MSX-JE with 16 KB SRAM" claim refers to **this**
  device's SRAM, mapped in the `[0x0000-0x3FFF]` region of the Halnote ROM window when enabled
  (`RomHalnote.cc:82-86,98-118`), not to any FM-PAC-style overlay in slot 3-3.
- `Sony_HB-F1XV.xml:179-197` — the `<MSX-MUSIC>` device at slot 3-3 has **no** `<sramname>` tag at
  all — zero battery-backed storage on the real chip-select this machine actually uses (A-M17-1).
  The 8 KB SRAM described in the YM2413 fact-sheet §1/§2 belongs to the **external FM-PAC cartridge**
  (`MSXFmPac`), which is a different, not-installed device on this machine.
- M13's own planner package already kept these two facts separate (D-3 "MSX-JE Halnote mapper (slot
  0-3) + 16 KB MSX-JE SRAM" vs the FM-MUSIC-ROM-presence row) — the *backlog's* B4 wording
  ("FM-PAC/MSX-JE SRAM store") is what introduced the ambiguity that M15's planner package flagged
  and left unresolved ("Q4: The backlog text is ambiguous," `docs/m15-planner-package.md` line 447),
  and that DEC-0009 only partially resolved by *sequencing* B4 into "the FM-PAC/MSX-MUSIC milestone
  (now M17)" without re-grounding which device the SRAM structurally belongs to.

**Conclusion:** B4 (MSX-JE 16 KB SRAM) is committed scope, but its true owner is **B6 (Halnote,
slot 0-3)**, not B3 (YM2413, slot 3-3). Wiring a 16 KB SRAM into the slot-3-3 MSX-MUSIC device in M17
would misrepresent real hardware (fabricate a chip-select overlay that does not exist on this
machine) and is explicitly avoided.

**Recommended disposition (needs explicit human confirmation before implementation — see final
report):**
- **Option A (recommended).** M17 extracts a reusable, deterministic, parametric-size
  battery-backed-store primitive — `src/devices/memory/battery_backed_sram.{h,cpp}` — generalizing
  `S1985Engine::load_backup_ram`/`save_backup_ram` (absent file → deterministic zero, write/save/
  reload round-trip, never wall-clock, no fabricated provenance). Unit-tested standalone at 16 KB
  (`0x4000`, matching `RomHalnote.cc`'s size exactly) so the future Halnote milestone (B6, indicative
  M20) can attach it directly to the real slot-0-3 SRAM window with no redesign. It is **not**
  instantiated inside `Hbf1xvMachine` and **not** wired to any slot in M17 — there is no real
  consumer yet (Halnote/B6 itself is still out of scope). B4 status becomes **OPEN, re-owned to B6**
  (a correction, not a new scope expansion) with the primitive ready for reuse recorded in its
  grounding column.
- **Option B.** Defer all of B4 strictly to the Halnote milestone (M20); touch nothing in M17.
  Simpler, but does not act on DEC-0009's sequencing intent at all and leaves the human directive's
  explicit "(c) MSX-JE 16KB SRAM: battery-backed store... define in the package" without a concrete
  deliverable.
- Planner recommends **Option A**. This is flagged as an **open question for the human** because it
  revises the practical reading of DEC-0009's Q4-seq note; see the final report for the explicit ask.

**Related finding (documentation-only correction, low risk, recommended for the SAME cycle).** The
M10-era inert 8 KB `sram_` region in `src/machine/hbf1xv_machine.h:117-121,130,280` is commented as
"FM-PAC battery SRAM inert region... the standard Panasonic FM-PAC carries 8 KB of battery-backed
SRAM" — this assumption is now shown to be built on the wrong device for this specific machine (the
real slot-3-3 device has no SRAM; the real 16 KB SRAM is Halnote's). **Recommendation:** update the
comment only (no size/behavior change, since the region is exercised by the M10 debug-dump golden
tests — `tests/unit/machine/hbf1xv_debug_dump_unit_test.cpp`,
`tests/unit/machine/hbf1xv_memory_regions_unit_test.cpp` — and a size/ownership change carries real
regression risk best taken when B6/Halnote is actually implemented). Track the eventual resize/
re-ownership as part of B6's acceptance criteria, not M17's.

### 2.4 Determinism (hard requirement)

- The YM2413 register model has **no time-dependent state** in M17 (no envelope/phase counters, no
  LFO, no DAC) — register reads/writes are pure combinational functions of the stored bytes. This is
  trivially deterministic (stronger than the M15 PSG precedent, which *did* need an explicit
  cycle-driven advance for its envelope generator).
- The `battery_backed_sram` primitive follows the exact M15 `.sram` discipline: absent/short/
  unreadable file → deterministic zero-fill (never fabricated), no wall-clock or host-filesystem
  nondeterminism beyond the one fixed load-at-setup read, write/save/reload round-trips byte-
  identical across runs.
- No change to `step_cpu_instruction`, T-state accounting, or `elapsed_cycles()` — M17 introduces no
  new clock consumer (unlike M15's PSG/RTC or M16's FDC, which needed the `elapsed_cycles()`-driven
  X4 pattern). This is a **lower-risk milestone on the CPU-timing-oracle axis** than M15/M16.

### 2.5 openMSX A/B acceptance plan

Because the OPLL is **write-only** (A-M17-5), the M11/M15-style technique of pulling device state
into CPU registers via `IN A,(port)` (used for the readable PSG/RTC/mapper ports,
`tools/openmsx-io-parity.ps1`) does **not** apply here — there is no CPU-readable port to diff via
the existing architectural-register trace comparator. Two complementary, real subjects:

1. **CPU-visible architectural-state parity across the write sequence (existing harness,
   unmodified).** Run a probe program that performs a representative `OUT (#7C),reg` / `OUT
   (#7D),value` sequence covering user-patch (`$00-$07`), per-channel F-Num/Block/Key-on
   (`$10-$18`/`$20-$28`), instrument/volume (`$30-$38`), and rhythm (`$0E`,`$36-$38`) registers, then
   diff PC/registers/flags per instruction against openMSX exactly as `tools/openmsx-io-parity.ps1`
   already does. This proves the instruction stream executes identically (no crash/hang/unexpected
   wait-state) when driving the real HB-F1XV I/O ports — a genuine, if limited, check.
2. **Internal register-file comparison via each emulator's own introspection (new; must be built and
   verified before being claimed).** openMSX exposes the OPLL's last-written register bytes through
   its `"MSX Music regs"` `SimpleDebuggable` (`references/openmsx-21.0/src/sound/YM2413.hh:40-44,
   YM2413.cc:19-29`, size `0x40`), reachable from the WSL Tcl console as `debug read {MSX Music
   regs} <addr>` (A-M17-6). Our emulator exposes the equivalent via `register_value(addr)`. The
   developer extends the parity tooling (e.g. a new `tools/openmsx-ym2413-parity.ps1`, following the
   `openmsx-io-parity.ps1` structure) to emit, after the same write sequence, a per-address
   comparison of these two sources for the addresses in the probe program. **This mechanism must be
   verified against a real WSL openMSX run before any parity claim is made** (R-M17-6) — if the Tcl
   `debug read` generalization to non-memory debuggables does not work as expected, the harness must
   report **BLOCKED** honestly (never fabricate), per guardrails and the DEC-0001 precedent.
- **Adversarial comparator check** (as in M10-M16): confirm an empty-side input → BLOCKED and a
  corrupted field → DIVERGENCE, so an empty diff is trustworthy.
- **Output:** `docs/m17-parity-trace-diff.md`, referenced from the implementation report + QA
  sign-off.

---

## 3. Milestones (Slice Plan M17-S1 … S5)

Every slice runs the full evidence-gate set (§6) and must leave `ctest` green.

### M17-S1 — YM2413 register file + two-port I/O protocol
- **Goal:** `Ym2413Opll` as a `core::IoDevice`: 64-byte register store, address-latch/data write
  protocol on `#7C`/`#7D` (A-M17-3), `reset()` (A-M17-4), `io_read` open-bus `0xFF` (A-M17-5),
  `register_value(addr)` debug accessor (A-M17-6).
- **Files:** `src/devices/audio/ym2413_opll.{h,cpp}`; `CMakeLists.txt`.
- **Unit tests** (`tests/unit/devices/audio_ym2413_opll_unit_test.cpp`): latch/data write lands in
  the correct register; latch masking `& 0x3F` (e.g. latch `0xFF` → register `0x3F`); reset zeroes
  all 64 bytes; `io_read` always `0xFF`; two-run determinism (identical write sequence → identical
  register state).
- **Gates:** build + ctest green.

### M17-S2 — Channel/rhythm decode + ROM instrument patch table
- **Goal:** per-channel decode (`f_number/block/key_on/sustain/instrument/volume/patch`), rhythm
  decode (`rhythm_enabled/bd_key/sd_key/tom_key/cym_key/hh_key` + rhythm volumes), the 15+3-entry ROM
  patch table with a `rom_patch(n)` accessor, user-patch (`instrument==0`) live decode from
  `$00-$07`.
- **Files:** `src/devices/audio/ym2413_opll.{h,cpp}` (extend).
- **Unit tests** (same test file or a companion `audio_ym2413_channel_decode_unit_test.cpp`):
  per-channel field decode for representative F-Num/Block/Key-on/Sustain/Instrument/Volume
  combinations across all 9 channels; rhythm register `$0E` bit decode + `$36-$38` volume nibble
  decode; all 15 melody + 3 rhythm ROM patches assert byte-exact against the §2.2 table; user-patch
  live decode reflects in-place edits to `$00-$07`.
- **Gates:** build + ctest green.

### M17-S3 — Machine wiring + system integration
- **Goal:** attach `Ym2413Opll` at `#7C`/`#7D` in `wire_bus()`; add to the `cold_boot()` reset
  sequence; expose `ym2413()` accessor; confirm `fmmusic_rom_` slot-3-3 attachment is untouched
  (A-M17-2).
- **Files:** `src/machine/hbf1xv_machine.{h,cpp}` (edit, additive only).
- **Integration tests** (`tests/integration/machine/hbf1xv_m17_ym2413_integration_test.cpp`): a real
  CPU program drives `OUT (#7C),A` / `OUT (#7D),A` over the M11 system bus and the machine's
  `ym2413().register_value(...)` reflects the write; `fmmusic_rom_` at slot 3-3 page 1 still returns
  ROM bytes unchanged (regression guard for A-M17-2); `IN A,(#7C)`/`IN A,(#7D)` over the bus reads
  `0xFF`; two-machine determinism (same program run twice → identical register state); **zero
  regression** on M1-M16 suites.
- **Gates:** build + ctest green (full suite).

### M17-S4 — Battery-backed SRAM primitive (standalone; B4 disposition per §3.3 Option A)
- **Goal:** `BatteryBackedSram` — a parametric-size (constructed with byte count, e.g. `0x4000` for
  the future Halnote use), deterministic byte-store generalizing
  `S1985Engine::load_backup_ram`/`save_backup_ram`. Absent/short/unreadable file → store left
  untouched (documented default: zero after construction); `load()`/`save()` round-trip identical
  bytes; no wall-clock, no fabricated provenance. **Not instantiated in `Hbf1xvMachine`.**
- **Files:** `src/devices/memory/battery_backed_sram.{h,cpp}`; `CMakeLists.txt`.
- **Unit tests** (`tests/unit/devices/memory_battery_backed_sram_unit_test.cpp`): construct at 16 KB
  (`0x4000`, matching `RomHalnote.cc:44`), absent file → all-zero + `load()` returns `false`;
  write bytes, `save()`, construct a fresh instance, `load()` → identical bytes; determinism across
  two independent round-trips; a too-short/corrupt file behaves like "absent" (documented, matching
  the S1985 precedent's discipline).
- **Gates:** build + ctest green. (No integration test — no wired consumer this milestone, by
  design; this is explicitly noted as the honest state of the primitive, not hidden.)

### M17-S5 — openMSX A/B parity + backlog finalization
- **Goal:** capture the two-subject A/B evidence (§2.5); verify (or honestly report BLOCKED for) the
  register-introspection comparison mechanism; finalize the full deferred-backlog review (§4) in the
  ledger; refresh checksums.
- **Files:** `tools/openmsx-ym2413-parity.ps1` (new, mirrors `tools/openmsx-io-parity.ps1`
  structure); `tests/parity/m17_ym2413_probe.bin`; `docs/m17-parity-trace-diff.md`; refreshed
  `docs/asset-checksums.txt`.
- **Tests:** the S3 integration test serves as the deterministic golden the A/B probe mirrors.
- **Gates:** all four **plus** the A/B gate. No parity claim without a genuine capture; if the
  register-introspection comparison mechanism (A-M17-6) does not work as expected against real WSL
  openMSX, report BLOCKED honestly and fall back to subject 1 only (documented, not fabricated).

---

## 4. Full Deferred-Backlog Review (mandatory, per DEC-0005 and the human's explicit instruction)

Source of truth: `agent-protocol/state/deferred-backlog.md`. **Every** row — A.B1-B9 and B.C1-C10 —
re-affirmed below with a one-line justification, per the human directive "review deferred items and
have them properly addressed during development." Nothing is silently dropped.

### Section A (human-directive-tracked rows)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| B1 | PSG/YM2149 internals | **DONE (M15)** — unchanged | Closed at M15; re-affirmed, no M17 impact. |
| B2 | RTC/RP5C01 internals | **DONE (M15)** — unchanged | Closed at M15; re-affirmed, no M17 impact. |
| B3 | FM-PAC (OPLL YM2413) device internals | **CLOSES this cycle (M17)** | Register-accurate YM2413 device (§2.2) wired to the real `#7C/#7D` I/O ports alongside the M13 FM-MUSIC ROM (§2.1); grounded in the YM2413 fact-sheet + `references/openmsx-21.0/src/sound/YM2413*`. DSP/audio-synthesis depth is explicitly split out as new row **E1** (not a partial-close — the *device contract* is what B3 names, mirroring the M14 VDP contract/depth split). |
| B4 | MSX-JE 16 KB SRAM | **RE-GROUNDED, NOT closed — recommend re-owned to B6** | §3.3: concrete XML + `RomHalnote.cc` evidence shows the real 16 KB SRAM belongs to the Halnote ROM at slot 0-3 (B6), not the YM2413 device (B3/slot 3-3, which the XML shows has **no** SRAM at all). M17 builds a reusable, standalone, deterministic battery-backed-store primitive (§3 M17-S4) sized to match, ready for B6 to wire — but does not close B4, since no real device consumes it yet. **Flagged as an open question for explicit human confirmation** (this revises the practical reading of DEC-0009's Q4-seq note). |
| B5 | Kanji-font I/O `#D8-DB` | OPEN — unchanged | Next milestone per the human's bundled M17+M18 directive (M18 peripheral I/O). No M17 dependency. |
| B6 | Halnote/MSX-JE firmware mapping (slot 0-3) | OPEN — **now the true owner of the 16 KB SRAM (see B4)** | 1 MB banked ROM + 16 KB SRAM; independent of the YM2413/slot-3-3 wiring M17 delivers. Grounding strengthened this cycle (`RomHalnote.cc:37-46`). |
| B7 | Cartridge loading | OPEN — unchanged | External slots 1/2; unrelated to M17. |
| B8 | FDC drive mechanics | DONE (M16) — unchanged | Closed at M16; re-affirmed. |
| B9 | VRAM/V9958 VDP | DONE (M14) — unchanged | Closed at M14; re-affirmed. |

### Section B (other tracked deferrals)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| C1 | Exact cycle/T-state timing parity | OPEN — unchanged | Unrelated to M17 (M17 introduces no new clock consumer, §2.4). Companion new row E2 (YM2413 write-timing) groups here for a future timing-fidelity milestone. |
| C2 | Z80 HALT-R increment | OPEN — unchanged | Per DEC-0004; unrelated to M17. |
| C3 | ZEXDOC/ZEXALL full sweep | OPEN — unchanged | Needs a legally-sourced ZEX binary; unrelated to M17. |
| C4 | S1985 backup-RAM `.sram` persistence | DONE (M15) — unchanged | Closed at M15; **its `load_backup_ram`/`save_backup_ram` pattern is the direct precedent M17-S4 generalizes** (§3.3). |
| C5 | Full boot past first device read | IN-PROGRESS (carried from M16) — unchanged | M17 adds no CPU-visible boot-path interaction (the YM2413 is write-only, off the disk-boot handshake path); the documented M16 residual (auto-disk-boot trigger investigation) is **not** this milestone's job to close, per the task brief and M16's closure terms (DEC-0011). |
| C6 | Keyboard matrix + joystick | DONE (M15) — unchanged | Closed at M15; re-affirmed. |
| C7 | Printer `#90/91` + cassette | OPEN — unchanged | Next milestone (M18, peripheral I/O), per the human's bundled directive. |
| C8 | Sony speed-controller + hardware PAUSE + Ren-Sha Turbo | OPEN — unchanged | HB-F1XV-specific companion-chip behavior; unrelated to M17. |
| C9 | SDL3 frontend | OPEN — unchanged | Presentation layer; will eventually consume the YM2413's *audio* output once E1 (DSP depth) exists — not this milestone. |
| C10 | FDC flux-level/DMK fidelity | OPEN — unchanged | Unrelated to M17 (audio device, not FDC). |
| D1 | Pixel-accurate raster rendering | OPEN — unchanged | VDP rendering depth; unrelated to M17. |
| D2 | Sprite rendering + collision | OPEN — unchanged | Unrelated to M17. |
| D3 | VDP command engine | OPEN — unchanged | Unrelated to M17. |
| D4 | Cycle-accurate VDP access-slot/command timing | OPEN — unchanged | Unrelated to M17. |
| D5 | YJK/YAE color decode + DAC | OPEN — unchanged | Unrelated to M17. |
| D6 | Horizontal scroll/interlace/blink/superimpose | OPEN — unchanged | Unrelated to M17. |
| D7 | G6/G7 VRAM planar interleave | OPEN — unchanged | Unrelated to M17. |

### New rows proposed this cycle (mirroring the M14 D1-D7 contract/depth split and the M16 B10 precedent)

| # | Item | Status | Candidate owner | Grounding |
|---|---|---|---|---|
| **E1** | **YM2413 FM-synthesis DSP/audio depth** — 18-slot TDM pipeline @ 49716 Hz, log-sin(256,12b)+exp(256,10b) operator, 128-level EG with global-counter rate mechanism (`eg_shift`/`eg_select`), 2-deep feedback averaging, AM/VIB LFOs, rhythm noise generator + double-output quirk, DAC/output-level nonlinearity. M17 delivers the register/channel/rhythm **contract** only (§2.2); zero waveform synthesis. | OPEN (new) | Future audio-rendering milestone, paired with C9 (SDL3 frontend) since that is where actual sound output matters | YM2413 fact-sheet §4/§5/§7/§9; `references/openmsx-21.0/src/sound/YM2413NukeYKT.{hh,cc}` (recommended DSP-accuracy reference, per A-M17-7 and the XML's `ym2413-core=NukeYKT` selection) |
| **E2** | **YM2413 register-write timing constraint** — ≥12 master-cycle address-write / ≥84 master-cycle data-write minimum spacing; violating writes corrupt/drop on real hardware. | OPEN (new) | Timing-fidelity milestone (with C1/C2/D4) | YM2413 fact-sheet §8 ("openMSX currently has the too-fast-access-timing emulation disabled") |

**Backlog bookkeeping note (to be executed at ledger-update time, not in this planning artifact):**
on M17 closure, mark B3 `DONE (M17)`; add E1/E2 as new OPEN rows; **do not** mark B4 `DONE` — instead
update its row text to read "re-grounded M17: true owner is B6 (Halnote, slot 0-3); reusable
persistence primitive built standalone, not yet wired" and leave its status `OPEN`, pending the
human's confirmation of §3.3's Option A/B choice.

---

## 5. Acceptance Criteria (M17 exit)

1. `Ym2413Opll` device (64-register file, two-port `#7C/#7D` write protocol with address-latch
   masking, per-channel decode, rhythm decode, 15+3-entry ROM instrument patch table, `reset()`,
   debug `register_value()`) implemented under `src/devices/audio/`, grounded in the YM2413 fact-
   sheet + `references/openmsx-21.0/src/sound/YM2413*` (concrete citations in the implementation
   report). *(S1, S2)*
2. Wired into the M11 `io_bus_` at `#7C`/`#7D` alongside the **unmodified** M13 `fmmusic_rom_` at
   slot 3-3 page 1 (no register overlay in memory space, per A-M17-2); `IN` on either port reads
   `0xFF`. *(S3)*
3. `BatteryBackedSram` reusable primitive implemented, unit-tested standalone at 16 KB, matching
   `RomHalnote.cc`'s SRAM size exactly; **not** wired to any slot this milestone (§3.3 Option A);
   deterministic load/save discipline mirrors the M15 S1985 `.sram` pattern. *(S4)*
4. Deterministic unit + system-integration tests cover every new behavior; two identical runs
   produce byte-identical register/SRAM-primitive state. *(S1-S4)*
5. Real openMSX A/B evidence captured for the register-write path (`docs/m17-parity-trace-diff.md`);
   if the register-introspection comparison mechanism cannot be verified against real WSL openMSX,
   the report states BLOCKED honestly rather than fabricating a result — no parity claim without a
   genuine capture. *(S5)*
6. **Zero regression** across M1-M16 suites, including the `fmmusic_rom_` slot-3-3 read-path
   regression guard (A-M17-2) and the CPU-timing oracles (untouched by construction, §2.4). *(S3, S5)*
7. Every deferred-backlog row (A.B1-B9, B.C1-C10) re-affirmed with justification (§4); B3 closes;
   B4 re-grounded (not closed) with an explicit open question recorded; E1/E2 proposed as new rows.
   *(§4, this package)*
8. Evidence gates executed and captured each cycle (validate-assets, checksum, Debug build, ctest).
9. QA sign-off recorded before closure (`docs/m17-qa-signoff.md`). **Normal human-release-decision
   gate — no auto-close** (per the task brief and DEC-0003's auto-close grant being M12-only);
   autopilot pauses at QA sign-off for the separate human release decision + tag.

---

## 6. Risks (each with a verification action)

| ID | Risk | Verification action |
|----|------|---------------------|
| R-M17-1 | Misidentifying the device as `MSXFmPac` (external cartridge) instead of `MSXMusic` (internal), leading to a fabricated bank-register/SRAM/ID-string implementation that doesn't match real HB-F1XV hardware. | A-M17-1/A-M17-2 ground the correct device class from the exact machine XML; implementation report must cite `Sony_HB-F1XV.xml:179-197` and `MSXMusic.cc` (not `MSXFmPac.cc`) as the behavioral reference. |
| R-M17-2 | B4 (MSX-JE SRAM) gets wired into slot 3-3 anyway (following the literal task-brief framing) despite grounding showing it belongs to Halnote (slot 0-3), fabricating hardware behavior. | §3.3 explicitly recommends NOT wiring it in M17 and flags the discrepancy as an open question for human confirmation before implementation proceeds; developer must not instantiate `BatteryBackedSram` inside `Hbf1xvMachine` without an explicit human go-ahead reversing this recommendation. |
| R-M17-3 | Register-address latch masking wrong (e.g. `& 0xFF` instead of `& 0x3F`, or masking at latch-time instead of use-time). | A-M17-3 cites the exact openMSX source lines; S1 unit test explicitly asserts the masking boundary (latch `0xFF` → register `0x3F`). |
| R-M17-4 | ROM instrument patch table transcription error (63 bytes across 18 patches is easy to mistranspose). | §2.2 table is reproduced directly from the fact-sheet; S2 unit test asserts every byte of every patch; code comment preserves the fact-sheet's own "not datasheet-certain" caveat (A-M17-7) so a future audit knows to re-verify against NukeYKT if DSP-accurate audio is ever built. |
| R-M17-5 | Scope creep into DSP/audio synthesis (tempting given the fact-sheet's rich detail) inflates M17 beyond the task brief's explicit "no audio waveform synthesis... required this milestone." | §1.2 and new backlog row E1 explicitly fence this out; any expansion requires a decisions-ledger entry per guardrails "Scope Control". |
| R-M17-6 | The openMSX Tcl `debug read {<name> regs} <addr>` register-introspection mechanism (A-M17-6) may not behave as expected (untested assumption about generalizing `tools/openmsx-io-parity.ps1`'s `debug read memory` pattern to a non-memory debuggable name). | Developer verifies this against a real WSL openMSX run in S5 BEFORE claiming subject-2 A/B parity; if it fails, report BLOCKED for subject 2 and rely on subject 1 only (documented, not fabricated) — mirrors the M13/M16 "honest BLOCKED" precedent. |
| R-M17-7 | `fmmusic_rom_`'s existing slot-3-3 attachment accidentally regresses (e.g. an errant wrap is added despite A-M17-2's finding that none is needed). | S3 integration test explicitly asserts ROM bytes at slot 3-3 page 1 are unchanged pre/post the M17 wiring change — a locked regression guard. |
| R-M17-8 | The M10-era 8 KB `sram_` inert-region comment correction (§3.3 "related finding") is skipped or, worse, its SIZE is changed without full regression analysis, breaking the M10 debug-dump golden. | Developer treats this as comment-only (no behavior/size change) in M17; any size/ownership change is explicitly deferred to B6/Halnote's own acceptance criteria, not silently done here. |

---

## 7. Developer Handoff

- **Start at M17-S1** (register file + two-port protocol) — grounded per A-M17-3/A-M17-4/A-M17-5;
  cite `references/openmsx-21.0/src/sound/YM2413Okazaki.cc` line ranges in code comments (never copy
  the code itself — GPL isolation).
- **Sequence S1→S5** in order; each runs the full four-gate evidence set; keep `ctest` green every
  cycle.
- **src/ placement fixed by §2.1:** `src/devices/audio/ym2413_opll.{h,cpp}` (device logic),
  `src/devices/memory/battery_backed_sram.{h,cpp}` (standalone reusable primitive), machine wiring
  only in `src/machine/hbf1xv_machine.{h,cpp}` (additive: two new `io_bus_.attach` calls + one new
  member + reset-sequence addition — no change to the existing slot-3-3 ROM attachment).
- **Critical architectural finding to honor (A-M17-1/A-M17-2):** this machine's sound device is the
  internal `MSX-Music` pattern (fixed ROM + I/O-port-only OPLL access, no SRAM, no bank register) —
  **not** the external FM-PAC cartridge pattern. Do not build a bank register, SRAM handshake, or
  ID-string detection logic for slot 3-3; those describe hardware this machine does not have.
- **B4/MSX-JE SRAM (§3.3):** build the reusable `BatteryBackedSram` primitive, standalone and
  unit-tested, but **do not wire it to slot 3-3 or any other slot** without an explicit human
  decision overriding this package's Option-A recommendation — flag this clearly in the
  implementation report if the human's review changes the disposition.
- **No new clock consumer (§2.4):** the YM2413 register model needs no `elapsed_cycles()` adapter —
  simpler than M15/M16's X4 pattern. Do not add one speculatively for future DSP work (that is E1's
  concern, later).
- **A/B (§2.5):** build/extend `tools/openmsx-ym2413-parity.ps1`; verify the `debug read {MSX Music
  regs} <addr>` Tcl mechanism against a real WSL openMSX run before claiming subject-2 parity; if it
  does not work, report BLOCKED honestly and still deliver subject 1 (CPU-visible architectural
  parity across the write sequence).
- **Ledger discipline (DEC-0005):** on closure, mark B3 `DONE (M17)`; update B4's row text per §4
  (re-grounded, re-owned to B6, NOT done) rather than marking it done; add new rows E1/E2; update
  `state/current-phase.md` and `state/milestones.md`.
- **Gate:** NORMAL human-release-decision gate — do not auto-close; pause at QA sign-off for the
  separate human release decision + tag (per the task brief and DEC-0003's M12-only auto-close
  scope).
- Deliverables: source per §2.1; tests per §3; `docs/m17-parity-trace-diff.md`; refreshed
  `docs/asset-checksums.txt`; an implementation report `docs/m17-implementation-report.md`; then hand
  to QA for `docs/m17-qa-signoff.md`.
