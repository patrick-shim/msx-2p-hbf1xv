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

#include "core/bus.h"
#include "core/device_contracts.h"
#include "devices/chipset/mapper_io.h"
#include "machine/memory_region.h"

namespace sony_msx::devices::memory {

// DEC-0052 (M36 stream-light): non-perturbing CPU-memory-write observer. When
// installed (non-null) it is notified on every mem_write() with the
// CPU-VISIBLE address (the 16-bit address the CPU wrote, NOT the folded
// physical segment offset), so a diagnostic can watch specific addresses
// (e.g. the 0x0038 IM1/RST-38 JP-target bytes). Default-null => zero
// behaviour change; like the WD2793 FdcSectorReadObserver it is an
// externally-owned lifecycle pointer, isolated from emulation (an
// implementation MUST NOT mutate memory/mapper/CPU/clock state -- it only
// inspects the supplied address+value, e.g. to log them).
class MemWriteObserver {
public:
    virtual ~MemWriteObserver() = default;
    virtual void on_mem_write(core::BusAddress address, core::BusData value) = 0;
};

// Memory-mapper RAM device occupying slot 3-0, pages 0-3 (M13-S1; RAM size
// parameterized by M42/DEC-0061).
//
// This is the CPU RAM backing for the HB-F1XV (Sony_HB-F1XV.xml:125-130,
// MemoryMapper size 64). It replaces the M11 inert flat `RamSlotBacking`.
//
// RAM size (M42, DEC-0061): the STOCK spec configuration is 64 KB = 4 segments
// (the default and the only stock size). The opt-in NON-STOCK `--ram` sizes
// 128/256/512 KB fit 8/16/32 segments (a fully-populated S1985; 512 KB is the
// internal ceiling of the 5-bit mapper read-back). The device is size-agnostic:
// its populated segment count is DERIVED at construction from the backing region
// (num_segments() == ram.size() / kSegmentBytes), never hard-coded to 4.
//
// Segment ownership is SPLIT (single source of truth): the M11
// chipset::MapperIo remains the SOLE owner of the four #FC-#FF segment registers
// and of the S1985 `100xxxxx` (0x80 | seg & 0x1F) 5-bit readback. This device is
// a pure CONSUMER: for each CPU access it reads the live segment for that page
// from MapperIo and folds it onto the physical store. No segment state is
// duplicated here, so a mapper write is observed on the very next CPU access.
//
// Physical address (behaviour reference — read only, never copied; GPL):
// references/openmsx-21.0/src/memory/MSXMemoryMapperBase.cc:72-83 folds
// segment*0x4000 + (addr & 0x3FFF), wrapping the segment with
// `segment & (numSegments - 1)`. numSegments is a power of two for every offered
// size, so the wrap == `segment & (numSegments-1)`: `& 3` at 64 KB, `& 7/15/31`
// at 128/256/512 KB. RAM detection relies on this fold-based mirroring (a fitted
// size < 32 segments mirrors its high segments onto the low ones, exactly as real
// hardware), NOT on the read-back width: the S1985 read-back stays authentically
// 5-bit for ALL sizes (Sony_HB-F1XV.xml:25 "includes 5 bits mapper-read-back") --
// at 512 KB the fold mask (0x1F) coincides with that 5-bit read-back mask, the
// exact ceiling.
class MemoryMapperRam final : public core::MemoryDevice {
public:
    // Fixed S1985 mapper segment granularity: 16 KB per segment, for EVERY
    // fitted RAM size (the chip's segment size does not depend on populated RAM).
    static constexpr std::size_t kSegmentBytes = 0x4000;
    // Stock HB-F1XV main RAM = 64 KB = 4 segments (the strict spec default, and
    // the only stock configuration). Documentation anchor only -- the ACTUAL
    // segment count is derived per instance (num_segments()); the fold never
    // hard-assumes 4.
    static constexpr int kStockSegments = 4;

    MemoryMapperRam(machine::MemoryRegion& ram, const chipset::MapperIo& mapper_io);

    // Populated 16 KB segment count = ram.size() / kSegmentBytes. A power of two
    // for every offered size (64/128/256/512 KB -> 4/8/16/32 segments).
    [[nodiscard]] int num_segments() const { return num_segments_; }

    // Fold a (segment, address) pair onto a physical store of `num_segments`
    // 16 KB segments, matching openMSX calcAddress
    // (references/openmsx-21.0/src/memory/MSXMemoryMapperBase.cc:72-83):
    //   physical = (segment & (num_segments - 1)) * 0x4000 + (addr & 0x3FFF).
    // `num_segments` MUST be a power of two (true for all four offered sizes);
    // the caller passes the fitted count -- there is NO built-in assumption of 4.
    [[nodiscard]] static std::size_t physical_address(std::uint8_t segment, core::BusAddress address,
                                                      int num_segments);

    // MemoryDevice contract.
    core::BusData mem_read(core::BusAddress address) override;
    void mem_write(core::BusAddress address, core::BusData value) override;

    // DEC-0052 stream-light: install (non-null) / remove (nullptr) the
    // non-perturbing memory-write observer. Default null => zero behaviour
    // change (mirrors Wd2793::set_sector_read_observer exactly); it is an
    // externally-owned lifecycle pointer, managed by the installing machine.
    void set_write_observer(MemWriteObserver* observer) { write_observer_ = observer; }

private:
    machine::MemoryRegion& ram_;
    const chipset::MapperIo& mapper_io_;
    // M42/DEC-0061: fitted segment count (ram_.size() / kSegmentBytes), computed
    // once in the constructor. A power of two for every offered size.
    int num_segments_;
    // DEC-0052 stream-light: externally-owned; default null => no-op.
    MemWriteObserver* write_observer_ = nullptr;
};

}  // namespace sony_msx::devices::memory
