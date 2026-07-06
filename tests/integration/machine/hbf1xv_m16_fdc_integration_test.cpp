// Suite: Machine_Hbf1xvM16Fdc_Integration  (M16-S5)
//
// Machine-level wiring of the Sony WD2793 FDC: SonyFdc (wrapping disk_rom_) is
// attached at slot (3,2,1), replacing the bare DISK ROM (planner §3.1); the FDC
// core and DiskDrive advance off the deterministic FdcClock (scheduler cycles).
//
// Cases:
//   1. A real CPU program running over the full M11 bus (SlotBus/IoBus) selects
//      slot 3-2 page 1 (the FDC) at page 1 while continuing to execute from page
//      3 RAM, issues a Type I Restore, then a Type II Read Sector of LBA 0, and
//      stores the 512 streamed bytes into RAM; the bytes match the mounted
//      deterministic DiskImage.
//   2. Motor-on (0x7FFD bit7) then motor-off: the drive stays "on" until the ~4 s
//      (14,318,180-cycle) deterministic delayed-motor-off deadline elapses.
//   3. DSKCHG (0x7FFD bit2) is readable through the bus and reflects the drive's
//      disk-changed latch.
//
// (Zero-regression across the full M1-M15 suite is confirmed separately by the
// full `ctest` run captured in the M16 implementation report, not re-asserted
// here.)

#include <array>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <vector>

#include "machine/hbf1xv_machine.h"

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// Tiny self-contained Z80 assembler helper: emits raw bytes and back-patches
// JR-relative (0x18/0x20/0x28/0x30 family) displacements against BYTE OFFSETS
// into the program vector (the CPU base address cancels out of the relative
// displacement, so working in offsets is exact and avoids manual arithmetic).
class Prog {
public:
    void emit(std::initializer_list<std::uint8_t> bytes) {
        for (const std::uint8_t b : bytes) {
            bytes_.push_back(b);
        }
    }

    [[nodiscard]] std::size_t here() const { return bytes_.size(); }

    // Emits `opcode, <displacement>` where the displacement targets `target`
    // (a previously-recorded offset, i.e. a backward jump).
    void emit_jr_back(const std::uint8_t opcode, const std::size_t target) {
        bytes_.push_back(opcode);
        const std::size_t next_pc = bytes_.size() + 1;
        const int disp = static_cast<int>(target) - static_cast<int>(next_pc);
        bytes_.push_back(static_cast<std::uint8_t>(disp & 0xFF));
    }

    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const { return bytes_; }

private:
    std::vector<std::uint8_t> bytes_;
};

constexpr std::uint16_t kBase = 0xC000;
constexpr std::uint16_t kBuffer = 0xC200;  // 512-byte landing zone, page-3 RAM

// Assembles the Restore + Read-Sector(LBA0) probe:
//   - select page 1 -> slot 3-2 (FDC) via the #FFFF sub-slot register, keeping
//     page 3 (running code) at slot 3-0 sub 0 (RAM);
//   - side=0, drive-select=A + motor on (0x7FFD);
//   - Restore (Type I), busy-poll 0x7FF8 bit0;
//   - TR=0, SR=1, Read Sector (Type II);
//   - poll DTRQ (0x7FFF bit7, active-low) and stream 512 bytes via 0x7FFB into
//     (HL)+, HALT when done.
std::vector<std::uint8_t> build_restore_read_sector_probe() {
    Prog p;
    p.emit({0x3E, 0x08});        // LD A,0x08
    p.emit({0x32, 0xFF, 0xFF});  // LD (0xFFFF),A  ; page1->slot3-2(FDC), page3 stays sub0
    p.emit({0x3E, 0x00});        // LD A,0x00
    p.emit({0x32, 0xFC, 0x7F});  // LD (0x7FFC),A  ; side = 0
    p.emit({0x3E, 0x80});        // LD A,0x80
    p.emit({0x32, 0xFD, 0x7F});  // LD (0x7FFD),A  ; drive-select=A, motor on
    p.emit({0x3E, 0x00});        // LD A,0x00
    p.emit({0x32, 0xF8, 0x7F});  // LD (0x7FF8),A  ; Restore (Type I)
    const std::size_t restore_wait = p.here();
    p.emit({0x3A, 0xF8, 0x7F});  // LD A,(0x7FF8)  ; status
    p.emit({0xE6, 0x01});        // AND 0x01       ; Busy
    p.emit_jr_back(0x20, restore_wait);  // JR NZ, restore_wait
    p.emit({0x3E, 0x00});        // LD A,0x00
    p.emit({0x32, 0xF9, 0x7F});  // LD (0x7FF9),A  ; TR = 0
    p.emit({0x3E, 0x01});        // LD A,0x01
    p.emit({0x32, 0xFA, 0x7F});  // LD (0x7FFA),A  ; SR = 1
    p.emit({0x3E, 0x80});        // LD A,0x80
    p.emit({0x32, 0xF8, 0x7F});  // LD (0x7FF8),A  ; Read Sector (Type II)
    p.emit({0x21, static_cast<std::uint8_t>(kBuffer & 0xFF),
             static_cast<std::uint8_t>(kBuffer >> 8)});  // LD HL,kBuffer
    p.emit({0x01, 0x00, 0x02});  // LD BC,0x0200   ; 512
    const std::size_t read_wait = p.here();
    p.emit({0x3A, 0xFF, 0x7F});  // LD A,(0x7FFF)  ; interface status
    p.emit({0xE6, 0x80});        // AND 0x80       ; !DTRQ (active-low)
    p.emit_jr_back(0x20, read_wait);  // JR NZ, read_wait (wait for DRQ)
    p.emit({0x3A, 0xFB, 0x7F});  // LD A,(0x7FFB)  ; data register
    p.emit({0x77});              // LD (HL),A
    p.emit({0x23});              // INC HL
    p.emit({0x0B});               // DEC BC
    p.emit({0x78});               // LD A,B
    p.emit({0xB1});               // OR C
    p.emit_jr_back(0x20, read_wait);  // JR NZ, read_wait (next byte)
    p.emit({0x76});               // HALT
    return p.bytes();
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;

    // --- CPU program: Restore + Read Sector(LBA0) over the full M11 bus. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();

        const std::vector<std::uint8_t> program = build_restore_read_sector_probe();
        expect(program.size() < (kBuffer - kBase), "Probe_FitsBeforeBuffer");
        machine.load_memory(kBase, program.data(), static_cast<std::uint32_t>(program.size()));
        machine.cpu().state().regs().pc = kBase;

        int guard = 0;
        while (!machine.cpu().state().halted() && guard < 2'000'000) {
            machine.step_cpu_instruction();
            ++guard;
        }
        expect(machine.cpu().state().halted(), "Probe_ReachesHalt");

        std::array<std::uint8_t, 512> expected{};
        expect(machine.disk_image().read_chs(0, 0, 1, expected.data()),
               "Probe_ImageReadChs_Track0Side0Sector1_Succeeds");
        bool identical = true;
        for (std::size_t i = 0; i < expected.size(); ++i) {
            if (machine.read_memory(static_cast<std::uint16_t>(kBuffer + i)) != expected[i]) {
                identical = false;
                break;
            }
        }
        expect(identical, "Probe_512BytesInRam_MatchMountedImage");
        expect(machine.fdc().intrq(), "Probe_FdcIntrq_SetAfterReadSectorCompletes");
    }

    // --- Motor-on then delayed motor-off (~4 s / 14,318,180 cycles). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.debug_io_write(0xA8, 0xFF);      // all pages -> primary slot 3
        machine.debug_bus_write(0xFFFF, 0x08);   // page1 -> sub2 (FDC), page3 stays sub0

        machine.debug_bus_write(0x7FFD, 0x80);   // motor on, drive A
        expect(machine.disk_drive().motor_on(machine.elapsed_cycles()),
               "Motor_OnImmediatelyAfterWrite");

        machine.debug_bus_write(0x7FFD, 0x00);   // motor off request (drive A still selected)
        expect(machine.disk_drive().motor_on(machine.elapsed_cycles()),
               "Motor_StaysOnRightAfterOffRequest_DelayedTimerArmed");

        using sony_msx::devices::fdc::DiskDrive;
        machine.run_cycles(DiskDrive::kMotorOffCycles - 1);
        expect(machine.disk_drive().motor_on(machine.elapsed_cycles()),
               "Motor_StillOn_JustBeforeDeadline");

        machine.run_cycles(2);
        expect(!machine.disk_drive().motor_on(machine.elapsed_cycles()),
               "Motor_OffAfterDeadlineElapses");
    }

    // --- DSKCHG (0x7FFD bit2) readable through the bus (active-low: 0=changed). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.debug_io_write(0xA8, 0xFF);
        machine.debug_bus_write(0xFFFF, 0x08);

        expect((machine.debug_bus_read(0x7FFD) & 0x04) != 0,
               "Dskchg_NotChanged_BitSet_OverBus");
        machine.disk_drive().set_disk_changed(true);
        expect((machine.debug_bus_read(0x7FFD) & 0x04) == 0,
               "Dskchg_Changed_BitClear_ActiveLow_OverBus");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
