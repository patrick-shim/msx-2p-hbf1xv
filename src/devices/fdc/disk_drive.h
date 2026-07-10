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

#include "devices/fdc/disk_image.h"

namespace sony_msx::devices::fdc {

// Built-in 3.5" 720 KB drive mechanism abstraction (M16-S4). Models head
// position (track), side select, motor on/off with the ~4 s delayed motor-off
// timer, ready / write-protect / track-00 / index-pulse / disk-changed sense,
// and sector access over the mounted DiskImage. Behaviour reference (read only,
// never copied, GPL isolation): references/openmsx-21.0/src/fdc/DiskDrive.cc,
// RealDrive.cc (delayed motor-off RealDrive.cc:263-321), DriveMultiplexer.cc.
//
// Single physical drive (<drives>1</drives>, Sony_HB-F1XV.xml). The Sony glue
// drive-select (0x7FFD bits1..0) chooses drive A (00/10), B (01) or NONE (11);
// selecting B/NONE presents as not-ready (available_ = false) because the second
// drive is absent (PhilipsFDC.cc:158-169; fact-sheet §7). All time inputs are
// absolute emulated cycle counts from the FdcClockSource — no wall clock.
class DiskDrive {
public:
    // 3,579,545 Hz system clock; ~4 s delayed motor-off = 4 * clock cycles.
    static constexpr std::uint64_t kSystemClockHz = 3579545;
    static constexpr std::uint64_t kMotorOffCycles = 4 * kSystemClockHz;  // 14,318,180
    // 300 rpm -> 200 ms index period; a ~5% index-pulse width window.
    static constexpr std::uint64_t kIndexPeriodCycles = kSystemClockHz / 5;  // 715,909
    static constexpr std::uint64_t kIndexPulseWidthCycles = kIndexPeriodCycles / 64;

    void reset();

    void attach_image(DiskImage* image) { image_ = image; }
    [[nodiscard]] DiskImage* image() const { return image_; }

    // Drive select: `available` true when a present physical drive is selected.
    void set_available(bool available) { available_ = available; }
    [[nodiscard]] bool available() const { return available_; }

    // Side latch (Sony 0x7FFC bit0).
    void set_side(std::uint8_t side) { side_ = side & 1u; }
    [[nodiscard]] std::uint8_t side() const { return side_; }

    // Motor control with delayed motor-off (now = absolute emulated cycles).
    void set_motor(bool on, std::uint64_t now);
    [[nodiscard]] bool motor_on(std::uint64_t now) const;

    // Head positioning (Type I mechanics). step_in increments the physical track
    // (toward 79), step_out decrements (toward 0); both clamp at the ends.
    void step(bool inward);
    void restore();  // seek physically to track 0
    [[nodiscard]] std::uint8_t physical_track() const { return physical_track_; }
    void set_physical_track(std::uint8_t track) { physical_track_ = track; }

    // Sense lines.
    [[nodiscard]] bool ready() const;               // disk inserted AND drive selected
    [[nodiscard]] bool is_track00() const;          // head at cylinder 0
    [[nodiscard]] bool write_protected() const;
    [[nodiscard]] bool index_pulse(std::uint64_t now) const;

    // Cycles remaining until the next index-pulse window begins (0 when `now`
    // is already inside one). Grounds WD2793 Type IV i2 (index-pulse IRQ)
    // scheduling: openMSX schedules INTRQ at the drive's next index pulse
    // rather than asserting it immediately (references/openmsx-21.0/src/fdc/
    // WD2793.cc:1049-1050 `irqTime = drive.getTimeTillIndexPulse(time)`).
    [[nodiscard]] std::uint64_t cycles_until_index_pulse(std::uint64_t now) const;

    // Cycles until the requested sector's ID address mark next rotates under the
    // head, from `now` (range 0 .. ~1 rotation). Grounds the WD2793 Type-II Read
    // Sector first-DRQ ROTATIONAL latency: real hardware / openMSX must wait for
    // the requested sector to come around, so the first-DRQ delay is VARIABLE, a
    // function of the disk's rotational angle at command start
    // (references/openmsx-21.0/src/fdc/RealDrive.cc:453 getNextSector -- finds the
    // next matching sector's addrIdx and returns the EmuTime it rotates under the
    // head; WD2793.cc:557 type2Search schedules from it).
    //
    // APPROXIMATION (documented, DEC-0055 slice C): our CHS image has no
    // per-sector byte-angular positions or real interleave, so the 9 sectors are
    // modelled as evenly spaced and sequential -- sector `sector_index` (0-based)
    // sits at angle sector_index/9 of the 715909-cycle rotation. This is the most
    // faithful rotational model our sector geometry can support; it is NOT
    // byte-identical to openMSX's raw-track model (which locks onto real addrIdx
    // positions with real interleave). Fully DETERMINISTIC: a pure function of
    // `now` and the fixed sector geometry -- no wall clock, no randomness.
    [[nodiscard]] std::uint64_t cycles_until_sector_id(std::uint32_t sector_index,
                                                       std::uint64_t now) const;

    // Disk-changed latch (Sony DSKCHG at 0x7FFD bit2, 0 = changed). Set true when
    // the mounted medium is (deterministically) swapped; cleared once acknowledged.
    //
    // disk_changed() is the NON-clearing const PEEK for snapshot/debug/inspection
    // paths (debug_snapshot.cpp fdc_section) -- it MUST NOT perturb the latch, so a
    // state dump never consumes the one-shot (Phase-3 determinism guarantee).
    [[nodiscard]] bool disk_changed() const { return disk_changed_; }
    void set_disk_changed(bool changed) { disk_changed_ = changed; }

    // Read-and-CLEAR DSKCHG one-shot: returns the current latch and clears it.
    // Models real hardware + openMSX, where READING the disk-changed line resets
    // it (references/openmsx-21.0/src/fdc/DiskChanger.cc:95-100 -- diskChanged()
    // returns the flag then sets diskChangedFlag=false). This is the MUTATING
    // accessor the FDC register read at 0x7FFD uses (mirroring the mutating
    // readMem path PhilipsFDC.cc:37, as opposed to the const peekMem :90). Without
    // it a swapped medium keeps DSKCHG asserted forever, so a game that re-checks
    // the disk after a swap (e.g. YS II's building-interior loader) sees a
    // perpetually-"changed" medium, retries/aborts (Force Interrupt) and drops
    // into DI;HALT -- the universal media-change freeze (M36 Bug B).
    [[nodiscard]] bool take_disk_changed() {
        const bool changed = disk_changed_;
        disk_changed_ = false;
        return changed;
    }

    // --- M36 Phase 3 debug snapshot: additive read-only introspection of the
    //     raw motor latch + delayed motor-off timer (planner §2.4 item 10).
    //     motor_on(now) stays the EFFECTIVE accessor; these expose the raw
    //     underlying state so the snapshot is restore-ready. const, ZERO
    //     behavior change. ---
    [[nodiscard]] bool motor_latched() const { return motor_on_; }
    [[nodiscard]] bool motor_off_pending() const { return motor_off_pending_; }
    [[nodiscard]] std::uint64_t motor_off_deadline() const { return motor_off_deadline_; }

    // Sector access at the current physical track + side latch.
    bool read_sector(std::uint8_t sector, std::uint8_t* out) const;
    bool write_sector(std::uint8_t sector, const std::uint8_t* in);

private:
    DiskImage* image_ = nullptr;
    std::uint8_t physical_track_ = 0;
    std::uint8_t side_ = 0;
    bool available_ = true;
    bool motor_on_ = false;
    bool motor_off_pending_ = false;
    std::uint64_t motor_off_deadline_ = 0;
    bool disk_changed_ = false;
};

}  // namespace sony_msx::devices::fdc
