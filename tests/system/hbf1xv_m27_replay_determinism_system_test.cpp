#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include "devices/cartridge/cartridge_mapper_type.h"
#include "machine/hbf1xv_machine.h"

// Suite: Hbf1xvM27ReplayDeterminism_System (M27-S3, "Production Hardening +
// Debug/Test Tooling" item 4, docs/m27-planner-package.md §2.5 -- the
// milestone's own headline claim).
//
// A concrete, adversarially-validated, byte-for-byte replay-determinism
// proof: two wholly independent, freshly-constructed Hbf1xvMachine
// instances, configured identically (same BIOS asset root, same cartridge --
// roms/aleste.rom, the project's own established, disclosed, non-identity-
// claiming Generic8kB smoke fixture, M19), run through an identical, fixed,
// bounded instruction count, produce byte-for-byte identical event-log
// files -- a real file-content comparison, not a same-object/same-pointer
// triviality. A THIRD machine, identical setup but with one deliberately
// different keyboard().set_key() call injected partway through, produces a
// DIFFERENT event log -- the adversarial negative control proving the
// equality check genuinely discriminates (mirrors the M24 ZEXALL
// corruption-self-check discipline: a check that always passes, even under
// deliberate divergence, is not a real check).
//
// R-M27-2 (critical ordering): set_event_logging_enabled(true) is called
// BEFORE cold_boot() on every instance, so the Reset event is genuinely
// captured (hbf1xv_machine.h:306-309's own documented requirement) -- a
// dedicated assertion below confirms this.
//
// DEVELOPER FINDING (this cycle, load-bearing for this test's exact design):
// the real, unmodified BIOS, driven purely by step_cpu_instruction() (no
// on_vsync(), matching items 1/3/4's own "never run_frame()" CPU-driving
// convention), spends its first >=20,000 instructions entirely inside the
// well-known MSX BIOS RAM/slot-detection routine -- confirmed this cycle by
// direct inspection of the loaded BIOS image bytes at the observed loop PCs
// (0x0472/0x0484: `IN A,(#A8)`, the PPI slot-select readback port, NOT the
// keyboard port #A9) and independently corroborated by this project's own
// existing backlog C5 disclosure (`agent-protocol/state/deferred-backlog.md`
// row C5: the auto-boot handshake is "genuinely never observed within an
// unattended... run, confirmed up to a 20,000,000-instruction diagnostic
// run"). A keyboard().set_key() call therefore has ZERO observable effect on
// the real BIOS's PC/opcode trace within any bounded window practical for a
// fast ctest case -- an honest architectural fact, not a test bug. This test
// therefore mirrors src/main.cpp's OWN established run_parity_trace()
// technique (M10-S4/M19-S6): load the real BIOS asset root AND the real
// cartridge (satisfying "same BIOS asset root, same cartridge" literally),
// then additionally map_flat_ram() and drive a small, deterministic,
// hand-verifiable Z80 driver program that GENUINELY reads the keyboard
// matrix via the real #AA (row-select)/#A9 (row-read) PPI ports and branches
// on the result -- a real, first-principles-verifiable proof that the
// injected keyboard state change propagates through the real I/O path into
// the CPU's own instruction trace, rather than an untested assumption that
// it would.

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif
#ifndef SONY_MSX_ROMS_DIR
#error "SONY_MSX_ROMS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif

namespace {

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// A small, deterministic, hand-verifiable keyboard-probe driver program
// (mirrors src/main.cpp's run_parity_trace()'s own established
// map_flat_ram()+load_memory() flat-RAM-driver technique):
//   0000  3E 02        LD A,#02          -- select keyboard row 2 ("A" lives here)
//   0002  D3 AA        OUT (#AA),A       -- PPI port C: row-select bits0-3
//   0004  DB A9        IN A,(#A9)        -- PPI port B: read row 2 columns (inverted)
//   0006  CB 77        BIT 6,A           -- test column 6 ("A" key's column)
//   0008  28 05        JR Z,+5           -- Z=1 (bit clear) means "A" IS pressed
//   000A  00 00 00 00 00   NOP x5        -- "not pressed" path (falls through)
//   000F  00 00 00 00 00   NOP x5        -- "pressed" path (jump target)
//   0014  76           HALT
// "A" -> (row=2, column=6) per src/peripherals/msx_key_names.cpp (R-M27-4).
// When NOT pressed: 16 total instructions retire before HALT (5+5 NOPs).
// When pressed: JR Z is taken, skipping the first 5 NOPs -- 11 total
// instructions retire before HALT, and every PC from the JR onward differs
// from the not-pressed run. Either difference alone is sufficient to prove
// genuine event-log divergence; both are asserted in Case 2 below.
constexpr std::uint8_t kKeyboardProbeProgram[] = {
    0x3E, 0x02,                          // LD A,#02
    0xD3, 0xAA,                          // OUT (#AA),A
    0xDB, 0xA9,                          // IN A,(#A9)
    0xCB, 0x77,                          // BIT 6,A
    0x28, 0x05,                          // JR Z,+5
    0x00, 0x00, 0x00, 0x00, 0x00,        // NOP x5 ("not pressed" path)
    0x00, 0x00, 0x00, 0x00, 0x00,        // NOP x5 ("pressed" path)
    0x76,                                // HALT
};

// A fixed, bounded instruction count -- generous enough to reach HALT on
// EITHER path (worst case 16), but still fast and deliberately NOT the slow
// ZEXALL class (planner §3 M27-S3's own "confirm and document its runtime"
// acceptance note).
constexpr std::uint32_t kBoundedSteps = 30;

std::vector<std::uint8_t> read_aleste_rom() {
    const std::filesystem::path path = std::filesystem::path(SONY_MSX_ROMS_DIR) / "aleste.rom";
    std::ifstream in(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// Builds+runs ONE independent machine instance: real BIOS asset root, real
// cartridge (roms/aleste.rom) loaded, event logging enabled BEFORE
// cold_boot() (R-M27-2), then the keyboard-probe driver program above run to
// completion (or the bounded step ceiling). `divergent_key_inject`: when
// true, injects ONE extra, real keyboard key-down call partway through the
// run (after the row-select OUT but BEFORE the IN A,(#A9) read that consumes
// it) -- a genuinely different machine trajectory, not a cosmetic
// difference. Returns the on-disk event-log path.
std::filesystem::path run_one_session(const std::vector<std::uint8_t>& cart_image,
                                      const std::filesystem::path& debug_root, const std::string& log_filename,
                                      const bool divergent_key_inject) {
    using sony_msx::devices::cartridge::CartridgeLoadResult;
    using sony_msx::devices::cartridge::CartridgeMapperType;

    sony_msx::machine::Hbf1xvMachine machine;
    machine.set_debug_root(debug_root);
    machine.set_event_logging_enabled(true);  // BEFORE cold_boot() -- R-M27-2.
    machine.set_asset_root(SONY_MSX_BIOS_DIR);
    machine.cold_boot();

    const CartridgeLoadResult load_result = machine.load_cartridge(1, CartridgeMapperType::Generic8kB, cart_image);
    if (load_result != CartridgeLoadResult::Ok) {
        std::cerr << "  cartridge load failed for " << log_filename << "\n";
    }

    machine.map_flat_ram();
    machine.load_memory(0x0000, kKeyboardProbeProgram, static_cast<std::uint32_t>(sizeof(kKeyboardProbeProgram)));
    machine.cpu().state().regs().pc = 0x0000;

    for (std::uint32_t step = 0; step < kBoundedSteps; ++step) {
        if (machine.cpu().state().halted()) {
            break;
        }
        // Injected exactly after step 2 (LD A,#02 and OUT (#AA),A have both
        // already retired) and BEFORE step 3 (IN A,(#A9), the instruction
        // that actually consumes this state) -- "partway through the run,
        // before continuing", via the SAME public API
        // (peripherals::KeyboardMatrix::set_key) item 3's scripted-input
        // player itself drives.
        if (divergent_key_inject && step == 2) {
            machine.keyboard().set_key(2, 6, true);  // "A" DOWN.
        }
        machine.step_cpu_instruction();
    }

    const bool write_ok = machine.write_event_log(log_filename);
    if (!write_ok) {
        std::cerr << "  write_event_log failed for " << log_filename << "\n";
    }
    return debug_root / "logs" / log_filename;
}

}  // namespace

int main() {
    const std::vector<std::uint8_t> cart_image = read_aleste_rom();
    if (cart_image.empty()) {
        std::cerr << "Case failed: AlesteRom_FileOpens_NonEmpty\n";
        return 1;
    }

    const std::filesystem::path temp_root =
        std::filesystem::temp_directory_path() / "sony-msx-hbf1xv-m27-replay-determinism-system-test";
    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);

    // --- Case 1: two INDEPENDENTLY constructed machine instances, identical
    // setup (including: "A" never pressed), produce byte-for-byte identical
    // event-log files. ---
    const std::filesystem::path path_a = run_one_session(cart_image, temp_root, "run1.log", false);
    const std::filesystem::path path_b = run_one_session(cart_image, temp_root, "run2.log", false);

    const std::string content_a = read_file(path_a);
    const std::string content_b = read_file(path_b);

    expect(!content_a.empty(), "Run1_EventLogNonEmpty");
    expect(!content_b.empty(), "Run2_EventLogNonEmpty");
    expect(content_a == content_b, "TwoIndependentIdenticalRuns_ByteForByteIdenticalEventLogs");

    // R-M27-2: the Reset event is genuinely present at sequence 0 -- proves
    // the set_event_logging_enabled(true)-before-cold_boot() ordering was
    // honored (silently missing it would omit this line entirely).
    expect(content_a.rfind("EVT=0000 T=0 TYPE=RESET", 0) == 0,
           "EventLog_ResetEventPresentAtSequence0_R_M27_2_OrderingHonored");

    // Hand-computable oracle for the "not pressed" path: 16
    // step_cpu_instruction() calls retire before HALT (5 opcodes before the
    // branch: LD/OUT/IN/BIT/JR, + 5 NOPs "not pressed" path + 5 NOPs
    // "pressed" path + HALT = 16), each producing one InstructionRetire
    // event; the SAME step that retires HALT ALSO produces an extra Halt
    // event (hbf1xv_machine.cpp:477-480: halted_before=false ->
    // cpu_.state().halted()==true after that step). Total = 1 Reset + 16
    // InstructionRetire + 1 Halt = 18 lines.
    {
        std::size_t line_count = 0;
        for (const char c : content_a) {
            if (c == '\n') {
                ++line_count;
            }
        }
        expect(line_count == 18, "NotPressedPath_EventCount_HandComputedOracle_Is18Lines");
    }

    // --- Case 2 (adversarial negative control, §2.5 point 5): a THIRD
    // machine, identical setup but with one deliberately DIFFERENT
    // keyboard().set_key() call injected partway through, produces a
    // DIFFERENT event log -- proving Case 1's byte-for-byte equality check
    // genuinely discriminates, not a vacuous no-op that would pass even if
    // write_event_log() were silently broken (mirrors the M24 ZEXALL
    // adversarial-corruption-self-check discipline). ---
    const std::filesystem::path path_c = run_one_session(cart_image, temp_root, "run3_divergent.log", true);
    const std::string content_c = read_file(path_c);
    expect(!content_c.empty(), "Run3Divergent_EventLogNonEmpty");
    expect(content_c != content_a, "AdversarialNegativeControl_DivergentRun_ProducesDifferentEventLog");

    // The divergent run's own hand-computable oracle: the "pressed" path
    // skips the first 5 NOPs, so only 11 step_cpu_instruction() calls retire
    // before HALT (5 opcodes before the branch + 5 NOPs + HALT). Total = 1
    // Reset + 11 InstructionRetire + 1 Halt = 13 lines -- genuinely,
    // hand-verifiably DIFFERENT from the not-pressed path's 18 (five fewer
    // InstructionRetire events, matching the five skipped NOPs exactly).
    {
        std::size_t line_count = 0;
        for (const char c : content_c) {
            if (c == '\n') {
                ++line_count;
            }
        }
        expect(line_count == 13, "PressedPath_EventCount_HandComputedOracle_Is13Lines_FiveFewerThanNotPressed");
    }
    // A genuine PC-level divergence too, not merely a line-count difference:
    // the not-pressed run's trace contains "PC=000A" (the skipped NOP), the
    // pressed run's does not.
    expect(content_a.find("PC=000A") != std::string::npos, "NotPressedPath_Trace_ContainsPc000A");
    expect(content_c.find("PC=000A") == std::string::npos, "PressedPath_Trace_NeverVisitsPc000A_JumpedOverIt");

    std::filesystem::remove_all(temp_root, ec);

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Hbf1xvM27ReplayDeterminism_System cases passed\n";
    return 0;
}
