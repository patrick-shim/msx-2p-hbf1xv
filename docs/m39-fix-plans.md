# M39 fix plans — voice (1-bit DAC / PSG-PCM) + vertical display position (R#18 / aspect)

Grounded specs from the read-only planning pass (openMSX-21.0 + our code). Both fixes are
UNIVERSAL / register-level, verified vs openMSX, license-isolated (re-derive, never copy).
Which branch to implement is decided by the Aleste-2 confirmation trace.

## Issue 1 — vertical display crop/mis-position

**Confirmed:** R#18 (display adjust) is read NOWHERE (`border_composer.h:56` even says "R#18 ...
not modeled"). Two presentation paths, and the path decides the cause:
- **Default (border_enabled=false, the shipped default; the human's launch cmd has no `--border`):**
  `blit_frame` uploads the bare active area (256xH) and `SDL_RenderTexture(NULL dst)` stretches it
  to fill the 320x240 logical area (`SDL_SetRenderLogicalPresentation(320,240,LETTERBOX)`,
  sdl3_app.cpp:187). 192-line -> uniform 1.25x (correct 4:3). **212-line -> 1.25x H but only
  1.132x V = vertical squish (NOT a literal crop).** On THIS path R#18 cannot manifest.
- **`--border`:** `compose_border_canvas` centers the active area at fixed y0 {24 for 192, 14 for
  212}, x0 {32/64/41/82}. On this path, R#18 unimplemented = the cause (openMSX shifts within the
  border per R#18; we pin at neutral).

**R#18 decode (verified vs openMSX VDP.cc:417-425 / VDP.hh:587-605,902-904, applied VDP.cc:481):**
- hshift = ((r18 & 0x0F) ^ 0x07) - 7   (+ = display moves RIGHT; range -7..+8; ×2 for 512-wide)
  (if r25&0x08 YJK, openMSX adds +4 before the -7)
- vshift = ((r18 >> 4) ^ 0x07) - 7     (+ = display moves DOWN)
- Both 0 when R#18=0 (neutral) => fix is a strict superset; R#18-neutral software byte-identical.
- openMSX neutral geometry: active-top-on-canvas = lineZero-18 = {24 @192, 14 @212} == our y0. Good.

**FIX (pick per the trace + which path the human uses):**
- If cause = R#18 (--border path or R#18≠0): in `border_composer.cpp border_geometry()`, after the
  neutral x0/y0, add `x0 += hshift*(width>=512?2:1)`, `y0 += vshift`; pass R#18 in (keep the fn
  pure/deterministic/headless-testable); add SYMMETRIC top/left clamp so negative offsets can't
  underflow `compose_border_canvas`'s dst pointer. Orthogonal to the M38 scroll code + 192/212
  height. Keep neutral output byte-identical (border oracles).
- If cause = aspect squish (default path + 212-line): make the default present aspect-correct —
  draw the active-area texture into a fixed 4:3 sub-rect preserving 1.25x on BOTH axes (so 212
  content isn't V-squished), OR route the default through the 320x240 border canvas. Real 2nd
  defect regardless of R#18. Validate vs the raster-true geometry in border_composer.h:30-58
  (openMSX active x∈[32,287], y∈[24,215]).

**Confirm:** trace R#18 via the existing `attach_register_write_observer` seam (v9958_vdp.cpp:97,
fired :336). If hshift/vshift both 0 across the scene AND the human uses default path -> it's the
aspect fix, not R#18. Pixel A/B via tools/frame-to-png.py + tools/m38-ab-diff.py.

## Issue 2 — digitized voice silent

**Root (deeper than a missing source):** audio is produced ONCE PER FRAME in a batch
(`run_one_frame` -> single `pump_and_push_paced`, sdl3_app.cpp:366-438); the mixer advances every
generator by cycles_per_sample INSIDE that batch using END-OF-FRAME register state. `advance_cycles`
has NO call site on register writes (grep-confirmed: only the pump + mixer). => ALL sub-frame audio
register/bit changes COLLAPSE. Music (≤60Hz) survives; sub-frame PCM does not. So BOTH:
- **Candidate A (1-bit DAC, PPI port-C bit7 keyclick):** unrouted to the mixer AND no cycle-stamped
  edge history. `Ppi8255::key_click()` = pure latch read (ppi_8255.cpp:117-119), written io_write
  case2 (:86-88) + write_control (:47-61) with no timestamp/sink.
- **Candidate B (PSG software-PCM, rapid R8/R9/R10 volume writes ~8kHz):** collapsed by the batched
  advance regardless of channel_audible() being correct. (Compile/Aleste2 heritage = plausible.)
Both share one need: intra-frame cycle-timestamped capture + sub-sample integration.

**openMSX grounding:** MSXPPI.cc:117-131 writeC1 edge-triggered (bit3 -> KeyClick.setClick;
bit1 -> cassetteOut bit5). KeyClick.setClick: dac.writeDAC(status?0xFF:0x80). DACSound8U
writeDAC(value-0x80) (idle 0x80 -> 0 silent); DACSound16S: blip.addDelta (SUB-SAMPLE + AC-COUPLED,
constant level -> 0). Machine volumes (Sony_HB-F1XV.xml): PPI/keyclick 16000, PSG 21000, OPLL 9000.

**FIX A (if trace = bit7 DAC; lower-risk, isolated, recommended default):**
1. Cycle-stamped edge capture: give Ppi8255 an optional click sink + cycle source (inject like the
   VDP render-sync seam, hbf1xv_machine.cpp:48-68,241). On any port-C write toggling bit7 (io_write
   case2 AND write_control), record edge (elapsed_cycle, new_level) — openMSX `(prevBits^value)&8`.
2. New additive band-limited mixer source `ClickDac` with advance_cycles(w)+take_integrated_sample(w)
   box-averaging (or delta/AC-coupled, matching openMSX) the edge timeline over each ~81-cyc window
   -> frac∈[0,1]; extend mixing law via a new overload (M31/M37 additive precedent):
   pcm += clamp(... + click_frac * kClickAmplitudeScale).
3. Amplitude (same method as kFmAmplitudeScale=21... use PSG_full=31*400=12400, V_psg=21000):
   kClickAmplitudeScale ≈ round(12400 * (16000/21000) * (127/128)) ≈ 9370 per channel, mono (both rails).
4. Sub-sample integration REQUIRED (bit toggles every ~300-450 cyc vs 81-cyc window); AC-coupled
   delta variant mirrors openMSX and avoids any DC offset.
5. Idle byte-identity: reset latch_c_&0x80==0 -> no edges -> integrated 0 -> exactly 0 -> existing
   audio oracles byte-identical (M29/M31/M34/M37 null-source pattern). Unit-test the null path.

**FIX B (if trace = PSG PCM):** sync-before-change on PSG writes — advance the PSG box-average
integrals up to the write cycle BEFORE each write_register (new psg.sync_to_cycle(now) called from
the PSG I/O write path, io_bus #A0-#A2, hbf1xv_machine.cpp:149-151). Reuses M34 integrals; larger
change; also improves music timing. The audio analog of the VDP render-sync seam.

**Decide via trace:** instrument PPI port-C bit7 toggles vs PSG R8/R9/R10 writes at the Aleste-2
~1:15 voice scene + Laydock speech. bit7 edges ≫60Hz & PSG quiet -> A. PSG hammered ~8kHz & bit7
static -> B. (May need BOTH long-term; do the confirmed one first.)

**A/B:** dump our WAV (psg_audio_dump.* + tools/audio-dump-to-wav.py) vs the human's openMSX voice
recording at ~1:15; tools/m34-audio-ab-analyze.py (voiced envelope appears where flat before).
Re-run mixer oracles for idle byte-identity; re-verify the int16 clamp worst-case incl. the click term.

**Determinism:** edge capture keyed off elapsed_cycles() (never wall-clock); advance the click source
in lockstep even when backpressure trims output (DEC-0033, sdl3_audio_presenter.cpp:130-134).
ZERO src/devices/cpu / src/core edits expected (ppi/frontend/machine/audio only) => ZEXALL withheld.
