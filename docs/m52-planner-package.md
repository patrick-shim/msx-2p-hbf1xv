# M52 Planner Package — SDL3 Master Volume Control + Hotkey Reshuffle + Disk-Writable Default ON

- Milestone: **M52** (DEC-0079, REQ-M52-001)
- Phase: PLANNING (planner-first)
- Spec owner: MSX Planner Agent
- Blast radius: **MED** (default-behavior change on the disk write-back path) → **NORMAL human-release gate, no auto-close**
- Reference precedence honored by construction (DEC-0073): no silicon behavior is touched; the DEC-0039 audio-balance calibration is explicitly out of bounds.
- Deferred backlog (DEC-0005 obligation): M52 **closes no backlog rows** and adds none (DEC-0079: "No new backlog rows"). All OPEN items — notably C1/D4 (exact VDP access-slot timing, UNBUILDABLE-WITHOUT-FABRICATION), E1/F1/F2/G-series — remain OPEN and untouched. M52 is frontend/config usability polish, not deferred device scope.

---

## 1. Scope and Assumptions

### 1.1 In scope (three owner-requested usability features)

1. **Master volume control** — a presentation-stage master gain applied strictly AFTER the DEC-0039 machine mix. Hotkeys Alt+D (down) / Alt+P (up), CLI `--volume <0..100>`, XML `<volume>` via the M50 seam, resolved CLI > XML > built-in default.
2. **Fast-disk hotkey reassignment** — Alt+D → **Alt+F** (frees Alt+D for volume-down); message/behavior otherwise unchanged.
3. **Disk-writable default ON (SDL3) + Alt+S runtime toggle** — flip the resolved SDL3 default to ON, add `--no-disk-writable`, add Alt+S with precisely-defined flush semantics; headless stays default OFF.

### 1.2 Non-goals (explicit)

- **No** change to the DEC-0039 PSG:FM:keyclick balance (`src/frontend/machine_audio_mixer.*`, ratios 21000:9000:16000 grounded in `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:63/:191`). The master gain is a **separate, post-mix** stage.
- **No** touch to `kAmplitudeScale = 400` (`src/frontend/sdl3_audio_presenter.h:82`) or any mixer amplitude constant (`kPsgAmplitudeScale`/`kSccAmplitudeScale`/`kFmAmplitudeScale`/`kClickAmplitudeScale`, `src/frontend/machine_audio_mixer.h:138-226`).
- **No** device/timing/silicon edits. ZERO `src/core`, ZERO `src/devices/cpu`, ZERO device-timing files. The Alt+S toggle gates **HOST-FILE write-back only**, never the in-emulator WD2793 FDC write path (M45/M45b/M47 write-integrity preserved).
- **No** headless determinism change: headless PSG/audio dumps and every ctest emulation fixture stay byte-identical; the volume knob is **SDL3-presentation-only**.
- **No** amplification (`--volume` max is 100 = unity; no gain > 1.0).

### 1.3 Assumptions (labeled; each with a verification action)

- **A1** — The live SDL3 audio path is `Sdl3App::run_one_frame()` → `Sdl3AudioPresenter::push_produced_paced()` (verified: `src/frontend/sdl3_app.cpp:711`, `src/frontend/sdl3_audio_presenter.cpp:149-186`). The batch path `pump_and_push_paced` and raw `pump_and_push` are "kept for plumbing tests" and not the live path (verified: `sdl3_audio_presenter.h:102-104`). *Verify at dev time:* grep confirms `push_produced_paced` is the only presenter push invoked from `run_one_frame`.
- **A2** — The headless audio dump path uses `MachineAudioMixer::produce_synced_sample` directly with no `Sdl3AudioPresenter` (the presenter is SDL3-frontend-only). *Verify:* `src/main.cpp` never constructs `Sdl3AudioPresenter`; the master gain therefore cannot reach a headless fixture. **This is the determinism guarantee for headless audio.**
- **A3** — `DiskImage::flush()` is a no-op returning `false` when `host_path_` is empty (verified: `src/devices/fdc/disk_image.h:106-109,115` — "empty => no host-file persistence"). Therefore `set_host_path(std::filesystem::path{})` UNBINDS an image without any new `DiskImage` API and without touching `src/devices/fdc`. **This is what lets Alt+S OFF stop future flushes with zero device-file edits.**
- **A4** — The shutdown-exit flush (`src/frontend/sdl3_app.cpp:419-420`) is guarded only by `initialized_ && !disk_images_.empty()`, relying on the host-path binding as the implicit gate (flush() no-ops when unbound). The swap flush (`sdl3_app.cpp:728-733`) is guarded by `config_.disk_writable`. Alt+S keeps `config_.disk_writable` AND the current image's host-path binding in sync so both gates agree. *Verify:* read both flush sites at dev time before editing.
- **A5** — Config-policy **unit tests** that assert the *old* default value of `disk_writable` (`emulator_config_unit_test.cpp:329`, `config_runtime_unit_test.cpp:107`) encode the very default M52 changes; updating their expectations is intended, NOT a regression. The "pre-M52 deterministic suite byte-identical" requirement applies to **emulation-output fixtures** (audio dumps, frame hashes, CPU traces, ZEXALL), not to config-policy assertions. *Verify:* QA confirms the only assertion flips are these enumerated policy cases (§4.5).
- **A6** — Alt+F, Alt+S, Alt+D (freed), Alt+P are collision-free (DEC-0079 audit + grep confirmed: only `SDL_SCANCODE_D` currently host-bound in `sdl3_app.cpp:519`; F6-F12/Pause/Alt+Enter/Alt+B/Alt+M/Alt+D are the existing host keys).

---

## 2. Spec Summary

### 2.1 Master volume — architecture (post-mix presentation scalar)

**Placement.** The master gain is a **pure post-mix scalar in the SDL3 audio presentation path** (`src/frontend/sdl3_audio_presenter.*`), applied to the already-mixed interleaved-stereo int16 PCM immediately before `SDL_PutAudioStreamData`. It is NOT in `MachineAudioMixer` (that is "the machine mix" whose ratios must stay byte-identical at full volume) and NOT a change to `kAmplitudeScale`.

**Gain law (PLANNER DECISION — linear integer percent).**
```
scaled = clamp_int16( (int32)sample * volume_percent / 100 )
```
- `volume_percent ∈ [0,100]`; **100 = unity** (full), **0 = silence**.
- At `volume_percent == 100`: `sample * 100 / 100 == sample` **exactly** for every int16 (no overflow: `|sample*100| ≤ 3,276,700 < INT32_MAX`), so the law is a mathematical identity at full volume.
- Because max volume is 100 (attenuation only), `|scaled| ≤ |sample| ≤ 32767` — the `clamp_int16` never actually triggers for `volume ≤ 100`; it is retained defensively/for documentation symmetry with the mixer.
- **Rationale for linear (not perceptual/log):** simple, deterministic, integer-exact, and directly unit-testable; matches the disclosed-simplification style of the existing audio path (`kAmplitudeScale` is itself a fixed linear scale, `sdl3_audio_presenter.h:78-82`). Disclosed simplification: perceived loudness is not linear in percent (a documented limitation, not a defect).

**Why not `SDL_SetAudioStreamGain`.** SDL3 offers `SDL_SetAudioStreamGain(SDL_AudioStream*, float)` (`references/sdl3/include/SDL3/SDL_audio.h:1234-1257`; "1.0f is no change, 0.0f is silence", applied inside `SDL_GetAudioStreamData`). It was **considered and rejected** for M52: it applies a float gain *inside SDL's pull*, off the deterministic, headlessly-testable path — we could not unit-test the attenuation without a live audio device, and the full-volume byte-identity oracle would move from "our produced PCM" to "SDL's internal output". Our own integer post-mix scalar keeps the whole gain law SDL-free and ctest-provable (the exact M29 `MachineAudioMixer` SDL-independence precedent), while still guaranteeing byte-identity at 100.

**Byte-identity guard (hard).** In `push_produced_paced` (and the plumbing `pump_and_push_paced`/`pump_and_push`), when `master_volume_ == 100` the presenter pushes the **original buffer bytes unchanged** (short-circuit — no copy, no scaling). Below 100 it fills a reusable member buffer with the scaled samples and pushes that. Silence-prime buffers (all zeros) are never scaled (0 is gain-invariant). Result: at the default volume 100, the SDL3 PCM stream is byte-for-byte what it is today.

**Gain helper (SDL-free, headlessly testable).** A new header-only `src/frontend/master_volume.h` (SDL-free, the `machine_audio_mixer.h` precedent) exposes the pure functions:
```cpp
namespace sony_msx::frontend {
  [[nodiscard]] std::int16_t apply_master_gain_sample(std::int16_t s, int volume_percent);   // s*vol/100, clamped
  void apply_master_gain(std::vector<std::int16_t>& pcm, int volume_percent);                 // in-place, no-op at 100
  [[nodiscard]] int step_master_volume(int current, int dir, int step);                       // grid-snap ±step, clamp [0,100]
}
```
`Sdl3AudioPresenter` holds `int master_volume_ = 100;` with `set_master_volume(int)` / `master_volume()`; it applies `apply_master_gain` via the helper. The helper is what the unit tests target (no SDL device needed).

**SDL3-only (headless untouched).** The gain lives entirely in `Sdl3AudioPresenter`, which the headless build never instantiates (A2). Headless `--audio-sync` dumps and every deterministic audio fixture stay byte-identical. **Answer to the task's explicit question: YES — the volume knob affects SDL3 audio only.**

### 2.2 Master volume — CLI / XML / precedence plumbing (M50 seam)

- **CLI** `--volume <0..100>` — new `std::optional<int> volume;` on `ParsedSdl3Cli` (`sdl3_cli.h`), parsed by mirroring the `--persistence` block verbatim (`sdl3_cli.cpp:228-247`): `std::from_chars` integer parse; non-numeric → `"sdl3_cli: --volume value is not a valid integer: '...'"`; out-of-range → `"sdl3_cli: --volume value out of range [0..100]: '...'"`; `has_value()` encodes "specified" (no shadow bool needed — same as `persistence`).
- **XML** `<volume percent="100"/>` under `<defaults>` — new `int master_volume = 100;` on `EmulatorConfig` (`emulator_config.h`), parsed by mirroring the `defaults/persistence@percent` case (`emulator_config.cpp:183-192`): `warn_unknown_attrs(..., {"percent"})`, `parse_int_strict` + range `[0,100]`, out-of-range → `warn_value(..., "defaults/volume@percent", *v, "expected 0..100", "100")`.
- **Resolution** CLI > XML > built-in default — in `resolve_runtime_config` (`config_runtime.cpp:100-103`), add: `r.master_volume = parsed.volume.has_value() ? *parsed.volume : cfg.master_volume;` (the exact `persistence` template at :103). New `int master_volume = 100;` on `ResolvedRuntimeConfig` (`config_runtime.h`).
- **Wiring** — `sdl3_main.cpp` (near :433): `config.master_volume = runtime.master_volume;`. New `int master_volume = 100;` on `Sdl3AppConfig` (`sdl3_app.h`) — **struct default stays at unity 100** (anti-drift; the flipped-default pattern only ever applies to disk-writable, see §2.4). `Sdl3App::init()` after `audio_presenter_->init()` (near `sdl3_app.cpp:281`): `audio_presenter_->set_master_volume(config_.master_volume);` and cache `master_volume_ = config_.master_volume;` for live hotkeys.

**Built-in default = 100 (PLANNER DECISION).** Recommended and justified: 100 (unity) is the only value that is **byte-identical to every pre-M52 session and fixture** — the owner asked for "cli with default volume value" and 100 is the obvious backward-compatible choice. Any lower default would silently attenuate all existing SDL3 audio and complicate the byte-identity oracle. The shipped XML documents `<volume percent="100"/>` and the round-trip anti-drift test proves shipped == compiled default (§2.5).

### 2.3 Hotkeys — volume steppers + fast-disk move

**Alt+D = volume down, Alt+P = volume up** (new), under the established host-hotkey discipline (`sdl3_app.cpp:496-552`):
- Consumed as HOST hotkeys via `event.key.mod & SDL_KMOD_ALT` on `SDL_SCANCODE_D` / `SDL_SCANCODE_P`; only the letter keydown is swallowed (`continue`), the standalone Alt keydown still reaches the MSX matrix exactly as Alt+Enter/Alt+B/Alt+M do.
- Each accepted step calls `master_volume_ = step_master_volume(master_volume_, ±1, kVolumeStep)`, then `audio_presenter_->set_master_volume(master_volume_)`, then one stderr feedback line, e.g. `sdl3: master volume 70% (Alt+D -10 / Alt+P +10)` / `... 0% (muted)` / `... 100% (full)`.

**Step size = 10 (PLANNER DECISION).** An 11-point grid `{0,10,…,100}` mirrors the established Alt+B phosphor-persistence step (`on_persistence_step_hotkey`, `sdl3_app.cpp:776-793`) for a consistent feel and reaches exact 0/100. `step_master_volume` grid-snaps a non-grid launch value (e.g. `--volume 55`) to the nearest grid point on first step, exactly like the persistence handler's `idx = (cur + 5) / 10`.

**CRITICAL difference from the persistence hotkey — CLAMP, do NOT wrap.** `on_persistence_step_hotkey` *wraps* 0↔100 (`(idx + dir + 11) % 11`). Volume MUST NOT wrap: turning volume down past 0 must NOT jump to 100 (a dangerous loudness surprise). `step_master_volume` **clamps** at both ends: `next = clamp(round_to_grid(current) + dir*step, 0, 100)`. This is an explicit, tested behavioral divergence from the persistence stepper.

**Key-repeat = HONORED for Alt+D/Alt+P (PLANNER DECISION).** The volume steppers **drop** the `!event.key.repeat` guard (unlike every existing host hotkey, which is fresh-keydown-only). Justification: (a) holding Alt+D/Alt+P to *ramp* volume is the ergonomic expectation for a volume control; (b) volume is a **pure presentation knob that never touches emulation, determinism, any recorded input, or any ctest fixture**, so honoring OS auto-repeat introduces zero determinism risk (contrast with a toggle, where repeat would thrash state); (c) the clamp makes an over-held key harmless (it saturates at 0 or 100). The per-step stderr line still prints on each accepted step (cheap; owner wants feedback). *Note for dev:* the repeated D/P keydowns are still swallowed as host hotkeys (they must not leak to the matrix); the matrix recorder path already skips `pressed && event.key.repeat` (`sdl3_app.cpp:565-568`), so recording stays clean.

**Fast-disk Alt+D → Alt+F.** Change `SDL_SCANCODE_D` → `SDL_SCANCODE_F` at the fast-disk branch (`sdl3_app.cpp:519-525`); keep `!event.key.repeat` (it is a **toggle** — repeat must not thrash). Behavior unchanged (`machine_.set_fast_disk(!machine_.fast_disk())`); update only the stderr mnemonic from `(Alt+D)` to `(Alt+F)`. This frees Alt+D for volume-down.

### 2.4 Disk-writable default ON (SDL3) + `--no-disk-writable` + Alt+S toggle

**Default flip (SDL3 only, via the M50 seam).**
- `EmulatorConfig::disk_writable` (`emulator_config.h:64`) flips `false → true` — this is the **XML base default** the resolver falls back to.
- Resolver unchanged in shape (`config_runtime.cpp:96`): `r.disk_writable = parsed.disk_writable_specified ? parsed.disk_writable : cfg.disk_writable;` — with `cfg.disk_writable` now `true`, an SDL3 launch with no explicit flag resolves ON.
- **`Sdl3AppConfig::disk_writable` struct default stays `false`** (`sdl3_app.h:66`) — the M46/M50 **anti-drift seam**: the flipped convenience default lives ONLY in the CLI-resolution layer, never in the struct/ctor default, so every direct-construction test stays byte-identical.
- Shipped root `sony_msx_hbf1xv.xml` `<disk-writable enabled="false"/>` → `enabled="true"` (keeps the round-trip anti-drift invariant shipped == compiled default).

**`--no-disk-writable` (SDL3 CLI).** New branch mirroring `--fast-disk`/`--no-fast-disk` (`sdl3_cli.cpp:138-143`) and `--border`/`--no-border` (`:129-134`): `--disk-writable` → `disk_writable=true; disk_writable_specified=true;` (already exists, `:135-137`); add `--no-disk-writable` → `disk_writable=false; disk_writable_specified=true;`. Last-wins on the linear scan. This is the explicit escape hatch now that the default is ON.

**Alt+S runtime toggle — flush semantics (PLANNER DEFINITION, precise).**
Alt+S is a HOST hotkey on `SDL_SCANCODE_S` + `SDL_KMOD_ALT`, fresh-keydown-only (`!event.key.repeat` — it is a toggle). It toggles a **runtime** `config_.disk_writable` and, in lockstep, binds/unbinds the **currently mounted** image's host path so the two flush gates (A4) always agree:

- **Toggle → ON:** `config_.disk_writable = true;` and, if a disk path exists for the current index, `machine_.disk_image().set_host_path(config_.disk_paths[current_disk_index_])`. Effect: subsequent swap/exit flushes write the current in-memory image (including any writes made while it was OFF) back to the host `.dsk`. stderr: `sdl3: disk-writable ENABLED (Alt+S) -- host .dsk saves persist on swap/exit`.
- **Toggle → OFF:** `config_.disk_writable = false;` and `machine_.disk_image().set_host_path(std::filesystem::path{})` (UNBIND — A3). Effect: future swap/exit flushes are no-ops for the current image. stderr: `sdl3: disk-writable disabled (Alt+S) -- writes stay in memory; current disk will NOT be saved`.

**Precise answer to "writes made while ON, then toggled OFF before swap/exit":**
- The writes **remain in the in-memory `DiskImage` `data_`** — the toggle **never rolls back** the image and **never touches the WD2793/FDC write path** (M45/M45b/M47 fully preserved; the toggle gates HOST write-back only).
- Because toggling OFF **unbinds the host path**, the exit flush (`sdl3_app.cpp:419-420`) and swap flush (`:728-733`) both become no-ops → **a dirty image toggled OFF is DISCARDED at exit (never written to host).** This is the intended, owner-friendly, non-surprising semantics: "OFF means this disk will not be saved," matching the stderr line.
- Symmetrically, if the user toggles **ON late** (after in-memory writes accumulated while OFF), the ON handler binds the host path, so the **whole** current in-memory image — including the earlier writes — is flushed at the next swap/exit. Documented and tested.

**No `src/devices/fdc` edit.** The unbind uses the existing `DiskImage::set_host_path` with an empty path (A3); no new device API, no timing file touched.

**Headless parity (PLANNER DECISION — headless stays default OFF).** `src/main.cpp` keeps `opts.disk_writable = false` (`main.cpp:750`); its own `--disk-writable` opt-in (`:934-935`) is unchanged. Headless is the deterministic test/CI driver: it never auto-loads config, and flipping its default ON would risk any test that passes a real `--disk` path starting to flush/mutate host fixtures — a direct violation of the CRITICAL determinism guard. `--no-disk-writable` is **NOT required for headless** (its default is already OFF, making the flag redundant); the developer MAY add it as a harmless explicit-off for CLI symmetry, but it is optional and non-blocking. This is exactly the M46/M50 discipline: convenience/default flips live only in the SDL3 CLI-resolution layer, never in the headless/ctor path.

### 2.5 Shipped XML + docs

- Root `sony_msx_hbf1xv.xml`: add `<volume percent="100"/>` in `<defaults>` (with a comment: `percent 0..100, default 100 (full); CLI: volume; SDL3 presentation only`); flip `<disk-writable enabled="true"/>` with an updated comment (`default true; CLI: disk-writable / no-disk-writable`).
- The shipped-config round-trip test (`hbf1xv_shipped_config_roundtrip_integration_test.cpp`) is EXTENDED: add `DIFF(master_volume)` to `report_drift`, add `"<volume"` (and optionally `"percent=\"100\""`) to `contains_all`. The invariant is preserved (shipped `<volume percent="100"/>` parses to 100 == compiled default; shipped `<disk-writable enabled="true"/>` == flipped default true).
- Banner/help: `sdl3_main.cpp` banner (`:312,:316`) — update the fast-disk line's `Alt+D` → `Alt+F`; add master-volume (Alt+D/Alt+P, `--volume`) and disk-writable (Alt+S, `--no-disk-writable`, default ON) lines. `main.cpp` usage (`:1983,:1996`) — no Alt-hotkey text there (headless), but ensure the `--disk-writable` help stays accurate for headless (default OFF).
- README: update the hotkey table (Alt+D now volume-down; Alt+F fast-disk; Alt+P volume-up; Alt+S disk-writable toggle) and the CLI list (`--volume`, `--no-disk-writable`, disk-writable default ON note).

### 2.6 Dependency map (`src/core|devices|peripherals|machine|frontend`)

| Layer | Files touched | Nature |
|---|---|---|
| `src/core` | **NONE** | hard-excluded |
| `src/devices` | **NONE** (Alt+S unbind reuses existing `DiskImage::set_host_path`) | hard-excluded from timing set; no device edit at all |
| `src/peripherals` | **NONE** | — |
| `src/machine` | `emulator_config.h` (+`master_volume` field, `disk_writable` default flip), `emulator_config.cpp` (`<volume>` parse) | config model + parser only |
| `src/frontend` | `master_volume.h` (NEW, SDL-free helper), `sdl3_audio_presenter.{h,cpp}` (master gain), `sdl3_cli.{h,cpp}` (`--volume`, `--no-disk-writable`), `config_runtime.{h,cpp}` (volume resolution), `sdl3_app.{h,cpp}` (hotkeys Alt+D/P/F/S, volume field, Alt+S bind/unbind), `sdl3_main.cpp` (wiring + banner) | presentation + CLI plumbing |
| root | `sony_msx_hbf1xv.xml` (`<volume>`, `<disk-writable>` flip) | shipped config |
| `src/main.cpp` | banner/help only; disk-writable default **unchanged** (stays OFF) | headless parity (no behavior change) |
| `tests/` | 2 new + ~6 extended/updated | see §5 |

Build order within the milestone: S1 (volume core+CLI+XML+resolution) → S2 (hotkeys) → S3 (disk-writable default+toggle+flush) → S4 (docs/config/round-trip). S1 and S3 both touch `emulator_config.*`/`config_runtime.*`/`sdl3_main.cpp` (different fields) — coordinate on a single edit pass per file to avoid churn.

---

## 3. Milestones (slice decomposition)

### S1 — Master volume core + CLI + XML + resolution
Add the SDL-free `master_volume.h` gain/step helper; add the `Sdl3AudioPresenter` master-gain stage (post-mix scalar, byte-identity short-circuit at 100); add `--volume <0..100>` (CLI), `<volume percent>` (XML/`EmulatorConfig` + parse), `ResolvedRuntimeConfig::master_volume` + `resolve_runtime_config` precedence, `Sdl3AppConfig::master_volume` + `sdl3_main.cpp` wiring + `init()` application. Built-in default 100.

### S2 — Hotkeys: volume steppers + fast-disk move
Add Alt+D (down) / Alt+P (up) volume hotkeys (honor key-repeat, grid-snap step 10, CLAMP not wrap, stderr feedback, host-hotkey discipline); move fast-disk Alt+D → Alt+F (keep fresh-keydown-only; message mnemonic updated).

### S3 — Disk-writable default ON + `--no-disk-writable` + Alt+S toggle + flush semantics
Flip `EmulatorConfig::disk_writable` to true (XML base default) + shipped XML; keep `Sdl3AppConfig` struct default false (anti-drift); add `--no-disk-writable`; add Alt+S runtime toggle with the §2.4 bind/unbind flush semantics; headless default stays OFF.

### S4 — Docs / config / round-trip
Update root `sony_msx_hbf1xv.xml` (`<volume>`, `<disk-writable>` flip) and the shipped-config round-trip test (new `<volume>` token + `DIFF(master_volume)`); update `sdl3_main.cpp` banner + README hotkey/CLI tables; keep `main.cpp` headless help accurate.

---

## 4. Acceptance Criteria

### 4.1 Per-slice acceptance

**S1 (volume core + plumbing)**
- S1-1 `apply_master_gain_sample(s, 100) == s` for all int16; `apply_master_gain(buf, 100)` leaves `buf` byte-identical; `apply_master_gain(buf, 0)` zeroes it; a mid value (e.g. 50) attenuates by `s*50/100` with no overflow.
- S1-2 `push_produced_paced` at `master_volume_ == 100` pushes the original buffer bytes unchanged (byte-identity short-circuit); at < 100 pushes the scaled buffer; silence-prime buffers are never scaled.
- S1-3 `--volume` parses valid `0/50/100`; rejects `-1`/`101` ("out of range [0..100]") and `abc` ("not a valid integer"); missing value → error (via `take_value`); `has_value()` marks specified.
- S1-4 `EmulatorConfig::parse` accepts `<volume percent="N"/>` for `0..100`, warns+keeps-default (100) on out-of-range/bad, default 100 when omitted; zero warnings on a valid document.
- S1-5 `resolve_runtime_config` yields master_volume with precedence CLI > XML > built-in default (5-case matrix mirroring persistence); no-config path resolves to 100 (byte-identical to pre-M52).
- S1-6 `sdl3_main.cpp` copies `runtime.master_volume` onto `config.master_volume`; `init()` calls `set_master_volume` once before playback.

**S2 (hotkeys)**
- S2-1 `step_master_volume`: `step(70,-1,10)=60`, `step(0,-1,10)=0` (clamp, no wrap to 100), `step(100,+1,10)=100`, `step(55,-1,10)=50` (grid-snap), `step(55,+1,10)=60`.
- S2-2 Alt+D decrements, Alt+P increments the presenter's master volume and print a stderr line; the standalone Alt keydown still reaches the matrix (only D/P swallowed); key-repeat produces additional steps for Alt+D/Alt+P (documented, honored).
- S2-3 Fast-disk toggle now fires on **Alt+F** (not Alt+D), fresh-keydown-only; `machine_.fast_disk()` flips; stderr says `(Alt+F)`; Alt+D no longer toggles fast-disk.

**S3 (disk-writable default + toggle)**
- S3-1 SDL3 resolved default disk-writable is ON with no flag (`resolve_runtime_config` fallback to flipped `cfg.disk_writable == true`); `--no-disk-writable` forces OFF; `--disk-writable` forces ON; last-wins.
- S3-2 `Sdl3AppConfig::disk_writable` struct default is still `false` (anti-drift, direct-construction unchanged).
- S3-3 Alt+S ON binds the current image host path; a subsequent write + exit flush persists to the host `.dsk`; the reloaded file is byte-identical to the written data.
- S3-4 Alt+S OFF unbinds (`set_host_path({})`); a subsequent flush is a no-op (`flush()==false`) and the host file is NOT written; the **in-memory image still contains the write** (no rollback); a dirty image toggled OFF is discarded at exit.
- S3-5 Writes accumulated while OFF are flushed if Alt+S is ON at exit (late-ON binds → full image flush).
- S3-6 Headless `src/main.cpp` default disk-writable stays OFF; no headless/CI path flushes a host file without an explicit `--disk-writable`.
- S3-7 M45/M45b/M47 WD2793 write-integrity tests pass unchanged (the toggle never touches the FDC write path).

**S4 (docs / round-trip)**
- S4-1 Shipped `sony_msx_hbf1xv.xml` documents `<volume percent="100"/>` and `<disk-writable enabled="true"/>`; the round-trip test proves parse(shipped) == compiled defaults with ZERO warnings, and `contains_all` includes the new `<volume` token.
- S4-2 `sdl3_main.cpp` banner shows Alt+F (fast-disk), Alt+D/Alt+P (volume), Alt+S (disk-writable), `--volume`, `--no-disk-writable`, disk-writable-default-ON; README hotkey/CLI tables updated; `main.cpp` headless help remains accurate (default OFF).

### 4.2 Milestone acceptance
- M-1 Master gain provably post-mix: full-volume (100) SDL3 output byte-identical to pre-M52; DEC-0039 ratios and every mixer amplitude constant untouched (empty diff over `machine_audio_mixer.*` amplitude constants; `kAmplitudeScale` unchanged).
- M-2 Hotkeys Alt+D (down) / Alt+P (up) / Alt+F (fast-disk) / Alt+S (disk-writable) live under the established host-hotkey discipline with stderr feedback; no collision (DEC-0079 audit + §A6).
- M-3 CLI > XML > built-in default precedence verified by deterministic tests for `--volume` and disk-writable.
- M-4 Disk-writable default ON (SDL3) with `--no-disk-writable` escape hatch and Alt+S toggle whose flush semantics match §2.4 exactly; headless default OFF; M45/M45b/M47 preserved.
- M-5 Pre-M52 deterministic **emulation-output** suite byte-identical (audio dumps, frame hashes, CPU traces, ZEXALL); the only test-expectation changes are the enumerated config-policy assertions (§4.5). New tests cover volume precedence, gain clamp/step, config parsing, CLI validation, full-volume byte-identity, disk-writable default + toggle + flush.
- M-6 Evidence gates (§6) all green, including the M50-style empty-diff accuracy gate over the HARD-EXCLUDE timing set.

---

## 5. Deterministic test obligations

New = 2 test executables; Extended/Updated = ~6 suites; ~28 individual cases.

**Unit (`tests/unit/…`)**
1. **`tests/unit/frontend/master_volume_unit_test.cpp` (NEW)** — `apply_master_gain_sample`/`apply_master_gain` identity@100 (byte-identity oracle), silence@0, mid-scale attenuation, no-overflow at extreme ±32767, in-place buffer no-op@100; `step_master_volume` step down/up, **clamp at 0 and 100 (no wrap)**, grid-snap of a non-grid value. Oracle: fixed input → fixed output, every run.
2. **`tests/unit/frontend/sdl3_cli_unit_test.cpp` (EXTEND)** — `--volume` valid (0/50/100), out-of-range, non-numeric, missing-value; `has_value()` specified semantics; `--no-disk-writable` sets false+specified; `--disk-writable`/`--no-disk-writable` last-wins.
3. **`tests/unit/machine/emulator_config_unit_test.cpp` (EXTEND + UPDATE)** — `<volume percent>` valid/out-of-range(warn+default 100)/missing(default 100); **UPDATE** the defaults assertion (`:329`) `disk_writable == false → true`; the existing FullValid `disk-writable enabled="true"` case (`:106`) stays true.
4. **`tests/unit/frontend/config_runtime_unit_test.cpp` (EXTEND + UPDATE)** — volume precedence 5-case matrix (no-config→100; XML-only wins; CLI overrides XML; CLI-only over default; XML-unoverridden stays XML); **UPDATE** `NoConfig_DiskWritableOff` (`:107`) → disk-writable now resolves ON with no config.

**Integration (`tests/integration/…`)**
5. **Full-volume byte-identity audio oracle (NEW, `tests/integration/frontend/master_volume_audio_identity_integration_test.cpp`)** — produce an interleaved-stereo PCM buffer from `MachineAudioMixer` (arbitrary PSG/FM/click input sequence), apply `apply_master_gain(buf, 100)`, assert byte-for-byte equal to the un-gained buffer; assert `apply_master_gain(buf, 0)` all-zero; assert a mid value attenuates deterministically. This is the hard "master gain is provably post-mix and identity at full volume" oracle without an audio device.
6. **Alt+S disk-writable flush-semantics (NEW, `tests/integration/frontend/disk_writable_toggle_flush_integration_test.cpp`)** — reproduce the Alt+S contract at the `Hbf1xvMachine` + `DiskImage` + temp-`.dsk` level (the `hbf1xv_m36_disk_save_persist_integration_test.cpp` pattern): (a) bind host path (ON) → write sector → `flush()` true → reload → byte-identical; (b) bind → write → `set_host_path({})` (OFF) → `flush()==false` and file unwritten, but `data_` still holds the write (no rollback); (c) write-while-OFF then bind (late-ON) → `flush()` persists the whole image. Deterministic oracle: fixed writes → fixed file bytes.
7. **`tests/integration/frontend/config_runtime_wiring_integration_test.cpp` (EXTEND)** — resolved master_volume and disk-writable reach the resolved runtime config with correct precedence in the wiring path.
8. **`tests/integration/machine/hbf1xv_shipped_config_roundtrip_integration_test.cpp` (EXTEND)** — add `DIFF(master_volume)`; add `"<volume"` token to `contains_all`; prove shipped(`<volume percent="100"/>`, `<disk-writable enabled="true"/>`) parses == compiled defaults, ZERO warnings.

**Hotkey dispatch honesty note.** The raw SDL-event → host-hotkey branch (which scancode+mod is swallowed) is inline in `poll_and_dispatch_events` and, like every existing host hotkey (Alt+D/Alt+B/Alt+M), has no direct ctest (it needs a live SDL event pump). M52 makes the **decidable logic** (volume grid/clamp math via `step_master_volume`; disk-writable bind/unbind via the flush test) pure and unit/integration-tested; the SDL event wiring is covered by the established discipline + QA manual smoke (§6). This is an explicit, honest coverage boundary, not a gap in the testable logic.

---

## 6. Evidence gates (run and capture; never fabricate)

- **Assets:** `tools/validate-assets.ps1` (BIOS present + ≥1 ROM).
- **Checksums:** `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`.
- **Build both exes:** `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=ON` then `cmake --build build --config Debug` (SDL3 superset → `sony_msx_headless.exe` + `sony_msx_sdl3.exe`).
- **Fast-subset ctest:** `ctest --test-dir build -C Debug --output-on-failure` — all pass; the pre-M52 **emulation-output** fixtures byte-identical; the only expectation changes are the enumerated config-policy assertions (§4.5 A5).
- **M50-style empty-diff accuracy gate (MANDATORY):** `git diff --exit-code -- src/core src/devices/cpu src/devices/video/vdp_access_timing.h src/devices/video/v9958_vdp.h src/devices/video/v9958_vdp.cpp src/devices/video/sprite_engine.h src/devices/video/sprite_engine.cpp src/devices/fdc/wd2793.cpp src/devices/chipset/s1985_engine.h src/devices/chipset/s1985_engine.cpp` — MUST be empty (no HARD-EXCLUDE timing file touched).
- **openMSX A/B + ZEXALL — NOT required** (frontend/config-only; same disposition as M50/DEC-0077). **Re-armed** if any `src/core` or `src/devices/cpu` file is edited (it must not be).
- **SDL3 API grounding:** any SDL audio claim cites the concrete header — `SDL_SetAudioStreamGain`/`SDL_GetAudioStreamGain` at `references/sdl3/include/SDL3/SDL_audio.h:1230-1257` (considered, rejected — §2.1); the push primitive `SDL_PutAudioStreamData` per `sdl3_audio_presenter.cpp:67/144/183`. No reference code is copied into `src/`.
- **QA manual smoke (SDL3, non-ctest):** owner/QA live check — Alt+D/Alt+P audibly step and saturate at 0/100; Alt+F still toggles fast-disk; Alt+S ON then a disk write persists on exit; Alt+S OFF then exit discards; `--volume 0` silent, `--volume 100` == default; `--no-disk-writable` prevents host mutation.
- **Operational precondition (M51 §8 incident):** a SINGLE automation session only during the developer/build phase.

---

## 7. Risks

| ID | Risk | Likelihood/Impact | Mitigation |
|---|---|---|---|
| R1 | Disk-writable default flip mutates game/fixture `.dsk` files unexpectedly | Med / High | SDL3-only flip (headless stays OFF, §2.4); struct-default anti-drift (false); `--no-disk-writable`; the empty-diff + fixture byte-identity gates; the flush-semantics integration test proves OFF discards. |
| R2 | Master gain accidentally re-balances the DEC-0039 mix / breaks byte-identity | Low / High | Post-mix scalar in the presenter (never in the mixer); default 100 = exact identity; 100 short-circuit pushes original bytes; the full-volume byte-identity oracle test (#5); empty diff over mixer amplitude constants. |
| R3 | Hotkey regression on the moved Alt+F / new Alt+D-P-S (letter leaks to matrix, or wrong branch) | Med / Med | Follow the exact `sdl3_app.cpp:496-552` discipline (mod-masked scancode, `continue` swallow, standalone Alt untouched); collision audit (§A6); `step_master_volume` unit-tested; QA manual smoke on all four. |
| R4 | Precedence / anti-drift bug in the M50 seam (volume or disk-writable resolves wrong) | Low / Med | Reuse the `--persistence` template verbatim (CLI parse, `has_value()` specified, resolver line); the 5-case precedence matrix test; the shipped round-trip anti-drift test. |
| R5 | Key-repeat on Alt+D/Alt+P floods stderr or surprises the user | Low / Low | Presentation-only, determinism-safe by construction; clamp saturates; documented decision; stderr line per step is cheap. If undesirable, the developer may gate the stderr print to fresh keydowns while still stepping on repeats (noted, non-blocking). |
| R6 | Alt+S OFF then swap loses expected writes silently | Low / Med | Precise semantics documented (§2.4) + stderr line "current disk will NOT be saved"; the flush test asserts discard-on-OFF and no in-memory rollback; the exit/swap gates (A4) verified at dev time before edit. |
| R7 | A config-policy test failing is misread as a determinism regression | Med / Low | §4.5/A5 enumerates the exact expected assertion flips (`emulator_config_unit_test.cpp:329`, `config_runtime_unit_test.cpp:107`) as intended; QA confirms no other fixture bytes move. |

### 4.5 Enumerated intended test-expectation changes (NOT regressions)
- `tests/unit/machine/emulator_config_unit_test.cpp:329` — `kDefaults.disk_writable == false` → `== true`.
- `tests/unit/frontend/config_runtime_unit_test.cpp:107` — `NoConfig_DiskWritableOff` → assert disk-writable resolves ON with no config.
- `tests/integration/machine/hbf1xv_shipped_config_roundtrip_integration_test.cpp` — add `<volume` token + `DIFF(master_volume)`; shipped `<disk-writable>` now `enabled="true"`.
These three encode the very defaults M52 changes; every other pre-M52 test remains byte-identical.

---

## 8. Developer Handoff

**Do NOT** touch any HARD-EXCLUDE timing file (`src/core`, `src/devices/cpu`, `src/devices/video/vdp_access_timing.h`, `v9958_vdp.*`, `sprite_engine.*`, `wd2793.cpp`, `s1985_engine.*`), any mixer amplitude constant, or `kAmplitudeScale`. Do NOT edit `src/devices/fdc` at all (the Alt+S unbind reuses `DiskImage::set_host_path` with an empty path — A3).

**Build order:** S1 → S2 → S3 → S4. Coordinate the shared-file edits (`emulator_config.*`, `config_runtime.*`, `sdl3_main.cpp` carry both a volume field (S1) and a disk-writable change (S3)) in one pass per file.

**Exact anchors (verified this cycle):**
- Master gain application: `src/frontend/sdl3_audio_presenter.cpp:149-186` (`push_produced_paced`, the live path) + `:97-147`/`:57-70` (plumbing paths); short-circuit at `master_volume_ == 100`. New SDL-free helper `src/frontend/master_volume.h`.
- Presenter field/accessor: `src/frontend/sdl3_audio_presenter.h` (add `int master_volume_ = 100;`, `set_master_volume`/`master_volume`); apply in `Sdl3App::init` near `sdl3_app.cpp:281`.
- CLI: `src/frontend/sdl3_cli.h` (`std::optional<int> volume;`), `sdl3_cli.cpp` mirror `--persistence` at `:228-247`; `--no-disk-writable` mirror `:129-137`.
- XML/model: `src/machine/emulator_config.h:64` (flip `disk_writable = true`) + new `int master_volume = 100;`; parse in `emulator_config.cpp` mirror the `persistence` case at `:183-192`, add under `<defaults>` dispatch near `:203`.
- Resolution: `src/frontend/config_runtime.h` (`ResolvedRuntimeConfig::master_volume`), `config_runtime.cpp:103` (add the volume line; disk-writable resolver at `:96` unchanged in shape).
- Wiring/banner: `src/frontend/sdl3_main.cpp:411` (disk-writable), near `:433` (`config.master_volume = runtime.master_volume`), banner `:312/:316`.
- Hotkeys: `src/frontend/sdl3_app.cpp:519-525` (Alt+D→Alt+F), add Alt+D/Alt+P (honor repeat) + Alt+S (fresh-keydown) branches in `poll_and_dispatch_events` (`:496-552` block); Alt+S bind/unbind on the current image; grid-snap math mirrors `on_persistence_step_hotkey` (`:776-793`) but **clamps, does not wrap**.
- Flush sites to respect: attach bind `:202-204`, swap flush `:728-733,:737-738`, exit flush `:419-420`, `flush_current_disk` `:808-815`.
- Shipped XML: `sony_msx_hbf1xv.xml:78` (disk-writable), add `<volume>` in `<defaults>` (near `:96`).
- Headless: `src/main.cpp:750` (default OFF — DO NOT flip), `:934-935` (`--disk-writable` unchanged), usage `:1983/:1996` (help text only).

**Definition of done:** all §4 acceptance criteria met; §6 evidence gates green (empty-diff accuracy gate MUST be empty; openMSX A/B + ZEXALL correctly not run); the three enumerated config-policy expectations updated (§4.5) and nothing else regressed; QA sign-off recommending Pass; **NORMAL human-release gate — no auto-close.**
