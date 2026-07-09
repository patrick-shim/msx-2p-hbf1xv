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

#include "devices/fdc/disk_drive.h"
#include "devices/fdc/fdc_clock_source.h"

namespace sony_msx::devices::fdc {

// WD2793 floppy-disk controller core (M16-S2/S3). The HB-F1XV physical chip is
// the Fujitsu MB89311, register- and command-compatible with the Western Digital
// WD2793 (fact-sheet "FDC for Sony HB-F1XV.md" §1). This models the five
// programmer-visible registers, the Type I/II/III/IV command set, the context-
// sensitive status-bit layouts, INTRQ/DRQ handshake, and deterministic
// Busy/DRQ/step/settle/index/motor timing computed in emulated cycles.
//
// Behaviour reference (read only, never copied, GPL isolation — guardrails):
// references/openmsx-21.0/src/fdc/WD2793.cc (status constants :15-26; command
// dispatch setCommandReg :111-157; getStatusReg layout :159-197; Type I
// startType1Cmd/seek/step :420-519; Type II startType2Cmd/read/write :522-817;
// Type III read-address/read-track/write-track :820-1033; Type IV force
// interrupt :1035-1060; endCmd :1062-1073).
//
// Determinism: every deadline is an ABSOLUTE emulated-cycle count from the
// attached FdcClockSource; each public accessor first sync()s the FSM to `now`.
// No wall clock, no host-disk dependency (planner §5.2 / A-M16-2).
class Wd2793 {
public:
    // Status register bits (WD2793.cc:15-26). Bit meanings are context-sensitive:
    // in Type I / after-FI, bit2=TRACK00, bit4=SEEK_ERROR, bit5=HEAD_LOADED; in
    // Type II/III, bit1=DRQ, bit2=LOST_DATA, bit4=RECORD_NOT_FOUND, bit5=RECORD_TYPE.
    static constexpr std::uint8_t kBusy = 0x01;
    static constexpr std::uint8_t kIndex = 0x02;
    static constexpr std::uint8_t kDrq = 0x02;
    static constexpr std::uint8_t kTrack00 = 0x04;
    static constexpr std::uint8_t kLostData = 0x04;
    static constexpr std::uint8_t kCrcError = 0x08;
    static constexpr std::uint8_t kSeekError = 0x10;
    static constexpr std::uint8_t kRecordNotFound = 0x10;
    static constexpr std::uint8_t kHeadLoaded = 0x20;
    static constexpr std::uint8_t kRecordType = 0x20;
    static constexpr std::uint8_t kWriteProtected = 0x40;
    static constexpr std::uint8_t kNotReady = 0x80;

    // Timing (fact-sheet §4; system clock 3,579,545 Hz). ~32 us/byte MFM.
    static constexpr std::uint64_t kSystemClockHz = 3579545;
    static constexpr std::uint64_t kCyclesPerByte = 114;  // 32 us DRQ cadence
    // Step rates 6/12/20/30 ms at a 1 MHz FDC clock (WD2793.cc:477-483).
    static constexpr std::uint64_t kStepCycles[4] = {21477, 42955, 71591, 107386};
    static constexpr std::uint64_t kSettleCycles = 107386;   // 30 ms (V/E flag)
    static constexpr std::uint64_t kReadStartCycles = 2 * kCyclesPerByte;
    // HLD (head-load) idle-timeout duration (WD2793.cc:42 `IDLE = 3s`): the
    // Type-I HEAD_LOADED status bit stays set for this long after HLD was last
    // (re)activated -- see start_type1/end_command.
    static constexpr std::uint64_t kHldIdleCycles = 3 * kSystemClockHz;

    void attach_clock_source(FdcClockSource* source) { clock_ = source; }
    void attach_drive(DiskDrive* drive) { drive_ = drive; }

    void reset();

    // Register writes (Command/Track/Sector/Data at Sony 0x7FF8-0x7FFB).
    void write_command(std::uint8_t value);
    void write_track(std::uint8_t value);
    void write_sector(std::uint8_t value);
    void write_data(std::uint8_t value);

    // Register reads. read_status has the INTRQ-clear side effect; read_data has
    // the DRQ-advance side effect (WD2793.cc:159-197, 249-312).
    [[nodiscard]] std::uint8_t read_status();
    [[nodiscard]] std::uint8_t read_track();
    [[nodiscard]] std::uint8_t read_sector();
    [[nodiscard]] std::uint8_t read_data();

    // Handshake lines (read back through the Sony 0x7FFF window, active-low).
    [[nodiscard]] bool intrq();
    [[nodiscard]] bool drq();

    // Non-perturbing introspection for deterministic tests.
    [[nodiscard]] std::uint8_t peek_status() const;
    [[nodiscard]] std::uint8_t track_register() const { return track_reg_; }
    [[nodiscard]] std::uint8_t sector_register() const { return sector_reg_; }
    [[nodiscard]] std::uint8_t command_register() const { return command_reg_; }

    // Cumulative, non-perturbing diagnostic counters (M16-S6): back the boot-
    // checkpoint acceptance signal (planner §6.3) — "a Read Sector command was
    // accepted", "512 DRQ byte-transfers occurred", "command completed with
    // INTRQ set and no error bits" — without guessing a disk-ROM disassembly.
    // Cleared by reset(); pure bookkeeping, no effect on emulated behaviour.
    [[nodiscard]] std::uint32_t read_sector_commands_accepted() const {
        return read_sector_commands_accepted_;
    }
    [[nodiscard]] std::uint32_t read_sector_bytes_transferred() const {
        return read_sector_bytes_transferred_;
    }
    [[nodiscard]] std::uint32_t read_sector_completions_ok() const {
        return read_sector_completions_ok_;
    }

private:
    enum class Phase : std::uint8_t {
        Idle,
        Type1Seek,
        ReadSector,
        WriteSector,
        ReadAddress,
        ReadTrack,
        WriteTrack,
    };

    [[nodiscard]] std::uint64_t now() const;
    void sync(std::uint64_t t);
    void end_command(std::uint64_t t);

    void start_type1(std::uint8_t cmd, std::uint64_t t);
    void start_type2(std::uint8_t cmd, std::uint64_t t);
    void start_type3(std::uint8_t cmd, std::uint64_t t);
    void start_type4(std::uint8_t cmd, std::uint64_t t);

    void begin_read_sector(std::uint64_t t);
    void begin_write_sector(std::uint64_t t);
    void finish_read_sector(std::uint64_t t);
    void finish_write_sector(std::uint64_t t);

    [[nodiscard]] bool is_type1_status() const;
    [[nodiscard]] bool transfer_drq(std::uint64_t t) const;

    void build_read_address_buffer();
    void build_read_track_buffer();
    void parse_write_track_byte(std::uint8_t value);

    FdcClockSource* clock_ = nullptr;
    DiskDrive* drive_ = nullptr;

    std::uint8_t status_ = 0;  // sticky bits; dynamic bits recomputed on read
    std::uint8_t command_reg_ = 0;
    std::uint8_t track_reg_ = 0xFF;
    std::uint8_t sector_reg_ = 0x01;
    std::uint8_t data_reg_ = 0;

    Phase phase_ = Phase::Idle;
    bool direction_in_ = true;
    bool intrq_pending_ = false;
    bool immediate_irq_ = false;

    // Type IV i2 (index-pulse IRQ) scheduling: INTRQ is armed to fire once, at
    // the drive's next index-pulse edge (WD2793.cc:1049-1050), not immediately.
    bool index_irq_armed_ = false;
    std::uint64_t index_irq_deadline_ = 0;

    // HLD (head-load): reflects the H-flag (0x08) of the most recently issued
    // Type-I command, NOT the drive motor state (WD2793.cc:420-433 -- "on all
    // MSX machines I checked HLT is just stubbed to +5V ... the head is loaded
    // immediately" once H is set). Stays active for kHldIdleCycles after the
    // last time it was (re)activated -- by a new H-flagged Type-I command, or
    // by a command completing while still active (WD2793.cc:1062-1073).
    bool hld_active_ = false;
    std::uint64_t hld_since_ = 0;

    std::uint64_t busy_until_ = 0;   // Type I completion / non-DRQ deadline
    std::uint64_t drq_deadline_ = 0;  // absolute cycle of the next DRQ window
    std::uint64_t last_sync_ = 0;

    std::vector<std::uint8_t> buffer_;  // transfer buffer (data / addr / track)
    int data_index_ = 0;
    int data_available_ = 0;

    // Write-sector staging: target coordinates + assembled data.
    std::uint8_t write_sector_num_ = 0;

    // Write-track parser state (standard 9-sector reformat).
    int wt_a1_run_ = 0;
    bool wt_expect_id_ = false;
    int wt_id_count_ = 0;
    std::uint8_t wt_id_[4] = {0, 0, 0, 0};
    bool wt_in_data_ = false;
    int wt_data_count_ = 0;
    std::uint8_t wt_sector_num_ = 0;
    std::vector<std::uint8_t> wt_data_;

    std::uint32_t read_sector_commands_accepted_ = 0;
    std::uint32_t read_sector_bytes_transferred_ = 0;
    std::uint32_t read_sector_completions_ok_ = 0;
};

}  // namespace sony_msx::devices::fdc
