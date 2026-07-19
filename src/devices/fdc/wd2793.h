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

#include "devices/fdc/disk_drive.h"
#include "devices/fdc/fdc_clock_source.h"

namespace sony_msx::devices::fdc {

// DEC-0052 stream-capture: a lightweight, NON-PERTURBING observer of completed
// sector reads. The machine installs one ONLY while live stream-capture is armed
// (default null => the whole notify path is skipped => byte-for-byte identical
// FDC behaviour AND timing). It is called synchronously from begin_read_sector
// the instant the 512 sector bytes have landed in the read buffer, BEFORE any
// DRQ byte transfer, so the observer sees the exact bytes the drive returned for
// the sector. Contract: an implementation MUST NOT mutate FDC/drive/timing state,
// issue reads, or advance any clock -- it only inspects the supplied bytes (e.g.
// to CRC + log them). It is a pure diagnostic sink, isolated from emulation.
class FdcSectorReadObserver {
public:
    virtual ~FdcSectorReadObserver() = default;
    // command = the WD2793 command register that drove this read (Type II/III);
    // track/side = the drive's current physical head position; sector = 1-based
    // SR; data/size = the sector payload just read (size == DiskImage::kSectorSize).
    virtual void on_sector_read(std::uint8_t command, std::uint8_t track, std::uint8_t side,
                                std::uint8_t sector, const std::uint8_t* data,
                                std::size_t size) = 0;
};

// DEF-M47-DISKWRITE stream-capture: a lightweight, NON-PERTURBING observer of
// the Write Sector byte stream, mirroring FdcSectorReadObserver. The machine
// installs one ONLY while live stream-capture is armed (default null => the
// whole notify path is skipped => byte-for-byte identical FDC behaviour AND
// timing). on_write_byte is called synchronously as each data byte is committed
// to the sector buffer (whether a genuine CPU byte or a substituted 0x00), and
// on_sector_write once the full sector is flushed to the (LATCHED) CHS. Same
// hard contract as FdcSectorReadObserver: an implementation MUST NOT mutate
// FDC/drive/timing state, issue reads, or advance any clock -- it only inspects
// the supplied values (e.g. to log a per-byte trace so a live in-game disk save
// can be
// byte-diffed against the raw .dsk). A pure diagnostic sink, isolated from
// emulation.
class FdcSectorWriteObserver {
public:
    virtual ~FdcSectorWriteObserver() = default;
    // command = the WD2793 command register driving this write (Type II Write
    // Sector); track/side/sector = the LATCHED target CHS (H4); lba = its LBA;
    // data_index = 0-based byte position within the sector; value = the byte
    // committed; substituted = true when it is a 0x00 un-serviced-slot fill (CPU
    // missed the DRQ window) rather than a genuine CPU byte; drq_deadline = the
    // absolute cycle of the next DRQ window after this commit.
    virtual void on_write_byte(std::uint8_t command, std::uint8_t track, std::uint8_t side,
                               std::uint8_t sector, std::uint32_t lba, int data_index,
                               std::uint8_t value, bool substituted,
                               std::uint64_t drq_deadline) = 0;
    // Called from finish_write_sector once the assembled 512-byte sector is
    // committed to the LATCHED CHS. crc = CRC-16-CCITT over the sector data.
    virtual void on_sector_write(std::uint8_t command, std::uint8_t track, std::uint8_t side,
                                 std::uint8_t sector, std::uint32_t lba, const std::uint8_t* data,
                                 std::size_t size, std::uint16_t crc) = 0;
};

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
    // Type-II Read Sector first-DRQ: intra-sector span from the ID address mark
    // (the 0xFE byte the sector search locks onto) to the first data byte.
    // openMSX schedules the first data DRQ gapLength + 2 byte periods after the
    // mark rotates under the head (WD2793.cc:624-644; gapLength = dataIdx -
    // addrIdx). Our IBM System-34 track (build_read_track_buffer: C/H/R/N + CRC +
    // Gap2 + sync + A1A1A1 + DAM) has gapLength = 45, so 45 + 2 = 47 byte
    // periods. The rotational wait for the mark to ARRIVE is added separately
    // (DiskDrive::cycles_until_sector_id; see begin_read_sector). Shared by
    // WRITE Sector too (begin_write_sector, DEF-M45-WRITEDRQ-FIX): the write
    // first-byte DRQ is likewise rotational-search-relative, so read and write
    // use the SAME rotational-wait + header model. DISTINCT from kReadStartCycles,
    // which read-address/read-track/write-track keep unchanged.
    static constexpr std::uint64_t kReadSectorHeaderCycles = 47 * kCyclesPerByte;
    // Write Sector first-byte CHECK_WRITE window (openMSX startWriteSector /
    // checkStartWrite, WD2793.cc:646-701). After the write-sector DRQ first
    // asserts, the CPU must supply the FIRST data byte within this window; if it
    // does not, the command ABORTS with LOST_DATA and NOTHING is written to disk
    // (no all-zero sector). On real hardware the DRQ is asserted only after the
    // rotational sector-search (begin_write_sector adds that variable latency),
    // and openMSX's own CHECK_WRITE is just 8 byte-periods after the DRQ because
    // there the CPU has already finished setup DURING the search (setDataReg
    // latches the first byte only while DRQ is up, WD2793.cc:235-247). Our model
    // can present a small (unlucky sector angle) or --fast-disk-collapsed
    // rotational wait while the CPU still runs real-time, so an 8-byte tail would
    // wrongly abort a valid slow-first-byte write (the DEF-M45 multi-disk-RPG
    // in-game-save
    // hang). We therefore allow a FULL further disk revolution after the DRQ --
    // the natural disk timescale, far beyond any real save-buffer setup, so a
    // valid write is NEVER aborted while a genuinely-absent first byte still
    // aborts deterministically. Deliberately NOT fast-scaled: the CPU runs
    // real-time in fast-disk mode, so the fault-detection window must stay
    // real-time too (kIndexPeriodCycles == kSystemClockHz / 5, one 300 rpm rev).
    static constexpr std::uint64_t kWriteFirstByteWindowCycles = kSystemClockHz / 5;
    // HLD (head-load) idle-timeout duration (WD2793.cc:42 `IDLE = 3s`): the
    // Type-I HEAD_LOADED status bit stays set for this long after HLD was last
    // (re)activated -- see start_type1/end_command.
    static constexpr std::uint64_t kHldIdleCycles = 3 * kSystemClockHz;

    // Fast-disk (turbo) timing -- an OPT-IN quality-of-life mode (default OFF).
    // When fast_disk_ is set, every read/seek WAIT above collapses to these tiny
    // values so disk loads finish near-instantly, while default-off timing stays
    // 100% cycle-accurate (byte-identical to now). These are deliberately kept
    // NON-ZERO so the Busy/DRQ FSM handshake and the status-poll ordering stay
    // valid: a poll loop still observes Busy->clear and !DRQ->DRQ transitions.
    // The per-byte cadence is small enough that the DRQ deadline never runs
    // ahead of a real-speed CPU disk loop, so the loop is never stalled; the
    // Lost-Data catch-up path is additionally suppressed in fast mode (a faster-
    // than-real disk would otherwise "outrun" the CPU and spuriously declare
    // lost data -- see read_data/write_data). Rotational latency (the dominant
    // cost) collapses in DiskDrive::cycles_until_sector_id under its own flag;
    // the index-pulse rotation model itself (Type IV i2 timing) is left intact.
    static constexpr std::uint64_t kFastCyclesPerByte = 8;
    static constexpr std::uint64_t kFastStepCycles = 64;
    static constexpr std::uint64_t kFastSettleCycles = 64;
    static constexpr std::uint64_t kFastReadStartCycles = 2 * kFastCyclesPerByte;       // 16
    static constexpr std::uint64_t kFastReadSectorHeaderCycles = 4 * kFastCyclesPerByte;  // 32

    void attach_clock_source(FdcClockSource* source) { clock_ = source; }
    void attach_drive(DiskDrive* drive) { drive_ = drive; }

    // Fast-disk (turbo) mode toggle. Default OFF => byte-identical accurate FDC
    // timing. Runtime-settable (CLI --fast-disk / SDL3 Alt+D). Deliberately NOT
    // cleared by reset() (a config toggle like clock_/drive_/observer), so a
    // cold_boot mid-session preserves it.
    void set_fast_disk(bool on) { fast_disk_ = on; }
    [[nodiscard]] bool fast_disk() const { return fast_disk_; }

    // DEC-0052 stream-capture: install (non-null) / remove (nullptr) the
    // non-perturbing sector-read observer. Default null => zero behaviour change;
    // reset() deliberately does NOT clear it (it is an externally-owned lifecycle
    // pointer, like clock_/drive_ above, managed by the installing machine).
    void set_sector_read_observer(FdcSectorReadObserver* observer) {
        sector_read_observer_ = observer;
    }

    // DEF-M47-DISKWRITE stream-capture: install (non-null) / remove (nullptr) the
    // non-perturbing Write Sector trace observer. Default null => zero behaviour
    // change; reset() deliberately does NOT clear it (externally-owned lifecycle
    // pointer, like clock_/drive_/sector_read_observer_ above).
    void set_sector_write_observer(FdcSectorWriteObserver* observer) {
        sector_write_observer_ = observer;
    }

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

    // --- M36 Phase 3 debug snapshot: additive read-only introspection of the
    //     FSM (phase / direction / INTRQ-DRQ pending / index-IRQ arm / HLD /
    //     timing deadlines / transfer-buffer cursor / write-track parser).
    //     const returns of existing members, ZERO behavior change (planner
    //     §2.4 item 9). Deliberately plain member reads -- they do NOT sync()
    //     the FSM, so they cannot mutate/advance controller state (unlike the
    //     public register reads); the snapshot stays non-perturbing. The enum
    //     phase returns a numeric code (self-describing snapshot). ---
    [[nodiscard]] std::uint8_t data_register() const { return data_reg_; }
    [[nodiscard]] std::uint8_t phase_code() const { return static_cast<std::uint8_t>(phase_); }
    [[nodiscard]] bool direction_in() const { return direction_in_; }
    [[nodiscard]] bool intrq_pending() const { return intrq_pending_; }
    [[nodiscard]] bool immediate_irq() const { return immediate_irq_; }
    [[nodiscard]] bool index_irq_armed() const { return index_irq_armed_; }
    [[nodiscard]] std::uint64_t index_irq_deadline() const { return index_irq_deadline_; }
    [[nodiscard]] bool hld_active() const { return hld_active_; }
    [[nodiscard]] std::uint64_t hld_since() const { return hld_since_; }
    [[nodiscard]] std::uint64_t busy_until() const { return busy_until_; }
    [[nodiscard]] std::uint64_t drq_deadline() const { return drq_deadline_; }
    [[nodiscard]] std::uint64_t write_check_deadline() const { return write_check_deadline_; }
    [[nodiscard]] std::uint64_t last_sync() const { return last_sync_; }
    [[nodiscard]] int data_index() const { return data_index_; }
    [[nodiscard]] int data_available() const { return data_available_; }
    [[nodiscard]] std::uint8_t write_sector_num() const { return write_sector_num_; }
    [[nodiscard]] int wt_a1_run() const { return wt_a1_run_; }
    [[nodiscard]] bool wt_expect_id() const { return wt_expect_id_; }
    [[nodiscard]] int wt_id_count() const { return wt_id_count_; }
    [[nodiscard]] std::uint8_t wt_id(int index) const { return wt_id_[static_cast<std::size_t>(index)]; }
    [[nodiscard]] bool wt_in_data() const { return wt_in_data_; }
    [[nodiscard]] int wt_data_count() const { return wt_data_count_; }
    [[nodiscard]] std::uint8_t wt_sector_num() const { return wt_sector_num_; }

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

    // Effective timing selectors: the accurate constants by default, the
    // collapsed kFast* values in fast-disk mode. ONE decision point per knob so
    // default-off stays byte-identical (fast_disk_==false returns the exact
    // pre-change constant).
    [[nodiscard]] std::uint64_t cycles_per_byte() const {
        return fast_disk_ ? kFastCyclesPerByte : kCyclesPerByte;
    }
    [[nodiscard]] std::uint64_t read_start_cycles() const {
        return fast_disk_ ? kFastReadStartCycles : kReadStartCycles;
    }
    [[nodiscard]] std::uint64_t read_sector_header_cycles() const {
        return fast_disk_ ? kFastReadSectorHeaderCycles : kReadSectorHeaderCycles;
    }
    [[nodiscard]] std::uint64_t step_cycles(std::uint8_t cmd) const {
        return fast_disk_ ? kFastStepCycles : kStepCycles[cmd & 0x03];
    }
    [[nodiscard]] std::uint64_t settle_cycles() const {
        return fast_disk_ ? kFastSettleCycles : kSettleCycles;
    }

    void start_type1(std::uint8_t cmd, std::uint64_t t);
    void start_type2(std::uint8_t cmd, std::uint64_t t);
    void start_type3(std::uint8_t cmd, std::uint64_t t);
    void start_type4(std::uint8_t cmd, std::uint64_t t);

    void begin_read_sector(std::uint64_t t);
    void begin_write_sector(std::uint64_t t);
    void finish_read_sector(std::uint64_t t);
    void finish_write_sector(std::uint64_t t);

    // DEF-M47-DISKWRITE: commit ONE Write Sector byte in-order at data_index_ and
    // advance the position (never drop). `substituted` distinguishes a genuine
    // CPU byte (re-bases the next DRQ on the actual service time `t`) from a
    // 0x00 un-serviced-slot fill (advances on the fixed disk cadence + sets
    // LOST_DATA). Fires the write observer and, at data_available_ == 0, flushes
    // via finish_write_sector.
    void commit_write_sector_byte(std::uint64_t t, std::uint8_t value, bool substituted);

    [[nodiscard]] bool is_type1_status() const;
    [[nodiscard]] bool transfer_drq(std::uint64_t t) const;

    void build_read_address_buffer();
    void build_read_track_buffer();
    void parse_write_track_byte(std::uint8_t value);

    FdcClockSource* clock_ = nullptr;
    DiskDrive* drive_ = nullptr;
    // Fast-disk (turbo) mode. Default false => accurate timing (byte-identical).
    // Not touched by reset() (see set_fast_disk).
    bool fast_disk_ = false;
    // DEC-0052 stream-capture sink (default null => never notified). Externally
    // owned; see set_sector_read_observer.
    FdcSectorReadObserver* sector_read_observer_ = nullptr;
    // DEF-M47-DISKWRITE Write Sector trace sink (default null => never notified).
    // Externally owned; see set_sector_write_observer.
    FdcSectorWriteObserver* sector_write_observer_ = nullptr;

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
    // Write Sector first-byte CHECK_WRITE gate (openMSX checkStartWrite,
    // WD2793.cc:674-682): absolute cycle by which the CPU must supply the FIRST
    // data byte of the current sector (set in begin_write_sector). If sync()
    // crosses it while data_index_ == 0 (no byte has landed), the command aborts
    // with LOST_DATA and writes NOTHING. Once the first byte lands the gate
    // closes, so a later mid-transfer stall never aborts (the one-byte pipeline
    // waits for each byte -- see write_data). Set to the rotational first-byte
    // DRQ + kWriteFirstByteWindowCycles (one revolution), so a valid slow-first-
    // byte write is never aborted. DEF-M45-WRITEDRQ (window fix).
    std::uint64_t write_check_deadline_ = 0;
    std::uint64_t last_sync_ = 0;

    std::vector<std::uint8_t> buffer_;  // transfer buffer (data / addr / track)
    int data_index_ = 0;
    int data_available_ = 0;

    // DEF-M47-DISKWRITE: a CPU data-register write is LATCHED here (fresh == a
    // byte is pending to be laid to disk). write_data commits it in-order at the
    // current position regardless of DRQ timing (never dropped -- decoupling the
    // byte-POSITION from the CPU-write TIMING); the flag distinguishes a genuine
    // CPU byte from a substituted 0x00 for the un-serviced-slot path.
    bool data_reg_fresh_ = false;

    // Write staging: the LATCHED target coordinates (H4 -- captured at
    // begin_write_sector / Write Track start, committed by finish_write_sector /
    // parse_write_track_byte, so a mid-transfer side/track change cannot redirect
    // the sector) + the assembled data (buffer_).
    std::uint8_t write_sector_num_ = 0;
    std::uint8_t write_track_num_ = 0;
    std::uint8_t write_side_ = 0;

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
