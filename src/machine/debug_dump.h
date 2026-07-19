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

#include <cstddef>
#include <cstdint>
#include <string>

#include "devices/cpu/z80a_state.h"

namespace sony_msx::machine::debug_dump {

// Deterministic full-state debug-dump serializers.
//
// Build a byte-stable, diff-friendly ASCII text representation of the
// machine's architectural state for offline inspection and regression
// locking. Determinism is guaranteed by construction: fixed field order,
// fixed-width uppercase hex, '\n' line endings, no locale/stream state, and
// no wall-clock or other environment-dependent input. Two identical runs
// produce byte-for-byte identical output.
//
// The region serializer emits a canonical folded hex dump: 16 bytes per line
// prefixed with an 8-hex-digit offset. Runs of identical interior 16-byte
// lines fold to a single '*' marker (the first and last line of every region
// are always emitted verbatim), so large zero-initialized regions stay
// compact while remaining unambiguous and reversible.

// Dump-format version tag emitted as the first line of a full-state dump.
inline constexpr const char* kDumpFormatTag = "HBF1XV-STATE-DUMP v1";

// Serialize the complete CPU architectural state (registers, shadow set, index
// registers, interrupt state, IM, halt flag, cumulative T-states) as a fixed
// multi-line block framed by "[CPU]".
std::string serialize_cpu(const devices::cpu::Z80aState& state);

// Serialize a byte region as a named, folded canonical hex dump framed by
// "[<name>] size=<n>". `data` may be null only when `size` is 0.
std::string serialize_region(const std::string& name, const std::uint8_t* data, std::size_t size);

}  // namespace sony_msx::machine::debug_dump
