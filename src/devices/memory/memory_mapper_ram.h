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

// 64 KB memory-mapper RAM device occupying slot 3-0, pages 0-3 (M13-S1).
//
// This is the CPU RAM backing for the HB-F1XV (Sony_HB-F1XV.xml:125-130,
// MemoryMapper size 64). It replaces the M11 inert flat `RamSlotBacking`.
//
// Segment ownership is SPLIT (single source of truth): the M11
// chipset::MapperIo remains the SOLE owner of the four #FC-#FF segment registers
// and of the S1985 `100xxxxx` (0x80 | seg & 0x1F) 5-bit readback. This device is
// a pure CONSUMER: for each CPU access it reads the live segment for that page
// from MapperIo and folds it onto the physical 64 KB store. No segment state is
// duplicated here, so a mapper write is observed on the very next CPU access.
//
// Physical address (behaviour reference — read only, never copied; GPL):
// references/openmsx-21.0/src/memory/MSXMemoryMapperBase.cc:72-83 folds
// segment*0x4000 + (addr & 0x3FFF), wrapping the segment with
// `segment & (numSegments - 1)`. For a 64 KB mapper numSegments == 4 (a power
// of two), so that wrap == `segment & 3` for every segment value. The readback
// (5-bit, chipset) and the physical fold (2-bit, here) authentically use
// different masks (Sony_HB-F1XV.xml:25 "includes 5 bits mapper-read-back").
class MemoryMapperRam final : public core::MemoryDevice {
public:
    // 64 KB / 16 KB = 4 populated segments.
    static constexpr int kSegments = 4;
    static constexpr std::size_t kSegmentBytes = 0x4000;

    MemoryMapperRam(machine::MemoryRegion& ram, const chipset::MapperIo& mapper_io);

    // Fold a (segment, address) pair onto the physical 64 KB store, matching
    // openMSX calcAddress for numSegments == 4.
    [[nodiscard]] static std::size_t physical_address(std::uint8_t segment, core::BusAddress address);

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
    // DEC-0052 stream-light: externally-owned; default null => no-op.
    MemWriteObserver* write_observer_ = nullptr;
};

}  // namespace sony_msx::devices::memory
