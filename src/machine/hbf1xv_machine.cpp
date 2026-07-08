#include "machine/hbf1xv_machine.h"

#include <fstream>
#include <ios>
#include <system_error>
#include <utility>

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

Hbf1xvMachine::Hbf1xvMachine() : cpu_bus_client_(bus_), cpu_(cpu_bus_client_) {
    wire_bus();
}

void Hbf1xvMachine::wire_bus() {
    // --- Memory fabric (SlotBus) ---
    // HB-F1XV slot/sub-slot/page population, derived from the authoritative
    // machine XML references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml (§2 of
    // docs/m13-planner-package.md). BOTH primary slots 0 and 3 are expanded (each
    // has four <secondary> children: XML lines 85-116 and 123-199; A-6 corrects
    // the M11 wiring that expanded only slot 3).
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
    // rom_visibility page 1, XML:161-176). M16 attaches the SonyFdc decode (which
    // WRAPS disk_rom_) here in place of the bare ROM, so page-1 reads route DISK
    // ROM + the 0x7FF8-0x7FFF WD2793/glue register window (planner §3.1). The FDC
    // core advances off the deterministic emulated-cycle FdcClock.
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
    // NOT all-zero (Sony_HB-F1XV.xml:129; openMSX Ram::clear, Ram.cc:37-78). The
    // FM-PAC SRAM remains zero-initialized (no initialContent).
    initialize_dram_pattern();
    sram_.clear();

    // The V9958 VDP resets its own state, including clearing its 128 KB VRAM to
    // zero (matching the retired vram_.clear()), reloading the boot palette, and
    // releasing the /INT line (M14-S1/S4).
    vdp_.reset();

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
    // openMSX repeats this pattern over the whole 64 KB (Ram::clear, Ram.cc:66-73).
    for (std::size_t i = 0; i < kDramBytes; ++i) {
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
    bios_rom_.set_image(loader.load({"f1xvbios.rom", 0x8000, "slot 0-0 pages 0-1 (BIOS+BASIC)"}));
    sub_rom_.set_image(loader.load({"f1xvext.rom", 0x4000, "slot 3-1 page 0 (SUB)"}));
    kanji_rom_.set_image(loader.load({"f1xvkdr.rom", 0x8000, "slot 3-1 pages 1-2 (Kanji driver)"}));
    disk_rom_.set_image(loader.load({"f1xvdisk.rom", 0x4000, "slot 3-2 page 1 (DISK presence)"}));
    fmmusic_rom_.set_image(loader.load({"f1xvmus.rom", 0x4000, "slot 3-3 page 1 (FM-MUSIC presence)"}));
    kanji_font_rom_.set_image(loader.load({"f1xvkfn.rom", 0x40000, "Kanji font ROM (I/O #D8-#DB)"}));
    halnote_.set_image(
        loader.load({"f1xvfirm.rom", 0x100000, "slot 0-3 pages 0-3 (Halnote/MSX-JE firmware)"}));

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

const std::vector<std::string>& Hbf1xvMachine::rom_diagnostics() const {
    return rom_diagnostics_;
}

void Hbf1xvMachine::on_vsync_boundary() {
    // M26-S1 (docs/m26-planner-package.md §2.3): a pure, mechanical extraction
    // of run_frame()'s pre-M26 body, EXCLUDING the scheduler_.tick(kFrameCycles)
    // call -- a real-time driver's own step_cpu_instruction() sub-loop has
    // already advanced the scheduler by an equivalent amount before calling
    // this. Same four operations, same order as before: frame counter, VDP
    // VBlank delivery, the M25 Speed-Controller duty-cycle hook, and the M23-S2
    // last-vsync bookkeeping.
    ++frame_count_;

    // Deterministic per-frame VBlank delivery (M14-S5). run_frame advances the
    // clock a whole frame atomically, so the VDP VBlank is modeled at the frame
    // boundary: on_vsync sets S#0 F and, when R#1 IE0 is enabled, asserts the
    // vertical /INT line. The CPU then accepts it on the next step_cpu_instruction
    // (which level-samples the held line). Exact sub-frame raster position is
    // DEFERRED (backlog D4).
    vdp_.on_vsync();
    // M25 Speed-Controller duty-cycle hook (backlog C8, planner §2.3 point
    // 4): a single additive line, immediately alongside the existing
    // vdp_.on_vsync() call above. Advances the PAUSE controller's internal
    // VBlank-synced duty-cycle window; the Speed Controller's own numeric
    // level defaults to 0 (never paused) post-reset(), so this is a no-op by
    // default (regression guard).
    pause_controller_.on_vsync();
    // M23-S2 bookkeeping (additive-only; the sole change to this function this
    // cycle): remember the cycle count at the most recent VSync so
    // cycles_since_last_vsync()/vdp_cycle_position() can report a raster
    // position relative to it. Does not affect scheduling/CPU timing.
    last_vsync_cycle_ = elapsed_cycles();
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
    // §2.3/§2.4). Consulted FIRST, before any opcode decode. When engaged
    // (button pressed, or the Speed Controller's own duty-cycle window),
    // this skips cpu_.step() ENTIRELY -- no M1/opcode-fetch cycle occurs at
    // all, so PC/R/every register stay completely frozen (A-M25-2's literal
    // reading of the S1985 fact-sheet §9: "physically halts the CPU and
    // cannot be bypassed in software"). Genuinely different from the Z80's
    // own HALT instruction (CPU-internal, R keeps incrementing, released by
    // any interrupt) -- see the planner package §2.3 comparison table. This
    // is a small, additive, early-return-only insertion; everything below is
    // otherwise byte-for-byte unchanged from pre-M25. VDP/RTC/FDC clock
    // sources are UNAFFECTED (their crystal is not gated by the Z80 WAIT pin
    // on real hardware) -- only CPU decode/execute is suppressed. The 1
    // T-state idle charge is a documented MODELING CHOICE (R-M25-7), not a
    // hardware-quantized fact (real hardware's WAIT-line hold is not
    // naturally discretized) -- the finest-grained unit this whole-
    // instruction-atomic engine can charge for an indefinitely-held external
    // WAIT condition, with no overshoot risk.
    if (pause_controller_.cpu_should_pause()) {
        constexpr std::uint32_t kPausedIdleTStates = 1;
        scheduler_.tick(kPausedIdleTStates);
        return kPausedIdleTStates;
    }

    // Level-sample the VDP's held /INT (M14-S4, R-1). The Z80A accept path
    // clears its internal pending flag on service, but real hardware holds /INT
    // until the S#0 status read releases it. Re-asserting the request from the
    // VDP's held level each step models that hold WITHOUT clobbering an
    // externally-injected interrupt (we only ASSERT here; the VDP releases via
    // the adapter's set_irq(false) on the status read). When the VDP line is
    // idle this is a no-op, so manual interrupt-injection tests are unaffected.
    if (vdp_.irq_active()) {
        cpu_.request_maskable_interrupt();
    }

    const bool halted_before = cpu_.state().halted();
    const std::uint16_t pre_pc = cpu_.state().regs().pc;
    const std::uint8_t opcode0 = bus_.read(pre_pc);

    // The Z80 core publishes datasheet T-states unchanged (M9 timing oracles stay
    // valid); the S1985 adds +1 T-state per M1 opcode-fetch cycle (fact-sheet §8;
    // A-4 / risk R-3). The MSX-accurate machine time is datasheet + M1 wait, and
    // that is what advances the scheduler and is reported/returned here.
    const std::uint32_t datasheet_tstates = cpu_.step();
    const std::uint32_t m1_wait = s1985_engine_.m1_wait_tstates(cpu_.m1_cycles_last_step());
    const std::uint32_t tstates = datasheet_tstates + m1_wait;
    scheduler_.tick(tstates);

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

    return tstates;
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
    bus_.io_write(port, value);
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

std::size_t Hbf1xvMachine::sram_size() const {
    return sram_.size();
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
    return vdp_frame_renderer_.render_frame(field);
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

devices::cartridge::CartridgeLoadResult Hbf1xvMachine::load_cartridge(
    const int slot_number, const devices::cartridge::CartridgeMapperType type, std::vector<std::uint8_t> image) {
    if (slot_number == 1) {
        return cartridge_slot1_.load(type, std::move(image));
    }
    if (slot_number == 2) {
        return cartridge_slot2_.load(type, std::move(image));
    }
    return devices::cartridge::CartridgeLoadResult::InvalidSlotNumber;
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

const devices::audio::PsgYm2149& Hbf1xvMachine::psg() const {
    return psg_;
}

devices::audio::PsgYm2149& Hbf1xvMachine::psg() {
    return psg_;
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

const MemoryRegion& Hbf1xvMachine::sram() const {
    return sram_;
}

MemoryRegion& Hbf1xvMachine::sram() {
    return sram_;
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
    out += debug_dump::serialize_region("SRAM", sram_.data(), sram_.size());
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

}  // namespace sony_msx::machine
