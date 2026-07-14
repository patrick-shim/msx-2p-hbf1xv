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

#include "machine/hbf1xv_machine.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <system_error>
#include <utility>

#include "devices/cartridge/cartridge_konami_scc_rom.h"
#include "machine/debug_dump.h"
#include "machine/debug_format.h"
#include "machine/frame_dump.h"

namespace sony_msx::machine {

namespace {
constexpr std::uint64_t kFrameCycles = 228 * 262;
}  // namespace

void Hbf1xvMachine::CpuIrqAdapter::set_irq(const bool asserted) {
    // Forward the V9958's level-held /INT to the M12 Z80A. Asserting requests a
    // maskable interrupt; releasing clears it. No acceptance logic is added here
    // — the IM1 accept path (14T, proven in M12) is reused unchanged.
    if (asserted) {
        cpu_.request_maskable_interrupt();
    } else {
        cpu_.clear_maskable_interrupt();
    }
}

Hbf1xvMachine::Hbf1xvMachine(const std::size_t dram_bytes)
    : dram_(dram_bytes), cpu_bus_client_(bus_), cpu_(cpu_bus_client_) {
    // M42 (DEC-0061): dram_ is sized from the constructor argument (default
    // kDramBytes == stock 64 KB, so every default construction is byte-identical).
    // ram_mapper_ derives its segment count from dram_.size() -- it is declared
    // after dram_, so dram_ is already sized when the mapper reads it.
    wire_bus();
}

void Hbf1xvMachine::VdpRenderSyncAdapter::on_before_state_change() {
    // M32-S2 (docs/m32-planner-package.md §2.3): commit the display lines the
    // beam has already passed from the PRE-write state before the write mutates
    // anything -- openMSX's sync-before-change protocol at line granularity
    // (references/openmsx-21.0/src/video/PixelRenderer.cc:253-394, 549-571).
    // Suspended while the non-perturbing debug_io_write() seam drives the bus
    // (§2.3 documented exclusion). Negative raster (border/vblank) clamps to a
    // no-op inside sync_to_line() -- vblank writes affect the whole next frame.
    if (suspended_) {
        return;
    }
    // Any real VDP write invalidates the boundary fast path's completed
    // frame -- state has moved past the sealed frame.
    machine_.scanline_accumulator_.mark_completed_frame_stale();
    const int line = machine_.vdp_.raster_display_line();
    if (line < 0) {
        return;
    }
    // M39 (DEC-0060) render-sync commit boundary -- the ACTIVE-DISPLAY LEFT
    // MARGIN rounding. The commit boundary here is the openMSX LINE-accuracy
    // renderer's `renderUntil()` rounding, NOT a raw scanline boundary:
    // openMSX rounds the sync point with the left margin --
    //   limitY = (limitTicks + TICKS_PER_LINE - 400) / TICKS_PER_LINE
    // (references/openmsx-21.0/src/video/PixelRenderer.cc:566-567) -- so a
    // register write is committed against the line whose ACTIVE pixels have
    // already been drawn, which lands one display line LATER than our raw
    // `raster_display_line()` (floor(tstates / kCpuTstatesPerLine), quantized at
    // the scanline START with no left-margin term, v9958_vdp.cpp
    // raster_display_line()). So the write takes effect from line L+2, not L+1:
    // lines [watermark, L+2) keep the pre-write state, the change applies from
    // L+2.
    //
    // CALIBRATION (openMSX 19.1 Sony_HB-F1XV, Aleste 2 gameplay, raw captures
    // debug/m39-hud/omsx_hud_*.png -- 4 stable frames): Aleste holds a fixed
    // top HUD over a vertically-scrolling playfield via cycle-timed R#1 (BL) +
    // R#23 (vertical scroll) writes with NO line interrupt. The BL-off write is
    // issued at raster_display_line()==13; openMSX blanks from display line 15
    // (blank band = display lines 15-16, HUD's 2nd text row fully rendered incl.
    // its bottom row at display line 14). The prior L+1 rule blanked from line
    // 14, clipping the bottom pixel row of the HUD glyphs (the human-reported
    // "letters chopped at the bottom"). L+2 lands the split exactly on openMSX's
    // scanline for ALL three per-frame writes (measured in-line tstates 44/98/
    // 169 -> uniform +1-line delta). This is a register/raster-level rule, not
    // an Aleste special-case: it shifts EVERY cycle-timed mid-frame VDP write
    // (R#23 splits, R#1 BL bands, R#7/palette raster bars) by the same margin,
    // for every game. raster_display_line() itself is UNCHANGED (line-interrupt
    // matching + S#2 VR/HR still use the raw raster, mirroring openMSX where the
    // render-sync rounding is independent of the VDP line-interrupt counter).
    const int target = line + 2;
    // M51 Task 2 trace: the beam seam's commit decision inputs (context for
    // event classes 1-4; identifies which VDP write opened/advanced the split).
    if (devices::video::m51_sprite_trace_enabled()) {
        devices::video::m51_sprite_trace(
            "SEAM raster=%d target=%d acc_wm=%d sprite_wm=%d last_beam=%d", line, target,
            machine_.scanline_accumulator_.watermark(),
            machine_.vdp_.sprite_engine().watermark(), last_beam_commit_target_);
    }
    // M49-S2 (docs/m49-planner-package.md §3 S2, backlog D9): keep the incremental
    // sprite plane in pace with the background BEFORE the background commits, at the
    // IDENTICAL beam+2 boundary. Driving the sprite check FIRST (from the CURRENT
    // pre-write registers/VRAM) makes the background read a per-line-live sprite
    // table for the lines it is about to commit, AND makes a mid-frame sprite-
    // relevant register change -- whose new value has NOT yet applied at this point
    // (io_write calls on_before_state_change before decode/commit, incl. the #99
    // two-write DATA byte) -- split the sprite plane at exactly the same line as the
    // background split (AC-S2). commit_sprite_split()'s check_until is advance-only,
    // so a non-sprite write is a cheap no-op once the beam's line is already checked.
    // `wrap` uses the same raster-DECREASE test the background uses below.
    machine_.vdp_.commit_sprite_split(target, target < last_beam_commit_target_);
    // M44 (DEF-M44-CMDSYNC Phase 1, DEC-0065): the command-engine row sink can
    // advance the accumulator watermark PAST the beam within a frame (committing
    // command rows at their destination lines). A subsequent same-frame seam
    // write would then have target < watermark and trip sync_to_line()'s
    // wrap-safety finalize() mid-frame. Distinguish the two cases by the raster
    // line's direction: a DECREASE (target < the last beam target) is a genuine
    // frame wrap or a D10 geometry jump -> keep the accumulator's wrap-safety
    // finalize (call sync_to_line unconditionally, as before). Otherwise the
    // raster is monotonic within the frame -> ADVANCE-ONLY (never finalize).
    //
    // Byte-identical to the pre-M44 seam for every non-command path: with the
    // command sink inert the watermark only ever advances via this seam, so the
    // beam is monotonic and target >= watermark always held -- the advance-only
    // branch then calls sync_to_line exactly when the old unconditional call
    // would have done non-trivial work, and skips only the target==watermark
    // no-op. The wrap branch is unchanged.
    if (target < last_beam_commit_target_) {
        machine_.scanline_accumulator_.sync_to_line(target);  // genuine wrap / D10 seal
    } else if (target > machine_.scanline_accumulator_.watermark()) {
        machine_.scanline_accumulator_.sync_to_line(target);  // monotonic advance
    }
    last_beam_commit_target_ = target;
}

void Hbf1xvMachine::VdpRenderSyncAdapter::on_commit_up_to(const int display_line) {
    // M44 (DEF-M44-CMDSYNC Phase 1, DEC-0065): the command-engine per-
    // destination-row commit primitive. V9958Vdp::command_row_sync has already
    // applied the bitmap-mode / visible-page / active-display gates and computed
    // `display_line` = the exact vertical-scroll inverse. Here we commit
    // [watermark, display_line) from the current live VRAM, mirroring
    // on_before_state_change but with NO +2 beam margin (the destination-line
    // mapping is exact, not a beam-rounding).
    if (suspended_) {
        return;  // debug_io_write exclusion (same as on_before_state_change)
    }
    machine_.scanline_accumulator_.mark_completed_frame_stale();
    // WRAP guard (§2.4 step 4): ADVANCE-ONLY. Skip when the destination display
    // line is at or below the accumulation point -- that row was already swept
    // (it reappears next frame, exactly as on real hardware) and a sync_to_line
    // below the watermark would otherwise seal a partial frame mid-command.
    if (display_line <= machine_.scanline_accumulator_.watermark()) {
        if (devices::video::m51_sprite_trace_enabled()) {
            devices::video::m51_sprite_trace(
                "CMDCOMMIT-SKIP display_line=%d acc_wm=%d sprite_wm=%d", display_line,
                machine_.scanline_accumulator_.watermark(),
                machine_.vdp_.sprite_engine().watermark());
        }
        return;
    }
    // M51 Task 2 trace (event classes 3+4): the command-row commit range
    // [acc_wm, display_line) with the SPRITE watermark at this instant, plus
    // the per-line visible-sprite counts the imminent sync_to_line() will
    // composite from, over the designated player band
    // (SONY_MSX_M51_TRACE_BAND="lo:hi", default 148:176 -- the Aleste 2 plane
    // rows; the counts here ARE the composite-time values because
    // sync_to_line() renders synchronously right after this hook).
    if (devices::video::m51_sprite_trace_enabled()) {
        static const auto band = [] {
            int lo = 148;
            int hi = 176;
            if (const char* b = std::getenv("SONY_MSX_M51_TRACE_BAND")) {
                (void)std::sscanf(b, "%d:%d", &lo, &hi);
            }
            return std::pair<int, int>(lo, hi);
        }();
        const int acc_wm = machine_.scanline_accumulator_.watermark();
        const int sprite_wm = machine_.vdp_.sprite_engine().watermark();
        devices::video::m51_sprite_trace("CMDCOMMIT range=[%d,%d) sprite_wm=%d", acc_wm,
                                         display_line, sprite_wm);
        const int lo = acc_wm > band.first ? acc_wm : band.first;
        const int hi = display_line < band.second ? display_line : band.second;
        for (int line = lo; line < hi; ++line) {
            devices::video::m51_sprite_trace(
                "BAND-ROW line=%d visible=%zu past_sprite_wm=%d", line,
                machine_.vdp_.sprite_engine().visible_sprites(line).size(),
                line >= sprite_wm ? 1 : 0);
        }
    }
    // M51 (DEC-0078) fix, branch (a) shape (i): pace the render-only sprite
    // pass to the SAME exclusive boundary BEFORE the background commits, so no
    // committed row composites from a per-line buffer that is empty purely
    // because of the lazy-open clear (the Aleste 2 / Firebird / Laydock 2
    // sprite-disappearance mechanism -- Task 2 trace: "CMDCOMMIT range=[69,231)
    // sprite_wm=69" sealing plane rows 148-175 with visible=0). This is the
    // consumer-side sync contract openMSX documents and implements
    // (PixelRenderer.cc:580-584 / SpriteChecker.hh:242-247, effect only) and
    // the M49 seam already applies on the beam path (commit_sprite_split at
    // on_before_state_change); the command sink now honors it too.
    // Advance-only-when-active: see V9958Vdp::commit_sprite_rows.
    machine_.vdp_.commit_sprite_rows(display_line);
    machine_.scanline_accumulator_.sync_to_line(display_line);
}

void Hbf1xvMachine::wire_bus() {
    // --- Memory fabric (SlotBus) ---
    // HB-F1XV slot/sub-slot/page population, from the machine XML
    // references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml (§2 of
    // docs/m13-planner-package.md). Both primary slots 0 and 3 are expanded
    // (each has four <secondary> children: XML lines 85-116 and 123-199; A-6
    // corrects the M11 wiring that expanded only slot 3).
    slot_bus_.set_expanded(0, true);
    slot_bus_.set_expanded(3, true);

    // Slot 0-0: MSX BIOS + BASIC ROM, <mem base=0x0000 size=0x8000> -> pages 0-1
    // (XML:87-98). Reset fetch origin once #A8=0.
    slot_bus_.attach(0, 0, 0, &bios_rom_);
    slot_bus_.attach(0, 0, 1, &bios_rom_);
    // Slot 0-1 / 0-2 empty (XML:101,103); slot 0-3 is the Halnote-mapped
    // MSX-JE firmware ROM (XML:106-114, `<mem base="0x0000" size="0x10000">`
    // -- ALL 4 pages of this sub-slot, A-M20-9, M20 backlog B6, closes B4
    // together). HalnoteRom carries zero slot-routing knowledge of its own;
    // it just answers the raw 16-bit CPU address handed to it.
    for (int page = 0; page < devices::chipset::SlotBus::kPages; ++page) {
        slot_bus_.attach(0, 3, page, &halnote_);
    }

    // Slot 3-0: 64 KB main-RAM MemoryMapper, pages 0-3 (XML:125-130). The mapper
    // RAM consumes mapper_io_'s live #FC-#FF segments (fact-sheet §4/§9).
    for (int page = 0; page < devices::chipset::SlotBus::kPages; ++page) {
        slot_bus_.attach(3, 0, page, &ram_mapper_);
    }
    // Slot 3-1: MSX Sub ROM at page 0 (<mem 0x0000/0x4000>, XML:134-145) and MSX
    // Kanji Driver/BASIC at pages 1-2 (<mem 0x4000/0x8000>, XML:146-157) coexist
    // at disjoint pages; page 3 of 3-1 stays open-bus.
    slot_bus_.attach(3, 1, 0, &sub_rom_);
    slot_bus_.attach(3, 1, 1, &kanji_rom_);
    slot_bus_.attach(3, 1, 2, &kanji_rom_);
    // Slot 3-2: WD2793 FDC (Sony connection style), page 1 (<mem 0x4000/0x4000>,
    // rom_visibility page 1, XML:161-176). M16 attaches the SonyFdc decode
    // (which wraps disk_rom_) here in place of the bare ROM, so page-1 reads
    // route DISK ROM + the 0x7FF8-0x7FFF WD2793/glue register window (planner
    // §3.1). The FDC core advances off the deterministic FdcClock.
    fdc_.attach_clock_source(&fdc_clock_);
    fdc_.attach_drive(&disk_drive_);
    disk_drive_.attach_image(&disk_image_);
    slot_bus_.attach(3, 2, 1, &sony_fdc_);
    // Slot 3-3: MSX-MUSIC ROM PRESENCE, page 1 (<mem 0x4000/0x4000>, XML:180-196).
    // A-M17-2 (hard regression guard): this attachment is UNCHANGED by M17 --
    // the real MSXMusic device has no memory-space register overlay (unlike
    // M16's SonyFdc, which had to wrap disk_rom_), so fmmusic_rom_ stays a
    // plain ROM window. The YM2413 OPLL device is attached separately below,
    // only on io_bus_ #7C/#7D.
    slot_bus_.attach(3, 3, 1, &fmmusic_rom_);

    // External cartridge bays: primary slots 1 and 2 (M19, backlog B7). Both
    // are bare, unexpanded XML `<primary external="true">` elements (A-M19-1,
    // Sony_HB-F1XV.xml:119,121) -- NO set_expanded call for either; each
    // CartridgeSlot is attached at ALL 4 pages of its own primary slot, sub 0
    // (SlotBus::sub_for_page already returns 0 unconditionally for a
    // non-expanded primary, so this requires zero change to SlotBus itself).
    for (int page = 0; page < devices::chipset::SlotBus::kPages; ++page) {
        slot_bus_.attach(1, 0, page, &cartridge_slot1_);
        slot_bus_.attach(2, 0, page, &cartridge_slot2_);
    }

    // --- I/O fabric (IoBus) ---
    // Full i8255 PPI on #A8-#AB (M15-S4, expands the M11 port-A-only seam), plus
    // the S1985 straight-alias mirror of all four PPI ports #A8-#AB -> #AC-#AF
    // (fact-sheet §3, §10). #A8 slot-select is preserved byte-for-byte (X1); the
    // keyboard rows on #A9-#AB (and their mirrors) are now live.
    io_bus_.attach(0xA8, &ppi_);
    io_bus_.attach(0xA9, &ppi_);
    io_bus_.attach(0xAA, &ppi_);
    io_bus_.attach(0xAB, &ppi_);
    io_bus_.register_mirror(0xA8, 0xAC);
    io_bus_.register_mirror(0xA9, 0xAD);
    io_bus_.register_mirror(0xAA, 0xAE);
    io_bus_.register_mirror(0xAB, 0xAF);

    // PSG (YM2149) on #A0-#A2 (M15-S1), replacing the M11 open-bus seam. The
    // joystick peripheral backs PSG port A/B (R14/R15) — X5: peripheral -> PSG.
    psg_.attach_port_source(&joystick_);
    io_bus_.attach(0xA0, &psg_);
    io_bus_.attach(0xA1, &psg_);
    io_bus_.attach(0xA2, &psg_);

    // M39-A: route PPI port-C bit-7 (key-click 1-bit DAC) edges into the
    // additive ClickDac mixer source, cycle-stamped off the scheduler. Both
    // seams are default-inert until click_dac_ capture is enabled (frontend /
    // audio-dump), so this attach changes no existing behaviour (idle-byte
    // identity): bit 7 = 0 at reset -> no edges -> the mixer's click term is
    // exactly 0. The #A8 slot-select path is byte-untouched.
    ppi_.attach_click_sink(&ppi_click_adapter_);
    ppi_.attach_cycle_source(&ppi_cycle_clock_);

    // M39-A Fix B: cycle-stamp source for the PSG sync-before-change seam (the
    // CONFIRMED digitized-voice fix -- software-PCM volume writes). Inert until
    // the frontend enables sync via psg().set_audio_sync_enabled(); with sync
    // off, write_register() does not sync, so every existing oracle is
    // byte-identical.
    psg_.attach_audio_cycle_source(&psg_cycle_clock_);

    // Cassette interface (M18-S3, backlog C7): read-only motor/output
    // derivation from ppi_ (bound by reference at construction, cassette_
    // {ppi_}); its deterministic synthetic-tape input model advances
    // pull-style off cassette_clock_ (A-M18-12 -- never wired into the CPU
    // stepping loop). Its output feeds joystick_'s PSG R14 bit7 seam via the
    // CassetteInputSource contract (A-M18-10), replacing the M15 hardcoded
    // idle-high stub with a nullptr-safe, regression-guarded injection.
    cassette_.attach_clock_source(&cassette_clock_);
    joystick_.attach_cassette_input_source(&cassette_);

    // Ren-Sha Turbo autofire (M25, backlog C8 sub-item). Additive OR-mask
    // attach points (M25-S3): keyboard row 8 bit0 (SPACE) and PSG R14 bit4
    // (joystick trigger A), driven by the deterministic emulated-cycle
    // rensha_clock_ adapter (X-pattern of rtc_clock_/fdc_clock_/
    // cassette_clock_). speed_=0 (disabled) post-reset() is the regression-
    // safe default -- this attach is unconditional, but produces zero
    // observable effect until rensha_turbo().set_speed() is called.
    rensha_turbo_.attach_clock_source(&rensha_clock_);
    keyboard_.attach_rensha_turbo(&rensha_turbo_);
    joystick_.attach_rensha_turbo(&rensha_turbo_);

    // Kanji font ROM on #D8-#DB (M18-S1, backlog B5). Direct-port dispatch
    // (A-M18-1: MSXKanji, NOT the switched-I/O MSXKanji12 used by other MSX
    // machines) -- the device itself decodes port & 0x03 internally.
    io_bus_.attach(0xD8, &kanji_font_rom_);
    io_bus_.attach(0xD9, &kanji_font_rom_);
    io_bus_.attach(0xDA, &kanji_font_rom_);
    io_bus_.attach(0xDB, &kanji_font_rom_);

    // Printer port on #90-#97 (M18-S2, A-M18-5 -- the XML claims the fuller
    // 8-port window directly, wider than the backlog's "#90/#91" shorthand,
    // §2.7 of docs/m18-planner-package.md). The device decodes port & 0x03
    // (mod-4 aliasing) internally.
    for (std::uint16_t port = 0x90; port <= 0x97; ++port) {
        io_bus_.attach(static_cast<std::uint8_t>(port), &printer_);
    }

    // YM2413 (OPLL) on #7C/#7D (M17-S3, backlog B3), the real MSX-MUSIC I/O
    // ports (A-M17-1; Sony_HB-F1XV.xml:194 `<io base="0x7C" num="2" type="O"/>`)
    // alongside the unmodified fmmusic_rom_ attach above (A-M17-2). The M28-S1
    // (E2) write-timing gate's clock source is wired unconditionally, but the
    // gate itself defaults OFF (Ym2413Opll::write_timing_enforced() ==
    // false), so this attach changes no existing behaviour by itself.
    ym2413_.attach_clock_source(&ym2413_clock_);
    io_bus_.attach(0x7C, &ym2413_);
    io_bus_.attach(0x7D, &ym2413_);

    // RTC (RP5C01) on #B4/#B5 (M15-S3), replacing the M11 open-bus seam. The RTC
    // advances its time READ-ONLY off the deterministic scheduler clock, and its
    // data path is gated by the #F5 system-control CLOCK-IC enable (bit 7).
    rtc_.attach_clock_source(&rtc_clock_);
    rtc_.attach_clock_gate(&system_control_);
    io_bus_.attach(0xB4, &rtc_);
    io_bus_.attach(0xB5, &rtc_);
    io_bus_.attach(0xF5, &system_control_);

    // #F4 reset-status latch (boot-logo fix; Sony_HB-F1XV.xml
    // `<ResetStatusRegister>` `<inverted>false</inverted>` `<io base="0xF4"
    // num="1"/>`). Cold power-up leaves bit 7 clear, which is what tells the
    // MSX2+ BIOS to run the animated MSX logo before reaching BASIC; the BIOS
    // then writes bit 7 set (observed on openMSX: #F4 reads 0x00 at power-on,
    // 0x80 once boot completes). Previously this port was open-bus 0xFF, so
    // the BIOS always took the warm-restart path and never showed the logo.
    io_bus_.attach(0xF4, &reset_status_);

    // VDP port mirror #98-#9B -> #9C-#9F (fact-sheet §7). The alias was pre-wired
    // in M11; the M14 V9958 is now attached on the four base ports, so it is
    // automatically reachable on #9C-#9F (A-1/A-2; the VDP decodes port & 0x03).
    io_bus_.register_mirror(0x98, 0x9C);
    io_bus_.register_mirror(0x99, 0x9D);
    io_bus_.register_mirror(0x9A, 0x9E);
    io_bus_.register_mirror(0x9B, 0x9F);

    // Attach the V9958 VDP on its four base ports (M14-S5). The S1985 straight-
    // alias mirror above routes #9C-#9F to the SAME device. The VDP owns the
    // 128 KB VRAM; the CPU reaches VRAM only through #98/#99 (+ the mirror).
    io_bus_.attach(0x98, &vdp_);
    io_bus_.attach(0x99, &vdp_);
    io_bus_.attach(0x9A, &vdp_);
    io_bus_.attach(0x9B, &vdp_);
    // The VDP owns its /INT line and drives the CPU through the adapter (M14-S4).
    vdp_.set_irq_line(&cpu_irq_adapter_);
    // S#2 VR/HR raster-position clock (bug fix, post-M28): read-only,
    // pull-style, consulted only from peek_status_register(2) -- never wired
    // into step_cpu_instruction()/run_cycles()/run_frame().
    vdp_.attach_clock_source(&vdp_raster_clock_);
    // M32-S2 render-sync seam: per-line frame accumulation as the raster
    // advances (Defect A of DEC-0039; docs/m32-planner-package.md §2.3).
    vdp_.attach_render_sync(&render_sync_adapter_);

    // Mapper I/O readback on #FC-#FF (fact-sheet §4), pattern configured by S1985.
    s1985_engine_.configure_mapper(mapper_io_);
    for (std::uint16_t port = 0xFC; port <= 0xFF; ++port) {
        io_bus_.attach(static_cast<std::uint8_t>(port), &mapper_io_);
    }

    // Switched I/O on #40-#4F with the S1985 backup RAM (ID 0xFE) attached
    // (fact-sheet §6, §10).
    switched_io_.attach(&s1985_engine_);
    for (std::uint16_t port = 0x40; port <= 0x4F; ++port) {
        io_bus_.attach(static_cast<std::uint8_t>(port), &switched_io_);
    }
}

void Hbf1xvMachine::cold_boot() {
    scheduler_.reset();
    // Main RAM power-on content (A-5): the XML alternating 00/FF initialContent,
    // NOT all-zero (Sony_HB-F1XV.xml:129; openMSX Ram::clear, Ram.cc:37-78).
    // (M36, DEC-0050: the speculative internal FM-PAC `sram_` region was
    // removed -- battery SRAM is a peripheral, on the external FM-PAC cartridge.)
    initialize_dram_pattern();

    // The V9958 VDP resets its own state, including clearing its 128 KB VRAM to
    // zero (matching the retired vram_.clear()), reloading the boot palette, and
    // releasing the /INT line (M14-S1/S4).
    vdp_.reset();
    // M32: cold power-up clears the scanline accumulator (no accumulated
    // lines, no completed frame) and invalidates the line-interrupt cache.
    // last_vsync_cycle_ returns to 0 alongside the scheduler reset above --
    // the scheduler restarts at cycle 0, so every vsync-relative raster
    // derivation (VdpRasterClock, the line-int schedule) must restart from
    // the same origin (a machine that ran frames before this cold_boot would
    // otherwise underflow cpu_tstates_since_vsync(); latent since M23, first
    // load-bearing for the M32 line-int cache).
    scanline_accumulator_.reset();
    line_int_cache_valid_ = false;
    line_int_next_cycle_ = kLineIntNever;
    last_vsync_cycle_ = 0;

    // Reset the S1985 chipset volatile state (primary select + sub-slot regs -> 0).
    slot_bus_.reset();
    switched_io_.reset();
    s1985_engine_.reset();

    // Reset the M15 devices to deterministic power-on state (X3). New devices are
    // reset after the existing ones; ordering additions only.
    keyboard_.reset();
    joystick_.reset();
    ppi_.reset();
    psg_.reset();
    // M39-A: clear the 1-bit-DAC edge timeline/integrator (capture-enabled
    // wiring config is preserved -- the frontend sets it once). consumed_cycle_
    // returns to 0 alongside the scheduler reset above, so edge cycle stamps
    // stay aligned across a cold boot.
    click_dac_.reset();
    ym2413_.reset();  // M17: zeroes all 64 registers + the address latch (A-M17-4)
    system_control_.reset();
    // Cold power-up clears the #F4 reset-status latch (bit 7 clear = cold
    // boot; the BIOS shows the boot-logo animation and then sets bit 7). Real
    // hardware preserves this latch across a warm RESET-button reset; this
    // machine model only exposes cold power-up.
    reset_status_.power_on_reset();
    rtc_.reset();
    halnote_.reset();  // M20: bank/flag state cleared + sram_ zeroed (A-M20-12)

    // M18 peripheral I/O devices (backlog B5/C7): Kanji font ROM address
    // counters, printer strobe/data/capture state, and the cassette
    // interface's tape/sample state all return to their deterministic
    // power-on defaults.
    kanji_font_rom_.reset();
    printer_.reset();
    cassette_.reset();

    // M25 hardware PAUSE + Speed Controller + Ren-Sha Turbo (backlog C8):
    // deterministic power-on idle defaults -- disengaged/speed-level-0 gate
    // (pause_controller_.cpu_should_pause() == false) and disabled autofire
    // (rensha_turbo_.signal() == false), matching the pre-M25 behavior for
    // any test/tool that does not explicitly engage either mechanism.
    pause_controller_.reset();
    rensha_turbo_.reset();

    // FDC (M16): reset the WD2793 core (TR=0xFF), the drive mechanism, and the
    // Sony glue latches; re-mount the deterministic default 720 KB medium so every
    // cold boot presents byte-identical disk content.
    disk_image_ = devices::fdc::DiskImage();
    disk_drive_.reset();
    disk_drive_.attach_image(&disk_image_);
    fdc_.reset();
    sony_fdc_.reset();

    // External cartridge slots (M19): reinitialize bank state of whatever is
    // currently loaded; no-op when empty; NEVER unloads (A-M19-9 -- matches
    // real hardware power-cycle-with-cartridge-inserted semantics).
    cartridge_slot1_.reset();
    cartridge_slot2_.reset();

    // Load the 16-byte S1985 backup RAM from its .sram file when configured
    // (M15-S5, backlog C4). Absent/short file -> deterministic zero state (the
    // reset() above), preserving the M11 golden.
    if (!backup_ram_path_.empty()) {
        s1985_engine_.load_backup_ram(backup_ram_path_);
    }

    // Load the 16 KB Halnote/MSX-JE SRAM from its file when configured (M20,
    // mirrors the S1985 backup-RAM precedent exactly, A-M20-12). Absent/short
    // file -> the halnote_.reset() zeroing above stands (deterministic zero).
    if (!halnote_sram_path_.empty()) {
        halnote_.sram().load(halnote_sram_path_);
    }

    // Populate the ROM devices from the local bios/ assets (missing-asset policy
    // A-7: absent/unreadable/wrong-size -> 0xFF fill + recorded diagnostic).
    load_rom_assets();

    // --- Authentic reset slot default (A-2 / discharges M11 R-1) ---
    // Real hardware resets #A8 to 0 and every sub-slot register to 0, so page 0
    // resolves to slot 0-0 = BIOS ROM and the Z80 fetches the reset vector at
    // 0x0000 from BIOS (Zilog Z80A §7; S1985 §3). This supersedes the M11
    // bring-up default (#A8 = 0xFF), now that slot 0 is populated with the BIOS
    // ROM device. Tests that run a program from RAM call map_flat_ram() to page
    // the 64 KB RAM in explicitly (M13-S4 reconciliation).
    ppi_.io_write(0xA8, 0x00);  // drives slot_bus_ primary select -> slot 0 (X1 preserved)
    slot_bus_.write_ffff(0x00);  // page-3 sub-slot register -> sub 0

    cpu_.reset();
    cpu_.set_interrupt_mode(devices::cpu::InterruptMode::Im1);
    frame_count_ = 0;

    // The event log is a fresh, deterministic stream per boot. Enable logging
    // BEFORE cold_boot to capture the Reset event that opens the stream.
    debug_event_log_.clear();
    if (event_logging_enabled_) {
        debug_event_log_.record(DebugEventType::Reset, elapsed_cycles(), "cold_boot");
    }
}

void Hbf1xvMachine::initialize_dram_pattern() {
    // Decoded XML initialContent (Sony_HB-F1XV.xml:129 comment):
    //   (chr(0)+chr(255))*128 + (chr(255)+chr(0))*128  -> a 512-byte pattern:
    //   bytes 0..255   alternate 00,FF (even index 00, odd FF)
    //   bytes 256..511 alternate FF,00 (even index FF, odd 00)
    // openMSX repeats this pattern over the whole RAM region (Ram::clear,
    // Ram.cc:66-73). M42 (DEC-0061): iterate over dram_.size() (the fitted size,
    // 64/128/256/512 KB), not the kDramBytes default -- so a larger --ram region
    // is fully pattern-filled; at the default 64 KB this is byte-identical.
    for (std::size_t i = 0; i < dram_.size(); ++i) {
        const std::size_t p = i & 0x1FFu;  // position within the 512-byte pattern
        std::uint8_t value;
        if (p < 256) {
            value = (p & 1u) ? 0xFF : 0x00;
        } else {
            value = (p & 1u) ? 0x00 : 0xFF;
        }
        dram_.write(i, value);
    }
}

void Hbf1xvMachine::load_rom_assets() {
    rom_diagnostics_.clear();
    RomAssetLoader loader(asset_root_);

    // filename / expected window size / slot-role label. Sizes are the XML <mem>
    // window sizes (§2). Local SHA1s were confirmed to match the XML "confirmed
    // by Meits" revisions via tools/checksum-assets.ps1 (A-1) — never asserted here.
    // M50-S3 (DEC-0077): the FILENAME comes from bios_filenames_ (config-fed,
    // default = the strict HB-F1XV spec set) so a user can point at differently-
    // named BIOS files via config; the expected size + role label stay code-owned
    // (a spec fact, not a user knob). A bare machine keeps the default filenames,
    // byte-identical to the pre-M50 literals.
    bios_rom_.set_image(
        loader.load({bios_filenames_.bios, 0x8000, "slot 0-0 pages 0-1 (BIOS+BASIC)"}));
    sub_rom_.set_image(loader.load({bios_filenames_.sub, 0x4000, "slot 3-1 page 0 (SUB)"}));
    kanji_rom_.set_image(
        loader.load({bios_filenames_.kanji_driver, 0x8000, "slot 3-1 pages 1-2 (Kanji driver)"}));
    disk_rom_.set_image(
        loader.load({bios_filenames_.disk, 0x4000, "slot 3-2 page 1 (DISK presence)"}));
    fmmusic_rom_.set_image(
        loader.load({bios_filenames_.fm_music, 0x4000, "slot 3-3 page 1 (FM-MUSIC presence)"}));
    kanji_font_rom_.set_image(
        loader.load({bios_filenames_.kanji_font, 0x40000, "Kanji font ROM (I/O #D8-#DB)"}));
    halnote_.set_image(
        loader.load({bios_filenames_.firmware, 0x100000, "slot 0-3 pages 0-3 (Halnote/MSX-JE firmware)"}));

    // Diagnostics go to the machine diagnostics list only (rom_diagnostics()),
    // NOT the execution-event log: the event stream must stay byte-deterministic
    // and independent of whether the working directory can resolve bios/ (the M10
    // event-log golden depends on it). rom_diagnostics() is the authoritative,
    // never-fabricated missing-asset record; a green run has it empty.
    rom_diagnostics_ = loader.diagnostics();
}

void Hbf1xvMachine::map_flat_ram() {
    // Page the 64 KB mapper RAM (slot 3-0) into all four CPU pages as a flat,
    // linear 64 KB view (see the header doc). #A8 = 0xFF -> every page primary
    // slot 3; slot-3 sub-slot register 0 -> sub 0 = RAM mapper; mapper segments
    // {0,1,2,3} -> page p maps to physical p*0x4000.
    ppi_.io_write(0xA8, 0xFF);
    slot_bus_.write_ffff(0x00);
    for (std::uint16_t page = 0; page < 4; ++page) {
        mapper_io_.io_write(static_cast<std::uint16_t>(0xFC + page), static_cast<std::uint8_t>(page));
    }
}

void Hbf1xvMachine::set_asset_root(std::filesystem::path root) {
    asset_root_ = std::move(root);
}

const std::filesystem::path& Hbf1xvMachine::asset_root() const {
    return asset_root_;
}

void Hbf1xvMachine::set_bios_filenames(EmulatorConfig::BiosRoms names) {
    bios_filenames_ = std::move(names);
}

const EmulatorConfig::BiosRoms& Hbf1xvMachine::bios_filenames() const {
    return bios_filenames_;
}

const std::vector<std::string>& Hbf1xvMachine::rom_diagnostics() const {
    return rom_diagnostics_;
}

void Hbf1xvMachine::on_vsync_boundary() {
    // M26-S1 (docs/m26-planner-package.md §2.3): a mechanical extraction of
    // run_frame()'s pre-M26 body, EXCLUDING the scheduler_.tick(kFrameCycles)
    // call -- a real-time driver's own step_cpu_instruction() sub-loop has
    // already advanced the scheduler by an equivalent amount before calling
    // this. Same four operations, same order as before: frame counter, VDP
    // VBlank delivery, the M25 Speed-Controller duty-cycle hook, and the M23-S2
    // last-vsync bookkeeping.
    ++frame_count_;

    // M51 Task 2 trace (event class 5): the boundary BEFORE on_vsync's
    // frame-atomic recompute and the accumulator finalize -- names how far the
    // committed (sealed) region reached when the frame ended.
    if (devices::video::m51_sprite_trace_enabled()) {
        devices::video::m51_sprite_trace("BOUNDARY acc_wm=%d sprite_wm=%d",
                                         scanline_accumulator_.watermark(),
                                         vdp_.sprite_engine().watermark());
    }

    // Deterministic per-frame VBlank delivery (M14-S5). run_frame advances the
    // clock a whole frame atomically, so the VDP VBlank is modeled at the frame
    // boundary: on_vsync sets S#0 F and, when R#1 IE0 is enabled, asserts the
    // vertical /INT line. The CPU accepts it on the next step_cpu_instruction
    // (which level-samples the held line). Exact sub-frame raster position is
    // DEFERRED (backlog D4).
    vdp_.on_vsync();
    // M32 (Defect A, §2.4): seal the frame that just ended. Every line the
    // raster passed is already committed (render-sync seam); finalize renders
    // the remaining lines from live end-of-frame registers and stores the
    // completed frame render_frame() returns at the boundary.
    //
    // ORDERING -- deliberate deviation from the planner package's "finalize
    // BEFORE vdp_.on_vsync()" clause, in service of the package's own hard
    // oracles AC-4/AC-5 (committed-evidence byte-identity): on_vsync()
    // mutates no register/VRAM/palette state the background renderer
    // consumes, but it DOES (a) recompute the sprite visibility tables from
    // the frame's own end-of-frame VRAM (v9958_vdp.cpp on_vsync ->
    // sprite_engine_.recompute_frame) and (b) advance the R#13 blink
    // countdown. The legacy snapshot renderer that produced every committed
    // evidence frame ran AFTER on_vsync() (step ... on_vsync_boundary() ...
    // render_frame()), i.e. against the post-recompute sprite table and
    // post-decrement blink state. Finalizing BEFORE on_vsync() would render
    // against the PREVIOUS boundary's sprite table -- one frame of sprite
    // lag vs both the legacy pipeline and real hardware (whose vblank-
    // handler attribute writes for frame F land after boundary F-1's
    // recompute and first appear at boundary F -- exactly the table this
    // finalize must use). Sprite per-line live fetching remains the named
    // D9 remainder.
    scanline_accumulator_.finalize(devices::video::Field::Progressive);
    // M25 Speed-Controller duty-cycle hook (backlog C8, planner §2.3 point
    // 4): advances the PAUSE controller's VBlank-synced duty-cycle window;
    // its level defaults to 0 (never paused) post-reset(), so this is a
    // no-op by default (regression guard).
    pause_controller_.on_vsync();
    // M23-S2 bookkeeping: remember the cycle count at the most recent VSync
    // so cycles_since_last_vsync()/vdp_cycle_position() can report a raster
    // position relative to it. Does not affect scheduling/CPU timing.
    last_vsync_cycle_ = elapsed_cycles();

    // DEC-0052 live stream-capture: at this clean frame boundary (frame_count_
    // incremented, id stable) write one full per-component snapshot bundle
    // into <debug_root>/snapshot/stream_<id>/<frame-id>/ -- the frame-by-frame
    // state evolution. Guarded -- disarmed => byte-for-byte unchanged. On a
    // HALT that finalized mid-frame, stream_active_ is already false, so the
    // crash frame is captured once (by finalize) and not duplicated here.
    //
    // DEC-0052 stream-light: in the lightweight mode the heavy per-frame
    // bundle is suppressed (its per-frame ~200 KB I/O is what bogs a long
    // armed session down); instead a coarse anchor snapshot is written every
    // kStreamLightSnapshotInterval frames (~2 s) as a periodic recovery
    // point. The crash/HALT/manual finalize snapshot is written separately.
    // Heavy mode (stream_light_ == false) keeps the every-frame bundle
    // exactly as before.
    if (stream_active_) {
        const bool write_bundle =
            !stream_light_ || (frame_count_ % kStreamLightSnapshotInterval == 0);
        if (write_bundle) {
            // Heavy-mode notes stay byte-for-byte identical to the pre-light
            // path (the regression oracle); light coarse anchors add a
            // self-identifying note so a diagnostic can tell a periodic
            // anchor apart from a heavy per-frame bundle.
            std::vector<std::string> notes = {
                "stream_id=" + stream_id_,
                "stream_frame=1",
                "instructions_seen=" + debug_format::to_dec(stream_seq_),
            };
            if (stream_light_) {
                notes.push_back("stream_light=1");
                notes.push_back("light_anchor_interval=" +
                                debug_format::to_dec(kStreamLightSnapshotInterval));
            }
            write_snapshot("stream_" + stream_id_ + "/" + snapshot_id(), notes);
        }
    }
}

void Hbf1xvMachine::run_frame() {
    scheduler_.tick(kFrameCycles);
    on_vsync_boundary();
}

void Hbf1xvMachine::run_frames(const std::uint32_t frames) {
    for (std::uint32_t i = 0; i < frames; ++i) {
        run_frame();
    }
}

void Hbf1xvMachine::run_cycles(const std::uint64_t cycles) {
    scheduler_.tick(cycles);
}

bool Hbf1xvMachine::run_until_cycle(const std::uint64_t target_cycle) {
    const std::uint64_t now = elapsed_cycles();
    if (target_cycle <= now) {
        return false;
    }

    run_cycles(target_cycle - now);
    return true;
}

std::uint32_t Hbf1xvMachine::step_cpu_instruction() {
    // M25 hardware PAUSE / Speed-Controller gate (backlog C8, planner
    // §2.3/§2.4). Consulted first, before any opcode decode. When engaged
    // (button pressed, or the Speed Controller's own duty-cycle window), this
    // skips cpu_.step() entirely -- no M1/opcode-fetch cycle occurs, so
    // PC/R/every register stay frozen (A-M25-2's literal reading of the
    // S1985 fact-sheet §9: "physically halts the CPU and cannot be bypassed
    // in software"). Different from the Z80's own HALT instruction
    // (CPU-internal, R keeps incrementing, released by any interrupt) -- see
    // the planner package §2.3 comparison table. Everything below is
    // otherwise byte-for-byte unchanged from pre-M25. VDP/RTC/FDC clock
    // sources are unaffected (their crystal is not gated by the Z80 WAIT pin
    // on real hardware) -- only CPU decode/execute is suppressed. The 1
    // T-state idle charge is a documented modeling choice (R-M25-7), not a
    // hardware-quantized fact -- the finest-grained unit this whole-
    // instruction-atomic engine can charge for an indefinitely-held external
    // WAIT condition, with no overshoot risk.
    if (pause_controller_.cpu_should_pause()) {
        constexpr std::uint32_t kPausedIdleTStates = 1;
        scheduler_.tick(kPausedIdleTStates);
        return kPausedIdleTStates;
    }

    // Level-sample the VDP's held /INT (M14-S4, R-1). The Z80A accept path
    // clears its internal pending flag on service, but real hardware holds
    // /INT until the S#0 status read releases it. Re-asserting the request
    // from the VDP's held level each step models that hold without
    // clobbering an externally-injected interrupt (we only assert here; the
    // VDP releases via the adapter's set_irq(false) on the status read). When
    // the VDP line is idle this is a no-op, so manual interrupt-injection
    // tests are unaffected.
    if (vdp_.irq_active()) {
        cpu_.request_maskable_interrupt();
    }

    const bool halted_before = cpu_.state().halted();
    const std::uint16_t pre_pc = cpu_.state().regs().pc;
    const std::uint8_t opcode0 = bus_.read(pre_pc);

    // DEC-0052 live stream-capture: snapshot the PRE-execution CPU record and
    // the current #A8 primary-slot select before cpu_.step() mutates the
    // registers. Guarded -- when the stream is disarmed this whole block is
    // skipped, so the default path is byte-for-byte unchanged. Non-
    // perturbing: opcode0 is the byte already fetched above, and
    // debug_io_read(#A8) is the same non-perturbing PPI-port-A read the
    // Phase-3 snapshot uses.
    devices::cpu::Z80aTraceRecord stream_pre_record;
    std::uint8_t stream_a8 = 0;
    if (stream_active_) {
        stream_pre_record = capture_stream_pre_record(pre_pc, opcode0);
        stream_a8 = debug_io_read(0xA8);
    }
    // DEC-0052 stream-light + DEC-0072 phys watchpoint: stamp this instruction's
    // PC. The mapper-RAM / VDP register-write observers fire DURING cpu_.step()
    // below and read this to report which instruction performed the write.
    if (stream_active_ || mem_watch_active_) {
        stream_pc_ = pre_pc;
    }

    // The Z80 core publishes datasheet T-states unchanged (M9 timing oracles stay
    // valid); the S1985 adds +1 T-state per M1 opcode-fetch cycle (fact-sheet §8;
    // A-4 / risk R-3). The MSX-accurate machine time is datasheet + M1 wait, and
    // that is what advances the scheduler and is reported/returned here.
    const std::uint32_t datasheet_tstates = cpu_.step();
    const std::uint32_t m1_wait = s1985_engine_.m1_wait_tstates(cpu_.m1_cycles_last_step());
    const std::uint32_t tstates = datasheet_tstates + m1_wait;
    scheduler_.tick(tstates);

    // M32-S2 line-interrupt delivery (DEC-0039 Defect A leg 2, D-1 ratified
    // in RESP-M32-001; docs/m32-planner-package.md §2.5). Machine-level,
    // immediately after the CPU step -- the M25 pause-gate precedent for
    // per-step machine logic; NOT in Z80aCpu, NOT in core::Scheduler. O(1):
    // an input-fingerprint compare plus one cached-cycle compare (see
    // poll_line_interrupt()). With R#0 IE1 disabled, vdp_.on_line_match()
    // is a proven no-op on the IRQ line and S#1 FH (v9958_vdp.cpp) -- zero
    // behavior change for IE1-off software.
    poll_line_interrupt();

    if (event_logging_enabled_) {
        const std::uint64_t stamp = elapsed_cycles();
        debug_event_log_.record(
            DebugEventType::InstructionRetire, stamp,
            "PC=" + debug_format::to_hex(pre_pc, 4) + " OP=" + debug_format::to_hex(opcode0, 2) +
                " T=" + debug_format::to_dec(tstates));
        if (!halted_before && cpu_.state().halted()) {
            debug_event_log_.record(DebugEventType::Halt, stamp,
                                    "PC=" + debug_format::to_hex(pre_pc, 4));
        }
    }

    // DEC-0052 live stream-capture: commit this instruction's record into the
    // bounded ring, then auto-stop + finalize on a crash signature. Guarded --
    // disarmed => untouched default path. Two triggers (mutually exclusive
    // via else-if; either finalize disarms so on_vsync_boundary never
    // double-writes):
    //   (a) the HALT transition (a clean Z80 HALT crash), and
    //   (b) a STACK RUNAWAY -- SP underflowing kStreamStackFloor. The M36 Bug
    //       B YS-II building-entry crash is NOT a HALT: PC derails into data,
    //       garbage CALLs push the stack down ~2 KB/frame until it collapses
    //       into an RST-38 loop (PC=0x0038, HALT=0). SP<0x4000 is unreachable
    //       in normal execution (the YS-II stack lives ~0xDAxx), so it is an
    //       unambiguous runaway; firing here still keeps the derail inside
    //       the 1M-record ring (~2.8 s ~= ~170 frames -- see
    //       kStreamStackFloor). Read SP through the existing const accessor
    //       (cpu_.state().regs().sp) -- zero src/devices/cpu/ edit.
    if (stream_active_) {
        stream_pre_record.instr_tstates = tstates;
        stream_pre_record.cumulative_tstates = elapsed_cycles();  // machine time (incl. M1 wait)
        push_stream_record(stream_pre_record, stream_a8);
        if (!halted_before && cpu_.state().halted()) {
            finalize_stream_capture("finalize_reason=halt", "HALT_");  // dump ring + snapshot + disarm
        } else if (cpu_.state().regs().sp < kStreamStackFloor) {
            finalize_stream_capture(
                "finalize_reason=sp_underflow sp=" + debug_format::to_hex(cpu_.state().regs().sp, 4),
                "CRASH_");
        }
    }

    return tstates;
}

void Hbf1xvMachine::poll_line_interrupt() {
    // §2.5 trigger rule -- the relation both behavior references agree on:
    // the horizontal scan (line) interrupt fires when the raster enters
    // screen-space display line M = (R#19 - R#23) & 0xFF.
    //   * openMSX references/openmsx-21.0/src/video/VDP.cc:518-576
    //     (scheduleHScan): the match moment is
    //     `((controlRegs[19] - controlRegs[23]) & 0xFF)` display lines
    //     after display start (:527-529), rescheduled on R#19/R#23/R#0
    //     writes (:518-524), with matches beyond the frame's line count
    //     mapped to "never" (:554-559).
    //   * fMSX references/fmsx-60/source/fMSX/MSX.c:2091-2104: fires when
    //     `(((ScanLine+VScroll)&0xFF)-VDP[19])&0xFF` reaches its
    //     coincidence value, IRQ gated on `VDP[0]&0x10` -- algebraically
    //     identical.
    // Precision disclosure (§2.5): openMSX raises FH at the matched line's
    // right border (VDP.cc:913-923); this poll fires at the first
    // instruction boundary at-or-after the raster enters the matched line --
    // up to one instruction early relative to openMSX, a ±1-line-class
    // deviation the split-test margins and the A/B gate encode.
    const std::uint8_t r19 = vdp_.control_register(19);
    const std::uint8_t r23 = vdp_.control_register(23);
    const std::uint8_t r9 = vdp_.control_register(9);
    const std::uint64_t now = scheduler_.total_cycles();

    if (!line_int_cache_valid_ || r19 != line_int_r19_ || r23 != line_int_r23_ ||
        r9 != line_int_r9_ || last_vsync_cycle_ != line_int_vsync_) {
        // An input changed since the cache was built (a register rewrite,
        // by ANY write path, detected by value; or a new vsync origin) --
        // the openMSX reschedule semantic (VDP.cc:518-524) at instruction
        // granularity.
        recompute_line_interrupt_cache(r19, r23, r9, now);
    }

    if (now >= line_int_next_cycle_) {
        // The raster crossed into the match line during the instruction
        // that just retired. Deliver exactly once per crossing: the
        // recompute below sees `now` at/inside the matched line, so its
        // strictly-future candidate lands in the NEXT frame window.
        vdp_.on_line_match();
        recompute_line_interrupt_cache(r19, r23, r9, now);
    }
}

void Hbf1xvMachine::recompute_line_interrupt_cache(const std::uint8_t r19, const std::uint8_t r23,
                                                   const std::uint8_t r9, const std::uint64_t now) {
    line_int_r19_ = r19;
    line_int_r23_ = r23;
    line_int_r9_ = r9;
    line_int_vsync_ = last_vsync_cycle_;
    line_int_cache_valid_ = true;

    const int match = (r19 - r23) & 0xFF;
    const bool ln212 = (r9 & 0x80) != 0;
    const int non_active_lines = ln212 ? 50 : 70;   // 262-212 / 262-192 (fact-sheet §7,
                                                    // same arithmetic as raster_display_line)
    const int active_lines = ln212 ? 212 : 192;
    if (match >= active_lines) {
        // openMSX's "never occurs" clamp, line-granular analogue
        // (VDP.cc:554-559).
        line_int_next_cycle_ = kLineIntNever;
        return;
    }

    constexpr std::uint64_t kLineCycles =
        static_cast<std::uint64_t>(devices::video::vdp_access_timing::kCpuTstatesPerLine);
    constexpr std::uint64_t kFrameCyclesLocal = kLineCycles * 262;
    // Raster origin: on_vsync() fires at the start of the lower border
    // (tstates-since-vsync == 0), so display line M of the frame being
    // scanned starts at (non_active_lines + M) lines after the origin.
    const std::uint64_t offset =
        (static_cast<std::uint64_t>(non_active_lines) + static_cast<std::uint64_t>(match)) * kLineCycles;
    const std::uint64_t base = (last_vsync_cycle_ <= now) ? last_vsync_cycle_ : now;
    const std::uint64_t windows = (now - base) / kFrameCyclesLocal;
    std::uint64_t candidate = base + windows * kFrameCyclesLocal + offset;
    while (candidate <= now) {
        // Strictly-future arming: a match moment already entered/passed in
        // this 262-line window schedules for the next window (the openMSX
        // `if (hScanSyncTime > time)` passed-moment rule, VDP.cc:571-574).
        candidate += kFrameCyclesLocal;
    }
    line_int_next_cycle_ = candidate;
}

void Hbf1xvMachine::load_memory(const std::uint16_t address, const std::uint8_t* bytes, const std::uint32_t size) {
    std::uint32_t write_address = address;
    for (std::uint32_t index = 0; index < size; ++index) {
        const std::uint8_t value = bytes[index];
        dram_.write(core::normalize_bus_address(write_address), value);
        ++write_address;
    }
}

std::uint8_t Hbf1xvMachine::read_memory(const std::uint16_t address) const {
    return dram_.read(address);
}

std::uint8_t Hbf1xvMachine::debug_bus_read(const std::uint16_t address) {
    return bus_.read(address);
}

void Hbf1xvMachine::debug_bus_write(const std::uint16_t address, const std::uint8_t value) {
    bus_.write(address, value);
}

std::uint8_t Hbf1xvMachine::debug_io_read(const std::uint16_t port) {
    return bus_.io_read(port);
}

void Hbf1xvMachine::debug_io_write(const std::uint16_t port, const std::uint8_t value) {
    // M32 (§2.3 documented exclusion): the non-perturbing debug seam does
    // NOT drive the render-sync hook -- scenes built through debug_io_write
    // are covered by the lazy projection/finalize path (nothing accumulates,
    // so render_frame() projects every line from the final state, exactly
    // the legacy snapshot semantics this seam always had). The line-int
    // cache needs no special handling: it re-fingerprints R#19/R#23/R#9 by
    // VALUE at the next step_cpu_instruction, independent of the write path.
    // M44 Phase 2a (DEF-M44-CMDSYNC, DEC-0069): suspend the command-engine CE
    // busy-window arming across the same debug seam, exactly as the render-sync
    // adapter is suspended, so debug-issued VDP commands stay byte-identical
    // (only real CPU-driven OUT command writes pace CE; §2.4.3).
    render_sync_adapter_.set_suspended(true);
    vdp_.set_command_timing_suspended(true);
    bus_.io_write(port, value);
    vdp_.set_command_timing_suspended(false);
    render_sync_adapter_.set_suspended(false);
    // A debug write to the VDP ports (#98-#9B + the S1985 #9C-#9F mirror)
    // still moves VDP state past any sealed frame -- drop the boundary fast
    // path so a subsequent render_frame() live-projects the new scene
    // (legacy snapshot semantics for debug-built scenes).
    if ((port & 0xF8) == 0x98) {
        scanline_accumulator_.mark_completed_frame_stale();
    }
}

bool Hbf1xvMachine::slot_expanded(const int primary) const {
    return slot_bus_.is_expanded(primary);
}

std::uint8_t Hbf1xvMachine::debug_sub_slot_register(const int primary) const {
    return slot_bus_.sub_slot_register(primary);
}

const devices::cpu::Z80aCpu& Hbf1xvMachine::cpu() const {
    return cpu_;
}

devices::cpu::Z80aCpu& Hbf1xvMachine::cpu() {
    return cpu_;
}

std::uint64_t Hbf1xvMachine::elapsed_cycles() const {
    return scheduler_.total_cycles();
}

std::uint64_t Hbf1xvMachine::frame_count() const {
    return frame_count_;
}

std::uint64_t Hbf1xvMachine::frame_cycles_per_frame() const {
    return kFrameCycles;
}

std::uint64_t Hbf1xvMachine::cycles_since_last_vsync() const {
    // Relative to the most recent run_frame()'s on_vsync() call, or program
    // start (last_vsync_cycle_ defaults to 0) if run_frame() has never been
    // called (R-M23-5's documented, tested boundary condition).
    return elapsed_cycles() - last_vsync_cycle_;
}

int Hbf1xvMachine::vdp_cycle_position() const {
    return devices::video::vdp_access_timing::vdp_cycle_within_line(cycles_since_last_vsync());
}

void Hbf1xvMachine::set_cpu_trace_enabled(const bool enabled) {
    cpu_.set_trace_observer(enabled ? &cpu_trace_sink_ : nullptr);
}

bool Hbf1xvMachine::cpu_trace_enabled() const {
    return cpu_.trace_observer() != nullptr;
}

const CpuTraceSink& Hbf1xvMachine::cpu_trace() const {
    return cpu_trace_sink_;
}

CpuTraceSink& Hbf1xvMachine::cpu_trace() {
    return cpu_trace_sink_;
}

std::size_t Hbf1xvMachine::dram_size() const {
    return dram_.size();
}

const MemoryRegion& Hbf1xvMachine::dram() const {
    return dram_;
}

MemoryRegion& Hbf1xvMachine::dram() {
    return dram_;
}

const devices::video::V9958Vdp& Hbf1xvMachine::vdp() const {
    return vdp_;
}

devices::video::V9958Vdp& Hbf1xvMachine::vdp() {
    return vdp_;
}

devices::video::FrameBuffer Hbf1xvMachine::render_frame(const devices::video::Field field) const {
    // M32 re-route (Defect A, §2.4). Even/Odd keep the legacy frozen-
    // snapshot semantics (test/debug-only fields; no production caller --
    // documented design choice, see the header comment).
    if (field != devices::video::Field::Progressive) {
        return vdp_frame_renderer_.render_frame(field);
    }

    const int line = vdp_.raster_display_line();
    if (line >= 0) {
        // Mid-display call: commit the lines the beam has already passed
        // (memoization -- see the logical-constness note in the header),
        // then compose accumulated past + live-projected future.
        // M51 (DEC-0078) fix, shape (i) invariant guard: pace the render-only
        // sprite pass to the same boundary FIRST, so a capture/debug caller's
        // memoization commit can never seal rows from the lazy-open-cleared
        // (empty) sprite buffers (same consumer-side rule as on_commit_up_to;
        // const_cast = the identical logical-constness memoization rationale
        // the accumulator sync on the next line already relies on -- the
        // render-only sprite watermark is presentation memoization state, not
        // CPU-visible machine state).
        const_cast<devices::video::V9958Vdp&>(vdp_).commit_sprite_rows(line + 1);
        scanline_accumulator_.sync_to_line(line + 1);
        return scanline_accumulator_.compose(field);
    }
    if (scanline_accumulator_.watermark() > 0) {
        // Beam in the border/vblank region BELOW a partially accumulated
        // frame (a step-only caller that never finalizes): the beam has
        // passed the whole display area -- committing to the bottom is
        // memoization too.
        // M51: same invariant guard as the mid-display branch above.
        const_cast<devices::video::V9958Vdp&>(vdp_).commit_sprite_rows(
            vdp_frame_renderer_.height());
        scanline_accumulator_.sync_to_line(vdp_frame_renderer_.height());
        return scanline_accumulator_.compose(field);
    }
    if (scanline_accumulator_.completed_frame_fresh()) {
        // Frame-boundary call (every production call site: immediately
        // after on_vsync_boundary(), before the next frame's display area
        // begins, with no VDP write in between): return the finalized
        // per-line frame of the frame that just ended.
        return scanline_accumulator_.completed_frame();
    }
    // Nothing accumulated and no fresh sealed frame (fresh machine, a
    // debug-built scene, or VDP writes since the last boundary while the
    // beam is still in the border): pure live projection -- byte-identical
    // to the legacy snapshot renderer by construction (AC-4).
    return scanline_accumulator_.compose(field);
}

std::uint64_t Hbf1xvMachine::RtcClock::cpu_cycles() const {
    return scheduler_.total_cycles();
}

std::uint64_t Hbf1xvMachine::FdcClock::cpu_cycles() const {
    return scheduler_.total_cycles();
}

std::uint64_t Hbf1xvMachine::CassetteClock::cpu_cycles() const {
    return scheduler_.total_cycles();
}

std::uint64_t Hbf1xvMachine::RenshaTurboClock::cpu_cycles() const {
    return scheduler_.total_cycles();
}

std::uint64_t Hbf1xvMachine::Ym2413Clock::cpu_cycles() const {
    return scheduler_.total_cycles();
}

const devices::fdc::Wd2793& Hbf1xvMachine::fdc() const {
    return fdc_;
}

devices::fdc::Wd2793& Hbf1xvMachine::fdc() {
    return fdc_;
}

const devices::fdc::DiskDrive& Hbf1xvMachine::disk_drive() const {
    return disk_drive_;
}

devices::fdc::DiskDrive& Hbf1xvMachine::disk_drive() {
    return disk_drive_;
}

const devices::fdc::DiskImage& Hbf1xvMachine::disk_image() const {
    return disk_image_;
}

devices::fdc::DiskImage& Hbf1xvMachine::disk_image() {
    return disk_image_;
}

void Hbf1xvMachine::set_fast_disk(const bool on) {
    // Propagate the turbo toggle to BOTH devices that own disk timing: the
    // WD2793 (per-byte DRQ cadence, first-DRQ header/start, step + settle) and
    // the DiskDrive (rotational latency). Keeping them in lockstep is the whole
    // contract -- one without the other would half-collapse the load.
    fdc_.set_fast_disk(on);
    disk_drive_.set_fast_disk(on);
}

bool Hbf1xvMachine::fast_disk() const {
    // Single source of truth is the WD2793 flag; the drive is always set in
    // lockstep by set_fast_disk() above.
    return fdc_.fast_disk();
}

devices::cartridge::CartridgeLoadResult Hbf1xvMachine::load_cartridge(
    const int slot_number, const devices::cartridge::CartridgeMapperType type, std::vector<std::uint8_t> image) {
    devices::cartridge::CartridgeLoadResult result =
        devices::cartridge::CartridgeLoadResult::InvalidSlotNumber;
    if (slot_number == 1) {
        result = cartridge_slot1_.load(type, std::move(image));
    } else if (slot_number == 2) {
        result = cartridge_slot2_.load(type, std::move(image));
    }
    // M36 (DEC-0050): on a successful FM-PAC insertion, load its battery SRAM
    // from the configured .sram path (mirrors the M20 Halnote load-at-setup:
    // absent file -> deterministic zero state, never fabricated). The load is
    // a single, fixed, load-at-insertion read -- no per-step host I/O, so
    // determinism is preserved.
    if (result == devices::cartridge::CartridgeLoadResult::Ok &&
        type == devices::cartridge::CartridgeMapperType::FmPac && !fmpac_sram_path_.empty()) {
        if (devices::cartridge::CartridgeFmPacRom* cart = fmpac(slot_number)) {
            (void)cart->load_sram(fmpac_sram_path_);
        }
    }
    return result;
}

void Hbf1xvMachine::unload_cartridge(const int slot_number) {
    if (slot_number == 1) {
        cartridge_slot1_.unload();
    } else if (slot_number == 2) {
        cartridge_slot2_.unload();
    }
}

const devices::cartridge::CartridgeSlot& Hbf1xvMachine::cartridge_slot1() const {
    return cartridge_slot1_;
}

devices::cartridge::CartridgeSlot& Hbf1xvMachine::cartridge_slot1() {
    return cartridge_slot1_;
}

const devices::cartridge::CartridgeSlot& Hbf1xvMachine::cartridge_slot2() const {
    return cartridge_slot2_;
}

devices::cartridge::CartridgeSlot& Hbf1xvMachine::cartridge_slot2() {
    return cartridge_slot2_;
}

devices::audio::SccWavetable* Hbf1xvMachine::scc_chip(const int slot_number) {
    // M29-S4 (backlog G1): non-null exactly when the named bay holds a
    // KonamiSCC cartridge (docs/m29-planner-package.md §2.3). The static_cast
    // is type-safe by construction: CartridgeKonamiScc is the ONLY mapper
    // whose mapper_type() reports KonamiSCC (cartridge_slot.cpp factory).
    devices::cartridge::CartridgeSlot* slot = nullptr;
    if (slot_number == 1) {
        slot = &cartridge_slot1_;
    } else if (slot_number == 2) {
        slot = &cartridge_slot2_;
    } else {
        return nullptr;
    }
    if (!slot->loaded() || slot->mapper_type() != devices::cartridge::CartridgeMapperType::KonamiSCC) {
        return nullptr;
    }
    return &static_cast<devices::cartridge::CartridgeKonamiScc*>(slot->mapper())->scc();
}

const devices::audio::SccWavetable* Hbf1xvMachine::scc_chip(const int slot_number) const {
    return const_cast<Hbf1xvMachine*>(this)->scc_chip(slot_number);
}

const devices::audio::PsgYm2149& Hbf1xvMachine::psg() const {
    return psg_;
}

devices::audio::PsgYm2149& Hbf1xvMachine::psg() {
    return psg_;
}

const devices::audio::ClickDac& Hbf1xvMachine::click_dac() const {
    return click_dac_;
}

devices::audio::ClickDac& Hbf1xvMachine::click_dac() {
    return click_dac_;
}

const devices::audio::Ym2413Opll& Hbf1xvMachine::ym2413() const {
    return ym2413_;
}

devices::audio::Ym2413Opll& Hbf1xvMachine::ym2413() {
    return ym2413_;
}

const devices::rtc::Rp5c01& Hbf1xvMachine::rtc() const {
    return rtc_;
}

devices::rtc::Rp5c01& Hbf1xvMachine::rtc() {
    return rtc_;
}

const devices::chipset::Ppi8255& Hbf1xvMachine::ppi() const {
    return ppi_;
}

devices::chipset::Ppi8255& Hbf1xvMachine::ppi() {
    return ppi_;
}

const peripherals::KeyboardMatrix& Hbf1xvMachine::keyboard() const {
    return keyboard_;
}

peripherals::KeyboardMatrix& Hbf1xvMachine::keyboard() {
    return keyboard_;
}

const peripherals::JoystickPorts& Hbf1xvMachine::joystick() const {
    return joystick_;
}

peripherals::JoystickPorts& Hbf1xvMachine::joystick() {
    return joystick_;
}

const devices::chipset::S1985Engine& Hbf1xvMachine::s1985() const {
    return s1985_engine_;
}

devices::chipset::S1985Engine& Hbf1xvMachine::s1985() {
    return s1985_engine_;
}

const devices::kanji::KanjiFontRom& Hbf1xvMachine::kanji() const {
    return kanji_font_rom_;
}

devices::kanji::KanjiFontRom& Hbf1xvMachine::kanji() {
    return kanji_font_rom_;
}

const peripherals::PrinterPort& Hbf1xvMachine::printer() const {
    return printer_;
}

peripherals::PrinterPort& Hbf1xvMachine::printer() {
    return printer_;
}

const peripherals::CassetteInterface& Hbf1xvMachine::cassette() const {
    return cassette_;
}

peripherals::CassetteInterface& Hbf1xvMachine::cassette() {
    return cassette_;
}

const devices::chipset::Mb670836PauseController& Hbf1xvMachine::pause_controller() const {
    return pause_controller_;
}

devices::chipset::Mb670836PauseController& Hbf1xvMachine::pause_controller() {
    return pause_controller_;
}

const peripherals::RenshaTurbo& Hbf1xvMachine::rensha_turbo() const {
    return rensha_turbo_;
}

peripherals::RenshaTurbo& Hbf1xvMachine::rensha_turbo() {
    return rensha_turbo_;
}

void Hbf1xvMachine::set_backup_ram_path(std::filesystem::path path) {
    backup_ram_path_ = std::move(path);
}

const std::filesystem::path& Hbf1xvMachine::backup_ram_path() const {
    return backup_ram_path_;
}

bool Hbf1xvMachine::flush_backup_ram() const {
    if (backup_ram_path_.empty()) {
        return false;
    }
    return s1985_engine_.save_backup_ram(backup_ram_path_);
}

const devices::halnote::HalnoteRom& Hbf1xvMachine::halnote() const {
    return halnote_;
}

devices::halnote::HalnoteRom& Hbf1xvMachine::halnote() {
    return halnote_;
}

void Hbf1xvMachine::set_halnote_sram_path(std::filesystem::path path) {
    halnote_sram_path_ = std::move(path);
}

const std::filesystem::path& Hbf1xvMachine::halnote_sram_path() const {
    return halnote_sram_path_;
}

bool Hbf1xvMachine::flush_halnote_sram() const {
    if (halnote_sram_path_.empty()) {
        return false;
    }
    return halnote_.sram().save(halnote_sram_path_);
}

devices::cartridge::CartridgeFmPacRom* Hbf1xvMachine::fmpac(const int slot_number) {
    // Mirror scc_chip(): non-null exactly when the named bay holds an FmPac
    // cartridge. The static_cast is type-safe by construction -- FmPac is the
    // only mapper whose mapper_type() reports FmPac (cartridge_slot.cpp).
    devices::cartridge::CartridgeSlot* slot = nullptr;
    if (slot_number == 1) {
        slot = &cartridge_slot1_;
    } else if (slot_number == 2) {
        slot = &cartridge_slot2_;
    } else {
        return nullptr;
    }
    if (!slot->loaded() || slot->mapper_type() != devices::cartridge::CartridgeMapperType::FmPac) {
        return nullptr;
    }
    return static_cast<devices::cartridge::CartridgeFmPacRom*>(slot->mapper());
}

const devices::cartridge::CartridgeFmPacRom* Hbf1xvMachine::fmpac(const int slot_number) const {
    return const_cast<Hbf1xvMachine*>(this)->fmpac(slot_number);
}

void Hbf1xvMachine::set_fmpac_sram_path(std::filesystem::path path) {
    fmpac_sram_path_ = std::move(path);
}

const std::filesystem::path& Hbf1xvMachine::fmpac_sram_path() const {
    return fmpac_sram_path_;
}

bool Hbf1xvMachine::flush_fmpac_sram() const {
    if (fmpac_sram_path_.empty()) {
        return false;
    }
    // Save whichever bay holds an FM-PAC (slot 1 preferred, then slot 2).
    if (const devices::cartridge::CartridgeFmPacRom* cart = fmpac(1)) {
        return cart->save_sram(fmpac_sram_path_);
    }
    if (const devices::cartridge::CartridgeFmPacRom* cart = fmpac(2)) {
        return cart->save_sram(fmpac_sram_path_);
    }
    return false;
}

void Hbf1xvMachine::set_event_logging_enabled(const bool enabled) {
    event_logging_enabled_ = enabled;
}

bool Hbf1xvMachine::event_logging_enabled() const {
    return event_logging_enabled_;
}

const DebugEventLog& Hbf1xvMachine::debug_event_log() const {
    return debug_event_log_;
}

DebugEventLog& Hbf1xvMachine::debug_event_log() {
    return debug_event_log_;
}

void Hbf1xvMachine::set_debug_root(std::filesystem::path root) {
    debug_root_ = std::move(root);
}

const std::filesystem::path& Hbf1xvMachine::debug_root() const {
    return debug_root_;
}

std::string Hbf1xvMachine::serialize_state_dump() const {
    std::string out;
    out += debug_dump::kDumpFormatTag;
    out.push_back('\n');
    out += debug_dump::serialize_cpu(cpu_.state());
    out += debug_dump::serialize_region("DRAM", dram_.data(), dram_.size());
    // M36 (DEC-0050): the bare HB-F1XV has NO internal SRAM (built-in MSX-MUSIC
    // is SRAM-less). The SRAM section reflects an inserted FM-PAC peripheral
    // cartridge's 8 KB battery SRAM when present, and is empty (size=0)
    // otherwise -- matching real hardware ("NO S-RAM AVAILABLE" on a bare
    // machine). Slot 1 preferred, then slot 2.
    if (const devices::cartridge::CartridgeFmPacRom* cart = fmpac(1)) {
        out += debug_dump::serialize_region("SRAM", cart->sram().data(), cart->sram().size());
    } else if (const devices::cartridge::CartridgeFmPacRom* cart = fmpac(2)) {
        out += debug_dump::serialize_region("SRAM", cart->sram().data(), cart->sram().size());
    } else {
        out += debug_dump::serialize_region("SRAM", nullptr, 0);
    }
    // VRAM now lives in the VDP device (M14-S1 migration, A-5). The dump reads it
    // through the VDP's const VRAM accessor; the boot content is still all-zero
    // (the VDP clears VRAM at reset), so the M10/M13 dump golden is unchanged (R-3).
    out += debug_dump::serialize_region("VRAM", vdp_.vram().data(), vdp_.vram().size());
    out += "[END]\n";
    return out;
}

bool Hbf1xvMachine::write_text_file(const std::filesystem::path& directory, const std::string& filename,
                                    const std::string& text) {
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        return false;
    }

    const std::filesystem::path path = directory / filename;
    // Binary mode so '\n' is written verbatim (no CRLF translation) — keeps the
    // on-disk bytes identical across platforms and byte-stable across runs.
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(file);
}

bool Hbf1xvMachine::append_text_file(const std::filesystem::path& directory,
                                     const std::string& filename, const std::string& text) {
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        return false;
    }
    const std::filesystem::path path = directory / filename;
    // Binary + append: '\n' written verbatim, each call adds to the end (the
    // per-stream FDC log grows one line per sector read). Determinism across
    // identical armed runs is preserved by removing any stale log at arm time.
    std::ofstream file(path, std::ios::binary | std::ios::app);
    if (!file) {
        return false;
    }
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(file);
}

bool Hbf1xvMachine::write_state_dump(const std::string& filename) {
    const std::string text = serialize_state_dump();
    const bool ok = write_text_file(debug_root_ / "traces", filename, text);
    if (event_logging_enabled_) {
        debug_event_log_.record(DebugEventType::Dump, elapsed_cycles(), "FILE=" + filename);
    }
    return ok;
}

bool Hbf1xvMachine::write_cpu_trace(const std::string& filename) const {
    return write_text_file(debug_root_ / "traces", filename, cpu_trace_sink_.serialize());
}

bool Hbf1xvMachine::write_event_log(const std::string& filename) const {
    return write_text_file(debug_root_ / "logs", filename, debug_event_log_.serialize());
}

std::string Hbf1xvMachine::serialize_frame_dump(const devices::video::Field field) const {
    return frame_dump::serialize_frame_dump(render_frame(field));
}

bool Hbf1xvMachine::write_frame_dump(const std::string& filename, const devices::video::Field field) {
    return write_text_file(debug_root_ / "frames", filename, serialize_frame_dump(field));
}

// --- M36 Phase 3 comprehensive debug snapshot (DEC-0051) --------------------

const devices::chipset::MapperIo& Hbf1xvMachine::mapper_io() const {
    return mapper_io_;
}

std::string Hbf1xvMachine::snapshot_id() const {
    // Deterministic frame/cycle stamp. Zero-padding only LENGTHENS the field
    // (never truncates), so long runs stay unambiguous. No wall-clock/RNG.
    auto pad = [](std::uint64_t value, std::size_t width) {
        std::string s = debug_format::to_dec(value);
        while (s.size() < width) {
            s.insert(s.begin(), '0');
        }
        return s;
    };
    return "f" + pad(frame_count_, 6) + "_c" + pad(elapsed_cycles(), 16);
}

debug_snapshot::Snapshot Hbf1xvMachine::serialize_snapshot(
    const std::string& id, const std::vector<std::string>& caller_notes) {
    debug_snapshot::Snapshot snap;
    snap.id = id;

    // cpu.txt -- CPU full (incl WZ/Q + interrupt/ei-delay/pending, the delta
    // over the golden serialize_cpu; the golden dump is NOT edited).
    snap.files.push_back(
        {"cpu.txt", debug_snapshot::frame_file(debug_snapshot::cpu_section(cpu_.state()))});

    // memory.txt -- 64 KB DRAM + the 4 mapper segment registers (raw + the
    // 0x80|seg&0x1F readback via the non-perturbing debug_io_read seam).
    {
        std::array<std::uint8_t, 4> readback{};
        for (std::size_t p = 0; p < 4; ++p) {
            readback[p] = debug_io_read(static_cast<std::uint16_t>(0xFC + p));
        }
        std::string body = debug_snapshot::dram_section(dram_) +
                           debug_snapshot::mapper_section(mapper_io_, readback);
        snap.files.push_back({"memory.txt", debug_snapshot::frame_file(body)});
    }

    // vram.txt -- 128 KB VRAM.
    snap.files.push_back(
        {"vram.txt", debug_snapshot::frame_file(debug_snapshot::vram_section(vdp_.vram()))});

    // vdp.txt -- the full V9958 register/status/latch/palette/mode/IRQ/command
    // set.
    snap.files.push_back(
        {"vdp.txt", debug_snapshot::frame_file(debug_snapshot::vdp_section(
                        vdp_, cycles_since_last_vsync(), vdp_cycle_position()))});

    // audio.txt -- built-in PSG + built-in OPLL (+ inserted FM-PAC OPLL, if any).
    {
        const devices::cartridge::CartridgeFmPacRom* fm = (fmpac(1) != nullptr) ? fmpac(1) : fmpac(2);
        snap.files.push_back(
            {"audio.txt",
             debug_snapshot::frame_file(debug_snapshot::audio_section(psg_, ym2413_, fm))});
    }

    // rtc.txt
    snap.files.push_back(
        {"rtc.txt", debug_snapshot::frame_file(debug_snapshot::rtc_section(rtc_))});

    // fdc.txt -- WD2793 + drive + disk.
    snap.files.push_back(
        {"fdc.txt", debug_snapshot::frame_file(debug_snapshot::fdc_section(
                        fdc_, disk_drive_, disk_image_, elapsed_cycles()))});

    // chipset.txt -- S1985 + slots (#A8/#FFFF/expanded) + sysctrl (#F4/#F5) +
    // clock counters + pause/speed/Ren-Sha.
    {
        std::array<std::uint8_t, 4> subslots{};
        std::array<bool, 4> expanded{};
        for (int p = 0; p < 4; ++p) {
            subslots[static_cast<std::size_t>(p)] = debug_sub_slot_register(p);
            expanded[static_cast<std::size_t>(p)] = slot_expanded(p);
        }
        std::string body;
        body += debug_snapshot::s1985_section(s1985_engine_);
        body += debug_snapshot::slots_section(debug_io_read(0xA8), subslots, expanded);
        body += debug_snapshot::sysctrl_section(debug_io_read(0xF4), debug_io_read(0xF5));
        body += debug_snapshot::clock_section(elapsed_cycles(), frame_count_, frame_cycles_per_frame(),
                                              cycles_since_last_vsync(), vdp_cycle_position());
        body += debug_snapshot::pause_rensha_section(pause_controller_, rensha_turbo_);
        snap.files.push_back({"chipset.txt", debug_snapshot::frame_file(body)});
    }

    // cartridges.txt -- external bays 1/2 (+ their banks/FM-PAC/SCC) + Halnote +
    // best-effort Kanji/printer/cassette.
    {
        std::string body;
        body += debug_snapshot::cartridge_section("CART1", cartridge_slot1_, fmpac(1), scc_chip(1));
        body += debug_snapshot::cartridge_section("CART2", cartridge_slot2_, fmpac(2), scc_chip(2));
        body += debug_snapshot::halnote_section(halnote_);
        body += debug_snapshot::peripherals_section(kanji_font_rom_, printer_, cassette_);
        snap.files.push_back({"cartridges.txt", debug_snapshot::frame_file(body)});
    }

    // manifest.txt -- built LAST (so it indexes + checksums the component files),
    // then inserted at the FRONT of the ordered list. Deterministic: file sizes
    // and FNV-1a checksums are pure functions of the (deterministic) content;
    // caller_notes (e.g. the frontend multi-disk index, planner A4) are appended
    // verbatim. NO wall-clock is embedded.
    {
        std::string body;
        body += "[SNAPSHOT]\n";
        body += "ID=" + id + "\n";
        body += "FRAME=" + debug_format::to_dec(frame_count_) + "\n";
        body += "CYCLE=" + debug_format::to_dec(elapsed_cycles()) + "\n";
        body += "COMPONENTS=" + debug_format::to_dec(snap.files.size()) + "\n";
        body += "[FILES]\n";
        for (const debug_snapshot::SnapshotFile& f : snap.files) {
            body += "FILE=" + f.name + " size=" + debug_format::to_dec(f.content.size()) +
                    " crc=" + debug_format::to_hex(debug_snapshot::checksum(f.content), 8) + "\n";
        }
        if (!caller_notes.empty()) {
            body += "[NOTES]\n";
            for (const std::string& note : caller_notes) {
                body += note + "\n";
            }
        }
        snap.files.insert(snap.files.begin(),
                          debug_snapshot::SnapshotFile{"manifest.txt", debug_snapshot::frame_file(body)});
    }

    return snap;
}

bool Hbf1xvMachine::write_snapshot(const std::string& id,
                                   const std::vector<std::string>& caller_notes) {
    const debug_snapshot::Snapshot snap = serialize_snapshot(id, caller_notes);
    const std::filesystem::path dir = debug_root_ / "snapshot" / id;
    bool ok = true;
    for (const debug_snapshot::SnapshotFile& f : snap.files) {
        ok = write_text_file(dir, f.name, f.content) && ok;
    }
    if (event_logging_enabled_) {
        debug_event_log_.record(DebugEventType::Dump, elapsed_cycles(), "SNAPSHOT=" + id);
    }
    return ok;
}

// --- DEC-0052 live stream-capture ------------------------------------------

void Hbf1xvMachine::set_stream_capture_enabled(const bool enabled, const std::string& stream_id,
                                              const bool light) {
    if (enabled) {
        if (stream_active_) {
            return;  // already armed; ignore a redundant ON
        }
        stream_id_ = stream_id.empty() ? snapshot_id() : stream_id;
        stream_light_ = light;
        stream_pc_ = cpu_.state().regs().pc;
        stream_ring_.clear();
        stream_ring_head_ = 0;
        stream_seq_ = 0;
        stream_active_ = true;
        // DEC-0052: install the FDC per-sector-read logger (default-off otherwise)
        // and remove any stale log from a prior armed run at this same id, so the
        // append-mode log is rebuilt byte-identically by an identical armed run.
        std::error_code ec;
        std::filesystem::remove(debug_root_ / "traces" / ("stream_" + stream_id_ + "_fdc.log"), ec);
        std::filesystem::remove(
            debug_root_ / "traces" / ("stream_" + stream_id_ + "_fdcwrite.log"), ec);
        fdc_.set_sector_read_observer(&fdc_stream_observer_);
        fdc_.set_sector_write_observer(&fdc_write_stream_observer_);
        // DEC-0052 stream-light: the mapper-RAM + VDP register-write watchlog
        // observers are a light-mode-only capability, so the heavy F10 path
        // stays byte-for-byte the prior M36 stream behavior (no watch.log).
        // Clear any stale watch.log too (same determinism rationale as the
        // fdc.log). All observers are removed in finalize_stream_capture, so
        // a disarmed machine is byte-for-byte the pre-DEC-0052 default
        // regardless.
        if (stream_light_) {
            std::filesystem::remove(debug_root_ / "traces" / ("stream_" + stream_id_ + "_watch.log"),
                                    ec);
            refresh_ram_write_observer();
            vdp_.attach_register_write_observer(&vdp_reg_watch_observer_);
        }
    } else {
        // Manual OFF (e.g. the frontend F10 toggle-off): finalize into a distinct
        // MANUAL_ bundle so the reason is self-evident vs a crash finalize.
        finalize_stream_capture("finalize_reason=manual", "MANUAL_");  // no-op if not armed
    }
}

bool Hbf1xvMachine::stream_capture_active() const {
    return stream_active_;
}

bool Hbf1xvMachine::stream_capture_light() const {
    return stream_active_ && stream_light_;
}

const std::string& Hbf1xvMachine::stream_capture_id() const {
    return stream_id_;
}

std::size_t Hbf1xvMachine::stream_trace_ring_size() const {
    return stream_ring_.size();
}

devices::cpu::Z80aTraceRecord Hbf1xvMachine::capture_stream_pre_record(
    const std::uint16_t pre_pc, const std::uint8_t opcode0) const {
    // Reuse the M27 Z80aTraceRecord convention: PRE-execution register/opcode
    // snapshot taken at the instruction boundary. instr/cumulative T-states are
    // filled by the caller AFTER the step retires.
    const devices::cpu::Z80aState& s = cpu_.state();
    const devices::cpu::Z80aRegisters& r = s.regs();
    devices::cpu::Z80aTraceRecord rec;
    rec.sequence = stream_seq_;
    rec.pc = pre_pc;
    rec.opcode_bytes[0] = opcode0;
    rec.opcode_length = 1;  // first opcode byte only (already-fetched; non-perturbing)
    rec.a = r.a();
    rec.f = r.f();
    rec.b = r.b();
    rec.c = r.c();
    rec.d = r.d();
    rec.e = r.e();
    rec.h = r.h();
    rec.l = r.l();
    rec.af = r.af;
    rec.bc = r.bc;
    rec.de = r.de;
    rec.hl = r.hl;
    rec.af_shadow = r.af_shadow;
    rec.bc_shadow = r.bc_shadow;
    rec.de_shadow = r.de_shadow;
    rec.hl_shadow = r.hl_shadow;
    rec.ix = r.ix;
    rec.iy = r.iy;
    rec.sp = r.sp;
    rec.i = r.i;
    rec.r = r.r;
    rec.iff1 = s.iff1();
    rec.iff2 = s.iff2();
    rec.im = s.interrupt_mode();
    return rec;
}

void Hbf1xvMachine::push_stream_record(const devices::cpu::Z80aTraceRecord& record,
                                       const std::uint8_t a8) {
    if (stream_ring_.size() < kStreamTraceRingCapacity) {
        stream_ring_.push_back(StreamTraceEntry{record, a8});
    } else {
        // Full: overwrite the oldest and advance the head (bounded memory).
        stream_ring_[stream_ring_head_] = StreamTraceEntry{record, a8};
        stream_ring_head_ = (stream_ring_head_ + 1) % kStreamTraceRingCapacity;
    }
    ++stream_seq_;
}

std::string Hbf1xvMachine::serialize_stream_trace() const {
    // Oldest -> newest. When the ring has not yet wrapped, head_ is 0 and this is
    // a plain [0, size) walk; once wrapped, size == capacity and head_ marks the
    // oldest entry, so the modulo walks the ring in age order.
    std::string out;
    const std::size_t n = stream_ring_.size();
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t idx = (stream_ring_head_ + i) % n;
        const StreamTraceEntry& e = stream_ring_[idx];
        out += CpuTraceSink::format_record(e.cpu);  // reuse the M27 formatter verbatim
        out += " A8=" + debug_format::to_hex(e.a8, 2);
        out.push_back('\n');
    }
    return out;
}

void Hbf1xvMachine::finalize_stream_capture(const std::string& reason_note,
                                            const std::string& bundle_prefix) {
    if (!stream_active_) {
        return;
    }
    // (a) Dump the per-instruction ring oldest->newest to a human-readable trace.
    write_text_file(debug_root_ / "traces", "stream_" + stream_id_ + "_trace.txt",
                    serialize_stream_trace());

    // (b) One FINAL full snapshot capturing the crash end-state, filed under the
    //     stream directory next to the per-frame bundles. `reason_note` (verbatim
    //     "finalize_reason=..." line) makes the cause self-evident; `bundle_prefix`
    //     (HALT_ / CRASH_ / MANUAL_) distinguishes the bundle directory.
    const std::vector<std::string> notes = {
        "stream_id=" + stream_id_,
        reason_note,
        "stream_finalize=1",
        "halt=" + std::string(cpu_.state().halted() ? "1" : "0"),
        "ring_entries=" + debug_format::to_dec(stream_ring_.size()),
        "instructions_seen=" + debug_format::to_dec(stream_seq_),
    };
    write_snapshot("stream_" + stream_id_ + "/" + bundle_prefix + snapshot_id(), notes);

    // (c) Uninstall every observer + disarm -- subsequent reads/steps/frames are
    //     no-ops until re-armed (default-off restored). DEC-0052 stream-light:
    //     the mapper-RAM + VDP register-write watchlog observers are removed here
    //     too, and stream_light_ is cleared so a following heavy arm is unaffected.
    fdc_.set_sector_read_observer(nullptr);
    fdc_.set_sector_write_observer(nullptr);
    vdp_.attach_register_write_observer(nullptr);
    stream_light_ = false;
    stream_active_ = false;
    // DEC-0072: keep the ram_mapper_ observer installed if the phys watchpoint
    // still wants it; only stream-light's need for it is gone here.
    refresh_ram_write_observer();
}

// --- DEC-0072 physical-DRAM memory-write watchpoint ------------------------

void Hbf1xvMachine::refresh_ram_write_observer() {
    const bool want = (stream_active_ && stream_light_) || mem_watch_active_;
    ram_mapper_.set_write_observer(want ? &mem_watch_observer_ : nullptr);
}

void Hbf1xvMachine::set_mem_watch(const std::size_t phys_lo, const std::size_t phys_hi,
                                  const std::uint32_t frame_from, const std::uint32_t frame_to) {
    mem_watch_lo_ = phys_lo;
    mem_watch_hi_ = phys_hi;
    mem_watch_from_ = frame_from;
    mem_watch_to_ = frame_to;
    mem_watch_dropped_ = 0;
    mem_watch_active_ = true;
    mem_watch_log_ =
        "frame,cyc,pc,seg,logical,phys,val,A,BC,DE,HL,IX,IY,SP\n";  // CSV header
    refresh_ram_write_observer();
}

void Hbf1xvMachine::clear_mem_watch() {
    mem_watch_active_ = false;
    refresh_ram_write_observer();
}

void Hbf1xvMachine::mem_watch_record(const core::BusAddress logical, const core::BusData value) {
    if (!mem_watch_active_) {
        return;
    }
    if (frame_count_ < mem_watch_from_ || frame_count_ > mem_watch_to_) {
        return;
    }
    // Fold the CPU-visible address onto the physical DRAM store exactly as
    // MemoryMapperRam::mem_write does, so the window is a TRUE physical range
    // (segment-independent): the game reaches its fixed save-build buffer through
    // whichever mapper segment currently pages it in.
    const int page = (logical >> 14) & 0x03;
    const std::uint8_t seg = mapper_io_.segment(page);
    const int nseg =
        static_cast<int>(dram_.size() / devices::memory::MemoryMapperRam::kSegmentBytes);
    const std::size_t phys =
        devices::memory::MemoryMapperRam::physical_address(seg, logical, nseg);
    if (phys < mem_watch_lo_ || phys >= mem_watch_hi_) {
        return;
    }
    if (mem_watch_log_.size() > 1 &&
        static_cast<std::uint64_t>(mem_watch_log_.size()) / 48u >= kMemWatchMaxRows) {
        ++mem_watch_dropped_;
        return;
    }
    const devices::cpu::Z80aRegisters& r = cpu_.state().regs();
    mem_watch_log_ += debug_format::to_dec(frame_count_);
    mem_watch_log_ += ',' + debug_format::to_dec(elapsed_cycles());
    mem_watch_log_ += ',' + debug_format::to_hex(stream_pc_, 4);
    mem_watch_log_ += ',' + debug_format::to_hex(seg, 2);
    mem_watch_log_ += ',' + debug_format::to_hex(logical, 4);
    mem_watch_log_ += ',' + debug_format::to_hex(static_cast<std::uint64_t>(phys), 5);
    mem_watch_log_ += ',' + debug_format::to_hex(value, 2);
    mem_watch_log_ += ',' + debug_format::to_hex(r.a(), 2);
    mem_watch_log_ += ',' + debug_format::to_hex(r.bc, 4);
    mem_watch_log_ += ',' + debug_format::to_hex(r.de, 4);
    mem_watch_log_ += ',' + debug_format::to_hex(r.hl, 4);
    mem_watch_log_ += ',' + debug_format::to_hex(r.ix, 4);
    mem_watch_log_ += ',' + debug_format::to_hex(r.iy, 4);
    mem_watch_log_ += ',' + debug_format::to_hex(r.sp, 4);
    mem_watch_log_.push_back('\n');
}

std::uint32_t Hbf1xvMachine::crc32(const std::uint8_t* data, const std::size_t size) {
    // Standard IEEE-802.3 / zlib CRC-32 (reflected input/output, reflected poly
    // 0xEDB88320 = reflect(0x04C11DB7), init 0xFFFFFFFF, final XOR 0xFFFFFFFF).
    // Bitwise (no table) -- deterministic, self-contained, and byte-for-byte
    // equal to python zlib.crc32 / binascii.crc32 (verified against the standard
    // check value 0xCBF43926 for "123456789"), so the human's raw-.dsk tooling
    // diffs our returned sector bytes directly.
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = (crc & 1u) != 0u ? 0xEDB88320u : 0u;
            crc = (crc >> 1) ^ mask;
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

void Hbf1xvMachine::log_stream_fdc_read(const std::uint8_t command, const std::uint8_t track,
                                        const std::uint8_t side, const std::uint8_t sector,
                                        const std::uint8_t* data, const std::size_t size) {
    // Defensive: the observer is installed only while armed, but stay silent if a
    // finalize raced in. Non-perturbing: reads the already-fetched bytes, CRCs
    // them, appends one line -- no emulation/scheduler/CPU effect.
    if (!stream_active_) {
        return;
    }
    const std::uint32_t lba = devices::fdc::DiskImage::chs_to_lba(track, side, sector);
    std::string line = "frame=" + debug_format::to_dec(frame_count_);
    line += " cmd=" + debug_format::to_hex(command, 2);
    line += " track=" + debug_format::to_dec(track);
    line += " side=" + debug_format::to_dec(side);
    line += " sector=" + debug_format::to_dec(sector);
    line += " lba=" + debug_format::to_dec(lba);
    line += " crc=" + debug_format::to_hex(crc32(data, size), 8);
    line.push_back('\n');
    append_text_file(debug_root_ / "traces", "stream_" + stream_id_ + "_fdc.log", line);
}

void Hbf1xvMachine::FdcStreamObserver::on_sector_read(const std::uint8_t command,
                                                      const std::uint8_t track,
                                                      const std::uint8_t side,
                                                      const std::uint8_t sector,
                                                      const std::uint8_t* data,
                                                      const std::size_t size) {
    machine_.log_stream_fdc_read(command, track, side, sector, data, size);
}

// --- DEF-M47-DISKWRITE Write Sector byte-trace ------------------------------

void Hbf1xvMachine::log_stream_fdc_write_byte(const std::uint8_t command, const std::uint8_t track,
                                              const std::uint8_t side, const std::uint8_t sector,
                                              const std::uint32_t lba, const int data_index,
                                              const std::uint8_t value, const bool substituted,
                                              const std::uint64_t drq_deadline) {
    // Defensive: the observer is installed only while armed. Non-perturbing:
    // inspects the committed byte + appends one line -- no emulation effect.
    if (!stream_active_) {
        return;
    }
    std::string line = "frame=" + debug_format::to_dec(frame_count_);
    line += " cmd=" + debug_format::to_hex(command, 2);
    line += " lba=" + debug_format::to_dec(lba);
    line += " chs=" + debug_format::to_dec(track) + "/" + debug_format::to_dec(side) + "/" +
            debug_format::to_dec(sector);
    line += " idx=" + debug_format::to_dec(static_cast<std::uint32_t>(data_index));
    line += " val=" + debug_format::to_hex(value, 2);
    line += substituted ? " SUBST" : " cpu";
    line += " drq=" + debug_format::to_dec(drq_deadline);
    line.push_back('\n');
    append_text_file(debug_root_ / "traces", "stream_" + stream_id_ + "_fdcwrite.log", line);
}

void Hbf1xvMachine::log_stream_fdc_write_sector(const std::uint8_t command, const std::uint8_t track,
                                                const std::uint8_t side, const std::uint8_t sector,
                                                const std::uint32_t lba, const std::uint8_t* data,
                                                const std::size_t size, const std::uint16_t crc) {
    if (!stream_active_) {
        return;
    }
    std::string line = "frame=" + debug_format::to_dec(frame_count_);
    line += " FINISH cmd=" + debug_format::to_hex(command, 2);
    line += " lba=" + debug_format::to_dec(lba);
    line += " chs=" + debug_format::to_dec(track) + "/" + debug_format::to_dec(side) + "/" +
            debug_format::to_dec(sector);
    line += " crc16=" + debug_format::to_hex(crc, 4);
    line += " crc32=" + debug_format::to_hex(crc32(data, size), 8);
    line.push_back('\n');
    append_text_file(debug_root_ / "traces", "stream_" + stream_id_ + "_fdcwrite.log", line);
}

void Hbf1xvMachine::FdcWriteStreamObserver::on_write_byte(
    const std::uint8_t command, const std::uint8_t track, const std::uint8_t side,
    const std::uint8_t sector, const std::uint32_t lba, const int data_index,
    const std::uint8_t value, const bool substituted, const std::uint64_t drq_deadline) {
    machine_.log_stream_fdc_write_byte(command, track, side, sector, lba, data_index, value,
                                       substituted, drq_deadline);
}

void Hbf1xvMachine::FdcWriteStreamObserver::on_sector_write(
    const std::uint8_t command, const std::uint8_t track, const std::uint8_t side,
    const std::uint8_t sector, const std::uint32_t lba, const std::uint8_t* data,
    const std::size_t size, const std::uint16_t crc) {
    machine_.log_stream_fdc_write_sector(command, track, side, sector, lba, data, size, crc);
}

// --- DEC-0052 stream-light WATCHLOG ----------------------------------------

void Hbf1xvMachine::log_stream_mem_write(const std::uint16_t address, const std::uint8_t value) {
    // Defensive: the observer is installed only while armed, but stay silent if a
    // finalize raced in. Watch ONLY the decisive M36 Bug B upstream addresses --
    // the IM1/RST-38 ISR-vector JP-target bytes (0x0039 low, 0x003A high; e.g.
    // minimal ISR A5E3 vs full ISR A5F5) and the sound-arm flag (0xA5E1). These
    // CPU-visible writes are RARE, so the per-write comparison overhead is
    // negligible over a long armed session. Deterministic content (machine
    // counters only; no RNG/wall-clock).
    if (!stream_active_) {
        return;
    }
    if (address != 0x0039 && address != 0x003A && address != 0xA5E1) {
        return;
    }
    std::string line = "frame=" + debug_format::to_dec(frame_count_);
    line += " cyc=" + debug_format::to_dec(elapsed_cycles());
    line += " pc=" + debug_format::to_hex(stream_pc_, 4);
    line += " WRITE addr=" + debug_format::to_hex(address, 4);
    line += " val=" + debug_format::to_hex(value, 2);
    line.push_back('\n');
    append_text_file(debug_root_ / "traces", "stream_" + stream_id_ + "_watch.log", line);
}

void Hbf1xvMachine::log_stream_vdp_register(const std::uint8_t reg, const std::uint8_t value) {
    // Watch ONLY VDP register R#1 -- the display/IE0 register (IE0 = bit5). The
    // machine funnels every R#0..R#31 control-register write here via the VDP
    // observer, so filter to R#1. Deterministic content (machine counters only).
    if (!stream_active_) {
        return;
    }
    if (reg != 1) {
        return;
    }
    std::string line = "frame=" + debug_format::to_dec(frame_count_);
    line += " cyc=" + debug_format::to_dec(elapsed_cycles());
    line += " pc=" + debug_format::to_hex(stream_pc_, 4);
    line += " VDP R1=" + debug_format::to_hex(value, 2);
    line.push_back('\n');
    append_text_file(debug_root_ / "traces", "stream_" + stream_id_ + "_watch.log", line);
}

void Hbf1xvMachine::MemWatchObserver::on_mem_write(const core::BusAddress address,
                                                   const core::BusData value) {
    machine_.log_stream_mem_write(address, value);   // DEC-0052 stream-light (self-guarded)
    machine_.mem_watch_record(address, value);       // DEC-0072 phys watchpoint (self-guarded)
}

void Hbf1xvMachine::VdpRegWatchObserver::on_register_write(const std::uint8_t reg,
                                                           const std::uint8_t value) {
    machine_.log_stream_vdp_register(reg, value);
}

}  // namespace sony_msx::machine
