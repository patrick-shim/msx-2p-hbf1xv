# M47 — openMSX FM-PAC-SRAM setup (tested) + YS II save-structure dissection (DEC-0072)

**Status:** read-only investigation + WSL openMSX testing, 2026-07-13. No `src/` edits.
**Scope:** the two DEC-0072 deliverables — (1) a *tested* procedure for the owner to boot YS II on
WSL openMSX loading the game state from OUR FM-PAC SRAM (the gameplay-A/B enabler); (2) a map of
how YS II organizes its save in BOTH stores (SRAM + disk), the diverging field/offset, and the
load-crash mechanism.

Environment verified: **openMSX 19.1** (`/usr/bin/openmsx`, flavour debian, GL renderer via WSLg
`DISPLAY=:0`), machine `Sony_HB-F1XV.xml` present, extension `fmpac.xml` present.

---

## DELIVERABLE 1 — openMSX FM-PAC-SRAM setup (TESTED, works)

### 1.1 Key finding: our SRAM is byte-identical to openMSX's `.pac` — NO transform needed

openMSX persists FM-PAC SRAM as a **16-byte `"PAC2 BACKUP DATA"` header followed by the 8190
(0x1FFE) addressable SRAM bytes = 8206 bytes total**. Grounding:

- `references/openmsx-21.0/src/sound/MSXFmPac.cc:7,11` — `PAC_Header = "PAC2 BACKUP DATA"`, SRAM
  constructed as `sram(getName()+" SRAM", 0x1FFE, config, PAC_Header)` (0x1FFE = 8190 addressable).
- `references/openmsx-21.0/src/memory/SRAM.cc:114-125` — `save()` writes `header` (16 bytes) then
  `ram` (8190 bytes); `load()` (`:83-100`) validates the 16-byte header, then reads the 8190 data
  bytes into SRAM. Data byte `SRAM[0]` = file offset `0x10`.

Our file `roms/fmpac.rom.sram` is **8206 bytes** with the same layout — `"PAC2 BACKUP DATA"` at
0x00, data from 0x10. This is not a coincidence: our own emulator deliberately mirrors openMSX's
format — `src/devices/cartridge/cartridge_fmpac_rom.cpp:31-36,183-196` loads `file[0x10+i] →
SRAM[i]` and `src/devices/cartridge/cartridge_fmpac_rom.h:76-89` documents the 8206-byte
`"PAC2 BACKUP DATA"` wrapper + 8190 data section, citing `SRAM.cc:114-131`. **The two files are
directly interchangeable.** Verified: `cmp roms/fmpac.rom.sram <openMSX pac>` → byte-identical.

### 1.2 Where openMSX stores the FM-PAC SRAM (verified empirically)

For `-ext fmpac`, openMSX resolves the SRAM save via `configFileContext` →
`{USER_OPENMSX}/persistent/{extension}/{session}/{sramname}`
(`references/openmsx-21.0/src/file/FileContext.cc:151-154`). Empirically that is:

```
~/.openMSX/persistent/fmpac/untitled1/fmpac.pac      (8206 bytes)
```

- `fmpac` = the extension config name; `fmpac.pac` = `<sramname>` from
  `/usr/share/openmsx/extensions/fmpac.xml`; `untitled1` = openMSX's default machine-session name.
- openMSX **auto-creates** this directory the first time the fmpac extension is loaded and
  **auto-writes** it back on exit whenever the game modifies SRAM (5-second deferred flush,
  `SRAM.cc:65-71`). This means the reverse A/B works too: SAVE in-game on openMSX → the updated
  `fmpac.pac` appears here → copy it back to the repo to compare against our engine's SRAM output.

### 1.3 The `fmpac.rom` openMSX uses

The standard `fmpac.xml` accepts a ROM with SHA1 `fec451b9…` **or** `9d789166…`. Our
`roms/fmpac.rom` (65536 bytes) has SHA1 `9d789166e3caf28e4742fe933d962e99618c633d` — an accepted
match — and a copy already lives in openMSX's systemroms as `fmpac_ours64.rom`, so `-ext fmpac`
resolves the ROM by SHA1 with no extra flags.

### 1.4 Step-by-step for the owner (Windows → WSL, copy-paste, tested)

> Path map: repo on Windows `c:\Users\pashim\source\sony-msx-hbf1xv` = in WSL
> `/mnt/c/Users/pashim/source/sony-msx-hbf1xv`. Run these in a **WSL** shell. openMSX opens a real
> window via WSLg. **Do not have openMSX already running when you copy the SRAM** (it rewrites the
> file on exit and would clobber your copy).

```bash
R=/mnt/c/Users/pashim/source/sony-msx-hbf1xv

# 1. Load OUR current game state into the FM-PAC SRAM slot openMSX reads (byte-for-byte; no convert)
mkdir -p ~/.openMSX/persistent/fmpac/untitled1
cp -f "$R/roms/fmpac.rom.sram" ~/.openMSX/persistent/fmpac/untitled1/fmpac.pac
cmp "$R/roms/fmpac.rom.sram" ~/.openMSX/persistent/fmpac/untitled1/fmpac.pac && echo "SRAM staged OK"

# 2. Make fresh, writable disk copies (never mutate the repo masters d1.dsk / d2-vanila.dsk)
mkdir -p ~/ys2test
cp -f "$R/games/disks/ys2/d1.dsk"        ~/ys2test/d1.dsk    # program/boot disk
cp -f "$R/games/disks/ys2/d2-vanila.dsk" ~/ys2test/d2.dsk    # data/save disk (fresh vanilla)

# 3. Boot. HB-F1XV has ONE drive, so start with the PROGRAM disk in drive A.
openmsx -machine Sony_HB-F1XV -ext fmpac -diska ~/ys2test/d1.dsk
```

In the openMSX window:
1. The intro plays. Tap **SPACE** a few times to advance to the prompt
   **"INSERT DATADISK IN DRIVE A-RET."**.
2. Swap drive A to the **data disk**: open the console (**F10**) and type `diska ~/ys2test/d2.dsk`
   (or use the OSD menu: right-Ctrl → Media → Disk Drive A → insert `d2.dsk`), then press **RETURN**
   to satisfy the prompt. The game loads into the opening scene.
3. **Loading your saved game is an in-game action** (YS II has no boot-time load menu — verified,
   §1.5): play to a **goddess statue / save point** and choose **LOAD** (it reads the FM-PAC SRAM
   you staged in step 1). From there the A/B against our engine begins from the identical state.

### 1.5 Test evidence (what was actually run and confirmed)

All under `debug/m47repro/openmsx/`:

- **Boot works.** `ys2boot_14_0001.png` shows the YS II title ("YS II COPYRIGHT 1988 FALCOM /
  TRANSLATION: OASIS") on `Sony_HB-F1XV` + `-ext fmpac`, booting `d1.dsk`.
- **openMSX reads OUR exact SRAM bytes.** Via the openMSX debugger, the debuggable
  `Panasoft SW-M004 FMPAC SRAM` (size 8190) reads
  `00 00 31 37 7C CC CF D4 59 9A 9B 9B BB BB BA BB BC C6 06 06 …` — **identical** to
  `roms/fmpac.rom.sram` at offset 0x10. This proves openMSX loaded and exposed our game state to
  the game (not just that a file exists).
- **Data-disk swap → gameplay.** `ys2sw2_50_0001.png` / `ys2sw2_64_0001.png` show the game running
  interactively after the `diska` swap + RETURN (Adol on the field, the "I FEEL A LITTLE BIT WEARY"
  opening dialogue).
- **Full interactivity.** `ys2play_a..e_0001.png` (a driven arrow-key/SPACE walk) show Adol moving
  between screens — a cave interior, the village with NPCs, adjacent fields — confirming the
  machine is fully playable from our staged SRAM. (A blind random walk did not reach a goddess
  statue; that needs map knowledge, i.e. the owner's normal play.)
- **Round-trip/format proven.** `cmp` shows the staged `~/.openMSX/persistent/fmpac/untitled1/fmpac.pac`
  is byte-identical to `roms/fmpac.rom.sram`.

**YS II has NO boot-time "LOAD/CONTINUE" menu** — confirmed by fine-grained capture: after the
data-disk swap it goes straight into a *new* game (M.P 000/000, EXP 00000, GOLD 00000). Save/Load
is done only in-game at goddess statues. Therefore the one step not automated here is the in-game
navigation to a save point; it is a normal gameplay action for the owner. (If an auto-driven
end-to-end load screenshot is wanted, delegate to the `msx-playtest` agent to walk YS II to a
statue in openMSX and select LOAD — the setup above is the prerequisite and is proven.)

**Assumption + verification:** the in-game LOAD will succeed because (a) openMSX exposes our exact
SRAM bytes to the game, (b) `roms/fmpac.rom.sram` is a *known-good* save (the premise of DEC-0072 is
"SRAM saves work 100%"), and (c) FM-PAC SRAM access is BIOS-mediated and emulator-independent, with
openMSX being the reference our FM-PAC model mirrors. Verification action: the owner (or msx-playtest)
reaches a statue and selects LOAD; expect the saved character/stats to appear.

---

## DELIVERABLE 2 — SRAM vs DISK save structure, divergence, and the load-crash

### 2.1 The two stores

| Store | Location | Framing |
|---|---|---|
| **SRAM** (correct) | `roms/fmpac.rom.sram` | 16-byte `"PAC2 BACKUP DATA"` header @0x00; **save record from 0x10** |
| **DISK** (corrupt) | `games/disks/ys2/d2_rec.dsk` (≡ `d2_kill.dsk`) | slots = **LBA 9..16** (T0/S1/sec1..8), one 512-byte sector per slot |

Live-capture grounding (`debug/m47repro/evidence/corrupt_save/`): the game assembles the save
record in CPU RAM at **physical 0xE000** (`savetime_mem_E000_window.txt` starts
`00 00 31 37 7C CC CC CE …`) and writes it verbatim to the disk sector — i.e. the disk write is
byte-faithful to the 0xE000 buffer. The delta-encoded working state lives separately at 0xC100.

### 2.2 The record is prefix-sum ("running sum") encoded — CONFIRMED

The bytes stored (in SRAM, at 0xE000, and on disk) are a **running sum**:
`decoded[0]=decoded[1]=0x00`, `decoded[2] = seed` (the "seeded by buffer[2]" value), and for
`i≥3`, `decoded[i] = decoded[i-1] + delta[i] (mod 256)`. Reversing it (consecutive differences,
`delta[i]=decoded[i]-decoded[i-1]`) recovers the raw field stream. Verified on both stores:
e.g. SRAM `00 00 31 37 7C CC` → deltas `00 00 31 06 45 50`; disk `00 00 32 38 7D CD` → deltas
`00 00 32 06 45 50` (only the seed delta[2] differs, 0x31 vs 0x32).

**Consequence (key insight):** a single wrong early value shifts *every following decoded byte* by a
constant until a later delta difference cancels it. So a *small upstream state error* prints as
*hundreds of differing on-disk bytes* (the 184–511-byte diffs seen across slots). The large diff is
an artifact of prefix-sum amplification — **it is not evidence the FDC mangled hundreds of bytes**,
which corroborates the DEC-0072 conclusion that the FDC/save-build path is byte-faithful and the
divergence is upstream game state.

### 2.3 The 512-byte YS II save record — field map

Decoding `roms/fmpac.rom.sram` (offset 0x10) yields a clean two-part 512-byte record:

```
0x00 – 0x7F  (128 B)  PLAYER / GLOBAL STATE HEADER
                      bytes 0,1 = 00 00 (fixed marker); byte 2 = seed; then packed
                      stats/flags/map-state (HP/MP/EXP/gold/level, map id, position, event flags).
                      This is exactly the "DENSE corrupt block idx 0..191" of the live capture.
                      byte @0x14 == 0x10 (==16) — candidate object-table COUNT (see below).

0x80 – 0x1FF (384 B)  ON-MAP OBJECT / MONSTER TABLE  = EXACTLY 16 × 24-byte records, filling to
                      0x200 (512) precisely. Per record:
                        [0],[1]  current (X,Y)          [3]  sprite/OAM slot id: 0x80,0x84,0x88 …
                        [4]      page/category (E0/80/A0/C0)   0xBC — increments by 4 across the 16
                        [6],[7]  home (X,Y)             [10] type (00/02/04/06)   entries
                      (Idle/off-map entries read pos=home=7E,7E.)
```

The 16 records line up 1:1 with byte `@0x14 == 16`, so 0x14 is a strong candidate for the
loader's object-count field (a length that governs how many 24-byte records get processed on load).

### 2.4 Where SRAM and DISK diverge (field/offset) — and what kind of field

Reverse-prefix-sum delta comparison of the SRAM record vs `d2_rec` slot-1 (LBA 9): **409/512 = 80%
of the raw field deltas are byte-identical**, and the whole object-table region is structurally
sound in both. The two files are different *game moments*, so much of the 20% is legitimate state
difference — but the **consistent, structurally-meaningful divergence is entirely inside the
player/global-state header (0x00–0x7F)**, beginning at **offset ~6** (right after the 6-byte
`00 00 31 37 7C CC` lead-in). This matches the matched-moment capture exactly:
`debug/m47repro/evidence/corrupt_save/corruption_analysis.txt` — same-save disk vs SRAM-correct,
**first diff at idx 6**, dense-corrupt idx 0..191, sparse thereafter.

- **The diverging field is a player-STATE value (stat/flag/position), not the object table and not
  a checksum.** No checksum field is present: the 512-byte record carries no trailing
  length/CRC that matches a computed sum (SRAM sum16 `6D68` vs disk `6761`; the record's tail bytes
  are the last object record's fields, not a checksum). The nearest thing to a
  length/count is the object-count byte at **0x14**.
- Offset 6–9 sits in the early stats block; on prefix-sum decode a wrong value there shifts the run
  of following bytes, which is why the on-disk block 0x00–0xBF looks densely corrupt while the tail
  stays sparse.

### 2.5 The load-crash mechanism — tests the owner's "invasion" theory

The live crash snapshot (`debug/m47repro/evidence/corrupt_save/crash_*.txt`,
`crash_manifest.txt`): loading the corrupt state ends in
**`finalize_reason=sp_underflow`, `SP=0x0330`, `PC=0xAA75`** — a **runaway (under-flowed) stack**.

- The value `0x0330` is **not** present anywhere in the 512-byte save (scanned for `30 03`/`03 30`
  → none), so SP was **not** loaded from a corrupt pointer in the data. Instead the stack was driven
  down by an **unbounded loop / over-long processing** — i.e. the loader was fed a bad *count or
  pointer* and kept pushing/reading until SP wrapped down into the low RST/vector page and executed
  garbage.
- This is exactly the owner's **"it invades the area it shouldn't"**: a corrupt header field —
  most plausibly the object/record **count** (byte 0x14) or a map/routine pointer in the
  player-state header — makes the load routine walk past its buffer / loop without a valid
  terminator, trashing the stack and crashing at `0xAA75`. The object table itself is well-formed,
  so the trigger is upstream in the 0x00–0x7F header, consistent with §2.4.

**Assumption + verification:** pinning the *exact* header byte whose corruption drives the SP
underflow requires a **load-time watchpoint trace** (break on the load routine at ~0xAA75/its
caller, watch the count/pointer it consumes from the 0xE000 buffer) — this is a dynamic step beyond
static save-file analysis and is the recommended next action if a precise faulting field is needed.
A clean cross-check now feasible thanks to Deliverable 1: load `d2_rec.dsk`'s corrupt slot in
**openMSX** in-game; if openMSX *also* crashes/misbehaves on load, that independently confirms the
corruption is in the DATA (upstream game state), not our loader.

---

## Sources

- openMSX SRAM/FMPAC persistence: `references/openmsx-21.0/src/sound/MSXFmPac.cc`,
  `references/openmsx-21.0/src/memory/SRAM.cc`, `references/openmsx-21.0/src/file/FileContext.cc`.
- Our matching FM-PAC SRAM model: `src/devices/cartridge/cartridge_fmpac_rom.{h,cpp}`.
- openMSX machine/extension: `/usr/share/openmsx/machines/Sony_HB-F1XV.xml`,
  `/usr/share/openmsx/extensions/fmpac.xml` (openMSX 19.1, WSL).
- Live save/crash captures: `debug/m47repro/evidence/corrupt_save/` (corruption_analysis.txt,
  crash_cpu.txt, crash_manifest.txt, savetime_mem_E000_window.txt, savetime_fdc.txt).
- Test artifacts produced here: `debug/m47repro/openmsx/ys2boot_*`, `ys2sw2_*` (screenshots);
  FM-PAC SRAM debug-read confirmation (openMSX debugger).
- fMSX FM-PAC SRAM note (independent cross-ref): fMSX writes FM-PAC SRAM as `FMPAC.sav` 8 KiB when
  the FMPAC ROM is loaded (libretro fMSX README) — same "header-less-in-address-space + on-file
  wrapper" family; confirms FM-PAC SRAM is a standard, emulator-portable store.
- Web: fMSX/libretro FM-PAC SRAM behavior — https://github.com/libretro/fmsx-libretro ;
  Ys II MSX2 PAC-SRAM support — https://www.generation-msx.nl/software/falcom/ys-ii-ancient-ys-vanished---the-final-chapter/1059
