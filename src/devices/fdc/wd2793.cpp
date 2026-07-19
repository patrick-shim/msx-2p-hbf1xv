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

#include "devices/fdc/wd2793.h"

#include <cstdlib>
#include <cstdio>

namespace sony_msx::devices::fdc {

namespace {

// Write-commit DIAGNOSTIC (DEC-0072; env-gated, default OFF => this whole path
// is skipped and
// emulation is byte-identical). Set SONY_MSX_FDCWLOG=<path> to stream every
// committed Write-Sector byte (genuine vs 0x00-substituted) for offline audit of
// whether our FDC lays the game's bytes down faithfully. Opened once, lazily.
std::FILE* fdc_write_log() {
    static std::FILE* f = []() -> std::FILE* {
        const char* p = std::getenv("SONY_MSX_FDCWLOG");
        return (p != nullptr && *p != '\0') ? std::fopen(p, "w") : nullptr;
    }();
    return f;
}

// Command-register flag bits (WD2793.cc:28-39).
constexpr std::uint8_t kVFlag = 0x04;   // Type I verify (settle delay)
constexpr std::uint8_t kEFlag = 0x04;   // Type II/III settle delay
constexpr std::uint8_t kHFlag = 0x08;   // Type I head-load (drives HLD/HEAD_LOADED)
constexpr std::uint8_t kTFlag = 0x10;   // Type I update-Track-Register
constexpr std::uint8_t kMFlag = 0x10;   // Type II multi-record
constexpr std::uint8_t kA0Flag = 0x01;  // Type II data-address-mark type
constexpr std::uint8_t kImmIrq = 0x08;  // Type IV i3 immediate interrupt
constexpr std::uint8_t kIdxIrq = 0x04;  // Type IV i2 index-pulse interrupt

// Standard MSX 9-sector MFM track layout length (fact-sheet §4/§5): ~6250 bytes
// unformatted per double-density track. Read Track and Write Track frame a full
// track image at this size.
constexpr int kStandardTrackLength = 6250;

// CRC-16-CCITT (poly 0x1021, init 0xFFFF) over the ID/data field (fact-sheet §5).
std::uint16_t crc16(const std::uint8_t* d, std::size_t n) {
    std::uint16_t c = 0xFFFF;
    for (std::size_t i = 0; i < n; ++i) {
        c ^= static_cast<std::uint16_t>(d[i]) << 8;
        for (int b = 0; b < 8; ++b) {
            c = (c & 0x8000) ? static_cast<std::uint16_t>((c << 1) ^ 0x1021)
                             : static_cast<std::uint16_t>(c << 1);
        }
    }
    return c;
}

}  // namespace

std::uint64_t Wd2793::now() const {
    return clock_ != nullptr ? clock_->cpu_cycles() : last_sync_;
}

void Wd2793::reset() {
    // Master reset: TR = 0xFF (fact-sheet §3 + §8 "TR=0xFF-at-reset"; WD2793.cc:75
    // then executes a Restore, but the register reads 0xFF until it completes).
    status_ = 0;
    command_reg_ = 0;
    track_reg_ = 0xFF;
    sector_reg_ = 0x01;
    data_reg_ = 0;
    phase_ = Phase::Idle;
    direction_in_ = true;
    intrq_pending_ = false;
    immediate_irq_ = false;
    index_irq_armed_ = false;
    index_irq_deadline_ = 0;
    hld_active_ = false;
    hld_since_ = 0;
    busy_until_ = 0;
    drq_deadline_ = 0;
    write_check_deadline_ = 0;
    last_sync_ = clock_ != nullptr ? clock_->cpu_cycles() : 0;
    data_index_ = 0;
    data_available_ = 0;
    data_reg_fresh_ = false;
    write_sector_num_ = 0;
    write_track_num_ = 0;
    write_side_ = 0;
    buffer_.clear();
    wt_a1_run_ = 0;
    wt_expect_id_ = false;
    wt_in_data_ = false;
    read_sector_commands_accepted_ = 0;
    read_sector_bytes_transferred_ = 0;
    read_sector_completions_ok_ = 0;
}

void Wd2793::sync(std::uint64_t t) {
    if (t < last_sync_) {
        t = last_sync_;  // monotonic guard
    }
    last_sync_ = t;
    if (phase_ == Phase::Type1Seek && t >= busy_until_) {
        // Type I command completes: clear Busy, raise INTRQ, and (per
        // endType1Cmd -> endCmd, WD2793.cc:512-519) restart any active HLD
        // idle-timeout window -- routed through end_command() so the HLD
        // bookkeeping in end_command() applies here too.
        end_command(t);
    }
    // Write Sector first-byte CHECK_WRITE gate (openMSX checkStartWrite,
    // WD2793.cc:674-682, DEF-M45-WRITEDRQ). If the FIRST data byte of the sector
    // has not been supplied by the check deadline, abort with LOST_DATA and
    // write NOTHING to disk (finish_write_sector is never reached, so no
    // all-zero sector is committed). data_index_ == 0 => no byte has landed yet;
    // once the first byte lands the gate closes and a mid-transfer stall never
    // aborts (the one-byte pipeline waits for each later byte). This is a single
    // scheduled event, NOT an accumulating deadline -- distinct from the removed
    // absolute-deadline zero-substitution that corrupted normally-timed writes.
    if (phase_ == Phase::WriteSector && data_index_ == 0 && t >= write_check_deadline_) {
        status_ |= kLostData;
        end_command(t);
    }
    // Mid-transfer abandoned Write Sector (DEF-M47-DISKWRITE; preserves the
    // DEF-M45-WRITEDRQ first-byte abort). This
    // is DISTINCT from the first-byte CHECK_WRITE gate above (which aborts an
    // ABSENT first byte, data_index_ == 0). Once the FIRST byte has landed
    // (data_index_ > 0), the WD2793 lays down a FULL 512-byte sector regardless of
    // whether the CPU keeps up (fact-sheet "FDC for Sony HB-F1XV.md" §8 "missed
    // DRQ mid-write -> Lost Data and a 0x00 byte substituted, command continues";
    // openMSX writeSectorData WD2793.cc:750-757 `if (dataRegWritten) ... else {
    // dataOutReg = 0; statusReg |= LOST_DATA; }` -- understanding only, never
    // copied, GPL isolation). So a CPU that stops servicing DRQ after the first
    // byte does NOT abort: the un-serviced slots are substituted 0x00 + LOST_DATA
    // and the position still ADVANCES (never drops, never hangs BUSY), and the
    // sector completes. The full-revolution grace (kWriteFirstByteWindowCycles)
    // deliberately keeps this OFF the normal poll/stall/drift/burst path -- those
    // commit EVERY byte on the CPU write (write_data -> commit_write_sector_byte)
    // and never reach here; it fires only for a genuinely-abandoned mid-sector
    // write, matching the CPU-gated stream model the read path documents. The gate
    // above is checked FIRST so this zero-substitution can never fire before the
    // first real byte lands.
    while (phase_ == Phase::WriteSector && data_index_ > 0 && data_available_ > 0 &&
           t >= drq_deadline_ + kWriteFirstByteWindowCycles) {
        commit_write_sector_byte(t, 0x00, /*substituted=*/true);
    }
    if (index_irq_armed_ && t >= index_irq_deadline_) {
        // Type IV i2 (index-pulse IRQ) fires once its scheduled deadline is
        // reached (WD2793.cc:1049-1050).
        intrq_pending_ = true;
        index_irq_armed_ = false;
    }
}

bool Wd2793::is_type1_status() const {
    // Type I or Type IV (force interrupt) -> Type I status-bit layout
    // (WD2793.cc:161).
    return ((command_reg_ & 0x80) == 0) || ((command_reg_ & 0xF0) == 0xD0);
}

bool Wd2793::transfer_drq(const std::uint64_t t) const {
    switch (phase_) {
        case Phase::ReadSector:
        case Phase::ReadAddress:
        case Phase::ReadTrack:
        case Phase::WriteSector:
        case Phase::WriteTrack:
            return data_available_ > 0 && t >= drq_deadline_;
        default:
            return false;
    }
}

std::uint8_t Wd2793::read_status() {
    const std::uint64_t t = now();
    sync(t);
    const std::uint8_t s = peek_status();
    // Reading the status register clears INTRQ (WD2793.cc:191-194; fact-sheet §6).
    intrq_pending_ = false;
    return s;
}

std::uint8_t Wd2793::peek_status() const {
    std::uint8_t s = status_;
    const bool ready = (drive_ != nullptr) && drive_->ready();
    if (is_type1_status()) {
        s &= ~(kIndex | kTrack00 | kHeadLoaded | kWriteProtected);
        if (drive_ != nullptr) {
            if (drive_->index_pulse(last_sync_)) {
                s |= kIndex;
            }
            if (drive_->is_track00()) {
                s |= kTrack00;
            }
            if (hld_active_ && last_sync_ >= hld_since_ &&
                last_sync_ < hld_since_ + kHldIdleCycles) {
                // HEAD_LOADED reflects the H-flag of the last Type-I command,
                // not the motor state (WD2793.cc:420-433/1064-1068).
                s |= kHeadLoaded;
            }
            if (drive_->write_protected()) {
                s |= kWriteProtected;
            }
        }
    } else {
        if (transfer_drq(last_sync_)) {
            s |= kDrq;
        } else {
            s &= ~kDrq;
        }
    }
    if (ready) {
        s &= ~kNotReady;
    } else {
        s |= kNotReady;
    }
    return s;
}

std::uint8_t Wd2793::read_track() {
    sync(now());
    return track_reg_;
}

std::uint8_t Wd2793::read_sector() {
    sync(now());
    return sector_reg_;
}

std::uint8_t Wd2793::read_data() {
    const std::uint64_t t = now();
    sync(t);
    const bool reading = (phase_ == Phase::ReadSector) || (phase_ == Phase::ReadAddress) ||
                         (phase_ == Phase::ReadTrack);
    if (reading && transfer_drq(t)) {
        // Catch up any DRQ windows the CPU missed: the disk byte is consumed but
        // the CPU never received it -> Lost Data (WD2793.cc:261-267; fact-sheet §8).
        // Suppressed in fast-disk mode: a turbo disk deliberately runs faster than
        // real, so a real-speed CPU loop would always be "late" -- honouring the
        // catch-up would skip (drop) bytes and corrupt the transfer. In fast mode
        // the model instead gates the stream on CPU reads (the DRQ deadline only
        // advances kFastCyclesPerByte per byte and never runs ahead), so bytes are
        // delivered SEQUENTIALLY with no loss -- data integrity preserved.
        while (!fast_disk_ && data_available_ > 1 && t >= drq_deadline_ + kCyclesPerByte) {
            status_ |= kLostData;
            ++data_index_;
            --data_available_;
            drq_deadline_ += kCyclesPerByte;
        }
        const bool counting_read_sector = (phase_ == Phase::ReadSector);
        data_reg_ = buffer_[static_cast<std::size_t>(data_index_++)];
        --data_available_;
        drq_deadline_ += cycles_per_byte();
        if (counting_read_sector) {
            ++read_sector_bytes_transferred_;
        }
        if (data_available_ == 0) {
            if (phase_ == Phase::ReadSector) {
                finish_read_sector(t);
            } else {
                end_command(t);
            }
        }
    }
    return data_reg_;
}

bool Wd2793::intrq() {
    const std::uint64_t t = now();
    sync(t);
    return intrq_pending_ || immediate_irq_;
}

bool Wd2793::drq() {
    const std::uint64_t t = now();
    sync(t);
    return transfer_drq(t);
}

void Wd2793::write_command(const std::uint8_t value) {
    const std::uint64_t t = now();
    sync(t);
    const std::uint8_t hi = value & 0xF0;
    if (hi != 0xD0) {
        // A new command (not Force Interrupt) supersedes any prior INTRQ,
        // including a still-pending Type IV i2 (index-pulse) schedule.
        intrq_pending_ = false;
        immediate_irq_ = false;
        index_irq_armed_ = false;
    }
    command_reg_ = value;
    if ((value & 0x80) == 0) {
        start_type1(value, t);
    } else if (hi == 0x80 || hi == 0x90 || hi == 0xA0 || hi == 0xB0) {
        start_type2(value, t);
    } else if (hi == 0xC0 || hi == 0xE0 || hi == 0xF0) {
        start_type3(value, t);
    } else {  // 0xD0
        start_type4(value, t);
    }
}

void Wd2793::write_track(const std::uint8_t value) {
    sync(now());
    track_reg_ = value;
}

void Wd2793::write_sector(const std::uint8_t value) {
    sync(now());
    sector_reg_ = value;
}

void Wd2793::write_data(const std::uint8_t value) {
    const std::uint64_t t = now();
    sync(t);
    // LATCH the CPU byte, then COMMIT it in-order at the
    // current sector position -- ALWAYS, never dropped (DEF-M47-DISKWRITE). The
    // byte-POSITION is
    // DECOUPLED from the CPU-write TIMING. The earlier model gated the commit on
    // transfer_drq(t) and SILENTLY DROPPED (data_reg_ updated, but buffer_/
    // data_index_/data_available_ NOT advanced) any byte that did not land
    // exactly inside the DRQ window -- an early / 2-bytes-per-DRQ-burst /
    // fixed-cadence-ahead-of-our-rotational-first-DRQ write. A dropped byte
    // shifts every later byte of the fully-committed sector -> the sporadic
    // multi-disk-RPG save corruption (DEC-0072: 3 coherent-shifted side-1 sectors). The real
    // WD2793 NEVER drops: it lays down EXACTLY 512 in-order bytes, substituting
    // 0x00 + LOST_DATA only for a genuinely UN-SERVICED slot while the position
    // ALWAYS advances (fact-sheet "FDC for Sony HB-F1XV.md" §8; openMSX
    // writeSectorData WD2793.cc:742-782; fMSX WD1793.c:344-370 stores every write
    // unconditionally + advances -- read only, never copied, license isolation).
    // So an early OR late byte lands at data_index_ and the next DRQ is re-based
    // on THIS write time: a full Write Sector is EXACTLY 512 in-order bytes for
    // EVERY cadence (early / late / burst / ISR-perturbed) -- no shift, no drop,
    // no stall. The un-serviced-slot 0x00 substitution lives in sync() (the
    // mid-transfer abandoned-write path) + the first-byte CHECK_WRITE gate;
    // behaviour is identical with and without --fast-disk (fast only scales
    // cycles_per_byte()).
    data_reg_ = value;
    data_reg_fresh_ = true;
    if (phase_ == Phase::WriteSector) {
        commit_write_sector_byte(t, value, /*substituted=*/false);
    } else if (phase_ == Phase::WriteTrack) {
        // Write Track: parse the streamed template byte, then ALWAYS advance the
        // track position (same never-drop decoupling as Write Sector), re-basing
        // the next DRQ on the actual write (openMSX writeTrackData
        // WD2793.cc:1004-1008 resets drqTime each byte).
        parse_write_track_byte(value);
        data_reg_fresh_ = false;
        if (data_available_ > 0) {
            --data_available_;
            drq_deadline_ = t + cycles_per_byte();
            if (data_available_ == 0) {
                end_command(t);
            }
        }
    } else {
        // No active data-accepting write phase: the byte is latched into the data
        // register (readable) but nothing is committed (the CHECK_WRITE gate has
        // already aborted, or no write command is running).
        data_reg_fresh_ = false;
    }
}

void Wd2793::commit_write_sector_byte(const std::uint64_t t, const std::uint8_t value,
                                      const bool substituted) {
    buffer_[static_cast<std::size_t>(data_index_)] = value;
    if (std::FILE* wlog = fdc_write_log()) {
        std::fprintf(wlog, "cmd=%02X lba=%d idx=%d val=%02X sub=%d t=%llu dl=%llu avail=%d\n",
                     command_reg_,
                     static_cast<int>(DiskImage::chs_to_lba(write_track_num_, write_side_,
                                                            write_sector_num_)),
                     static_cast<int>(data_index_), value, substituted ? 1 : 0,
                     static_cast<unsigned long long>(t),
                     static_cast<unsigned long long>(drq_deadline_),
                     static_cast<int>(data_available_));
    }
    if (sector_write_observer_ != nullptr) {
        // Non-perturbing trace: log the committed byte (genuine vs 0x00-
        // substituted), its in-sector index, and the target LBA at the LATCHED
        // CHS. Default null => this whole path is skipped => byte-identical.
        sector_write_observer_->on_write_byte(
            command_reg_, write_track_num_, write_side_, write_sector_num_,
            DiskImage::chs_to_lba(write_track_num_, write_side_, write_sector_num_), data_index_,
            value, substituted, drq_deadline_);
    }
    ++data_index_;
    --data_available_;
    data_reg_fresh_ = false;
    if (substituted) {
        // Un-serviced slot: 0x00 + LOST_DATA, position advances on the FIXED disk
        // cadence (the CPU is not driving it).
        status_ |= kLostData;
        drq_deadline_ += cycles_per_byte();
    } else {
        // Genuine CPU byte: RE-BASE the next DRQ on the actual service time so
        // per-byte drift can never accumulate (drift-free, CPU-paced).
        drq_deadline_ = t + cycles_per_byte();
    }
    if (data_available_ == 0) {
        finish_write_sector(t);
    }
}

void Wd2793::start_type1(const std::uint8_t cmd, const std::uint64_t t) {
    status_ &= ~(kSeekError | kCrcError);
    status_ |= kBusy;
    phase_ = Phase::Type1Seek;
    const std::uint8_t hi = cmd & 0xF0;
    std::uint32_t steps = 0;

    // HLD (head-load): H-flag activates it immediately; without H it is
    // deactivated (WD2793.cc:425-433). Placed before the sub-command switch,
    // matching openMSX's startType1Cmd ordering.
    if (cmd & kHFlag) {
        hld_active_ = true;
        hld_since_ = t;
    } else {
        hld_active_ = false;
    }

    if (drive_ != nullptr) {
        switch (hi) {
            case 0x00: {  // Restore (seek track 0)
                // Fact-sheet §3 "Restore" + §8 "Seek to track 0 edge cases": if !TR00 is
                // already active, TR<-0 immediately; else issue step-out pulses at rate
                // r1r0 until TR00 asserts, capped at 255 pulses; Seek Error is set (only
                // reported when V=1) if TR00 never asserts within the cap (e.g. no/other
                // drive selected, so is_track00() can never observe it).
                track_reg_ = 0xFF;
                data_reg_ = 0x00;
                direction_in_ = false;
                std::uint32_t pulses = 0;
                while (!drive_->is_track00() && pulses < 255) {
                    drive_->step(false);
                    ++pulses;
                }
                steps = pulses;
                if (drive_->is_track00()) {
                    track_reg_ = 0;
                } else if (cmd & kVFlag) {
                    status_ |= kSeekError;
                }
                break;
            }
            case 0x10: {  // Seek
                const int target = data_reg_;
                const int cur = drive_->physical_track();
                const int delta = target - cur;
                direction_in_ = delta > 0;
                steps = static_cast<std::uint32_t>(std::abs(delta));
                int clamped = target;
                if (clamped < 0) clamped = 0;
                if (clamped > static_cast<int>(DiskImage::kTracks - 1)) {
                    clamped = static_cast<int>(DiskImage::kTracks - 1);
                }
                drive_->set_physical_track(static_cast<std::uint8_t>(clamped));
                track_reg_ = static_cast<std::uint8_t>(target);
                break;
            }
            case 0x20:
            case 0x30:  // Step (retain last direction)
                steps = 1;
                if (cmd & kTFlag) {
                    track_reg_ = static_cast<std::uint8_t>(track_reg_ + (direction_in_ ? 1 : -1));
                }
                drive_->step(direction_in_);
                break;
            case 0x40:
            case 0x50:  // Step-In
                direction_in_ = true;
                steps = 1;
                if (cmd & kTFlag) {
                    ++track_reg_;
                }
                drive_->step(true);
                break;
            case 0x60:
            case 0x70:  // Step-Out
                direction_in_ = false;
                steps = 1;
                if (cmd & kTFlag) {
                    --track_reg_;
                }
                drive_->step(false);
                if (drive_->is_track00()) {
                    track_reg_ = 0;
                }
                break;
            default:
                break;
        }
    }

    const std::uint64_t step_rate = step_cycles(cmd);
    busy_until_ = t + static_cast<std::uint64_t>(steps) * step_rate +
                  ((cmd & kVFlag) ? settle_cycles() : 0);
}

void Wd2793::start_type2(const std::uint8_t cmd, const std::uint64_t t) {
    status_ &= ~(kLostData | kRecordNotFound | kRecordType | kWriteProtected);
    status_ |= kBusy;

    if (drive_ == nullptr || !drive_->ready()) {
        end_command(t);
        return;
    }
    // Type II activates HLD unconditionally once the command is accepted
    // (WD2793.cc:522-533 startType2Cmd -- "see comment in startType1Cmd").
    hld_active_ = true;
    hld_since_ = t;

    const bool write = (cmd & 0xE0) == 0xA0;
    // Locate the ID: the requested sector must exist on the current physical
    // track (fact-sheet §3 "Read Sector"; RECORD_NOT_FOUND otherwise).
    const bool found = drive_->physical_track() == track_reg_ && sector_reg_ >= 1 &&
                       sector_reg_ <= DiskImage::kSectorsPerTrack;
    if (!found) {
        status_ |= kRecordNotFound;
        end_command(t);
        return;
    }

    if (write) {
        if (drive_->write_protected()) {
            status_ |= kWriteProtected;
            end_command(t);
            return;
        }
        begin_write_sector(t);
    } else {
        ++read_sector_commands_accepted_;
        begin_read_sector(t);
    }
}

void Wd2793::begin_read_sector(const std::uint64_t t) {
    buffer_.assign(DiskImage::kSectorSize, 0);
    if (!drive_->read_sector(sector_reg_, buffer_.data())) {
        status_ |= kRecordNotFound;
        end_command(t);
        return;
    }
    // Stream-capture (DEC-0052): notify the (non-perturbing) observer of the
    // COMPLETED sector read -- the 512 bytes are now in buffer_ and no DRQ
    // transfer has begun. Default null => skipped => byte-for-byte identical FDC
    // behaviour + timing (nothing below reads the observer's result). track/side
    // come from the drive's current head position; the observer only inspects.
    if (sector_read_observer_ != nullptr) {
        sector_read_observer_->on_sector_read(command_reg_, drive_->physical_track(),
                                              drive_->side(), sector_reg_, buffer_.data(),
                                              buffer_.size());
    }
    data_index_ = 0;
    data_available_ = static_cast<int>(DiskImage::kSectorSize);
    // Index-pulse-relative first DRQ (DEC-0055 / DEC-0053).
    // Real hardware/openMSX must wait for the requested sector to rotate under
    // the head before the data stream begins, so the first-DRQ latency is
    // VARIABLE (0..~1 rotation) -- NOT the old fixed 2-byte kReadStartCycles
    // (which modelled ZERO rotational latency). Faithful to openMSX
    // WD2793.cc:544/557/624 (type2Loaded -> type2Search[getNextSector] ->
    // startReadSector): the drive returns the rotational wait until the
    // sector's ID mark arrives, then the fixed ID-header -> first-data span is
    // added. sector_reg_ is validated 1..9 by the caller (start_type2 /
    // finish_read_sector multi), so sector_reg_ - 1 is a valid 0-based index.
    // Inter-byte cadence (kCyclesPerByte) is unchanged -- only the FIRST DRQ
    // moves.
    const std::uint64_t rotational_wait = drive_->cycles_until_sector_id(sector_reg_ - 1u, t);
    drq_deadline_ = t + rotational_wait + read_sector_header_cycles();
    phase_ = Phase::ReadSector;
    status_ |= kBusy;
}

void Wd2793::begin_write_sector(const std::uint64_t t) {
    write_sector_num_ = sector_reg_;
    // LATCH the target (track, side) at command START (DEF-M47-DISKWRITE) -- the
    // moment the sector's address mark is under the head. The head cannot move
    // and the side cannot be re-selected to a DIFFERENT physical sector while the
    // command is BUSY, so finish_write_sector commits to THESE latched
    // coordinates -- NOT the live drive->physical_track()/side(), which a
    // mid-transfer glue-register write (Sony 0x7FFC side latch / a seek) could
    // have changed and thereby redirected the committed sector (all 3 observed
    // corrupt sectors were side 1). Mirrors the read path capturing at START
    // (begin_read_sector reads the drive position before the DRQ stream).
    write_track_num_ = drive_->physical_track();
    write_side_ = drive_->side();
    // Pre-size the transfer buffer. Every slot is overwritten by a genuine CPU
    // byte before finish_write_sector runs (data_available_ only reaches 0 after
    // 512 real commits), so the pre-fill can no longer leak zeros to disk -- and
    // finish_write_sector is unreachable unless the full sector was supplied.
    buffer_.assign(DiskImage::kSectorSize, 0);
    data_index_ = 0;
    data_available_ = static_cast<int>(DiskImage::kSectorSize);
    // First data-byte DRQ is asserted only AFTER the rotational sector-search --
    // the SAME variable latency the read path uses (begin_read_sector). On a real
    // WD2793 / openMSX the write DRQ + CHECK_WRITE are scheduled from the moment
    // the sector's address mark rotates under the head (startWriteSector,
    // openMSX 21.0: src/fdc/WD2793.cc:646-672, reached via
    // type2Search/getNextSector's post-search time), NOT a fixed offset from
    // command issue. An earlier fix wrongly used the fixed 2-byte read_start_cycles()
    // here (zero rotational latency), so DRQ + the CHECK_WRITE window fired ~1140
    // cycles after the command -- ABORTING a valid game (e.g. a multi-disk RPG
    // title) whose in-game
    // save does buffer setup before writing byte 1. sector_reg_ is validated 1..9
    // by the caller (start_type2 / finish_write_sector multi), so sector_reg_ - 1
    // is a valid 0-based sector index (mirrors begin_read_sector). Fast-disk
    // collapses the rotational wait + header via the same selectors as the read
    // path (byte-identical accurate-mode timing when off).
    const std::uint64_t rotational_wait = drive_->cycles_until_sector_id(sector_reg_ - 1u, t);
    drq_deadline_ = t + rotational_wait + read_sector_header_cycles();
    // First-byte CHECK_WRITE gate (openMSX checkStartWrite, WD2793.cc:674-701):
    // if the CPU has not supplied the first data byte by this deadline, sync()
    // aborts with LOST_DATA and writes NOTHING. openMSX schedules its CHECK_WRITE
    // only 8 byte-periods after the DRQ -- but there the CPU has ALREADY finished
    // its setup DURING the (long) rotational search and is polling DRQ when it
    // asserts (setDataReg latches the first byte only while DRQ is up, WD2793.cc:
    // 235-247), so 8 byte-periods suffices. Our model likewise gates writes on
    // transfer_drq, but the rotational wait can be small (unlucky sector angle) or
    // collapsed to ~0 by --fast-disk while the CPU still runs real-time, so an
    // 8-byte tail would abort a valid slow-first-byte write. We therefore give the
    // first byte a FULL further disk revolution after the DRQ -- the natural disk
    // timescale, far longer than any real save-buffer setup, so a valid write is
    // NEVER aborted, while a genuinely-absent first byte still aborts
    // deterministically. Re-armed per sector (multi-record write re-enters here).
    write_check_deadline_ = drq_deadline_ + kWriteFirstByteWindowCycles;
    phase_ = Phase::WriteSector;
    status_ |= kBusy;
}

void Wd2793::finish_read_sector(const std::uint64_t t) {
    // Data is always correct in the flat image -> CRC ok (WD2793.cc:275-278).
    status_ &= ~kCrcError;
    if ((command_reg_ & kMFlag) != 0) {
        // Multi-sector read: auto-increment SR and continue until off the track
        // (fact-sheet §8; WD2793.cc:287-294).
        ++sector_reg_;
        if (sector_reg_ >= 1 && sector_reg_ <= DiskImage::kSectorsPerTrack) {
            begin_read_sector(t);
            return;
        }
    }
    if ((status_ & (kCrcError | kRecordNotFound | kLostData)) == 0) {
        // Genuine end of the (possibly multi-record) Read Sector command, no
        // error bits set (the boot-checkpoint acceptance signal).
        ++read_sector_completions_ok_;
    }
    end_command(t);
}

void Wd2793::finish_write_sector(const std::uint64_t t) {
    // Commit to the LATCHED (track, side) captured at
    // begin_write_sector, NOT the live drive position -- so a mid-transfer side/
    // track change cannot land the sector on the wrong CHS. (DEF-M47-DISKWRITE)
    drive_->write_sector_at(write_track_num_, write_side_, write_sector_num_, buffer_.data());
    if (sector_write_observer_ != nullptr) {
        // Non-perturbing finish trace: the assembled sector + its CRC at the
        // latched CHS/LBA. Default null => skipped => byte-identical.
        sector_write_observer_->on_sector_write(
            command_reg_, write_track_num_, write_side_, write_sector_num_,
            DiskImage::chs_to_lba(write_track_num_, write_side_, write_sector_num_), buffer_.data(),
            buffer_.size(), crc16(buffer_.data(), buffer_.size()));
    }
    if ((command_reg_ & kMFlag) != 0) {
        ++sector_reg_;
        if (sector_reg_ >= 1 && sector_reg_ <= DiskImage::kSectorsPerTrack) {
            begin_write_sector(t);
            return;
        }
    }
    end_command(t);
}

void Wd2793::start_type3(const std::uint8_t cmd, const std::uint64_t t) {
    status_ &= ~(kLostData | kRecordNotFound | kRecordType);
    status_ |= kBusy;
    if (drive_ == nullptr || !drive_->ready()) {
        end_command(t);
        return;
    }
    // Type III activates HLD unconditionally once the command is accepted
    // (WD2793.cc:820-834 startType3Cmd -- "see comment in startType1Cmd").
    hld_active_ = true;
    hld_since_ = t;
    const std::uint8_t hi = cmd & 0xF0;
    if (hi == 0xC0) {  // Read Address
        build_read_address_buffer();
        data_index_ = 0;
        data_available_ = 6;
        drq_deadline_ = t + read_start_cycles();
        phase_ = Phase::ReadAddress;
    } else if (hi == 0xE0) {  // Read Track
        build_read_track_buffer();
        data_index_ = 0;
        data_available_ = static_cast<int>(buffer_.size());
        drq_deadline_ = t + read_start_cycles();
        phase_ = Phase::ReadTrack;
    } else {  // 0xF0 Write Track (format)
        if (drive_->write_protected()) {
            status_ |= kWriteProtected;
            end_command(t);
            return;
        }
        // Latch (track, side) at command START (DEF-M47-DISKWRITE), as Write
        // Sector does -- the formatted sectors commit to these coordinates via
        // write_sector_at (parse_write_track_byte), so a mid-format side/track
        // change cannot redirect them. The head stays on the physical track for
        // the whole revolution while BUSY.
        write_track_num_ = drive_->physical_track();
        write_side_ = drive_->side();
        wt_a1_run_ = 0;
        wt_expect_id_ = false;
        wt_in_data_ = false;
        wt_data_.assign(DiskImage::kSectorSize, 0);
        data_index_ = 0;
        data_available_ = kStandardTrackLength;
        drq_deadline_ = t + read_start_cycles();
        phase_ = Phase::WriteTrack;
    }
    (void)cmd;
}

void Wd2793::build_read_address_buffer() {
    // 6 ID bytes: Track, Side, Sector, Length(=2 for 512), CRC1, CRC2. Track is
    // copied into SR (WD2793.cc:868-870; fact-sheet §3 "Read Address").
    const std::uint8_t track = drive_->physical_track();
    const std::uint8_t side = drive_->side();
    const std::uint8_t sector = 1;
    const std::uint8_t n = 0x02;
    const std::uint8_t id[8] = {0xA1, 0xA1, 0xA1, 0xFE, track, side, sector, n};
    const std::uint16_t crc = crc16(id, 8);
    buffer_.assign({track, side, sector, n, static_cast<std::uint8_t>(crc >> 8),
                    static_cast<std::uint8_t>(crc & 0xFF)});
    sector_reg_ = track;
}

void Wd2793::build_read_track_buffer() {
    // Build a standard IBM System 34 MFM 9-sector track image (fact-sheet §5),
    // interleaving the live sector data from the mounted image. Read Track returns
    // the raw logical track stream.
    buffer_.clear();
    buffer_.reserve(kStandardTrackLength);
    auto put = [&](std::uint8_t v, int n) {
        for (int i = 0; i < n; ++i) buffer_.push_back(v);
    };
    put(0x4E, 80);              // Gap4a
    put(0x00, 12);              // Sync
    put(0xC2, 3); put(0xFC, 1); // IAM
    put(0x4E, 50);              // Gap1
    const std::uint8_t track = drive_->physical_track();
    const std::uint8_t side = drive_->side();
    std::uint8_t sector_data[DiskImage::kSectorSize];
    for (std::uint8_t s = 1; s <= DiskImage::kSectorsPerTrack; ++s) {
        put(0x00, 12);                              // Sync
        put(0xA1, 3); put(0xFE, 1);                 // IDAM
        buffer_.push_back(track);
        buffer_.push_back(side);
        buffer_.push_back(s);
        buffer_.push_back(0x02);
        put(0x4E, 2);                               // (CRC placeholder region)
        put(0x4E, 22);                              // Gap2
        put(0x00, 12);                              // Sync
        put(0xA1, 3); put(0xFB, 1);                 // DAM
        if (!drive_->read_sector(s, sector_data)) {
            for (auto& b : sector_data) b = 0x00;
        }
        for (std::uint32_t j = 0; j < DiskImage::kSectorSize; ++j) {
            buffer_.push_back(sector_data[j]);
        }
        put(0x4E, 2);                               // (CRC placeholder region)
        put(0x4E, 82);                              // Gap3
    }
    while (static_cast<int>(buffer_.size()) < kStandardTrackLength) {
        buffer_.push_back(0x4E);                    // Gap4b
    }
    buffer_.resize(kStandardTrackLength);
}

void Wd2793::parse_write_track_byte(const std::uint8_t value) {
    // Parse the streamed track template to reformat a standard 9-sector track
    // (fact-sheet §5 "How the WD2793 constructs this"). 0xF5 -> A1 sync mark;
    // 0xFE after A1 = IDAM (C,H,R,N follow); 0xFB/0xF8 after A1 = DAM (512 data +
    // 0xF7 CRC follow). Arbitrary flux fidelity is deliberately not modelled.
    if (value == 0xF5) {
        ++wt_a1_run_;
        return;
    }
    const bool after_a1 = wt_a1_run_ > 0;
    wt_a1_run_ = 0;
    if (after_a1 && value == 0xFE) {
        wt_expect_id_ = true;
        wt_id_count_ = 0;
    } else if (after_a1 && (value == 0xFB || value == 0xF8)) {
        wt_in_data_ = true;
        wt_data_count_ = 0;
        wt_data_.assign(DiskImage::kSectorSize, 0);
    } else if (wt_expect_id_) {
        if (wt_id_count_ < 4) {
            wt_id_[wt_id_count_] = value;
        }
        ++wt_id_count_;
        if (wt_id_count_ == 4) {
            wt_sector_num_ = wt_id_[2];  // R = sector number
            wt_expect_id_ = false;
        }
    } else if (wt_in_data_) {
        if (value == 0xF7 && wt_data_count_ >= static_cast<int>(DiskImage::kSectorSize)) {
            if (drive_ != nullptr) {
                // Commit to the LATCHED (track, side) captured at
                // start_type3, not the live drive position. (DEF-M47-DISKWRITE)
                drive_->write_sector_at(write_track_num_, write_side_, wt_sector_num_,
                                        wt_data_.data());
            }
            wt_in_data_ = false;
        } else if (wt_data_count_ < static_cast<int>(DiskImage::kSectorSize)) {
            wt_data_[static_cast<std::size_t>(wt_data_count_)] = value;
            ++wt_data_count_;
        } else {
            ++wt_data_count_;
        }
    }
}

void Wd2793::start_type4(const std::uint8_t cmd, const std::uint64_t t) {
    // Force Interrupt (WD2793.cc:1035-1060): terminate any running command, reset
    // Busy. i3 (0x08) = immediate INTRQ; i2 (0x04) arms INTRQ at the drive's NEXT
    // index pulse (WD2793.cc:1049-1050 getTimeTillIndexPulse — NOT immediate);
    // flags==0 -> terminate with no INTRQ (unless one is already pending/armed).
    const std::uint8_t flags = cmd & 0x0F;
    if (flags == 0x00) {
        immediate_irq_ = false;
    }
    if ((flags & kIdxIrq) != 0 && drive_ != nullptr && drive_->ready()) {
        index_irq_armed_ = true;
        index_irq_deadline_ = t + drive_->cycles_until_index_pulse(t);
    }
    if (flags & kImmIrq) {
        immediate_irq_ = true;
        intrq_pending_ = true;
    }
    phase_ = Phase::Idle;
    data_available_ = 0;
    status_ &= ~kBusy;
}

void Wd2793::end_command(const std::uint64_t t) {
    if (hld_active_ && t >= hld_since_ && t < hld_since_ + kHldIdleCycles) {
        // HLD was active: restart its idle-timeout window from the command's
        // completion time (WD2793.cc:1062-1073 endCmd).
        hld_since_ = t;
    }
    status_ &= ~kBusy;
    intrq_pending_ = true;
    phase_ = Phase::Idle;
    data_available_ = 0;
}

}  // namespace sony_msx::devices::fdc
