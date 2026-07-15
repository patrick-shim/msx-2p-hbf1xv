# M46 / v1.1.2 Planner Package — Convenience-First Defaults + CLI Slot Rename + Enriched Banner

- Decision of record: **DEC-0071** (`agent-protocol/channels/decisions.md:857-871`), 2026-07-13.
- Tag target: **v1.1.2**. Owner-run tag+push per DEC-0047.
- Spec Owner: MSX Planner Agent. Developer Owner: MSX Developer Agent. QA Owner: MSX QA Agent.
- Blast radius (hard): **`src/frontend/*` (CLI parse + banner) + `src/machine/*` (FM-PAC slot-2
  auto-load wiring) ONLY. ZERO `src/core/`, ZERO `src/devices/cpu/`.** ZEXALL
  withheld-substituted (git-diff proof of an empty `src/devices/cpu` + `src/core` diff, the
  DEC-0071 / M42 / M43 precedent).

This is a **frontend + a-little-machine-wiring** milestone. No device behavior changes. The
machine, when constructed stock (`--ram 64` / `--stock` / no FM-PAC), stays **byte-identical** to
v1.1.1.

---

## 1. Scope and Assumptions

### 1.1 In scope

1. **Flipped convenience defaults — applied at the CLI-resolution layer ONLY** (see §2.7 for the
   architectural seam that keeps constructor/struct defaults stock):
   - `--ram` default **512 KB** (was 64). `--ram 64` = stock.
   - `--fast-disk` default **ON** (was opt-in). New `--no-fast-disk` disables; Alt+D still toggles
     live in the SDL3 window.
   - **FM-PAC auto-loads into MSX primary slot 2** from `roms/fmpac.rom`, SRAM auto-persists to
     `roms/fmpac.rom.sram`. New `--no-fmpac` opts out; an explicit `--slot2`/`--cart2` cart (or an
     already-present FM-PAC) overrides the auto-load; **skipped gracefully with a banner note if
     `roms/fmpac.rom` is absent or fails to load** (never fails the boot).
2. **New `--stock` flag** — one-shot authentic bare machine (RAM 64 + accurate/fast-disk OFF + no
   FM-PAC). Precedence rule specified in §2.4. Both executables.
3. **CLI rename `--cart1/--cart2` → `--slot1/--slot2`** and `--cart1-type/--cart2-type` →
   `--slot1-type/--slot2-type` (official MSX term). `--cartN`/`--cartN-type` KEPT as accepted
   silent backward-compat aliases (help text shows `--slotN`; aliases still parse). Both
   executables (SDL3 `sdl3_cli` + headless `main.cpp` `--debug-session`).
4. **Banner "This session" enrichment** (`print_startup_summary` in `src/frontend/sdl3_main.cpp`),
   reading the RESOLVED machine state; plus a compact headless equivalent (§2.6).
5. **Test-baseline migration** so the flipped frontend defaults do not silently drift the accuracy
   / A-B baseline (§5.2 — the critical planning item).

### 1.2 Out of scope (would need a new decision entry)

- Any change to `Hbf1xvMachine`'s constructor default (`kDramBytes = 64 KB`,
  `hbf1xv_machine.h:83,202`) — it STAYS stock.
- Any change to the `Sdl3AppConfig` struct field defaults (`sdl3_app.h:34-177`) — they STAY stock
  (§2.7).
- Any device/timing behavior (VDP/PSG/OPLL/FDC/CPU). No `src/devices/` edit except — if strictly
  required — a read-only accessor already present; the FM-PAC device is reused as-is.
- New RAM sizes beyond the DEC-0061 `{64,128,256,512}` enum; the `>512 KB → external RAMPAC`
  boundary is unchanged.
- Changing the parity-trace / VDP / sprite / YM2413 / halnote / cpm harness machines (they
  construct their own stock machines and never read `--ram`/convenience — §5.2).

### 1.3 Assumptions (each with a verification action)

- **Assumption A1:** `roms/fmpac.rom` exists locally and is a valid FM-PAC image (1–4 × 16 KB
  banks; `CartridgeFmPacRom::is_valid_image_size`, `cartridge_fmpac_rom.cpp:41-45`). `roms/` is
  untracked per DEC-0047 so the planner cannot assert the file's presence.
  *Verification:* developer confirms locally (`Test-Path roms/fmpac.rom`) AND the graceful-skip
  path (§2.5 edges e/f) makes a missing/corrupt file a banner note, never a boot failure — so
  correctness holds either way.
- **Assumption A2:** The MSX main BIOS boot-time slot scan reaches primary slot 2 and calls an
  FM-PAC ROM's `INIT` there exactly as it does in slot 1 (the human historically inserted FM-PAC
  in slot 1). Slot fabric attaches `cartridge_slot2_` at primary slot 2
  (`hbf1xv_machine.cpp:206-207`), which is a standard unexpanded primary slot, so `CALL FMPAC` +
  SRAM discovery *should* work identically.
  *Verification (REQUIRED, developer live/headless):* headless `--debug-session` loading FM-PAC
  into slot 2 and running a typed-BASIC `CALL FMPAC` script (or an FM-PAC-saving disk game) must
  reach the FM-PAC manager / persist SRAM. If slot-2 discovery fails, escalate — this is the one
  genuinely uncertain behavioral item and gates the "FM-PAC auto-load actually usable" AC.
- **Assumption A3:** `sdl3_main.cpp`'s `main()` and `main.cpp`'s run paths are NOT covered by any
  ctest harness (only the pure parser `parse_sdl3_cli` / `parse_cartridge_cli` and the
  `Sdl3AppConfig`-driven `Sdl3App` are). *Verification:* grep confirms tests call
  `parse_sdl3_cli` / construct `Sdl3AppConfig` directly (`tests/unit/frontend/sdl3_cli_unit_test.cpp`,
  `tests/integration/frontend/*`), never the process `main`. This is WHY §2.7 mandates a pure,
  unit-testable resolver.
- **Assumption A4:** The FM-PAC canonical ROM is 64 KB (M43 reconciled a test "to the canonical
  64 KB ROM", `state/current-phase.md`), within the 1–4-bank valid range. *Verification:* the
  size validator accepts it; corrupt/wrong-size falls to graceful skip (§2.5 edge f).

---

## 2. Spec Summary

### 2.1 Flag / default matrix (old → new; both executables)

| Flag | Old default | New default | New sibling / alias | Notes |
|---|---|---|---|---|
| `--ram <64\|128\|256\|512>` | absent → 64 KB | absent → **512 KB** | `--stock`/`--ram 64` → 64 | Enum policy unchanged (`parse_ram_kb`, `sdl3_cli.cpp:59-80`). Machine-ctor default STAYS 64. |
| `--fast-disk` | off (opt-in) | **ON** | **NEW `--no-fast-disk`** (off) | Alt+D still toggles live (SDL3). |
| FM-PAC auto-load (slot 2) | none | **auto-load `roms/fmpac.rom`** | **NEW `--no-fmpac`** (off) | Skipped if slot 2 explicitly used / FM-PAC already present / file absent-or-corrupt. |
| FM-PAC SRAM path | `<cart>.rom.sram` (manual cart) | auto `roms/fmpac.rom.sram` | `--fmpac-sram`/`--no-fmpac-sram` (existing) | Reuses `set_fmpac_sram_path` (`hbf1xv_machine.h:360`). |
| **NEW `--stock`** | — | one-shot bare preset | — | RAM 64 + fast-disk OFF + no FM-PAC. §2.4. |
| `--cart1` | slot-1 path | (unchanged behavior) | **rename → `--slot1`** (`--cart1` silent alias) | Official MSX term. |
| `--cart2` | slot-2 path | (unchanged behavior) | **rename → `--slot2`** (`--cart2` silent alias) | |
| `--cart1-type` | slot-1 mapper | (unchanged) | **rename → `--slot1-type`** (`--cart1-type` silent alias) | |
| `--cart2-type` | slot-2 mapper | (unchanged) | **rename → `--slot2-type`** (`--cart2-type` silent alias) | |

All other flags (`--disk`, `--disk-writable`, `--scale`, `--filter`, `--fullscreen`, `--speed`,
`--border`, `--capture`, `--softwaredb`, debug flags) are UNCHANGED.

### 2.2 `--slotN` alias implementation (S1)

`parse_cartridge_cli` (`src/machine/cartridge_cli.cpp:55-111`) is the single shared parser used by
both exes. Add four alias arms writing the **same** `slot1`/`slot2` fields:

- `--slot1` ≡ `--cart1` → `slot1.path`
- `--slot2` ≡ `--cart2` → `slot2.path`
- `--slot1-type` ≡ `--cart1-type` → `slot1.type` / `type_was_explicit`
- `--slot2-type` ≡ `--cart2-type` → `slot2.type` / `type_was_explicit`

Contract:
- Both spellings parse to byte-identical `ParsedCartridgeCli`. (New AC + test, §4.)
- **Alias collision** (`--slot1` and `--cart1` both given for the SAME slot): they write the same
  field on a linear scan → **last occurrence wins** (identical to today's repeated-flag semantics).
  Documented, not an error. (New AC + test.)
- Help text (`sdl3_main.cpp` `print_usage`, and `main.cpp` `--debug-session` usage) shows only
  `--slotN`/`--slotN-type`; a one-line "(`--cartN` accepted as an alias)" note.

### 2.3 Session-defaults resolution — the central new logic (S2)

Because `main()` is untestable (Assumption A3), introduce ONE pure, headlessly-unit-testable
resolver in `src/frontend/` (mirrors the `parse_ram_kb` / `parse_speed_level` shared-validator
precedent, `sdl3_cli.h:148-167`). Suggested name: `resolve_session_defaults(...)`.

Inputs: the parsed CLI intent (tri-state per convenience field — see below) + `--stock` present?
Output: a small resolved struct — `{ std::size_t ram_bytes; bool fast_disk; bool fmpac_autoload;
bool is_stock; }` — that BOTH `sdl3_main.cpp` and `main.cpp` consume verbatim.

**Tri-state parser fields.** So the resolver can distinguish "user asked" from "unspecified", the
parser must expose intent, not a pre-baked default:
- `--ram` already tri-state (`std::optional<int> ram_kb`, absent = nullopt). Keep.
- `--fast-disk` / `--no-fast-disk` → `std::optional<bool> fast_disk_opt` (nullopt = unspecified).
  (Do NOT keep the current `bool fast_disk = false` as the source of the default; the *default*
  now lives in the resolver, not the field.)
- `--no-fmpac` → `bool no_fmpac` (a bare opt-out; auto-load is the default so only the negative
  needs a field). `--stock` → `bool stock`.

**Resolution algorithm (precedence: explicit per-field flag > `--stock` preset > convenience
default; positional-INDEPENDENT — see §2.4):**

```
ram_bytes      = ram_kb.has_value() ? ram_kb*1024              // explicit wins
                 : stock            ? 64*1024                  // --stock preset
                 :                    512*1024                 // convenience default
fast_disk      = fast_disk_opt.has_value() ? *fast_disk_opt    // explicit --fast-disk/--no-fast-disk wins
                 : stock                    ? false            // --stock preset
                 :                            true             // convenience default
fmpac_autoload = no_fmpac ? false                              // explicit opt-out wins
                 : stock   ? false                             // --stock preset
                 :           true                              // convenience default
is_stock       = stock                                         // banner label only
```

FM-PAC auto-load is ALSO suppressed at the load layer when slot 2 is explicitly occupied or an
FM-PAC is already present (§2.5) — that is an occupancy check, orthogonal to `fmpac_autoload`.

### 2.4 `--stock` preset + precedence rule (S2)

- `--stock` sets the preset **{RAM 64, fast-disk OFF, no FM-PAC}**.
- **Precedence = specificity, NOT order:** an explicit per-field flag always overrides the
  corresponding `--stock` field, regardless of argv order. Examples:
  - `--stock --ram 512` → RAM 512, fast-disk OFF, no FM-PAC.
  - `--ram 512 --stock` → **same** (RAM 512) — order-independent (matches the codebase's
    order-independent scan discipline, `cartridge_cli.h:55-61`).
  - `--stock --fast-disk` → RAM 64, fast-disk ON, no FM-PAC.
  - `--stock` alone → RAM 64, fast-disk OFF, no FM-PAC (full bare machine).
- **`--ram` + `--stock` conflict resolution** (edge from the task): `--ram <N>` is an explicit
  per-field flag → it wins over `--stock`'s RAM=64. (`--stock` alone yields 64.)

Rationale for specificity-over-order: the entire CLI is already a positional-independent scan;
making `--stock` order-sensitive would be a surprising inconsistency and hard to test. This gives
a single deterministic outcome the resolver's unit test pins exactly.

### 2.5 FM-PAC slot-2 auto-load resolution algorithm (S3)

Runs inside the asset-load path AFTER explicit `--slotN` carts are loaded — SDL3
`Sdl3App::load_configured_assets()` (`sdl3_app.cpp:51-155`) and the headless `--debug-session`
load path (`main.cpp` around 982-990). Gated by the resolved `fmpac_autoload` (§2.3).

```
if (!fmpac_autoload) -> skip (reason: --no-fmpac or --stock).            // banner: "not loaded"
else if (slot2 explicitly occupied by a --slot2/--cart2 cart) -> skip.   // banner: user cart wins
else if (an FM-PAC is already loaded in slot 1 OR slot 2) -> skip.       // avoid double FM-PAC
else if (!exists("roms/fmpac.rom")) -> skip.                             // banner: "auto-load skipped: roms/fmpac.rom not found"
else:
    read roms/fmpac.rom
    set_fmpac_sram_path( "roms/fmpac.rom.sram"  unless --fmpac-sram/--no-fmpac-sram override )   // BEFORE load
    result = machine.load_cartridge(2, CartridgeMapperType::FmPac, image)
    if (result != Ok) -> skip.                                          // banner: "auto-load skipped: roms/fmpac.rom invalid"
    else -> FM-PAC now in slot 2, SRAM bound.                           // banner: "loaded in slot 2"
```

Edge-case table (task-enumerated):

| Case | Behavior |
|---|---|
| slot 2 explicitly used (`--slot2 game.rom`) | auto-load SKIPPED (user cart wins). |
| both slots explicitly used | auto-load SKIPPED (slot 2 occupied). |
| slot 1 game only, slot 2 free | FM-PAC auto-loads into slot 2 (coexist). |
| `--no-fmpac` | auto-load SKIPPED. |
| `--stock` | auto-load SKIPPED. |
| `roms/fmpac.rom` missing | SKIPPED gracefully + banner note; boot proceeds. |
| `roms/fmpac.rom` corrupt/wrong-size | `load_cartridge` → `ImageSizeInvalidForMapperType` (`cartridge_slot.h:33`, validator `cartridge_fmpac_rom.cpp:41-45`) → SKIPPED gracefully + banner note; boot proceeds. |
| FM-PAC already present (e.g. `--slot1 roms/fmpac.rom`, the human's old habit) | auto-load into slot 2 SKIPPED (no double FM-PAC); SRAM path still bound to the slot-1 FM-PAC via the existing per-cart derive. |
| `--no-fmpac` + `--fmpac-sram <p>` | no FM-PAC loaded; `--fmpac-sram` is a no-op (existing "no-op unless an FM-PAC resolves" contract, `sdl3_app.h:141-143`). |

DEC-0050 correctness invariant: when `fmpac_autoload` resolves false OR the file is
absent/invalid, **no FM-PAC is inserted → "NO S-RAM AVAILABLE" remains correct** for the bare
machine. The auto-load never fabricates internal SRAM; it only inserts the external FM-PAC
cartridge peripheral. `--stock` / `--no-fmpac` therefore reproduce the exact v1.1.1 bare-machine
SRAM semantics.

Path note: `roms/fmpac.rom` is CWD-relative (matches how `roms/` is referenced elsewhere and how
tools launch from repo root). No new flag for the auto-load path; `--slot2 <path>` is the explicit
override, `--fmpac-sram`/`--no-fmpac-sram` override the SRAM path.

### 2.6 Banner enrichment (S4)

**Source of truth = the RESOLVED machine state, not the raw config.** Extend
`print_startup_summary` (currently takes only `Sdl3AppConfig`, `sdl3_main.cpp:146`) to also read
the initialized `Sdl3App`/`Hbf1xvMachine` (it is printed AFTER `app.init()`, `sdl3_main.cpp:312`),
so the banner reflects the actual auto-load / graceful-skip outcome. Recommended: pass `app` (or a
small `SessionSummary` populated by the app) so the banner can query:
`machine.dram_size()`, `machine.cartridge_slot1()/slot2().loaded()` + `mapper_type()`,
`machine.fmpac(2)`, `machine.fmpac_sram_path()`.

Fields to add / adjust (keep the exact existing box + aligned `Label : value` aesthetic the human
"really likes"):

- **Machine mode tag** — `[stock]` / `[--stock]` / `[convenience defaults]`.
- **Main RAM** — size + stock/modded label: `512 KB  (modded; --ram 64 or --stock for stock)` vs
  `64 KB  (stock)`.
- **VRAM** — `128 KB` (fixed; sourced from spec — the V9958 owns 128 KB VRAM, M14).
- **Slot 1 / Slot 2** — resolved contents: `roms/aleste.rom  (KonamiSCC)` / `roms/fmpac.rom
  (FM-PAC, auto-loaded)` / `(empty)`. (Renamed from "Cartridge 1/2".)
- **FM-PAC** — one of: `loaded in slot 2` / `loaded in slot 1` / `not loaded (--no-fmpac)` /
  `not loaded (--stock)` / `auto-load skipped: roms/fmpac.rom not found` / `auto-load skipped:
  roms/fmpac.rom invalid`.
- **SRAM** — `available -> <resolved abs path>` (when an FM-PAC is loaded and persistence is on)
  vs `not available (no FM-PAC; bare machine, DEC-0050)`.
- **Fast-disk** — explicit `ON (near-instant; Alt+D toggles)` vs `OFF (accurate timing)` (today it
  only prints a line when ON).
- **Disk-writable** — keep in the Disk line (`[writable -- saves persist]` / `[read-only ...]`).
- **Speed** — `0 (full speed)` … `N`.
- **Video** — keep existing `WxH, border-mode, filter`.

**Mock layout (SDL3, convenience defaults, YS II two-disk + auto FM-PAC):**

```
======================================================================
  Sony HB-F1XV  -  MSX2+ emulator                        [convenience defaults]
  Z80A @ 3.58 MHz | Yamaha V9958 VDP (128 KB VRAM)
  Audio: PSG (YM2149) + MSX-MUSIC FM (YM2413) + Konami SCC
  Storage: WD2793 FDC, 720 KB 3.5" floppy
----------------------------------------------------------------------
  This session:
    Main RAM   : 512 KB  (modded; --ram 64 or --stock for stock)
    VRAM       : 128 KB
    BIOS       : C:\...\bios  (7 ROMs)
                 f1xvbios.rom  f1xvsub.rom  f1xvkfn.rom
                 f1xvdisk.rom  f1xvmus.rom  ...
    Slot 1     : (empty)
    Slot 2     : roms\fmpac.rom  (FM-PAC, auto-loaded)
    FM-PAC     : loaded in slot 2
    SRAM       : available -> C:\...\roms\fmpac.rom.sram
    Disk A     : disks\ys2-d1.dsk  (+1 more; F11 cycles)  [writable -- saves persist]
    Fast-disk  : ON (near-instant loads; Alt+D toggles)
    Speed      : 0 (full speed)
    Video      : 960x720, edge-to-edge (Sony), linear
----------------------------------------------------------------------
  Hotkeys: ... (unchanged) ...
  Run with --help for all launch options.
======================================================================
```

**Mock (`--stock`):**

```
  Sony HB-F1XV  -  MSX2+ emulator                        [--stock]
    Main RAM   : 64 KB  (stock)
    ...
    Slot 1     : (empty)
    Slot 2     : (empty)
    FM-PAC     : not loaded (--stock)
    SRAM       : not available (no FM-PAC; bare machine, DEC-0050)
    Fast-disk  : OFF (accurate timing)
```

**Mock (auto-load skipped, file absent):**

```
    FM-PAC     : auto-load skipped: roms\fmpac.rom not found
    SRAM       : not available (no FM-PAC; bare machine, DEC-0050)
```

**Headless equivalent (S4, secondary):** the headless `--debug-session` today prints only ad-hoc
stderr diagnostics (`rom_diagnostics`, the fast-disk note, `main.cpp:969-980`). Add a compact,
same-content "This session" block to `--debug-session` (RAM, fast-disk, FM-PAC slot, slot1/2,
disk, `[stock]` tag) so the playtest agent / human see the resolved config and so the two exes are
consistent. Lower priority than the SDL3 banner; the SDL3 one is the human-facing artifact.

### 2.7 Architectural seam — WHY defaults live in the CLI layer, not the struct

To prevent silent drift (§5.1), the flipped defaults are applied ONLY where the CLI intent is
resolved (`sdl3_main.cpp` / `main.cpp` via the §2.3 resolver). Everything below stays stock:

- `Hbf1xvMachine` ctor default `kDramBytes = 64 KB` — UNCHANGED. Direct-construction tests
  (`hbf1xv_ram_size_integration_test.cpp:108`, and every unit/integration test that does
  `Hbf1xvMachine m;`) are UNAFFECTED.
- `Sdl3AppConfig` struct field defaults (`ram_bytes = 64 KB`, `fast_disk = false`, no auto-load
  field set) — UNCHANGED. Frontend integration tests that construct a default `Sdl3AppConfig`
  (`sdl3_app_scaling_integration_test.cpp:157` "The Sdl3AppConfig default fields are the oracle";
  `sdl3_app_multi_disk_integration_test.cpp:62`; `sdl3_app_fmpac_sram_integration_test.cpp:80`) are
  UNAFFECTED.
- The new FM-PAC auto-load is gated by a new `Sdl3AppConfig` field
  (e.g. `bool fmpac_autoload = false`) that DEFAULTS FALSE (stock); `sdl3_main.cpp` sets it true
  as the convenience default. So a bare `Sdl3AppConfig{}` never auto-loads → tests stay stock.

This is the single most important design decision in M46: **"frontend default" = the argv→config
mapping in the two `main` files, NOT any struct/ctor default.** DEC-0071 explicitly requires the
machine-constructor defaults stay stock; this package extends the same discipline to
`Sdl3AppConfig` for the identical anti-drift reason.

---

## 3. Milestones

Single milestone **M46** (per the template), decomposed into five deterministic slices.

- Milestone ID: **M46**
- Title: Convenience-first defaults + CLI slot rename + enriched startup banner (v1.1.2)
- Objective: Make the out-of-box launch a ready-to-game config (512 KB RAM, fast-disk on, FM-PAC
  in slot 2) while preserving one-flag stock authenticity and the full accuracy/A-B baseline.
- Included Scope: §1.1 items 1–5.
- Excluded Scope: §1.2.
- Dependencies: DEC-0061 (`--ram` enum), DEC-0050 (FM-PAC-is-external / no internal SRAM), DEC-0056
  (`--speed`/`--filter`/`--scale`), M36 (FM-PAC cartridge + SRAM persistence), M30 (cartridge
  identification / `--softwaredb`). All already in `main`.
- Interfaces Affected: `src/machine/cartridge_cli.{h,cpp}`, `src/frontend/sdl3_cli.{h,cpp}`,
  `src/frontend/sdl3_main.cpp`, `src/frontend/sdl3_app.{h,cpp}`, `src/main.cpp` (headless
  `--debug-session` load path + usage), plus a new resolver TU under `src/frontend/`. Machine
  wiring reuses existing `Hbf1xvMachine::load_cartridge` / `set_fmpac_sram_path` /
  `set_fast_disk` / `fmpac()` (no new machine method required).

**Slices:**

- **S1 — CLI slot rename + aliases.** `parse_cartridge_cli` gains `--slotN`/`--slotN-type`
  writing the same fields as `--cartN`/`--cartN-type`; help text updated in both exes. Pure parser,
  no behavior change. (`cartridge_cli.{h,cpp}`, usage text.)
- **S2 — Session-defaults resolver + `--stock`/`--no-fast-disk`/`--no-fmpac`.** New pure resolver
  (§2.3) + parser fields (tri-state `fast_disk_opt`, `no_fmpac`, `stock`); `sdl3_main.cpp` and
  `main.cpp` consume it. (`sdl3_cli.{h,cpp}`, new resolver TU, both `main`s.)
- **S3 — FM-PAC slot-2 auto-load wiring.** The §2.5 algorithm in `Sdl3App::load_configured_assets`
  + the headless `--debug-session` load path; new `Sdl3AppConfig fmpac_autoload` field (default
  false). (`sdl3_app.{h,cpp}`, `main.cpp`.)
- **S4 — Banner enrichment.** `print_startup_summary` reads resolved machine state (§2.6) + compact
  headless block. (`sdl3_main.cpp`, `main.cpp`.)
- **S5 — Test-baseline migration + new tests + tool `--stock` updates + evidence gates.** §4 + §5.2.

Regression Impact: see §5. Exit Criteria: see §4 (all ACs + evidence gates green + QA sign-off).

---

## 4. Acceptance Criteria

Deterministic, evidence-gated. Each maps to a new or migrated test.

**Defaults & `--stock` (S2):**
- **AC-1 (default-512 present):** the §2.3 resolver with EMPTY CLI yields `ram_bytes == 512*1024`,
  `fast_disk == true`, `fmpac_autoload == true`. Unit test (pure resolver).
- **AC-2 (`--ram 64` / `--stock` byte-identical to old default):** resolver with `--ram 64` and
  with `--stock` both yield `ram_bytes == 64*1024`; a `Hbf1xvMachine(64*1024)` constructed from
  either is byte-identical to a default `Hbf1xvMachine{}` (reuse the
  `hbf1xv_ram_size_integration_test` byte-identity proof — UNCHANGED, it already pins default ctor
  == 64 KB; see §5.2). `--stock` also yields `fast_disk == false`, `fmpac_autoload == false`.
- **AC-3 (`--stock` precedence):** `--stock --ram 512` → 512; `--ram 512 --stock` → 512
  (order-independent); `--stock --fast-disk` → fast-disk ON. Unit test (resolver).
- **AC-4 (fast-disk default ON):** resolver empty CLI → `fast_disk == true`; `--no-fast-disk` →
  false; `--fast-disk` → true. Unit test.

**FM-PAC slot-2 auto-load (S3):**
- **AC-5 (auto-load into slot 2 + SRAM bound):** with a real FM-PAC image at the auto-load path and
  slots free, after `init()`/load, `machine.fmpac(2) != nullptr`, `machine.cartridge_slot1()` is
  empty, and `machine.fmpac_sram_path()` resolves to the `.sram` beside the ROM. Integration test
  (temp-copy FM-PAC ROM per the `sdl3_app_fmpac_sram_integration_test.cpp:50` discipline; skip
  gracefully if no FM-PAC asset available).
- **AC-6 (coexist with a slot-1 game):** `--slot1 <konami game>` + auto-load → FM-PAC in slot 2 AND
  the game mapper in slot 1 (`fmpac(2) != nullptr` && `cartridge_slot1().loaded()`).
- **AC-7 (explicit slot 2 wins):** `--slot2 <cart>` → no auto-load (`fmpac(2)` reflects the user
  cart, not a forced FM-PAC).
- **AC-8 (`--no-fmpac` / `--stock` → no FM-PAC + SRAM correctness):** neither slot holds an FM-PAC;
  DEC-0050 "NO S-RAM AVAILABLE" holds (no internal SRAM fabricated).
- **AC-9 (graceful missing/corrupt ROM):** auto-load path with an absent file, and with a
  wrong-size file, each → boot proceeds, no FM-PAC loaded, banner note emitted, non-error exit.
  Integration test (absent + deliberately-truncated image).
- **AC-10 (slot-2 discovery usable — Assumption A2):** developer live/headless proof that
  `CALL FMPAC` / FM-PAC SRAM works with FM-PAC in slot 2 (typed-BASIC or an FM-PAC-saving disk
  game). If not reproducible headlessly, a documented live human verify; escalate on failure.

**CLI rename / aliases (S1):**
- **AC-11 (alias equivalence):** `parse_cartridge_cli` yields byte-identical results for
  `--slot1 x --slot1-type Konami` vs `--cart1 x --cart1-type Konami` (path, type,
  `type_was_explicit`). Unit test.
- **AC-12 (alias collision last-wins):** `--slot1 a --cart1 b` → `slot1.path == b`;
  `--cart1 b --slot1 a` → `slot1.path == a`. Unit test.
- **AC-13 (`auto` value + softwaredb still flow through `--slotN-type`):** `--slot1-type auto` →
  `type_was_explicit == false` (mirrors the existing `--cart1-type auto` case). Unit test.

**Banner (S4):**
- **AC-14 (banner correctness):** the enriched banner reports VRAM 128 KB, RAM stock/modded label,
  FM-PAC status (loaded slot N / not loaded / auto-load-skipped-absent / auto-load-skipped-invalid),
  SRAM availability + resolved path, fast-disk on/off, disk-writable, speed, scale/filter, the
  `--stock`/convenience tag, and resolved slot 1/2 contents. Verified by (a) a snapshot/text
  assertion on the resolver + a banner-formatting helper if extractable as a pure function, and
  (b) a manual/scripted run capture for the three states (convenience / `--stock` / fmpac-absent).
  Prefer extracting the banner body into a testable helper that takes the resolved
  `SessionSummary` and returns a string, so AC-14 has a deterministic ctest seam (no process
  capture needed).

**Migration / regression (S5):**
- **AC-15 (baseline unweakened):** `hbf1xv_ram_size_integration_test` still proves default ctor ==
  64 KB stock byte-identity, UNCHANGED (§5.2 sentinel). Machine-ctor + `Sdl3AppConfig` struct
  defaults verified stock by reading the source (constructor `hbf1xv_machine.cpp:44-51`; struct
  `sdl3_app.h:176`).
- **AC-16 (tool migration applied):** every `--debug-session`/exe-launch tool that compares to
  openMSX bare or reproduces a stock golden passes `--stock` (§5.2 inventory); at least one openMSX
  A/B harness re-run WITH `--stock` reproduces an empty/parity diff (conditional openMSX A/B gate).

**Evidence gates (mandatory, run + captured; never fabricated):**
- **AC-17:** `tools/validate-assets.ps1` PASS (BIOS present + ≥1 ROM).
- **AC-18:** `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` refreshed.
- **AC-19:** `cmake --build build --config Debug` (SDL3=ON) both exes build.
- **AC-20:** `ctest --test-dir build -C Debug --output-on-failure` fully green (existing suite +
  new M46 cases).
- **AC-21 (conditional openMSX A/B):** one A/B harness re-run WITH `--stock` → parity preserved
  (AC-16). Store under `docs/`.
- **AC-22 (ZEXALL withheld-substituted):** `git diff v1.1.1..HEAD -- src/devices/cpu src/core`
  is EMPTY → Z80/core byte-identical → ZEXALL sweep withheld (the M42/M43 precedent). Record the
  empty-diff proof.

Unit Test Plan: resolver (AC-1..4, AC-3 precedence), `parse_cartridge_cli` aliases (AC-11..13),
`parse_sdl3_cli` new flags (`--no-fast-disk`/`--no-fmpac`/`--stock`), banner-formatting helper
(AC-14).
System Integration Test Plan: FM-PAC slot-2 auto-load + SRAM + coexist + skip paths (AC-5..9),
slot-2 discovery live/headless (AC-10), migrated A/B parity (AC-16/21).
Regression Impact: §5. Exit Criteria: all ACs + evidence gates green + QA sign-off.

---

## 5. Risks

### 5.1 Silent baseline drift (the critical risk)

Flipping the frontend defaults changes what a bare launch produces (512 KB + fast-disk + FM-PAC).
Any test/tool that launched an exe relying on defaults now silently gets a different machine.
**Mitigations:** (1) defaults live in the CLI resolver, not the ctor/struct (§2.7) — direct
construction stays stock; (2) the §5.2 migration inventory adds `--stock` to every stock-reliant
launch; (3) AC-15 keeps the byte-identity sentinel unchanged; (4) AC-22 pins zero cpu/core drift.

### 5.2 Test-baseline migration inventory (each site + exact fix)

**C++ tests (compile against the machine/config directly — mostly unaffected by CLI defaults):**

| Test | Reliance | Fix |
|---|---|---|
| `tests/integration/machine/hbf1xv_ram_size_integration_test.cpp` | Constructs `Hbf1xvMachine` directly; `Hbf1xvMachine def;` asserts default == 64 KB stock (line 108-113). | **NO CHANGE — this is the SENTINEL** proving the ctor default stays stock. Do NOT flip its expectation; if someone flips the ctor default to 512 this test must fail. |
| `tests/integration/frontend/sdl3_app_scaling_integration_test.cpp` (and `_multi_disk_`, `_snapshot_`, `_input_script_`, `_headless_`, `_fmpac_sram_`) | Construct a default `Sdl3AppConfig` as "the oracle" (`scaling` line 157). | **NO CHANGE**, provided `Sdl3AppConfig` struct defaults stay stock (§2.7). Verify the new `fmpac_autoload` field DEFAULTS FALSE so these configs never auto-load. |
| `tests/unit/frontend/sdl3_cli_unit_test.cpp` | Pins `parse_sdl3_cli` field behavior (incl. `--ram` absent → nullopt, Case 16). | **ADD** cases: `--no-fast-disk`/`--fast-disk` tri-state, `--no-fmpac`, `--stock`, and (if fast-disk becomes `std::optional<bool>`) update any assertion that reads `fast_disk` as a plain bool. Existing `--ram`/`--speed`/etc. cases stay valid (parser still reports intent). |
| `tests/unit/machine/cartridge_cli_unit_test.cpp` | Pins `--cartN`/`--cartN-type` parsing. | **ADD** alias-equivalence + collision-last-wins + `--slotN-type auto` cases (AC-11..13). Existing `--cartN` cases stay valid. |
| `tests/integration/frontend/sdl3_cli_session_integration_test.cpp` | `config_from_args` REPLICATES the `sdl3_main.cpp` argv→config mapping (line 77-95). | **UPDATE** to route through the new §2.3 resolver so it asserts the convenience defaults + `--stock` outcome (otherwise it diverges from production). Add assertions for the resolved `ram_bytes`/`fast_disk`/`fmpac_autoload`. |
| NEW: FM-PAC slot-2 auto-load integration test | — | **ADD** (AC-5..9): temp-copy the FM-PAC ROM (mirror `sdl3_app_fmpac_sram_integration_test.cpp:50`), assert slot-2 load + SRAM bind + coexist + explicit-slot2-wins + missing/corrupt graceful skip; skip gracefully when no FM-PAC asset is available. |
| NEW: resolver unit test | — | **ADD** (AC-1..4, precedence). |

**Tools / harnesses (launch the real exe) — `--debug-session`-based (INHERIT flipped defaults →
ADD `--stock`):**

| Tool | Launch (evidence) | Fix |
|---|---|---|
| `tools/openmsx-m28-c5-boot-parity.ps1` | `--debug-session $BiosDir N --disk $Disk --trace-cpu ...` (line 71) — openMSX bare-machine boot A/B. | **ADD `--stock`.** |
| `tools/m41-typed-audio-ab.ps1` | `--debug-session ... --input-script ... --frames N --audio-dump ... --audio-sync` (line 107-108) — openMSX audio A/B; `--audio-dump` mixes FM-PAC OPLL, so an auto-loaded FM-PAC would corrupt the audio A/B. | **ADD `--stock`** (required for audio parity). |
| `tools/m41-run.ps1` | `--debug-session ... (--disk\|--cart1) --frames ... --dump-frame` (line 84-100). | **ADD `--stock`** (reproduces M41-era stock captures). |
| `tools/m41-disk-scenario.sh` | `--debug-session ... --disk ... --input-script ... --frames ... --disk-writable` (line 29-31) — openMSX disk A/B. | **ADD `--stock`.** |
| `tools/msx-basic-to-disk.py` | `--debug-session ... --disk ... --frames 600` (line 313-314) — deterministic disk-fixture authoring; fast-disk-off matters for disk-write timing. | **ADD `--stock`.** |
| `tools/playtest-capture.ps1` | `--debug-session ... --frames --disk --input-script --dump-frame` (line 133-148). | **ADD `--stock`** to preserve current captures. (Optional: add a `-Convenience` opt-in switch for playtests that WANT fast-disk/FM-PAC — the playtest agent has no golden baseline, so either is defensible; default to `--stock` for reproducibility.) |
| `tools/laydock-sweep.sh` | `--debug-session ... --frames --disk --input-script` (line 17-18). | **ADD `--stock`.** |
| `tools/capture-metalgear-evidence.ps1` | `--debug-session ... --frames --cart1 ... --cart1-type Konami --input-script --dump-frame` (line 79-87) — golden captures; slot 2 free → FM-PAC would auto-load. | **ADD `--stock`.** |
| `tools/capture-aleste-play-evidence.ps1` | `--debug-session ... --frames --cart1 ... --cart1-type KonamiSCC --input-script --dump-frame` (line 80-88) — golden captures. | **ADD `--stock`.** |

**Tools — default-run / parity modes (do NOT read `--ram`/convenience → mostly inert):**

| Tool | Launch | Fix |
|---|---|---|
| `tools/openmsx-m30-identification-ab.ps1` | `sony_msx_headless --cart1 <rom>` (default-run path). | Affected only by the `--ram` default flip, which is inert for SHA1/heuristic identification. **ADD `--stock`** for a clean bare-machine match (low priority; behaviorally inert). |
| `--parity-trace` harnesses (`openmsx-cpu-parity`, `-io-parity`, `-mem-parity`, `-peripheral-io-parity`, `-m16-boot-parity`, `-m19-cartridge-parity`, `-m29-scc-parity`, `-fdc-read-timing-ab`, `-ym2413-parity`) | `--parity-trace` / `--ym2413-parity` construct their own machines BEFORE the `--ram` block; never auto-load FM-PAC. | **NO CHANGE** (inherently stock). Document as verified-unaffected. |
| `--vdp-parity` / `--vdp-render-parity` / `--sprite-cmd-parity` (`openmsx-vdp-parity`, `-m21-vdp-render-parity`, `-m22-sprite-cmd-parity`) | own machines. | **NO CHANGE.** |
| `--halnote-parity` (`openmsx-m20-halnote-parity`) | own machine. | **NO CHANGE.** |
| `--cpm`/`--cpm-run` (`zexall-report.py` parses output) | own machine. | **NO CHANGE.** |

Rule for future tools: any launch that (a) compares to openMSX bare, or (b) reproduces a pre-M46
golden, MUST pass `--stock`. Playtest/authoring tools without a golden may opt into convenience
but must do so EXPLICITLY (never rely on the flipped implicit default).

### 5.3 FM-PAC slot-2 discovery uncertainty (Assumption A2)

Highest behavioral risk. If the BIOS slot scan or our slot-2 subslot decode doesn't surface the
FM-PAC ROM header the way slot 1 does, `CALL FMPAC` / SRAM won't work in slot 2 → the convenience
default would insert a non-functional FM-PAC. Mitigation: AC-10 live/headless verify BEFORE
closing; if it fails, escalate (options: keep auto-load but into slot 1, or fix slot-2 decode —
either would be a new decision).

### 5.4 Double-OPLL audio when FM-PAC auto-loaded

The machine already mixes `fmpac(1)`/`fmpac(2)` OPLL on top of built-in MSX-MUSIC
(`sdl3_app.cpp:423-434`, `main.cpp:1116-1143`). An auto-loaded, un-keyed FM-PAC OPLL contributes
silence, so a default launch that never touches FM-PAC should be audibly unchanged — but this is
why the A/B harnesses MUST use `--stock` (§5.2) to keep matching openMSX's single built-in OPLL.
Low functional risk; covered by the migration.

### 5.5 Order-independence of `--stock`

Resolved by the specificity-over-order rule (§2.4). Risk if implemented as order-sensitive
last-wins over individual fields; the resolver's unit test (AC-3) pins both orders to the same
outcome.

### 5.6 Blast-radius creep

Wiring FM-PAC auto-load + `--ram` into the headless `--debug-session` path touches `main.cpp`
(currently `--debug-session` uses the default ctor `Hbf1xvMachine machine;`, `main.cpp:951`, and
does NOT read `--ram`). This is frontend/CLI code reusing existing machine APIs — no `src/devices/`
or `src/core/` edit. Guardrail: confirm the AC-22 empty cpu/core diff at every gate.

---

## 6. Developer Handoff

### 6.1 File / blast-radius map (frontend + machine only; zero cpu/core)

- `src/machine/cartridge_cli.{h,cpp}` — S1: `--slotN`/`--slotN-type` aliases (same fields as
  `--cartN`).
- `src/frontend/sdl3_cli.{h,cpp}` — S2: tri-state `fast_disk_opt`, `no_fmpac`, `stock` parser
  fields; new `--no-fast-disk`/`--no-fmpac`/`--stock` arms; the shared `resolve_session_defaults`
  declaration/definition (or a new `src/frontend/session_defaults.{h,cpp}` TU — developer's call
  per `src/CLAUDE.md`).
- `src/frontend/sdl3_app.{h,cpp}` — S3: new `Sdl3AppConfig fmpac_autoload = false` (default
  stock); FM-PAC slot-2 auto-load in `load_configured_assets()` (§2.5) reusing
  `set_fmpac_sram_path` + `load_cartridge(2, FmPac, …)`; a `SessionSummary` accessor for the
  banner (S4).
- `src/frontend/sdl3_main.cpp` — S2 resolve + apply to `Sdl3AppConfig`; S1 help text (`--slotN`);
  S4 enriched `print_startup_summary` reading resolved state.
- `src/main.cpp` — S2 resolve + apply (`--ram`/`--stock`/fast-disk/FM-PAC) in the
  `--debug-session` load path; S1 usage text; S4 compact headless banner. **No** change to the
  parity/vdp/sprite/cpm/halnote dispatch (they stay stock).
- Machine reuse only (NO new method needed): `load_cartridge`, `set_fmpac_sram_path`, `fmpac`,
  `set_fast_disk`, `dram_size`, `cartridge_slot1/2`, ctor `Hbf1xvMachine(dram_bytes)`.

**Do NOT touch:** `src/devices/cpu/*`, `src/core/*`, `Hbf1xvMachine` ctor default, `Sdl3AppConfig`
struct field defaults (add the new `fmpac_autoload` field defaulting FALSE), any device.

### 6.2 Recommended build order

S1 (aliases, isolated, low-risk) → S2 (resolver + flags, the logic core) → S3 (FM-PAC auto-load) →
S4 (banner) → S5 (tests + tool `--stock` migration + gates). Run the fast ctest subset after S1–S4;
run the full evidence-gate battery + at least one `--stock` A/B at S5. Per the slow-test cadence
memory, ZEXALL is withheld-substituted (AC-22) since there is zero cpu/core diff.

### 6.3 Live-verify obligations (developer, before QA)

- **AC-10 FM-PAC slot-2 discovery** (Assumption A2) — the one genuinely uncertain item. Headless
  typed-BASIC `CALL FMPAC` or an FM-PAC-saving disk game; confirm SRAM persists. Escalate on
  failure.
- Re-run one migrated openMSX A/B harness WITH `--stock` → parity preserved (AC-16/21).

### 6.4 Open questions for the coordinator/human

1. **Headless `--debug-session` convenience-default scope.** DEC-0071 says "both executables get
   consistent defaults." This package flips the convenience defaults in the headless
   `--debug-session` game path (with the §5.2 `--stock` migration). Confirm this is desired vs.
   flipping ONLY the SDL3 exe and leaving headless stock (narrower, no tool migration, but the two
   exes then differ). Recommendation: **flip both** (DEC intent + task wording), migrate tools.
2. **`playtest-capture.ps1` config.** Default it to `--stock` (reproducible, matches history) or to
   convenience (fast-disk speeds up captures)? Recommendation: **`--stock` default + optional
   `-Convenience` switch.**
3. **FM-PAC auto-load path override.** Fixed `roms/fmpac.rom`, overridable only via explicit
   `--slot2 <path>`. Confirm no dedicated `--fmpac <path>` flag is wanted (keeps the surface small).
4. **Fallback if AC-10 fails** (slot-2 discovery broken): auto-load into slot 1 instead, or fix
   slot-2 decode? Both are new decisions; surface only if the live verify fails.

### 6.5 Ledger obligations

Update `agent-protocol/state/current-phase.md` + `state/milestones.md` (M46 block) each cycle;
append-only channels; ISO-8601 timestamps. DEC-0071 is the authorizing decision — no new scope
decision needed unless an open question (§6.4) changes the plan.
