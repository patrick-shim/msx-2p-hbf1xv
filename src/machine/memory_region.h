#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sony_msx::machine {

// Inert, pure-storage byte region (M10-S2).
//
// A MemoryRegion is a fixed-size, deterministically zero-initialized byte
// buffer that is addressable, readable, writable, loadable, and dumpable. It
// carries NO device behavior whatsoever: no slot/subslot/mapper decoding, no
// V9958 VDP command/rendering semantics, no FM-PAC/SRAM mapper or battery
// persistence, and no I/O bus. Those subsystems are owned by SEPARATE
// milestones (planner package DP-1 slot/mapper, DP-2 V9958 VDP, DP-3
// FM-PAC/SRAM). This type only provides the minimum backing storage the
// machine layer needs so that DRAM/VRAM/SRAM can be wired, reset, dumped, and
// reloaded deterministically.
//
// All accessors are bounds-safe and deterministic: out-of-range reads yield
// 0x00 and out-of-range writes are ignored, so no access can perturb state
// outside the region or read uninitialized memory.
class MemoryRegion final {
public:
    explicit MemoryRegion(std::size_t size);

    [[nodiscard]] std::size_t size() const;

    // Deterministic zero-initialization (the reset / cold-boot default).
    void clear();

    // Bounds-checked single-byte access.
    [[nodiscard]] std::uint8_t read(std::size_t offset) const;
    void write(std::size_t offset, std::uint8_t value);

    // Bulk load of `count` bytes starting at `offset`. Bytes that would fall
    // past the end of the region are silently dropped (no overflow); a null
    // source or zero count is a no-op.
    void load(std::size_t offset, const std::uint8_t* bytes, std::size_t count);

    // Full snapshot copy of the region for dump / round-trip verification.
    [[nodiscard]] std::vector<std::uint8_t> dump() const;

    // Read-only contiguous view of the backing store.
    [[nodiscard]] const std::uint8_t* data() const;

private:
    std::vector<std::uint8_t> bytes_;
};

}  // namespace sony_msx::machine
