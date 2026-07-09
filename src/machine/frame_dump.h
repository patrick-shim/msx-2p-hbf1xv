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
#include <string>

#include "devices/video/frame_buffer.h"

namespace sony_msx::machine::frame_dump {

// Dump-format version tag emitted as the first line (mirrors debug_dump.h's
// kDumpFormatTag precedent).
inline constexpr const char* kFrameDumpFormatTag = "HBF1XV-FRAME-DUMP v1";

// Deterministic decoded-FrameBuffer dump serializer (M26-S4, the ONE new
// debug/testing capability this milestone authorizes -- docs/m26-planner-
// package.md §2.5). Dumps the DECODED FrameBuffer (M21 VdpFrameRenderer
// output, RGB555 pixels) -- NOT raw VRAM bytes (tools/mem-to-png.py, M10-S5,
// is explicitly insufficient for this: it visualizes raw memory as grayscale
// noise with zero VDP-mode/palette awareness).
//
// Mirrors debug_dump.h's exact ASCII discipline (fixed field order,
// fixed-width uppercase hex, '\n' line endings, zero wall-clock/environment
// content) -- two identical FrameBuffer values produce byte-for-byte
// identical dump text.
//
// Format:
//   HBF1XV-FRAME-DUMP v1
//   [FRAME] width=<w> height=<h> border=<HHHH>
//   [PIXELS] size=<n>   (folded canonical hex, REUSING the existing, already-
//                         proven debug_dump::serialize_region() routine --
//                         genuine reuse, not a parallel reimplementation,
//                         planner §2.5 point 1)
//   [END]
//
// The pixel buffer is reinterpreted as raw bytes in HOST-NATIVE byte order.
// This project targets little-endian x86/x64 development hosts only -- the
// same assumption SDL3's own SDL_PIXELFORMAT_XRGB1555 zero-conversion claim
// (A-M26-3) already relies on (both are plain uint16_t values in memory).
[[nodiscard]] std::string serialize_frame_dump(const devices::video::FrameBuffer& frame);

// Reverses serialize_frame_dump(): parses width/height/border + the folded
// pixel hex dump back into a FrameBuffer. Used by the round-trip unit test
// (M26-S4 Acceptance Criterion 5) and available to any future tool/test that
// needs to re-load a captured frame dump. Throws std::runtime_error on a
// malformed dump (never silently returns a partial/garbage FrameBuffer).
[[nodiscard]] devices::video::FrameBuffer parse_frame_dump(const std::string& text);

}  // namespace sony_msx::machine::frame_dump
