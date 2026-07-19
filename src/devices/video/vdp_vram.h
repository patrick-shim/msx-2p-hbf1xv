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
#include <vector>

namespace sony_msx::devices::video {

// 128 KB V9958 Video RAM store, owned by the V9958 VDP device.
//
// Authoritative VRAM backing store for the HB-F1XV; migrated out of the
// machine layer's inert MemoryRegion into the VDP device where it belongs
// (V9958 fact sheet §2: HB-F1XV = 128 KB fixed, no expansion socket). The
// CPU never addresses VRAM directly -- only through the VDP I/O ports
// #98/#99 (+ the S1985 #9C/#9D mirror).
//
// Flat, linear 128 KB byte buffer with a 17-bit address space (0..0x1FFFF).
// The G6/G7 planar interleave (V9958 fact sheet §2)
// is applied by the CALLERS (V9958Vdp::effective_address(),
// vdp_command_address.h) as an address transform; this store itself stays
// flat/linear, matching openMSX's flat VRAM store (behavior reference only:
// openMSX 21.0: src/video/VDPVRAM.hh — never copied here, GPL
// isolation).
//
// All accessors are bounds-safe and deterministic: out-of-range reads yield
// 0x00, out-of-range writes are ignored. The public surface mirrors the
// machine's MemoryRegion (size/clear/read/write/load/dump/data) so the debug
// full-state dump and load/dump round-trip tooling carried over unchanged
// after the migration.
class VdpVram final {
public:
    // HB-F1XV VRAM size, fixed (V9958 fact sheet §2); replaces the machine's
    // retired kVramBytes constant.
    static constexpr std::size_t kVramBytes = 128 * 1024;

    VdpVram();

    [[nodiscard]] std::size_t size() const;

    // Deterministic zero-init: the reset/cold-boot default (matches the
    // retired machine vram_.clear()).
    void clear();

    // Bounds-checked single-byte access over the 17-bit address space.
    [[nodiscard]] std::uint8_t read(std::size_t address) const;
    void write(std::size_t address, std::uint8_t value);

    // Bulk load of `count` bytes at `offset`; bytes past the end are dropped.
    void load(std::size_t offset, const std::uint8_t* bytes, std::size_t count);

    // Full snapshot copy for dump / round-trip verification.
    [[nodiscard]] std::vector<std::uint8_t> dump() const;

    // Read-only contiguous view of the backing store (for the debug dump).
    [[nodiscard]] const std::uint8_t* data() const;

private:
    std::vector<std::uint8_t> bytes_;
};

}  // namespace sony_msx::devices::video
