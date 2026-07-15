# M50 Planner Package — Externalized Strict-XML Configuration (production hardening)

- Milestone ID: M50
- Title: Externalized strict-XML configuration (defaults/knobs out of hardcoded C++)
- Phase: PLANNING (opened DEC-0077, 2026-07-14)
- Spec Owner: MSX Planner Agent
- Ledger: DEC-0077 (decisions), M50 entry (`agent-protocol/state/milestones.md`), phase note
  (`agent-protocol/state/current-phase.md`)
- Grounding: this is a CONFIGURATION-PLUMBING milestone. It introduces NO new device/timing
  behavior; every value the XML can set was already delivered + (where behavior-affecting)
  openMSX-A/B-validated in its origin milestone (`--ram` → M42/DEC-0061, fast-disk/FM-PAC
  auto-load → M37/M46, video/persistence knobs → M37/M39). Reference precedence (DEC-0073) is
  therefore not exercised by new behavior here; it is honored by the HARD-EXCLUDE of all silicon
  timing constants (§3), which stay in code as the tier-1 machine spec.

---

## 1. Scope and Assumptions

### 1.1 Objective

Move the emulator's DEFAULTS and KNOBS out of hardcoded C++ into a single strict-XML config file
so the machine is configurable WITHOUT recompiling. Behavior:

- Config file FOUND → parse, validate, and use its values (subject to the precedence rule §4.1).
- Config file NOT found → print ONE clear WARNING line and continue on the built-in defaults
  (the emulator still runs standalone with zero config).
- "Strict XML" = well-formed enough to tokenize + per-value VALIDATED: known keys only, correct
  type, range/enum-checked. A bad value never crashes and never silently corrupts state — it warns
  (naming the offending key) and falls back to that key's built-in default (§4.2).

### 1.2 What EXTERNALIZES to the XML (confirmed owner scope: "Settings + machine config")

Grounded in the code audit below. Two categories:

**(A) CLI-option / session defaults** (each is an existing knob whose default is currently
hardcoded in the M46 anti-drift CLI-resolution layer or the `Sdl3AppConfig` struct):

| Knob | CLI flag(s) | Current hardcoded default | Source of truth today |
| --- | --- | --- | --- |
| Main RAM KB | `--ram <64\|128\|256\|512>` | convenience 512 / stock 64 | `resolve_session_defaults()` (`src/frontend/sdl3_cli.h:214-235`), ctor `kDramBytes=64KB` (`src/machine/hbf1xv_machine.h:202`) |
| Fast-disk (FDC turbo) | `--fast-disk` / `--no-fast-disk` | convenience ON | `resolve_session_defaults()`; `Sdl3AppConfig.fast_disk=false` (`sdl3_app.h:66`) |
| FM-PAC auto-load | `--no-fmpac` (opt-out) | ON | `resolve_session_defaults()`; `Sdl3AppConfig.fmpac_autoload=false` (`sdl3_app.h:214`) |
| FM-PAC auto-load slot | (implicit) | slot 2 | `sdl3_main.cpp` banner + load layer |
| Scale (window size) | `--scale <N>` | 3 → 960×720 | `Sdl3AppConfig.window_width/height=960/720` (`sdl3_app.h:72-73`) |
| Texture filter | `--filter <nearest\|linear>` | Linear | `Sdl3AppConfig.texture_filter=SDL_SCALEMODE_LINEAR` (`sdl3_app.h:180`); `ParsedSdl3Cli.filter=Linear` (`sdl3_cli.h:135`) |
| Persistence | `--persistence <0..100>` | 0 (OFF) | `Sdl3AppConfig.persistence=0` (`sdl3_app.h:189`) |
| Persistence mode | `--persistence-mode <avg\|peak>` | Average | `Sdl3AppConfig.persistence_mode=Average` (`sdl3_app.h:197`) |
| Speed level | `--speed <0..7>` | 0 (full speed) | `ParsedSdl3Cli.speed_level=nullopt` → level 0 (`sdl3_cli.h:126`) |
| Border | `--border` / `--no-border` | OFF (edge-to-edge) | `Sdl3AppConfig.border_enabled=false` (`sdl3_app.h:87`); `ParsedSdl3Cli.border_enabled=false` (`sdl3_cli.h:63`) |
| Fullscreen | `--fullscreen` | OFF | `ParsedSdl3Cli.fullscreen=false` (`sdl3_cli.h:154`) |
| Capture (F10 gate) | `--capture <on\|off>` | OFF (F10 inert) | `Sdl3AppConfig.capture_enabled=false` (`sdl3_app.h:174`); `ParsedSdl3Cli.capture_enabled=false` (`sdl3_cli.h:163`) |
| Disk-writable | `--disk-writable` | OFF (in-memory only) | `Sdl3AppConfig.disk_writable=false` (`sdl3_app.h:61`) |

**(B) Machine sizing/layout + asset paths** (currently hardcoded literals in the machine):

| Item | Current hardcoded value | Source of truth today |
| --- | --- | --- |
| Main RAM KB | 64 KB (`kDramBytes`) | `hbf1xv_machine.h:202` — same knob as (A) `--ram`, unified under `<machine><ram>` (§4.3) |
| VRAM KB | 128 KB (`VdpVram::kVramBytes`, `constexpr`) | `src/devices/video/vdp_vram.h:47` — strict-spec value; validated-to-128 (§4.4) |
| BIOS/ROM directory | `"bios"` | `hbf1xv_machine.h:1043` (`asset_root_{"bios"}`), overridable via `--bios-dir` |
| 7 BIOS ROM filenames + sizes | `f1xvbios.rom`(0x8000), `f1xvext.rom`(0x4000), `f1xvkdr.rom`(0x8000), `f1xvdisk.rom`(0x4000), `f1xvmus.rom`(0x4000), `f1xvkfn.rom`(0x40000), `f1xvfirm.rom`(0x100000) | `hbf1xv_machine.cpp:513-520` (`load_rom_assets()` — hardcoded `RomAssetLoader::Spec` literals) |
| FM-PAC ROM path | `"roms/fmpac.rom"` | `Sdl3AppConfig.fmpac_autoload_rom_path` (`sdl3_app.h:220`) |
| FM-PAC SRAM path | auto-derived `<cart>.rom.sram` | `Sdl3AppConfig.fmpac_sram_path` (`sdl3_app.h:154`) |
| softwaredb path | `"references/openmsx-21.0/share/softwaredb.xml"` | `kDefaultSoftwareDbPath` (`src/machine/cartridge_identifier.h:39`) |
| Slot layout | built-in HB-F1XV map (BIOS 0-0, RAM 3-0, SUB+Kanji 3-1, DISK 3-2, FM-MUSIC 3-3, Halnote 0-3, slots 0 & 3 expanded) | hardcoded in `hbf1xv_machine.cpp` wiring | OPTIONAL/advanced; validated-to-builtin (§4.5) |

### 1.3 Assumptions

- **Assumption A-1:** the full deterministic ctest suite baseline is **235 tests** (owner's stated
  current count). Verification action: the developer captures the exact pre-M50 clean-rebuild count
  and pins it; every M50 acceptance gate is expressed against "the pre-M50 baseline count," so a
  stale literal never masks a regression.
- **Assumption A-2:** the M46/DEC-0071 anti-drift architecture is intact — the FLIPPED convenience
  defaults live ONLY in `resolve_session_defaults()` and `sdl3_main.cpp`'s post-parse resolution;
  the `Hbf1xvMachine` ctor default (`kDramBytes`) and the `Sdl3AppConfig` struct field defaults stay
  STOCK. Verified in `src/frontend/sdl3_cli.h:207-235` + `src/frontend/sdl3_main.cpp:378-389`. M50
  plugs into this SAME layer (§4.1); it MUST NOT alter the ctor/struct defaults.
- **Assumption A-3:** the tolerant XML tokenizer in `src/machine/software_db.cpp:37-136`
  (`TagScanner` + `trim`/`decode_entities`) is fit to REUSE for the config parser (subset schema,
  attribute-aware, comment/CDATA/DOCTYPE-tolerant, never throws). Verified by reading the file. No
  new third-party XML dependency is warranted (§5).
- **Assumption A-4:** the shipped reference `sony_msx_hbf1xv.xml` placed at repo ROOT is NOT
  gitignored. Verified against `.gitignore` — no `*.xml` rule; root is not under any ignore pattern
  (`/build*/`, `/docs/`, `/roms/*`, … do not match a root `sony_msx_hbf1xv.xml`).
- **Assumption A-5:** no ctest test invokes the SDL3 exe interactively without `--hidden-window`,
  and no test passes `--config`. Verification action: the developer greps the test target for exe
  invocations and confirms the determinism rule (§4.6) leaves every test on the no-config path.

### 1.4 Non-goals (explicitly OUT of M50)

- **HARD-EXCLUDE all silicon timing constants** (§3) — they stay in code as the immutable machine
  spec; the XML can never touch them.
- Runtime LIVE re-sizing of VRAM to a non-128 value (would touch the V9958 device; VRAM stays
  validated-to-128, §4.4). Live slot-layout REMAPPING to an arbitrary map (validated-to-builtin
  only, §4.5).
- A general-purpose XML library, XSD schema validation engine, or config hot-reload/watch.
- New device behavior, new CLI flags beyond `--config <path>`, or any change to what a given knob
  VALUE does (M50 only changes WHERE the default comes from).
- Environment-variable or registry configuration; multiple stacked config files / includes.

---

## 2. Code audit (grounding for §1.2)

Read during planning (concrete paths, per guardrails):

- **CLI surface (SDL3):** `src/frontend/sdl3_cli.h:45-235` (`ParsedSdl3Cli`, `Sdl3AppConfig`
  feeders, `resolve_session_defaults`, `SessionDefaultsRequest`, `ResolvedSessionDefaults`),
  `src/frontend/sdl3_app.h:35-220` (`Sdl3AppConfig` struct defaults), `src/frontend/sdl3_main.cpp:
  240-414` (post-parse resolution + banner + FM-PAC slot).
- **CLI surface (headless):** `src/main.cpp:716-1000` (`DebugSessionOptions`,
  `parse_debug_session_options`) — the `--debug-session` game path consumes the SAME
  `resolve_session_defaults()` (`--ram`/`--fast-disk`/`--no-fmpac`/`--stock`). The many diagnostic
  flags there (`--trace-cpu`, `--snapshot`, `--watch-mem`, `--audio-dump`, …) are one-shot
  INVOCATION switches, NOT persistent defaults → they are correctly OUT of the XML (§1.4).
- **Machine sizing/paths:** `src/machine/hbf1xv_machine.h:83,202,932,1043`
  (`Hbf1xvMachine(dram_bytes=kDramBytes)`, `kDramBytes=64KB`, `dram_{kDramBytes}`,
  `asset_root_{"bios"}`); `src/machine/hbf1xv_machine.cpp:506-528` (`load_rom_assets()` — the 7
  hardcoded `RomAssetLoader::Spec` literals); `src/devices/video/vdp_vram.h:47`
  (`kVramBytes=128KB`); `src/machine/rom_asset_loader.h:40-65` (the load/spec seam);
  `src/machine/cartridge_identifier.h:39` (`kDefaultSoftwareDbPath`).
- **Reusable XML parser:** `src/machine/software_db.{h,cpp}` — `TagScanner::next()` tolerant
  tokenizer (`software_db.cpp:43-122`), `trim()` (`:138-148`), `decode_entities()` (`:152-188`),
  flat state-machine parse (`:230-444`). Anonymous-namespace/file-local today → §5 extraction plan.

---

## 3. HARD-EXCLUDE — silicon timing constants stay in code (non-negotiable)

The XML MUST NOT be able to reach any hardware-TIMING constant. These are the silicon spec
(reference-precedence **tier 1**, `references/fact-sheets/`), NOT "settings." A bad XML edit must
never be able to break emulation accuracy — that is the core safety property of this milestone.
Excluded (stay hardcoded, immutable):

- **Z80A 3.58 MHz** system clock (`kSystemClockHz`) and frame-cycle constants — `src/core/`,
  `src/machine/`.
- **Interrupt-ack timing** IM1 = 13T / IM2 = 19T / NMI = 11T (DEC-0074) — `src/devices/cpu/`.
- **S1985 +1 M1 opcode-fetch wait** — `src/devices/chipset/s1985_engine.*`.
- **V9958 access-slot counts** 154 / 88 / 31 and **1368 VDP cyc/line** + the M48/M49-era parity
  values — `src/devices/video/vdp_access_timing.h`, `src/devices/video/v9958_vdp.*`,
  `src/devices/video/sprite_engine.*`.
- **FDC (WD2793) cycle constants** — `src/devices/fdc/wd2793.cpp`.

**Evidence gate (accuracy protection):** after M50, `git diff` of `src/core/`, `src/devices/cpu/`,
`src/devices/video/vdp_access_timing.h`, `src/devices/video/v9958_vdp.*`,
`src/devices/video/sprite_engine.*`, `src/devices/fdc/wd2793.cpp`, and
`src/devices/chipset/s1985_engine.*` must be EMPTY (M50 never touches them). This doubles as the
"no cpu/core hardware-constant edit" gate the owner requested and keeps the ZEXALL sweep un-triggered
per `feedback_slow_test_cadence.md`.

---

## 4. Spec Summary — schema, precedence, fallback, determinism

### 4.1 The insertion layer (anti-drift + determinism seam)

The config loader is a **Layer-B** concept, plugging into the SAME CLI-resolution layer as
`resolve_session_defaults()` — NEVER into the machine ctor or the `Sdl3AppConfig`/`ParsedSdl3Cli`
struct field defaults (which stay stock, A-2). Concretely the resolved-effective-value pipeline
becomes, per knob:

```
effective = CLI-flag-if-user-specified
            else XML-value-if-present-and-valid
            else built-in-default   (the current hardcoded default)
```

For the convenience knobs that flow through `resolve_session_defaults()` (RAM, fast-disk, FM-PAC
auto-load), the XML value becomes the BASE DEFAULT the resolver falls back to when the CLI field is
unspecified — i.e. the config replaces the hardcoded convenience default, and an explicit CLI flag
still wins over both. For the presentation knobs fed to `Sdl3AppConfig` (scale/filter/persistence/
persistence-mode/speed/border/fullscreen/capture), the XML value overrides the struct default and a
present CLI flag overrides the XML.

### 4.2 Missing / malformed handling (owner wants graceful — decided)

- **File not found** (no config in the search order, §4.6): ONE stderr WARNING line
  (`config: WARNING no sony_msx_hbf1xv.xml found (searched: …); using built-in defaults`) then
  continue on built-in defaults. This is the expected zero-config first-run behavior.
- **Per-key fallback (default policy for VALUE errors):** an unknown key, wrong type, or
  out-of-range/enum value warns naming the offending key
  (`config: WARNING <key> = '<value>' invalid (<reason>); using default <default>`) and falls back
  to THAT key's built-in default. Every other valid key still applies. This is the graceful,
  least-surprising policy and is the DECIDED default.
- **Whole-file fallback (only for STRUCTURAL failure):** if the file cannot be tokenized to a
  recognizable `<hbf1xv-config>` root at all (truncated/binary/garbage, wrong root element), warn
  once (`config: WARNING <path> is not a valid hbf1xv-config file; ignoring it, using built-in
  defaults`) and use all built-in defaults. The tolerant tokenizer (§5) never throws, so "structural
  failure" is detected by "no usable root/keys recovered," not by an exception.
- Never crash on any input. Never partially-apply a rejected key.

### 4.3 XML schema (element/attribute layout, types, ranges)

Root `<hbf1xv-config version="1">`. Two sections: `<defaults>` (session/CLI knobs) and `<machine>`
(sizing + paths). All elements/attributes OPTIONAL — an omitted knob keeps its built-in default.
Attribute-based (matches the reused tokenizer's attribute handling; values are attribute strings).

```xml
<hbf1xv-config version="1">

  <!-- ============ Session / CLI-option defaults ============ -->
  <defaults>
    <fast-disk    enabled="true"/>                 <!-- bool: true|false -->
    <fmpac        autoload="true" slot="2"/>        <!-- autoload bool; slot 1|2 -->
    <border       enabled="false"/>                 <!-- bool -->
    <disk-writable enabled="false"/>                <!-- bool -->
    <speed        level="0"/>                        <!-- int 0..7 -->
    <!-- SDL3-presentation-only (never affects emulation/determinism/headless output) -->
    <video        scale="3" filter="linear"/>        <!-- scale int 1..8; filter nearest|linear -->
    <persistence  percent="0" mode="avg"/>           <!-- percent int 0..100; mode avg|peak -->
    <fullscreen   enabled="false"/>                  <!-- bool -->
    <capture      enabled="false"/>                  <!-- bool (F10 gate) -->
  </defaults>

  <!-- ============ Machine sizing + asset paths ============ -->
  <machine>
    <ram  kb="512"/>            <!-- 64|128|256|512  (unified main-RAM knob; --ram overrides) -->
    <vram kb="128"/>            <!-- strict spec: 128 ONLY (validated; see §4.4) -->
    <bios dir="bios">
      <rom role="bios"         file="f1xvbios.rom"/>  <!-- 32 KB BIOS+BASIC -->
      <rom role="sub"          file="f1xvext.rom"/>   <!-- 16 KB SUB (MSX-BASIC V3.0) -->
      <rom role="kanji-driver" file="f1xvkdr.rom"/>   <!-- 32 KB Kanji driver -->
      <rom role="disk"         file="f1xvdisk.rom"/>  <!-- 16 KB DISK ROM -->
      <rom role="fm-music"     file="f1xvmus.rom"/>   <!-- 16 KB MSX-MUSIC BIOS (APRLOPLL) -->
      <rom role="kanji-font"   file="f1xvkfn.rom"/>   <!-- 256 KB Kanji font (#D8-DB) -->
      <rom role="firmware"     file="f1xvfirm.rom"/>  <!-- 1 MB Halnote/MSX-JE -->
    </bios>
    <fmpac      rom="roms/fmpac.rom" sram="roms/fmpac.rom.sram"/>
    <softwaredb path="references/openmsx-21.0/share/softwaredb.xml"/>
    <!-- <slots> OPTIONAL/advanced: omit to use the built-in HB-F1XV map (§4.5). -->
  </machine>

</hbf1xv-config>
```

Type/range validation table (per-key, §4.2 per-key fallback on failure):

| Key | Type | Allowed | Built-in default |
| --- | --- | --- | --- |
| `defaults/fast-disk@enabled` | bool | true\|false | (convenience) true |
| `defaults/fmpac@autoload` | bool | true\|false | true |
| `defaults/fmpac@slot` | int | 1\|2 | 2 |
| `defaults/border@enabled` | bool | true\|false | false |
| `defaults/disk-writable@enabled` | bool | true\|false | false |
| `defaults/speed@level` | int | 0..7 (`Mb670836PauseController::kMaxSpeedLevel`) | 0 |
| `defaults/video@scale` | int | 1..8 | 3 (960×720) |
| `defaults/video@filter` | enum | nearest\|linear | linear |
| `defaults/persistence@percent` | int | 0..100 | 0 |
| `defaults/persistence@mode` | enum | avg\|peak | avg |
| `defaults/fullscreen@enabled` | bool | true\|false | false |
| `defaults/capture@enabled` | bool | true\|false | false |
| `machine/ram@kb` | enum | 64\|128\|256\|512 (reuse `parse_ram_kb`, `sdl3_cli.h:194`) | 512 conv / 64 stock |
| `machine/vram@kb` | int | 128 only (§4.4) | 128 |
| `machine/bios@dir` | path | any string | `bios` |
| `machine/bios/rom@file` (×7) | filename | any string; role ∈ {bios,sub,kanji-driver,disk,fm-music,kanji-font,firmware} | see §1.2 table |
| `machine/fmpac@rom` / `@sram` | path | any string | `roms/fmpac.rom` / auto-derive |
| `machine/softwaredb@path` | path | any string | `references/openmsx-21.0/share/softwaredb.xml` |

Notes: booleans accept `true/false` (case-insensitive), rejecting anything else per-key. The `speed`
range reuses `parse_speed_level()` (`sdl3_cli.h:205`); `ram@kb` reuses `parse_ram_kb()` — sharing the
already-tested validators (no duplicate range policy).

### 4.4 VRAM sizing — validated-to-128, honest ceiling

The owner listed VRAM KB as an externalize candidate. Reality (audited): VRAM is a `constexpr
kVramBytes = 128*1024` (`vdp_vram.h:47`) with NO runtime-sizing seam, and 128 KB is a strict,
non-negotiable line in the Target Machine Specification. Decision: `<machine><vram kb>` is accepted
and self-documenting in the schema, but **validated to 128 ONLY**; any other value warns
(`config: WARNING machine/vram@kb = 'N' is not 128 (strict HB-F1XV spec); using 128`) and is clamped
to 128. Live VRAM re-sizing (which would touch the V9958 device — a hardware-adjacent change) is
explicitly OUT of M50 (§1.4). This honors the owner's list without letting a bad edit break the VDP.

### 4.5 Slot layout — optional, validated-to-builtin

`<machine><slots>` is OPTIONAL/advanced. Omitted (the common case) → the built-in HB-F1XV map is
used verbatim. If present, it is strictly validated against the known-good structure; a layout that
does not match the built-in HB-F1XV map warns and falls back to the built-in map (whole-`<slots>`
fallback, since a partially-applied slot map is unsafe). M50 does NOT implement arbitrary live slot
REMAPPING — the built-in wiring in `hbf1xv_machine.cpp` remains the single source of the actual map;
`<slots>` in v1 is a validated, documented echo. Rationale: slot layout is the highest-blast-radius
externalization (a wrong map breaks every subsystem); the "never break accuracy" mandate makes
validate-and-echo the correct first step, with true remapping deferred as a follow-on if ever wanted.

### 4.6 Determinism rule (CRITICAL — keeps the 235-test suite byte-identical)

The deterministic headless + hidden-window paths MUST NOT be perturbed by a stray `config.xml`.
Exact rule:

1. `config.xml` is **auto-loaded ONLY by a genuinely interactive launch**: the SDL3 executable
   run WITHOUT `--hidden-window`.
2. The **headless executable NEVER auto-loads** `config.xml` — not the default scaffold path, not
   `--debug-session`, not any `*-parity` mode.
3. The **SDL3 executable under `--hidden-window`** (the deterministic test/CI harness mode) does
   **NOT** auto-load.
4. **`--config <path>`** is an explicit opt-in that FORCES load in ANY mode (headless or SDL3,
   hidden or not) — the ONLY way a test could ever load config, and no test passes it.
5. Consequence: every ctest test stays on the no-config path (headless tests use
   `--debug-session`/parity modes; SDL3 tests pass `--hidden-window`; unit tests construct
   `Hbf1xvMachine` directly). The full suite stays **byte-identical**.

Auto-load search order (interactive SDL3 only), first found wins:
`--config <path>` (explicit, any mode) → `<exe-dir>/sony_msx_hbf1xv.xml` → `<cwd>/sony_msx_hbf1xv.xml`.
None found → the §4.2 file-not-found WARNING + built-in defaults.

The shipped reference (§4.7) lives at repo root and is NOT auto-found from `build/Debug/`; the user
copies it next to the exe (or into the CWD) to activate it — matching "runs standalone by default."

### 4.7 Shipped reference config

Ship a TRACKED, fully-commented `sony_msx_hbf1xv.xml` at **repo root** (verified not gitignored, A-4)
listing EVERY knob with its default value and allowed range as XML comments — the canonical file the
owner asked for ("xml has all the knobs with default values"). A user copies + edits it. Its values
MUST equal the built-in defaults so that "ship + load with no edits" is behavior-identical to
"no config at all" (the round-trip gate, AC-S4-2). Because M46 gives RAM/fast-disk/FM-PAC a
convenience default (512/ON/ON) that differs from the stock struct default, the shipped file
documents the CONVENIENCE defaults (what an interactive launch actually resolves to today), and a
prominent comment block explains stock vs convenience + how `--stock` / per-knob edits peel it back.

---

## 5. Reused-parser plan (no new dependency)

Reuse the existing tolerant XML machinery in `src/machine/software_db.cpp` (owner directive). Plan:

- **Extract** the file-local `TagScanner` tokenizer + `trim()` + `decode_entities()` from
  `software_db.cpp`'s anonymous namespace into a small shared header/unit
  `src/machine/xml_tokenizer.{h,cpp}` (SDL-free, throw-free, attribute-aware). Refactor
  `software_db.cpp` to consume it — a BEHAVIOR-PRESERVING change guarded by the existing
  software-db tests (which must stay green, AC-S1-4).
- **Extend** the tokenizer minimally: config values are ATTRIBUTES (`enabled="true"`), whereas the
  softwaredb schema used element text. The tokenizer already tracks quoted attribute spans
  (`software_db.cpp:92-108`) but discards them; add attribute name→value extraction (the smallest
  addition that keeps the softwaredb text path untouched). If — and only if — attribute extraction
  proves to entangle the softwaredb path, the fallback is an element-text schema for config (no
  tokenizer change), justified in the implementation report. No third-party XML library is
  introduced either way (justification: the subset is tiny and the tolerant scanner already exists).
- New `src/machine/emulator_config.{h,cpp}`: the `EmulatorConfig` model (per-knob `std::optional`s
  + validation) + `load_from_file(path, warnings)` returning the model and a `std::vector<std::string>`
  of per-key WARNING lines (mirrors `SoftwareDb::load_from_file`'s `diagnostics` posture,
  `software_db.h:66`). Pure, headlessly unit-testable. Placement in `src/machine/` matches the
  boundary rules (machine composition/sizing/paths; consumed by BOTH executables like
  `software_db`/`cartridge_cli`).

---

## 6. Milestones / slice breakdown

Dependency-sequenced S1 → S2 → S3 → S4 (each slice ships with its own tests + green gates).

### S1 — Config model + strict parser/validator (reusing the existing XML reader)
- Extract/extend the tokenizer (§5); add `src/machine/emulator_config.{h,cpp}` with the schema (§4.3)
  model, per-key type/range validation, and the graceful per-key + whole-file fallback (§4.2) using
  the shared `parse_ram_kb`/`parse_speed_level` validators.
- No wiring into the run paths yet — S1 is the pure, unit-testable core.

### S2 — Wire config into CLI-default resolution (precedence CLI > XML > default)
- Insert the Layer-B pipeline (§4.1) in `sdl3_main.cpp` (post-parse, pre-app-construction) and the
  `--debug-session` resolution in `src/main.cpp`, for the session/convenience knobs (RAM/fast-disk/
  FM-PAC auto-load + slot) and the presentation knobs (scale/filter/persistence/persistence-mode/
  speed/border/fullscreen/capture/disk-writable).
- Implement the determinism rule + `--config <path>` + the auto-load search order (§4.6). NO change
  to the ctor/struct stock defaults (A-2).

### S3 — Machine-config sizing/paths
- Route `<machine>` values into the machine: `asset_root`/BIOS dir, the 7 BIOS filenames (replace the
  hardcoded `RomAssetLoader::Spec` literals in `load_rom_assets()` with config-fed specs keeping the
  built-in defaults + the strict expected-size per role), FM-PAC ROM/SRAM paths, softwaredb path, and
  main RAM KB (unify with the S2 `--ram` resolution). VRAM validated-to-128 (§4.4); `<slots>`
  validated-to-builtin (§4.5). Expected-size per BIOS role stays code-owned (it is a spec fact, not a
  user knob) — only the filename/dir externalize.

### S4 — Shipped reference config + docs
- Add the tracked, fully-commented `sony_msx_hbf1xv.xml` at repo root (§4.7); README/usage note for
  `--config` + the search order + the copy-next-to-exe workflow; the round-trip test proving shipped
  == built-in behavior.

---

## 7. Acceptance Criteria

### Milestone-level
- **AC-1 (byte-identical fallback):** with NO config file present, and on every headless/`--debug-
  session`/parity path and every `--hidden-window` SDL3 run, behavior is byte-identical to pre-M50
  and the full deterministic suite (235 per A-1, exact count pinned by the developer) is GREEN and
  UNCHANGED.
- **AC-2 (shipped == built-in):** loading the shipped `sony_msx_hbf1xv.xml` unedited produces behavior
  identical to no-config (round-trip identity, §4.7).
- **AC-3 (a changed value takes effect):** a config with a changed knob (e.g. `machine/ram@kb=128`,
  `defaults/fast-disk@enabled=false`) demonstrably changes the resolved effective value.
- **AC-4 (precedence CLI > XML > default):** with a config setting `ram=128`, an explicit `--ram 256`
  wins (256); with no CLI flag the XML wins (128); with neither, the built-in default wins.
- **AC-5 (graceful malformed handling — no crash):** an unknown key, a wrong-typed value, and an
  out-of-range value each produce a clear WARNING naming the offending key and fall back to that
  key's default (per-key, §4.2), the rest of the file still applying; a structurally-broken file
  warns once and falls back whole-file. No input crashes.
- **AC-6 (accuracy protection — empty timing diff):** `git diff` over the §3 exclusion set is EMPTY;
  no cpu/core/timing hardware constant is edited.
- **AC-7 (determinism rule):** headless never auto-loads; SDL3 `--hidden-window` never auto-loads;
  `--config <path>` forces load in any mode; verified no ctest test loads config.
- **AC-8 (reuse):** the config parser reuses the extracted `software_db` tokenizer; no new
  third-party dependency; the software-db refactor is behavior-preserving (its tests stay green).

### Per-slice
- **AC-S1-1** `EmulatorConfig::load_from_file` parses the full §4.3 schema into per-knob optionals.
- **AC-S1-2** every validation-table (§4.3) row is enforced; each invalid input yields the exact
  per-key WARNING + default fallback (non-tautological unit tests: assert the WARNING text names the
  key AND the effective value is the default).
- **AC-S1-3** a structurally-broken file yields whole-file fallback + one WARNING, never a throw.
- **AC-S1-4** the extracted tokenizer keeps `software_db` tests byte-identical (behavior-preserving
  refactor).
- **AC-S2-1** CLI > XML > default precedence holds for every session + presentation knob (§4.1),
  proven per knob.
- **AC-S2-2** the determinism rule (§4.6) is implemented + unit/integration-covered (headless no
  auto-load; hidden-window no auto-load; `--config` forces).
- **AC-S2-3** the ctor/`Sdl3AppConfig`/`ParsedSdl3Cli` stock defaults are UNCHANGED (A-2 grep).
- **AC-S3-1** BIOS dir + 7 filenames + FM-PAC ROM/SRAM + softwaredb path resolve from config with
  built-in defaults on omission; expected ROM sizes stay code-owned.
- **AC-S3-2** `machine/ram@kb` unifies with the S2 `--ram` resolution (one knob, one precedence
  chain); `vram@kb` validated-to-128; `<slots>` validated-to-builtin.
- **AC-S4-1** the tracked, fully-commented `sony_msx_hbf1xv.xml` exists at repo root, documents every
  knob + default + range, and is confirmed not gitignored.
- **AC-S4-2** the round-trip test proves shipped-config == no-config behavior (AC-2).

---

## 8. Evidence gates (run + capture; never fabricate)

- `tools/validate-assets.ps1` — required BIOS present + ≥1 ROM (BIOS-dir/filename externalization
  must not break asset resolution with the shipped defaults).
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` — reproducible checksums.
- `cmake --build build --config Debug` — both executables build.
- `ctest --test-dir build -C Debug --output-on-failure` — full deterministic suite GREEN, count ==
  pre-M50 baseline (A-1) + the new config unit/round-trip tests; the pre-M50 tests byte-identical.
- **New M50 tests:** parse/validate/precedence/fallback unit tests (S1/S2, non-tautological per
  AC-S1-2), the determinism-rule test (AC-S2-2), and the shipped-config round-trip test (AC-S4-2).
- **§3 empty-diff gate** (AC-6) — the accuracy-protection grep/diff.
- **openMSX A/B:** NOT required for M50. Rationale: M50 is pure configuration plumbing; it adds NO
  device/timing behavior, and every value the XML can set was already A/B-validated in its origin
  milestone (RAM → M42, FM-PAC/fast-disk → M46, video/persistence → M37/M39). The CONDITIONAL A/B
  gate is not triggered because the §3 exclusion set diff is empty. (Stated explicitly so QA does not
  read the absence as an oversight.)
- **ZEXALL/ZEXDOC:** NOT run — zero `src/devices/cpu` / `src/core` diff (`feedback_slow_test_cadence.md`).

---

## 9. Risks

- **R1 — Determinism perturbation (HIGH impact, LOW residual after §4.6).** A stray/shipped
  `config.xml` silently changing headless captures or the 235-test suite. Mitigation: the airtight
  §4.6 rule (headless + hidden-window never auto-load; only `--config` forces; shipped file lives at
  repo root, not the exe dir) + AC-1/AC-7 gates + the A-5 grep of the test target.
- **R2 — Precedence bugs (MED).** CLI/XML/default ordering wrong for a knob, or the XML leaking into
  the stock ctor/struct defaults (drift). Mitigation: the single Layer-B pipeline (§4.1), per-knob
  precedence tests (AC-S2-1), and the anti-drift grep (AC-S2-3).
- **R3 — Malformed-input robustness (MED).** A crash/partial-apply on adversarial XML. Mitigation:
  reuse the throw-free tolerant tokenizer, per-key + whole-file graceful fallback (§4.2), adversarial
  unit tests (AC-S1-2/3, AC-5).
- **R4 — Accuracy leak (HIGH impact, LOW residual).** A timing constant sneaking into the schema.
  Mitigation: the explicit §3 HARD-EXCLUDE + the empty-diff gate (AC-6); VRAM validated-to-128
  (§4.4); slots validated-to-builtin (§4.5).
- **R5 — software_db refactor regression (MED).** Extracting the tokenizer breaks cartridge
  auto-ID. Mitigation: behavior-preserving extraction guarded by the existing software-db tests
  (AC-S1-4); the fallback element-text schema if attribute extraction entangles the text path (§5).
- **R6 — BIOS-path externalization breaks asset loading (MED).** A config-fed filename/dir that
  misses assets. Mitigation: `RomAssetLoader`'s existing graceful 0xFF-fill + diagnostic
  (`rom_asset_loader.h:31-37`) already handles absent/wrong files deterministically; the
  `validate-assets` gate still fails a genuinely-missing required BIOS; expected-size stays
  code-owned so a filename swap cannot change a role's window size.
- **R7 — Shipped file accidentally gitignored / stale (LOW).** Mitigation: A-4 verification +
  AC-S4-1 + the round-trip gate (AC-S4-2) catching drift between shipped and built-in.

---

## 10. Dependency sequencing + developer handoff

- **Sequence:** S1 (pure model/parser — no run-path change) → S2 (CLI-resolution wiring +
  determinism rule; depends on S1) → S3 (machine sizing/paths; depends on S1 model, S2 layer) →
  S4 (shipped reference + docs + round-trip; depends on S1-S3 for the round-trip to be meaningful).
- **Start point:** S1. It is self-contained and fully unit-testable with zero run-path risk, and it
  de-risks R3/R5 (the parser + refactor) before any wiring.
- **Non-negotiables for the developer:** never touch the §3 exclusion set (AC-6); never alter the
  stock ctor/`Sdl3AppConfig`/`ParsedSdl3Cli` defaults (A-2/AC-S2-3); reuse the extracted tokenizer,
  no new dependency (AC-8); the determinism rule §4.6 is load-bearing — implement it before shipping
  any auto-load; capture the exact pre-M50 test count (A-1) as the regression baseline.
- **Blast radius:** MED. Expected edits confined to `src/machine/{emulator_config,xml_tokenizer,
  software_db}.*`, `src/machine/hbf1xv_machine.cpp` (`load_rom_assets` path resolution),
  `src/frontend/{sdl3_cli,sdl3_main}.*`, `src/main.cpp` (`--debug-session` resolution + `--config`),
  plus the new tests and the root `sony_msx_hbf1xv.xml`. ZERO `src/core`, ZERO `src/devices/cpu`, ZERO
  timing constants → NORMAL human-release gate (no auto-close), ZEXALL un-triggered.
```
