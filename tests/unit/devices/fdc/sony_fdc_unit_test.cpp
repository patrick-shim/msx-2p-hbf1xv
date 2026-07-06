// Suite: Devices_Fdc_SonyFdc_Unit  (M16-S4)
//
// Sony connection-style memory decode: SonyFdc wraps the DISK-ROM RomDevice and
// decodes 0x7FF8-0x7FFF per the AUTHORITATIVE openMSX PhilipsFDC.cc table (NOT
// the fact-sheet's inferred convention - planner §3.2 / R-M16-1). Verifies each
// bit position explicitly, including the two corrections vs the fact-sheet's
// guess: DSKCHG lives at 0x7FFD bit2 (not 0x7FFF), and 0x7FFF bit6/bit7 are
// ACTIVE-LOW INTRQ/DTRQ (not active-high).
//
// Grounding (read only, never copied - GPL isolation):
// references/openmsx-21.0/src/fdc/PhilipsFDC.cc:24-172 (readMem/peekMem/writeMem,
// reset :17-22).

#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/fdc/disk_drive.h"
#include "devices/fdc/disk_image.h"
#include "devices/fdc/fdc_clock_source.h"
#include "devices/fdc/sony_fdc.h"
#include "devices/fdc/wd2793.h"
#include "devices/memory/rom_device.h"

namespace {

using sony_msx::devices::fdc::DiskDrive;
using sony_msx::devices::fdc::DiskImage;
using sony_msx::devices::fdc::FdcClockSource;
using sony_msx::devices::fdc::SonyFdc;
using sony_msx::devices::fdc::Wd2793;
using sony_msx::devices::memory::RomDevice;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

class FakeClock final : public FdcClockSource {
public:
    [[nodiscard]] std::uint64_t cpu_cycles() const override { return cycles; }
    std::uint64_t cycles = 0;
};

std::vector<std::uint8_t> rom_pattern() {
    std::vector<std::uint8_t> bytes(0x4000);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<std::uint8_t>((i * 5 + 3) & 0xFFu);
    }
    return bytes;
}

struct Fixture {
    FakeClock clock;
    DiskImage image;
    DiskDrive drive;
    Wd2793 fdc;
    RomDevice rom{0x4000, 0x4000, rom_pattern()};
    SonyFdc sony_fdc{rom, fdc, drive, clock};

    Fixture() {
        drive.attach_image(&image);
        fdc.attach_clock_source(&clock);
        fdc.attach_drive(&drive);
        fdc.reset();
        drive.reset();
        drive.attach_image(&image);
        sony_fdc.reset();
    }
};

}  // namespace

int main() {
    // --- 0x4000 reads the DISK ROM image (not a register). ---
    {
        Fixture f;
        const std::vector<std::uint8_t> pattern = rom_pattern();
        expect(f.sony_fdc.mem_read(0x4000) == pattern[0], "Read0x4000_DiskRomByte0");
        expect(f.sony_fdc.mem_read(0x5000) == pattern[0x1000], "Read0x5000_DiskRomByteMidWindow");
    }

    // --- 0x7FF7 (last byte before the register window) reads ROM. ---
    {
        Fixture f;
        const std::vector<std::uint8_t> pattern = rom_pattern();
        expect(f.sony_fdc.mem_read(0x7FF7) == pattern[0x3FF7], "Read0x7FF7_DiskRomByte");
    }

    // --- 0x7FF8 read = WD2793 status; write = WD2793 command (reaches the core). ---
    {
        Fixture f;
        expect(f.sony_fdc.mem_read(0x7FF9) == 0xFF,
               "PreCommand_TrackRegisterOver0x7FF9_ResetValue0xFF");
        // Issue a Restore (drive already at track 0) through the register window.
        // The observable, timing-independent effect (Restore forces TR<-0 as soon
        // as TR00 is sensed) proves the write reached the WD2793 core.
        f.sony_fdc.mem_write(0x7FF8, 0x00);
        expect(f.sony_fdc.mem_read(0x7FF9) == 0,
               "Write0x7FF8_Restore_ReachesWd2793Core_TrackBecomesZero");
    }

    // --- Data register round-trips through 0x7FFB for a Read Sector. ---
    {
        Fixture f;
        std::uint8_t expected[DiskImage::kSectorSize] = {};
        f.image.read_chs(0, 0, 1, expected);
        f.sony_fdc.mem_write(0x7FF9, 0);  // TR = 0
        f.sony_fdc.mem_write(0x7FFA, 1);  // SR = 1
        f.sony_fdc.mem_write(0x7FF8, 0x80);  // Read Sector
        int guard = 0;
        while (!f.fdc.drq() && guard < 4'000'000) {
            ++f.clock.cycles;
            ++guard;
        }
        expect(f.sony_fdc.mem_read(0x7FFB) == expected[0],
               "Read0x7FFB_DataRegister_FirstByteMatchesImage");
    }

    // --- Side-select latch write is observable via 0x7FFC read (and DiskDrive). ---
    {
        Fixture f;
        f.sony_fdc.mem_write(0x7FFC, 0x01);
        expect(f.sony_fdc.mem_read(0x7FFC) == 0x01, "Read0x7FFC_SideRegister_ReflectsWrite");
        expect(f.drive.side() == 1, "Write0x7FFC_SideSelect_ReachesDiskDrive");
        expect(f.sony_fdc.side_register() == 0x01, "SideRegisterAccessor_MatchesLatch");
    }

    // --- Drive/motor latch write (0x7FFD) is observable; motor-on reaches DiskDrive. ---
    {
        Fixture f;
        f.sony_fdc.mem_write(0x7FFD, 0x80);  // motor on, drive select bits = 00 (drive A)
        expect(f.sony_fdc.drive_register() == 0x80, "DriveRegisterAccessor_MatchesLatch");
        expect(f.drive.motor_on(f.clock.cycles), "Write0x7FFD_MotorOn_ReachesDiskDrive");
        expect(f.drive.available(), "Write0x7FFD_DriveSelectA_DriveAvailable");

        f.sony_fdc.mem_write(0x7FFD, 0x03);  // drive-select bits=11 -> NONE, motor off
        expect(!f.drive.available(), "Write0x7FFD_DriveSelectNone_DriveUnavailable");
    }

    // --- DSKCHG at 0x7FFD bit2 (active-low: 0 = changed), NOT at 0x7FFF. ---
    {
        Fixture f;
        f.sony_fdc.mem_write(0x7FFD, 0x00);
        expect((f.sony_fdc.mem_read(0x7FFD) & 0x04) != 0,
               "Read0x7FFD_Bit2_SetWhenNotChanged");
        f.drive.set_disk_changed(true);
        expect((f.sony_fdc.mem_read(0x7FFD) & 0x04) == 0,
               "Read0x7FFD_Bit2_ClearWhenChanged_ActiveLow");
        f.drive.set_disk_changed(false);
        expect((f.sony_fdc.mem_read(0x7FFD) & 0x04) != 0,
               "Read0x7FFD_Bit2_RestoresWhenAcknowledged");
    }

    // --- 0x7FFE is unused: always reads 0xFF. ---
    {
        Fixture f;
        expect(f.sony_fdc.mem_read(0x7FFE) == 0xFF, "Read0x7FFE_Unused_Always0xFF");
        f.sony_fdc.mem_write(0x7FFE, 0x00);  // no-op per the decode table
        expect(f.sony_fdc.mem_read(0x7FFE) == 0xFF, "Read0x7FFE_UnusedAfterWrite_Still0xFF");
    }

    // --- 0x7FFF: bit6 = !INTRQ, bit7 = !DTRQ (ACTIVE-LOW), all other bits pulled
    //     to 1 - the fact-sheet's "active-high" convention would be WRONG here. ---
    {
        Fixture f;
        // Idle: neither INTRQ nor DRQ asserted -> 0xFF.
        expect(f.sony_fdc.mem_read(0x7FFF) == 0xFF, "Read0x7FFF_Idle_AllOnes");

        // Force Interrupt with immediate IRQ (i3) sets INTRQ -> bit6 clears.
        f.sony_fdc.mem_write(0x7FF8, 0xD8);
        const std::uint8_t after_fi = f.sony_fdc.mem_read(0x7FFF);
        expect((after_fi & 0x40) == 0, "Read0x7FFF_Bit6_ClearsWhenIntrqAsserted_ActiveLow");
        expect((after_fi & 0x80) != 0, "Read0x7FFF_Bit7_StaysSetWhenNoDtrq");

        // Start a Read Sector: once DRQ is asserted, bit7 clears (DTRQ active).
        Fixture f2;
        f2.sony_fdc.mem_write(0x7FF9, 0);
        f2.sony_fdc.mem_write(0x7FFA, 1);
        f2.sony_fdc.mem_write(0x7FF8, 0x80);
        int guard = 0;
        while (!f2.fdc.drq() && guard < 4'000'000) {
            ++f2.clock.cycles;
            ++guard;
        }
        const std::uint8_t during_drq = f2.sony_fdc.mem_read(0x7FFF);
        expect((during_drq & 0x80) == 0, "Read0x7FFF_Bit7_ClearsWhenDtrqAsserted_ActiveLow");
    }

    // --- ROM writes elsewhere in page 1 are ignored (mask-ROM semantics). ---
    {
        Fixture f;
        const std::vector<std::uint8_t> pattern = rom_pattern();
        f.sony_fdc.mem_write(0x4000, 0xAA);
        expect(f.sony_fdc.mem_read(0x4000) == pattern[0], "Write0x4000_RomWrite_Ignored");
    }

    // --- Reset writes 0x3FFC=0 and 0x3FFD=0 (PhilipsFDC.cc:17-22): side 0,
    //     drive/motor cleared. ---
    {
        Fixture f;
        f.sony_fdc.mem_write(0x7FFC, 0x01);
        f.sony_fdc.mem_write(0x7FFD, 0x83);
        f.sony_fdc.reset();
        expect(f.sony_fdc.mem_read(0x7FFC) == 0x00, "Reset_SideRegister_Cleared");
        expect((f.sony_fdc.mem_read(0x7FFD) & ~0x04) == 0x00, "Reset_DriveRegister_Cleared");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
