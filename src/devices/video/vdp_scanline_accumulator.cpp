#include "devices/video/vdp_scanline_accumulator.h"

#include <algorithm>
#include <span>

namespace sony_msx::devices::video {

VdpScanlineAccumulator::VdpScanlineAccumulator(const V9958Vdp& vdp, const VdpFrameRenderer& renderer)
    : vdp_(&vdp), renderer_(&renderer) {
    rows_.resize(static_cast<std::size_t>(kMaxLines));
    reset();
}

void VdpScanlineAccumulator::reset() {
    for (Row& row : rows_) {
        row.valid = false;
        row.width = 0;
        // pixels keeps its capacity -- reused staging rows (R-M32-3).
    }
    watermark_ = 0;
    completed_frame_ = FrameBuffer{};
    has_completed_frame_ = false;
    completed_frame_stale_ = false;
}

void VdpScanlineAccumulator::sync_to_line(const int exclusive_end_line) {
    const int height = renderer_->height();
    const int end = std::clamp(exclusive_end_line, 0, std::min(height, kMaxLines));

    if (end > 0 && end < watermark_) {
        // Wrap safety (§2.2): a POSITIVE raster position behind the
        // accumulation point -- the previous frame was never finalized
        // (step-only caller, DEC-0034 loop-shape class) or a mid-frame
        // geometry change moved the active window (D10 class). Seal
        // deterministically and start over. (end == 0 -- a border/vblank
        // position -- is always a plain no-op: border writes never reset a
        // partial frame.)
        finalize(Field::Progressive);
        // finalize() reset watermark_ to 0; fall through to accumulate
        // [0, end) of the new frame below.
    }
    if (end <= watermark_) {
        return;  // idempotent no-op (includes border/vblank writes, end == 0)
    }

    const int width = renderer_->width();
    for (int line = watermark_; line < end; ++line) {
        Row& row = rows_[static_cast<std::size_t>(line)];
        row.width = width;
        row.valid = true;
        if (static_cast<int>(row.pixels.size()) != width) {
            row.pixels.assign(static_cast<std::size_t>(width), 0);
        }
        // The M21 per-line workhorse against LIVE registers -- the openMSX
        // sync-before-change analogue at line granularity
        // (PixelRenderer.cc:549-571 Accuracy::LINE rounding).
        renderer_->render_line(line, Field::Progressive,
                               std::span<std::uint16_t>(row.pixels.data(), row.pixels.size()));
    }
    watermark_ = end;
}

void VdpScanlineAccumulator::emit_adapted_row(const Row& row, std::uint16_t* const out,
                                              const int out_width, const std::uint16_t border) const {
    if (row.width == out_width) {
        std::copy_n(row.pixels.data(), static_cast<std::size_t>(out_width), out);
        return;
    }
    // Deterministic width-adaptation policy (§2.4 geometry policy; named
    // remainder D10 covers per-line-perfect fidelity for geometry-mixed
    // frames -- no title in the current evidence set switches width
    // mid-frame).
    if (row.width * 2 == out_width) {
        for (int x = 0; x < row.width; ++x) {
            out[x * 2 + 0] = row.pixels[static_cast<std::size_t>(x)];
            out[x * 2 + 1] = row.pixels[static_cast<std::size_t>(x)];
        }
        return;
    }
    if (row.width == out_width * 2) {
        for (int x = 0; x < out_width; ++x) {
            out[x] = row.pixels[static_cast<std::size_t>(x) * 2];
        }
        return;
    }
    // Non-2x mismatch (e.g. the 240/480-wide TEXT families vs a bitmap
    // width): copy the overlapping prefix, pad the remainder with the
    // border color. Deterministic and documented; D10-class depth.
    const int n = std::min(row.width, out_width);
    std::copy_n(row.pixels.data(), static_cast<std::size_t>(n), out);
    for (int x = n; x < out_width; ++x) {
        out[x] = border;
    }
}

FrameBuffer VdpScanlineAccumulator::compose(const Field field) const {
    FrameBuffer fb;
    fb.width = renderer_->width();
    fb.height = renderer_->height();
    fb.border_color = renderer_->border_color();
    fb.pixels.assign(static_cast<std::size_t>(fb.width) * static_cast<std::size_t>(fb.height), 0);

    const int committed = std::min(watermark_, fb.height);
    for (int line = 0; line < committed; ++line) {
        const Row& row = rows_[static_cast<std::size_t>(line)];
        std::uint16_t* const out =
            fb.pixels.data() + static_cast<std::size_t>(line) * static_cast<std::size_t>(fb.width);
        if (row.valid) {
            emit_adapted_row(row, out, fb.width, fb.border_color);
        } else {
            // Defensive: a committed index without a valid row cannot occur
            // through the public API; render live for determinism anyway.
            renderer_->render_line(line, field,
                                   std::span<std::uint16_t>(out, static_cast<std::size_t>(fb.width)));
        }
    }
    for (int line = committed; line < fb.height; ++line) {
        std::uint16_t* const out =
            fb.pixels.data() + static_cast<std::size_t>(line) * static_cast<std::size_t>(fb.width);
        renderer_->render_line(line, field,
                               std::span<std::uint16_t>(out, static_cast<std::size_t>(fb.width)));
    }
    return fb;
}

void VdpScanlineAccumulator::finalize(const Field field) {
    // Seal on the frame's own end state: commit every remaining line from
    // live registers (also covers "height grew mid-frame" -- those lines
    // were never accumulated and render here), then materialize under the
    // end-of-frame geometry.
    const int height = std::min(renderer_->height(), kMaxLines);
    if (watermark_ < height) {
        // Direct commit loop rather than sync_to_line() -- finalize() must
        // never recurse through the wrap-safety path.
        const int width = renderer_->width();
        for (int line = watermark_; line < height; ++line) {
            Row& row = rows_[static_cast<std::size_t>(line)];
            row.width = width;
            row.valid = true;
            if (static_cast<int>(row.pixels.size()) != width) {
                row.pixels.assign(static_cast<std::size_t>(width), 0);
            }
            renderer_->render_line(line, field,
                                   std::span<std::uint16_t>(row.pixels.data(), row.pixels.size()));
        }
        watermark_ = height;
    }

    completed_frame_ = compose(field);
    has_completed_frame_ = true;
    completed_frame_stale_ = false;

    for (Row& row : rows_) {
        row.valid = false;
    }
    watermark_ = 0;
}

}  // namespace sony_msx::devices::video
