// ============================================================================
//  Sony HB-F1XV MSX2+ Emulator
//  Copyright (c) 2026 Patrick Shim <patrick.shim@live.co.kr>
//
//  LEGAL NOTICE - Personal reference only.
//  This source code is made available solely for personal, non-commercial
//  reference and educational study. Commercial use, sale, or redistribution
//  for profit is not permitted without the author's written consent.
//  Provided "AS IS", without warranty of any kind.
//  Proprietary BIOS/ROM/disk assets remain the property of their respective
//  rights holders and are NOT licensed by this notice.
// ============================================================================

#pragma once

#include <cstdint>
#include <vector>

#include "devices/video/frame_buffer.h"
#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_frame_renderer.h"

namespace sony_msx::devices::video {

// Scanline-accumulation frame store (M32-S1, Defect A of DEC-0039;
// docs/m32-planner-package.md §2.2).
//
// Real V9958 output is produced as the beam advances: a register/VRAM write
// between lines takes effect on the FOLLOWING line. openMSX models this with
// a sync-before-change protocol -- every VDP state-change notification
// renders up to the current beam position before the change applies
// (references/openmsx-21.0/src/video/PixelRenderer.cc:253-394 register/
// palette updates, :510-517 VRAM writes, :527-547 sync(), :219-228
// frameEnd()); its first-class Accuracy::LINE mode uses a whole line index
// as the beam limit (PixelRenderer.cc:549-571). fMSX independently
// corroborates the per-scanline architecture: its display pipeline runs one
// RefreshLine per scanline as the scan advances
// (references/fmsx-60/source/fMSX/MSX.c:209-224, 2149-2155). This class is
// the line-granular accumulation store both references imply: the machine
// commits display lines [0, watermark_) as the raster passes them (via the
// V9958Vdp render-sync seam); the rest of the frame renders from live
// registers at finalize.
//
// The existing per-line workhorse is reused as-is: every committed row comes
// from VdpFrameRenderer::render_line() against the LIVE V9958Vdp it
// references (M21). The legacy VdpFrameRenderer::render_frame() snapshot
// path is deliberately untouched -- it remains the in-test static-frame
// equivalence oracle (§2.2: "NOT modified and NOT removed").
//
// Determinism: every output is a pure function of the machine's cycle
// history and sync-point sequence (themselves cycle-derived). No wall
// clock, no host state, no allocation churn (row buffers are reused across
// frames -- R-M32-3 mitigation).
//
// Field note (documented design choice, §2.4 consumers audit): accumulation
// always renders Field::Progressive -- the only field any production caller
// uses (Sdl3App::run_one_frame(), write_frame_dump() defaults, main.cpp
// evidence mode). Interlace Even/Odd stay frame-snapshot semantics at the
// machine level (Hbf1xvMachine::render_frame() routes non-Progressive
// requests to the legacy snapshot renderer).
class VdpScanlineAccumulator {
public:
    VdpScanlineAccumulator(const V9958Vdp& vdp, const VdpFrameRenderer& renderer);

    // Deterministic power-on/cold-boot state: no lines accumulated, no
    // completed frame. Row buffers keep their capacity (reuse).
    void reset();

    // Commit display lines [watermark_, min(exclusive_end_line, height))
    // via VdpFrameRenderer::render_line() against LIVE registers, then
    // advance watermark_. Idempotent: a no-op for end <= watermark_ except
    // the wrap-safety rule below. Negative/border raster positions clamp to
    // 0 (a caller passing raster_line+1 for raster_line < 0 commits nothing
    // -- writes during vblank/border affect the whole next frame).
    //
    // Wrap safety (§2.2): if 0 < end < watermark_, the observed raster
    // position is behind the accumulation point within the same frame -- a
    // step-only caller that never calls on_vsync_boundary() let the
    // 262-line arithmetic wrap (the DEC-0034 loop-shape class), or a
    // mid-frame LN/mode change moved the active-window origin (the §2.4/D10
    // geometry-adaptation class). The accumulator then finalizes-to-bottom
    // and resets deterministically before committing [0, end). No crash, no
    // host-state dependence.
    void sync_to_line(int exclusive_end_line);

    // Seal the current frame: sync_to_line(height), materialize under the
    // end-of-frame geometry policy (§2.4: width/height/border from the live
    // end-of-frame register state, matching what the legacy snapshot
    // renderer reports; rows accumulated under a different width adapt
    // deterministically -- 256->512 pixel-double, 512->256 even-sample,
    // other mismatches copy-and-pad with the border color; rows above the
    // final height are dropped; rows never accumulated because height grew
    // render at finalize from live state), store it as the completed
    // frame, and reset the line store for the next frame. `field` selects
    // the field for lines rendered live at finalize (production passes
    // Field::Progressive).
    void finalize(Field field);

    // "Accumulated past + projected future" snapshot without sealing: rows
    // [0, watermark_) from the store (geometry-adapted), rows [watermark_,
    // height) rendered from live registers directly into the returned frame
    // (not committed -- a later hooked write may still re-render them). For
    // a frame with no accumulated rows this is byte-identical to the legacy
    // snapshot renderer by construction (every line renders from the same
    // live register state).
    [[nodiscard]] FrameBuffer compose(Field field) const;

    [[nodiscard]] int watermark() const { return watermark_; }
    [[nodiscard]] bool has_completed_frame() const { return has_completed_frame_; }
    [[nodiscard]] const FrameBuffer& completed_frame() const { return completed_frame_; }

    // Staleness guard for the frame-boundary fast path: any VDP state
    // mutation after finalize (hooked io_write -- including border-region
    // writes that commit nothing -- or a machine debug_io_write to the VDP
    // ports) marks the completed frame stale. The machine returns the
    // completed frame at a boundary only while fresh; once state moved on,
    // a boundary-position render falls back to live projection (the exact
    // legacy snapshot semantics for scenes rebuilt after a frame ran --
    // e.g. debug-built test scenes on a machine that previously called
    // run_frame()).
    void mark_completed_frame_stale() { completed_frame_stale_ = true; }
    [[nodiscard]] bool completed_frame_fresh() const {
        return has_completed_frame_ && !completed_frame_stale_;
    }

private:
    struct Row {
        int width = 0;
        bool valid = false;
        std::vector<std::uint16_t> pixels;  // capacity reused across frames
    };

    // Write one output row of `out_width` pixels from a committed row,
    // applying the deterministic width-adaptation policy (§2.4 / D10).
    void emit_adapted_row(const Row& row, std::uint16_t* out, int out_width,
                          std::uint16_t border) const;

    const V9958Vdp* vdp_;
    const VdpFrameRenderer* renderer_;

    // Maximum V9958 display height this machine renders (LN=1 -> 212).
    static constexpr int kMaxLines = 212;

    std::vector<Row> rows_;      // kMaxLines entries, buffers reused
    int watermark_ = 0;          // first display line NOT yet accumulated
    FrameBuffer completed_frame_{};
    bool has_completed_frame_ = false;
    bool completed_frame_stale_ = false;
};

}  // namespace sony_msx::devices::video
