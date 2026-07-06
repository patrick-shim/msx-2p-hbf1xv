#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sony_msx::devices::video {

// 128 KB V9958 Video RAM store, owned by the V9958 VDP device (M14-S1).
//
// This is the authoritative VRAM backing store for the HB-F1XV. It migrated out
// of the machine layer's inert MemoryRegion into the VDP device where it belongs
// (fact-sheet §2 line 38: HB-F1XV = 128 KB fixed, no expansion socket). The CPU
// never addresses VRAM directly; it reaches it only through the VDP I/O ports
// #98/#99 (+ the S1985 #9C/#9D mirror).
//
// The store itself is a flat, linear 128 KB byte buffer with a 17-bit address
// space (0..0x1FFFF). The G6/G7 planar interleave (fact-sheet §2 line 42) is a
// DISPLAY/COMMAND-path transform and is DEFERRED (backlog D7); this store stays
// flat and the CPU-port addressing is linear, matching openMSX's flat VRAM store
// (behavior reference only: references/openmsx-21.0/src/video/VDPVRAM.hh —
// never copied here, GPL isolation).
//
// All accessors are bounds-safe and deterministic: out-of-range reads yield 0x00
// and out-of-range writes are ignored. The public surface intentionally mirrors
// the machine's MemoryRegion (size/clear/read/write/load/dump/data) so the
// debug full-state dump and load/dump round-trip tooling carry over unchanged
// after the migration.
class VdpVram final {
public:
    // HB-F1XV VRAM size, fixed (fact-sheet §2). Authoritative post-migration:
    // the machine's retired kVramBytes constant moves here.
    static constexpr std::size_t kVramBytes = 128 * 1024;

    VdpVram();

    [[nodiscard]] std::size_t size() const;

    // Deterministic zero-initialization (the reset / cold-boot default; matches
    // the retired machine vram_.clear()).
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
