# debug/ — runtime debug output

This tree is populated **at runtime** by the emulator's debug facilities. Everything here
is regenerable and gitignored by pattern (`/debug/**/*.png`, `*.wav`, `*.txt`, `*.log`,
`*.trace`, `*.bin`, `*.dump`, `*.frame` — see `.gitignore`), EXCEPT the explicitly
whitelisted committed evidence files listed below.

## Layout (created on demand; debug root defaults to `debug/`, override with `--debug-root`)

- `traces/` — full-state dumps (`write_state_dump`, tag `HBF1XV-STATE-DUMP v1`: CPU + DRAM
  + SRAM + VRAM) and per-instruction CPU traces (`write_cpu_trace`). Produced by
  `--dump-state <name>` / `--trace-cpu <name>` on both executables.
- `logs/` — execution-event logs (`write_event_log`, deterministic and byte-identical
  across identical runs). Produced by `--event-log <name>`.
- `frames/` — decoded-FrameBuffer dumps (`HBF1XV-FRAME-DUMP v1`) and PNGs. Produced by
  `--dump-frame <name>` (headless `--debug-session`) / `--frame-dump-demo`; converted with
  `tools/frame-to-png.py`.
- `sounds/` — decoded PSG audio dumps (`HBF1XV-AUDIO-DUMP v1`) and WAVs. Produced by
  `--audio-dump-demo`; converted with `tools/audio-dump-to-wav.py`.

Raw memory regions extracted from a state dump can additionally be visualized with
`tools/mem-to-png.py` / auditioned with `tools/mem-to-audio.py` (inert raw-byte views).

## Committed evidence exceptions (do NOT delete or casually overwrite)

These files are referenced by frozen milestone/investigation docs and are whitelisted via
`.gitignore` exception blocks (each block names its owning document):

- M26/M27 capability examples: `frames/m26-example-boot.{png,frame}`,
  `sounds/m27-example-tone.{wav,dump}`.
- Live-playtesting-arc fix evidence (before/after/openMSX-reference sets):
  `frames/mg-sprite-invisibility-*` (DEC-0029), `frames/boot-logo-*` and
  `frames/border-*` (DEC-0031), `frames/konami-splash-*` (DEC-0032 arc).
- M29 Aleste-2 smoke: `frames/m29-aleste-f{240,500,899}.png`.
- M31 RC smoke matrix: `frames/m31-rc-*.png`, `sounds/m31-mg2-scc-title.wav`,
  `sounds/m31-fm-aleste-fm{ON,OFF}.wav`.
- DEC-0034 C5 closure: `frames/c5-verify-settled.png`.
- M32 defect-pair evidence: `frames/m32-aleste-play-*.png`, `frames/m32-split-ab-*.png`,
  `frames/m32-divergence-*.png`, `sounds/m32-fm-aleste-fm{ON,OFF}.wav`.

`frames/m31-rc-metalgear-f1100.png` has a permanent recorded regeneration recipe:
`tools/capture-metalgear-evidence.ps1` (two-run byte-identity self-asserting). Superseded
originals of the M32-regenerated artifacts remain retrievable at git tag `v1.0.32`.

Anything else found here is scratch output from a local run and can be deleted freely.
