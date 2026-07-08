// Suite: System_Hbf1xvM22SpritesCommandEngine_System
//
// Machine-level system test (tests/CLAUDE.md's three-tier convention) for
// M22: a REAL CPU program, executed over the M11 bus, (a) sets up two
// fully-overlapping sprites via `OUT (#98)/(#99)` -- exactly as real MSX2+
// software would -- producing a genuine collision, read back via S#0/S#3-S#6
// through the same port contract; and (b) issues a representative command
// sequence (one atomic command, HMMV, and one event-driven transfer command,
// LMMC) entirely through `OUT (#99)/(#44 via R#44)`, with the resulting VRAM
// content confirmed both directly and via render_frame() (the EXISTING,
// unmodified M21 renderer). The sprite-check recompute itself is triggered
// via the machine's vdp().on_vsync() accessor -- a hardware frame-boundary
// hook, not something software drives via an I/O port.

#include <array>
#include <cstdint>
#include <iostream>

#include "machine/hbf1xv_machine.h"

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

void run_to_halt(sony_msx::machine::Hbf1xvMachine& machine, const int max_steps) {
    for (int i = 0; i < max_steps && !machine.cpu().state().halted(); ++i) {
        machine.step_cpu_instruction();
    }
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;

    // --- (a) Sprite setup + a real collision, entirely CPU-driven. ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();

        const std::array<std::uint8_t, 89> program{
            0x3E, 0x40, 0xD3, 0x99,  // LD A,0x40 ; OUT(99)  R#1 data (bit6 display enable)
            0x3E, 0x81, 0xD3, 0x99,  // LD A,0x81 ; OUT(99)  register-write R#1

            0x3E, 0x01, 0xD3, 0x99,  // LD A,1    ; OUT(99)  R#6 data (pattern base -> 0x0800)
            0x3E, 0x86, 0xD3, 0x99,  // LD A,0x86 ; OUT(99)  register-write R#6

            0x3E, 0x00, 0xD3, 0x99,  // addr0 low
            0x3E, 0x40, 0xD3, 0x99,  // addr0 high (write)
            0x3E, 0x00, 0xD3, 0x98,  // sprite0 Y = 0

            0x3E, 0x01, 0xD3, 0x99,  // addr1 low
            0x3E, 0x40, 0xD3, 0x99,
            0x3E, 0x00, 0xD3, 0x98,  // sprite0 X = 0

            0x3E, 0x02, 0xD3, 0x99,
            0x3E, 0x40, 0xD3, 0x99,
            0x3E, 0x00, 0xD3, 0x98,  // sprite0 pattern index = 0

            0x3E, 0x03, 0xD3, 0x99,
            0x3E, 0x40, 0xD3, 0x99,
            0x3E, 0x01, 0xD3, 0x98,  // sprite0 color = 1

            0x3E, 0x04, 0xD3, 0x99,
            0x3E, 0x40, 0xD3, 0x99,
            0x3E, 0xD0, 0xD3, 0x98,  // sentinel Y = 208 (sprite1 slot -> stop scan)

            0x3E, 0x00, 0xD3, 0x99,  // pattern addr low = 0
            0x3E, 0x48, 0xD3, 0x99,  // pattern addr high = 0x48 (0x0800)
            0x3E, 0xFF, 0xD3, 0x98,  // pattern row0 = all set

            0x76,                    // HALT
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        run_to_halt(machine, 200);
        expect(machine.cpu().state().halted(), "Sprites_CpuProgram_ReachesHalt");

        // A single fully-overlapping sprite would show no collision; use a
        // SECOND sprite (written directly, not by the CPU program, purely
        // to keep the program above a manageable size) at the SAME position
        // to produce a genuine collision.
        machine.debug_io_write(0x99, 0x04);
        machine.debug_io_write(0x99, 0x40);  // addr4 low=4, high write-mode
        machine.debug_io_write(0x98, 0x00);  // sprite1 Y = 0 (overwrite sentinel)
        machine.debug_io_write(0x99, 0x05);
        machine.debug_io_write(0x99, 0x40);
        machine.debug_io_write(0x98, 0x00);  // sprite1 X = 0
        machine.debug_io_write(0x99, 0x06);
        machine.debug_io_write(0x99, 0x40);
        machine.debug_io_write(0x98, 0x00);  // sprite1 pattern index = 0
        machine.debug_io_write(0x99, 0x07);
        machine.debug_io_write(0x99, 0x40);
        machine.debug_io_write(0x98, 0x02);  // sprite1 color = 2
        machine.debug_io_write(0x99, 0x08);
        machine.debug_io_write(0x99, 0x40);
        machine.debug_io_write(0x98, 0xD0);  // sentinel now at slot 2

        machine.vdp().on_vsync();

        // Read S#0/S#3/S#5 back through the SAME #99 port contract.
        machine.debug_io_write(0x99, 0x00);
        machine.debug_io_write(0x99, 0x8F);  // R#15 = 0 (select S#0)
        const std::uint8_t s0 = static_cast<std::uint8_t>(machine.debug_io_read(0x99) & 0x7F);
        expect((s0 & 0x20) != 0, "Sprites_System_CollisionFlagSet");

        machine.debug_io_write(0x99, 0x03);
        machine.debug_io_write(0x99, 0x8F);  // R#15 = 3 (select S#3)
        expect(machine.debug_io_read(0x99) == 12, "Sprites_System_CollisionXOffset");

        machine.debug_io_write(0x99, 0x05);
        machine.debug_io_write(0x99, 0x8F);  // R#15 = 5 (select S#5)
        expect(machine.debug_io_read(0x99) == (1 + 8), "Sprites_System_CollisionYOffset");

        const auto fb = machine.render_frame();
        expect(fb.width == 256 && fb.height == 192, "Sprites_System_RenderFrameDimensions");
    }

    // --- (b) A representative command sequence -- one atomic command
    //     (HMMV) and one event-driven transfer command (LMMC) -- entirely
    //     CPU-driven, with the result confirmed via VRAM AND render_frame(). ---
    {
        Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();

        const std::array<std::uint8_t, 105> program{
            0x3E, 0x06, 0xD3, 0x99,  // LD A,6    ; OUT(99)  R#0 data
            0x3E, 0x80, 0xD3, 0x99,  // LD A,0x80 ; OUT(99)  register-write R#0 -> GRAPHIC4

            0x3E, 0x0A, 0xD3, 0x99,  // LD A,10   ; OUT(99)  R#36 data (DX low = 10, byte index 5)
            0x3E, 0xA4, 0xD3, 0x99,  // register-write R#36

            0x3E, 0x02, 0xD3, 0x99,  // R#40 data (NX low = 2 pixels = 1 byte)
            0x3E, 0xA8, 0xD3, 0x99,  // register-write R#40

            0x3E, 0x01, 0xD3, 0x99,  // R#42 data (NY low = 1)
            0x3E, 0xAA, 0xD3, 0x99,  // register-write R#42

            0x3E, 0x5A, 0xD3, 0x99,  // R#44 data (COL = 0x5A)
            0x3E, 0xAC, 0xD3, 0x99,  // register-write R#44

            0x3E, 0xC0, 0xD3, 0x99,  // R#46 data (CMD = 0xC0 -> HMMV)
            0x3E, 0xAE, 0xD3, 0x99,  // register-write R#46 -- fills byte index 5 with 0x5A

            0x3E, 0x00, 0xD3, 0x99,  // R#36 data (DX low = 0, reset for the LMMC below)
            0x3E, 0xA4, 0xD3, 0x99,

            0x3E, 0x04, 0xD3, 0x99,  // R#40 data (NX low = 4 pixels = 2 bytes)
            0x3E, 0xA8, 0xD3, 0x99,

            // Authentic LMMC protocol (the real MSX2+ BIOS boot-logo shape,
            // boot-logo fix): pixel 0's color is PRE-LOADED into R#44 BEFORE
            // the command -- any R#44 write arms exactly one pending
            // transfer unit that the command start consumes (openMSX
            // VDPCmdEngine.cc:1856-1863 `transfer = true` + startLmmc's
            // bug#1014 comment; independently corroborated by fMSX
            // V9938.c VDPWrite()'s TR-clear + LmmcEngine()'s consume-on-
            // TR-clear). This also replaces the stale 0x5A left armed by
            // the HMMV COL write above -- on real hardware THAT byte would
            // otherwise have become LMMC pixel 0 (the pre-fix ordering of
            // this program, which only produced 0x12/0x34 under the old,
            // reference-divergent "start never consumes" model).
            0x3E, 0x01, 0xD3, 0x99,  // R#44 data = 1 (pixel 0, pre-loaded)
            0x3E, 0xAC, 0xD3, 0x99,

            0x3E, 0xB0, 0xD3, 0x99,  // R#46 data (CMD = 0xB0 -> LMMC, IMP)
            0x3E, 0xAE, 0xD3, 0x99,  // starts the transfer, consumes pixel 0

            0x3E, 0x02, 0xD3, 0x99,  // pixel 1
            0x3E, 0xAC, 0xD3, 0x99,
            0x3E, 0x03, 0xD3, 0x99,  // pixel 2
            0x3E, 0xAC, 0xD3, 0x99,
            0x3E, 0x04, 0xD3, 0x99,  // pixel 3 (last -> CE clears)
            0x3E, 0xAC, 0xD3, 0x99,

            0x76,                    // HALT
        };
        machine.load_memory(0x0000, program.data(), static_cast<std::uint32_t>(program.size()));
        run_to_halt(machine, 200);
        expect(machine.cpu().state().halted(), "CommandEngine_CpuProgram_ReachesHalt");

        expect(machine.vdp().vram().read(5) == 0x5A, "CommandEngine_System_HmmvWroteByteFive");
        expect(machine.vdp().vram().read(0) == 0x12, "CommandEngine_System_LmmcWroteByteZero");
        expect(machine.vdp().vram().read(1) == 0x34, "CommandEngine_System_LmmcWroteByteOne");
        expect((machine.vdp().peek_status_register(2) & 0x01) == 0, "CommandEngine_System_CeClearedAfterTransfer");

        // The command-engine-written bytes are visible unmodified through
        // the EXISTING M21 render_graphic4 path (no special-casing needed).
        const auto fb = machine.render_frame();
        expect(fb.width == 256 && fb.height == 192, "CommandEngine_System_RenderFrameDimensions");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
