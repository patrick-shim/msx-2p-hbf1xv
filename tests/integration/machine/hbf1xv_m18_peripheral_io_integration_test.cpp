// Suite: Machine_Hbf1xvM18PeripheralIo_Integration  (M18-S4, backlog B5/C7)
//
// System-integration coverage for all three new M18 peripheral I/O devices
// wired onto the M11 SystemBus/IoBus:
//   1. A real CPU program drives OUT (#D8),A / OUT (#D9),A / IN A,(#D9) (and
//      the #DA/#DB JIS2-half equivalents) over the bus and reads back REAL
//      bios/f1xvkfn.rom content at a handful of known offsets (byte-exact,
//      sourced directly from the checksummed file -- never fabricated).
//   2. A CPU program drives the printer write protocol over the bus;
//      machine.printer().captured_bytes() reflects it.
//   3. PPI port C bit4/bit5 patterns drive machine.cassette()'s live
//      derivation; the cassette-input default (no synthetic tape loaded)
//      still gives PSG R14 bit7 = 1 exactly as pre-M18 (R-M18-5 regression
//      guard), even though joystick_ IS wired to cassette_ at the machine
//      level (attach_cassette_input_source is no longer nullptr -- the
//      DEFAULT observable behavior is what must stay unchanged).
//
// Zero-regression across the full M1-M17 suite (in particular the M15
// JoystickPorts/Ppi8255 goldens and the M9/M12 CPU-timing oracles) is
// confirmed separately by the full `ctest` run captured in the M18
// implementation report, not re-asserted here.

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <istream>
#include <iterator>
#include <string>
#include <tuple>
#include <vector>

#include "machine/hbf1xv_machine.h"

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build"
#endif

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
}

// One Kanji-device read probe: select `addr` (block-aligned, low 5 bits
// irrelevant -- both writes clear them per A-M18-3) via the JIS1 (#D8/#D9)
// or JIS2 (#DA/#DB) port pair, then sequentially read `count` bytes into RAM
// starting at `dest`.
struct KanjiProbe {
    std::uint32_t addr;
    bool jis2;
    int count;
    std::uint16_t dest;
};

// Assembles: LD A,lo ; OUT (port_lo),A ; LD A,hi ; OUT (port_hi),A ; then
// `count` times: IN A,(port_read) ; LD (dest+i),A. HALT at the very end.
std::vector<std::uint8_t> build_kanji_probe(const std::vector<KanjiProbe>& probes) {
    std::vector<std::uint8_t> program;
    for (const auto& p : probes) {
        const std::uint8_t port_lo = p.jis2 ? 0xDA : 0xD8;
        const std::uint8_t port_hi = p.jis2 ? 0xDB : 0xD9;
        const std::uint8_t port_read = p.jis2 ? 0xDB : 0xD9;
        const std::uint8_t lo = static_cast<std::uint8_t>((p.addr >> 5) & 0x3F);
        const std::uint8_t hi = static_cast<std::uint8_t>((p.addr >> 11) & 0x3F);
        program.insert(program.end(), {0x3E, lo, 0xD3, port_lo});  // LD A,lo ; OUT (port_lo),A
        program.insert(program.end(), {0x3E, hi, 0xD3, port_hi});  // LD A,hi ; OUT (port_hi),A
        for (int i = 0; i < p.count; ++i) {
            const std::uint16_t dest = static_cast<std::uint16_t>(p.dest + i);
            program.insert(program.end(), {0xDB, port_read});  // IN A,(port_read)
            program.insert(program.end(),
                           {0x32, static_cast<std::uint8_t>(dest & 0xFF),
                            static_cast<std::uint8_t>((dest >> 8) & 0xFF)});  // LD (dest),A
        }
    }
    program.push_back(0x76);  // HALT
    return program;
}

void run_to_halt(sony_msx::machine::Hbf1xvMachine& machine, const int max_steps = 20000) {
    int guard = 0;
    while (!machine.cpu().state().halted() && guard < max_steps) {
        machine.step_cpu_instruction();
        ++guard;
    }
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;

    // --- Case 1: Kanji font ROM -- real CPU program drives #D8/#D9 (JIS1)
    // and #DA/#DB (JIS2) over the bus; readback matches REAL
    // bios/f1xvkfn.rom content at representative offsets spanning both
    // halves, including near-boundary addresses. ---
    {
        const std::vector<std::uint8_t> real_rom =
            read_file(std::string(SONY_MSX_BIOS_DIR) + "/f1xvkfn.rom");
        expect(real_rom.size() == 0x40000, "RealKanjiRom_IsExpected256KB");

        const std::vector<KanjiProbe> probes{
            {0x00020, false, 8, 0x9000},  // JIS1, near the start
            {0x1FFE0, false, 8, 0x9010},  // JIS1, near the top of the half
            {0x20020, true, 8, 0x9020},   // JIS2, near the start
            {0x2FFE0, true, 8, 0x9030},   // JIS2, mid-range
        };
        const std::vector<std::uint8_t> program = build_kanji_probe(probes);

        Hbf1xvMachine machine;
        machine.set_asset_root(SONY_MSX_BIOS_DIR);
        machine.cold_boot();
        machine.map_flat_ram();
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        machine.cpu().state().regs().pc = 0x0000;
        run_to_halt(machine);
        expect(machine.cpu().state().halted(), "KanjiProbe_ReachesHalt");

        bool all_ok = true;
        for (const auto& p : probes) {
            for (int i = 0; i < p.count; ++i) {
                const std::uint8_t got = machine.read_memory(static_cast<std::uint16_t>(p.dest + i));
                const std::uint8_t expected = real_rom[p.addr + static_cast<std::uint32_t>(i)];
                if (got != expected) {
                    all_ok = false;
                    std::cerr << "  mismatch at addr=0x" << std::hex << (p.addr + static_cast<std::uint32_t>(i))
                              << " got=0x" << int(got) << " expected=0x" << int(expected) << std::dec << "\n";
                }
            }
        }
        expect(all_ok, "KanjiProbe_ReadbackMatchesRealRomContent");

        // Direct device-level re-check via the const accessor (non-CPU path).
        expect(machine.kanji().image()[0x00020] == real_rom[0x00020], "KanjiDevice_ImageAccessor_MatchesRealFile");
    }

    // --- Case 2: printer write protocol driven by a real CPU program over
    // the bus; machine.printer().captured_bytes() reflects it. ---
    {
        // For each byte: LD A,byte ; OUT (#91),A ; XOR A ; OUT (#90),A (strobe
        // low, falling edge from idle-high) ; LD A,1 ; OUT (#90),A (strobe
        // back high, idle). HALT at the end.
        const std::array<std::uint8_t, 3> bytes{0x48, 0x49, 0x21};  // 'H','I','!'
        std::vector<std::uint8_t> program;
        for (const std::uint8_t b : bytes) {
            program.insert(program.end(), {0x3E, b, 0xD3, 0x91});  // LD A,b ; OUT (#91),A
            program.insert(program.end(), {0xAF, 0xD3, 0x90});     // XOR A  ; OUT (#90),A (strobe low)
            program.insert(program.end(), {0x3E, 0x01, 0xD3, 0x90});  // LD A,1 ; OUT (#90),A (strobe high)
        }
        program.push_back(0x76);  // HALT

        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        machine.cpu().state().regs().pc = 0x0000;
        run_to_halt(machine);
        expect(machine.cpu().state().halted(), "PrinterProbe_ReachesHalt");

        const std::vector<std::uint8_t> expected(bytes.begin(), bytes.end());
        expect(machine.printer().captured_bytes() == expected, "PrinterProbe_CapturedBytes_MatchWrittenSequence");
    }

    // --- Case 3: PPI port C bit4/bit5 patterns drive machine.cassette()'s
    // live derivation; the cassette-input default (no tape loaded) still
    // gives PSG R14 bit7 = 1 exactly as pre-M18 (R-M18-5 regression guard). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();

        // Idle read of R14 at cold boot: nothing pressed, cassette idle-high
        // -> 0xFF (byte-for-byte the pre-M18 golden).
        machine.debug_io_write(0xA0, 14);  // select R14
        expect(machine.debug_io_read(0xA2) == 0xFF, "ColdBoot_PsgR14_IdleAllHigh");

        // Port C bit4 (cassette motor off) and bit5 (cassette output/CMO).
        machine.debug_io_write(0xAA, 0x10);  // bit4=1 -> motor off
        expect(!machine.cassette().motor_on(), "PortCBit4Set_Cassette_MotorOff");
        expect(!machine.cassette().output_level(), "PortCBit4Only_OutputLevelFalse");

        machine.debug_io_write(0xAA, 0x20);  // bit4=0 (motor on), bit5=1 (output high)
        expect(machine.cassette().motor_on(), "PortCBit4Clear_Cassette_MotorOn");
        expect(machine.cassette().output_level(), "PortCBit5Set_Cassette_OutputHigh");

        machine.debug_io_write(0xAA, 0x00);  // both clear
        expect(machine.cassette().motor_on(), "PortCAllClear_Cassette_MotorOn");
        expect(!machine.cassette().output_level(), "PortCAllClear_Cassette_OutputLow");

        // Cassette-input default (no synthetic tape loaded): PSG R14 bit7 is
        // STILL 1 -- the machine wires joystick_.attach_cassette_input_source
        // (&cassette_) (no longer nullptr), but cassette_'s own idle-high
        // default (A-M18-11) must reproduce the exact pre-M18 observable bit.
        machine.debug_io_write(0xA0, 14);  // re-select R14 (port C writes above didn't touch R15 selection)
        expect((machine.debug_io_read(0xA2) & 0x80) != 0, "CassetteDefault_PsgR14Bit7_StillOneAfterPortCWrites");
    }

    // --- Zero-regression sanity: the M15 boot-checkpoint style idle read
    // (R14 all-high, keyboard idle) still holds on a fresh machine after all
    // M18 wiring (mirrors the M15/M16/M17 pattern of re-checking a prior
    // golden inline; the FULL ctest run is the authoritative regression
    // gate, captured in the implementation report). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.debug_io_write(0xA0, 14);
        expect(machine.debug_io_read(0xA2) == 0xFF, "FreshMachine_PsgR14_StillAllHigh");
        expect(machine.debug_io_read(0xA9) == 0xFF, "FreshMachine_KeyboardIdleRow_StillAllHigh");
    }

    // --- Determinism: two fresh machines produce identical Kanji/printer/
    // cassette state after the SAME driving sequence. ---
    {
        auto probe = []() {
            Hbf1xvMachine machine;
            machine.set_asset_root(SONY_MSX_BIOS_DIR);
            machine.cold_boot();
            machine.debug_io_write(0xD8, 0x01);
            machine.debug_io_write(0xD9, 0x00);
            const std::uint8_t k0 = machine.debug_io_read(0xD9);
            const std::uint8_t k1 = machine.debug_io_read(0xD9);
            machine.debug_io_write(0x91, 0x5A);
            machine.debug_io_write(0x90, 0x00);
            machine.debug_io_write(0x90, 0x01);
            const std::uint8_t p0 = machine.printer().captured_bytes().empty()
                                         ? 0
                                         : machine.printer().captured_bytes().front();
            machine.debug_io_write(0xAA, 0x20);
            const bool motor = machine.cassette().motor_on();
            const bool output = machine.cassette().output_level();
            return std::make_tuple(k0, k1, p0, motor, output);
        };
        expect(probe() == probe(), "TwoMachineDeterminism_IdenticalDrivingSequence_IdenticalState");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvM18PeripheralIo_Integration cases passed\n";
    return 0;
}
