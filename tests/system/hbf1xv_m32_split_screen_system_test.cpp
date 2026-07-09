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

// Suite: System_Hbf1xvM32SplitScreen_System (M32-S3, Defect A of DEC-0039,
// docs/m32-planner-package.md §2.6 / test matrix row 7)
//
// THE ALESTE-SHAPE UNIVERSAL ORACLE, with zero game assets and zero game
// knowledge: a synthetic in-RAM Z80 program (M24 flat-RAM-program precedent)
// programs GRAPHIC4, enables IE0+IE1, arms R#19=80/R#23=0, and installs an
// IM1 handler that dispatches on S#1 FH:
//   * line-interrupt path: read S#1 (clears FH), count it, write R#23=128;
//   * VBlank path: read S#0, rewrite R#23=0 AND R#19=80 -- the re-arm the
//     both-references (R#19 - R#23) & 0xFF relation makes MANDATORY after
//     the mid-frame R#23 rewrite (openMSX VDP.cc:527-529; fMSX
//     MSX.c:2094-2100).
// Driven through the REAL frame-loop shape (step_cpu_instruction() +
// on_vsync_boundary(), the Sdl3App::run_one_frame()/C5-closure shape) for
// several frames, one rendered frame must show TWO regions with different
// vertical-scroll offsets: rows above the split at offset 0, rows below at
// offset 128 (mod-256 wrap), with the split boundary within the §2.5
// precision-disclosure band of R#19=80.
//
// Adversarial arm (AC-7): the IDENTICAL program with IE1 masked off
// produces NO split (every row at offset 0) and the handler's FH counter
// stays 0 -- proving the oracle discriminates and the line-interrupt
// delivery leg is load-bearing.
//
// Deterministic oracle: two independent machines produce byte-identical
// frames and identical handler counters (M27 determinism pattern).
// Oracles are legacy-renderer references (full static renders at R#23=0 and
// R#23=128) -- the untouched M21 snapshot renderer (§2.2).

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "devices/video/frame_buffer.h"
#include "machine/frame_dump.h"
#include "machine/hbf1xv_machine.h"

namespace {

using sony_msx::devices::video::FrameBuffer;
using sony_msx::machine::Hbf1xvMachine;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

constexpr int kSplitLine = 80;  // R#19 value S

void debug_set_register(Hbf1xvMachine& m, const std::uint8_t reg, const std::uint8_t value) {
    m.debug_io_write(0x99, value);
    m.debug_io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

// Per-row-unique GRAPHIC4 signature content (row r byte c = (r+c) & 0xFF)
// across all 256 rows of page 0 -- every output row's applied scroll offset
// is machine-checkable against the references. NOTE: GRAPHIC4 must be set
// BEFORE the 32 KB auto-increment fill -- the 14-bit VRAM pointer carries
// into R#14 only in V9938 modes (v9958_vdp.cpp advance_vram_pointer();
// legacy modes wrap at 16 KB).
void fill_signature_vram(Hbf1xvMachine& m) {
    debug_set_register(m, 0, 0x06);  // GRAPHIC4 (V9938 mode: R#14 carry)
    m.debug_io_write(0x99, 0x00);
    m.debug_io_write(0x99, 0x40);
    for (int r = 0; r < 256; ++r) {
        for (int c = 0; c < 128; ++c) {
            m.debug_io_write(0x98, static_cast<std::uint8_t>((r + c) & 0xFF));
        }
    }
    debug_set_register(m, 14, 0x00);  // restore the address-high register
}

// The synthetic split program (hand-assembled; addresses in comments).
// `ie1` selects the real arm (R#0 = 0x16: GRAPHIC4 + IE1) or the
// adversarial arm (R#0 = 0x06: GRAPHIC4, IE1 masked off).
std::vector<std::uint8_t> build_program(const bool ie1) {
    const std::uint8_t r0_value = ie1 ? 0x16 : 0x06;
    std::vector<std::uint8_t> image(0x0200, 0x00);

    // 0x0000: JP 0x0100 (main)
    image[0x0000] = 0xC3; image[0x0001] = 0x00; image[0x0002] = 0x01;
    // 0x0038: JP 0x0140 (IM1 handler)
    image[0x0038] = 0xC3; image[0x0039] = 0x40; image[0x003A] = 0x01;

    // main @ 0x0100
    const std::uint8_t main_code[] = {
        0xF3,                    // 0100 DI
        0xED, 0x56,              // 0101 IM 1
        0x31, 0x80, 0xF3,        // 0103 LD SP,0xF380
        0x3E, 0x00, 0xD3, 0x99,  // 0106 R#23 <- 0   (data)
        0x3E, 0x97, 0xD3, 0x99,  // 010A             (commit)
        0x3E, kSplitLine, 0xD3, 0x99,  // 010E R#19 <- 80
        0x3E, 0x93, 0xD3, 0x99,  // 0112
        0x3E, 0x02, 0xD3, 0x99,  // 0116 R#8 <- SPD (sprites off)
        0x3E, 0x88, 0xD3, 0x99,  // 011A
        0x3E, r0_value, 0xD3, 0x99,  // 011E R#0 <- G4 [| IE1]
        0x3E, 0x80, 0xD3, 0x99,  // 0122
        0x3E, 0x60, 0xD3, 0x99,  // 0126 R#1 <- BL | IE0
        0x3E, 0x81, 0xD3, 0x99,  // 012A
        0xFB,                    // 012E EI
        0x18, 0xFE,              // 012F JR $ (spin; interrupts break in)
    };
    for (std::size_t i = 0; i < sizeof(main_code); ++i) {
        image[0x0100 + i] = main_code[i];
    }

    // handler @ 0x0140
    const std::uint8_t handler_code[] = {
        0xF5,                    // 0140 PUSH AF
        0xC5,                    // 0141 PUSH BC
        0x3E, 0x01, 0xD3, 0x99,  // 0142 R#15 <- 1
        0x3E, 0x8F, 0xD3, 0x99,  // 0146
        0xDB, 0x99,              // 014A IN A,(#99)  ; S#1 (clears FH when held)
        0x47,                    // 014C LD B,A
        0x3E, 0x00, 0xD3, 0x99,  // 014D R#15 <- 0
        0x3E, 0x8F, 0xD3, 0x99,  // 0151
        0x78,                    // 0155 LD A,B
        0xE6, 0x01,              // 0156 AND 1      ; FH?
        0x28, 0x14,              // 0158 JR Z,+0x14 -> 0x016E (VBlank path)
        // line-interrupt path @ 015A
        0x3A, 0x00, 0x80,        // 015A LD A,(0x8000)  ; fh_seen counter
        0x3C,                    // 015D INC A
        0x32, 0x00, 0x80,        // 015E LD (0x8000),A
        0x3E, 0x80, 0xD3, 0x99,  // 0161 R#23 <- 128 (the split!)
        0x3E, 0x97, 0xD3, 0x99,  // 0165
        0xC1,                    // 0169 POP BC
        0xF1,                    // 016A POP AF
        0xFB,                    // 016B EI
        0xED, 0x4D,              // 016C RETI
        // VBlank path @ 016E
        0xDB, 0x99,              // 016E IN A,(#99)  ; S#0 (clears F, releases /INT)
        0x3E, 0x00, 0xD3, 0x99,  // 0170 R#23 <- 0 (re-arm base)
        0x3E, 0x97, 0xD3, 0x99,  // 0174
        0x3E, kSplitLine, 0xD3, 0x99,  // 0178 R#19 <- 80 (mandatory re-arm)
        0x3E, 0x93, 0xD3, 0x99,  // 017C
        0x3A, 0x01, 0x80,        // 0180 LD A,(0x8001)  ; vblank counter
        0x3C,                    // 0183 INC A
        0x32, 0x01, 0x80,        // 0184 LD (0x8001),A
        0xC1,                    // 0187 POP BC
        0xF1,                    // 0188 POP AF
        0xFB,                    // 0189 EI
        0xED, 0x4D,              // 018A RETI
    };
    for (std::size_t i = 0; i < sizeof(handler_code); ++i) {
        image[0x0140 + i] = handler_code[i];
    }
    return image;
}

struct RunResult {
    FrameBuffer frame;
    std::uint8_t fh_seen = 0;
    std::uint8_t vblanks = 0;
};

RunResult run_split_scenario(const bool ie1, const int frames) {
    Hbf1xvMachine m;
    m.cold_boot();
    m.map_flat_ram();
    fill_signature_vram(m);

    const std::vector<std::uint8_t> program = build_program(ie1);
    m.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
    const std::uint8_t zeros[2] = {0, 0};
    m.load_memory(0x8000, zeros, 2);  // counters (DRAM boots to the 00/FF pattern)

    // The REAL frame-loop shape: step to each boundary, then
    // on_vsync_boundary() -- never run_frame() (R-M25-5).
    const std::uint64_t target = m.frame_cycles_per_frame();
    for (int frame = 0; frame < frames; ++frame) {
        const std::uint64_t start = m.elapsed_cycles();
        while (m.elapsed_cycles() - start < target) {
            m.step_cpu_instruction();
        }
        m.on_vsync_boundary();
    }
    RunResult result;
    result.frame = m.render_frame();
    result.fh_seen = m.read_memory(0x8000);
    result.vblanks = m.read_memory(0x8001);
    return result;
}

FrameBuffer reference_frame(const std::uint8_t r23) {
    Hbf1xvMachine ref;
    ref.cold_boot();
    fill_signature_vram(ref);
    debug_set_register(ref, 0, 0x06);
    debug_set_register(ref, 8, 0x02);
    debug_set_register(ref, 1, 0x60);
    debug_set_register(ref, 19, kSplitLine);
    debug_set_register(ref, 23, r23);
    return ref.render_frame();  // pure live projection (legacy semantics)
}

bool row_equals(const FrameBuffer& a, const FrameBuffer& b, const int y) {
    for (int x = 0; x < a.width; ++x) {
        if (a.at(x, y) != b.at(x, y)) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    // Optional evidence-dump mode for tools/openmsx-m32-split-ab.ps1 (side
    // A of the §2.7 A/B): `--dump-frame <path>` additionally serializes the
    // real-arm split frame as a HBF1XV-FRAME-DUMP v1 file. The default
    // (no-argv, ctest) invocation is byte-for-byte unchanged.
    std::string dump_path;
    if (argc >= 3 && std::string(argv[1]) == "--dump-frame") {
        dump_path = argv[2];
    }

    const FrameBuffer ref0 = reference_frame(0);
    const FrameBuffer ref128 = reference_frame(128);
    expect(ref0.width == 256 && ref0.height == 192, "Reference_Graphic4Dimensions");
    bool rows_discriminate = true;
    for (int y = 0; y < ref0.height; ++y) {
        if (row_equals(ref0, ref128, y)) {
            rows_discriminate = false;
        }
    }
    expect(rows_discriminate, "Reference_EveryRow_DistinguishesScroll0From128");

    // --- The real arm: IE1 on => a genuine two-region split frame. ---
    {
        const RunResult run = run_split_scenario(true, 5);
        if (!dump_path.empty()) {
            std::ofstream out(dump_path, std::ios::binary | std::ios::trunc);
            const std::string text = sony_msx::machine::frame_dump::serialize_frame_dump(run.frame);
            out.write(text.data(), static_cast<std::streamsize>(text.size()));
            std::cerr << "split-system-test: wrote " << dump_path << "\n";
        }
        expect(run.fh_seen >= 3, "SplitRun_HandlerObservedS1FH_AtLeastOncePerFrame");
        expect(run.vblanks >= 3, "SplitRun_VblankHandlerRan");
        expect(run.frame.width == ref0.width && run.frame.height == ref0.height,
               "SplitRun_FrameDimensions");

        // Region structure: find the first row that left the offset-0
        // region; everything above matches ref0, everything at/below
        // matches ref128, and the boundary sits inside the §2.5 precision
        // band [S, S+4) (poll granularity + IM1 accept + handler OUT chain
        // ~= one line, plus the two-write commit).
        int boundary = -1;
        for (int y = 0; y < run.frame.height; ++y) {
            if (!row_equals(run.frame, ref0, y)) {
                boundary = y;
                break;
            }
        }
        expect(boundary >= 0, "SplitRun_SplitBoundaryExists");
        expect(boundary >= kSplitLine && boundary < kSplitLine + 4,
               "SplitRun_BoundaryWithinPrecisionBand_S_to_SPlus4");
        bool top_ok = true;
        for (int y = 0; y < boundary && top_ok; ++y) {
            top_ok = row_equals(run.frame, ref0, y);
        }
        bool bottom_ok = boundary >= 0;
        for (int y = boundary; y < run.frame.height && bottom_ok; ++y) {
            bottom_ok = row_equals(run.frame, ref128, y);
        }
        expect(top_ok, "SplitRun_RowsAboveBoundary_Offset0");
        expect(bottom_ok, "SplitRun_RowsFromBoundary_Offset128_ModuloWrap");

        // Determinism: an independent machine reproduces frame AND counters
        // byte-for-byte (M27 pattern).
        const RunResult again = run_split_scenario(true, 5);
        expect(run.frame.pixels == again.frame.pixels && run.fh_seen == again.fh_seen &&
                   run.vblanks == again.vblanks,
               "SplitRun_TwoMachines_ByteIdenticalFrameAndCounters");
    }

    // --- The adversarial arm: IE1 masked off => NO split, FH never seen.
    //     Proves the interrupt-delivery leg is load-bearing (§2.6 item 6). --
    {
        const RunResult run = run_split_scenario(false, 5);
        expect(run.fh_seen == 0, "AdversarialIe1Off_HandlerNeverSawFH");
        expect(run.vblanks >= 3, "AdversarialIe1Off_VblankStillRuns");
        expect(run.frame.pixels == ref0.pixels, "AdversarialIe1Off_NoSplit_WholeFrameOffset0");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All System_Hbf1xvM32SplitScreen_System cases passed\n";
    return 0;
}
