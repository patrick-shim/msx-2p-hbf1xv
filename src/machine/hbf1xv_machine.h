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
#include <filesystem>
#include <string>
#include <vector>

#include "core/bus.h"
#include "core/scheduler.h"
#include "devices/audio/psg_ym2149.h"
#include "devices/audio/scc_wavetable.h"
#include "devices/audio/ym2413_opll.h"
#include "devices/chipset/io_bus.h"
#include "devices/chipset/mapper_io.h"
#include "devices/chipset/mb670836_pause.h"
#include "devices/chipset/ppi_8255.h"
#include "devices/chipset/s1985_engine.h"
#include "devices/chipset/slot_bus.h"
#include "devices/chipset/switched_io.h"
#include "devices/chipset/system_bus.h"
#include "devices/chipset/reset_status_register.h"
#include "devices/chipset/system_control.h"
#include "devices/cartridge/cartridge_fmpac_rom.h"
#include "devices/cartridge/cartridge_mapper_type.h"
#include "devices/cartridge/cartridge_slot.h"
#include "devices/cpu/cpu_bus_client.h"
#include "devices/cpu/z80a_cpu.h"
#include "devices/fdc/disk_drive.h"
#include "devices/fdc/disk_image.h"
#include "devices/fdc/fdc_clock_source.h"
#include "devices/fdc/sony_fdc.h"
#include "devices/fdc/wd2793.h"
#include "devices/halnote/halnote_rom.h"
#include "devices/kanji/kanji_font_rom.h"
#include "devices/memory/memory_mapper_ram.h"
#include "devices/memory/rom_device.h"
#include "devices/rtc/rp5c01.h"
#include "devices/video/frame_buffer.h"
#include "devices/video/irq_line.h"
#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_access_timing.h"
#include "devices/video/vdp_frame_renderer.h"
#include "devices/video/vdp_scanline_accumulator.h"
#include "machine/cpu_trace_sink.h"
#include "machine/debug_event_log.h"
#include "machine/debug_snapshot.h"
#include "machine/memory_region.h"
#include "machine/rom_asset_loader.h"
#include "peripherals/cassette_interface.h"
#include "peripherals/joystick.h"
#include "peripherals/keyboard_matrix.h"
#include "peripherals/printer_port.h"
#include "peripherals/rensha_turbo.h"

namespace sony_msx::machine {

class Hbf1xvMachine {
public:
    Hbf1xvMachine();

    void cold_boot();

    // Directory the ROM asset loader resolves the local bios/*.rom images from
    // (default "bios"). Set BEFORE cold_boot; cold_boot (re)loads the images and
    // records any missing-asset diagnostics (A-7). An absolute path keeps asset
    // loading independent of the working directory (tests inject the repo path).
    void set_asset_root(std::filesystem::path root);
    [[nodiscard]] const std::filesystem::path& asset_root() const;

    // Missing-asset diagnostics recorded by the most recent cold_boot (empty when
    // every required ROM was present + correctly sized). Never fabricated.
    [[nodiscard]] const std::vector<std::string>& rom_diagnostics() const;

    // Test/debug helper (M13-S4, discharges the M11 R-1/R-2 obligation): pages the
    // 64 KB mapper RAM (slot 3-0) into all four CPU pages as a FLAT, linear 64 KB
    // view -- the configuration CPU-over-RAM tests page in explicitly after
    // cold_boot, now that cold_boot resets #A8 to the authentic 0 (BIOS). Sets
    // #A8 so every page selects primary slot 3, the slot-3 sub-slot register to 0
    // (RAM mapper), and mapper segments {0,1,2,3} for pages {0,1,2,3} (page p ->
    // physical p*0x4000).
    void map_flat_ram();

    void run_frame();
    // Frame-boundary bookkeeping for a REAL-TIME driver (the M26 SDL3
    // frontend) that steps the CPU itself via step_cpu_instruction() until
    // elapsed_cycles() reaches a frame boundary, instead of run_frame()'s
    // coarse tick (which does not drive the CPU at all). Performs exactly
    // run_frame()'s non-tick side effects (frame counter, VDP on_vsync(),
    // pause-controller on_vsync(), last-vsync bookkeeping) but does NOT call
    // scheduler_.tick(kFrameCycles) -- the caller's own step loop has already
    // advanced the scheduler by that amount. Calling this AND run_frame() for
    // the same frame boundary would double-count elapsed cycles (R-M25-5) --
    // a session must pick ONE driving model and never mix them. A
    // behavior-preserving extraction of run_frame()'s body
    // (docs/m26-planner-package.md §2.3, M26-S1); run_frame() itself is
    // unchanged for every pre-M26 caller.
    void on_vsync_boundary();
    void run_frames(std::uint32_t frames);
    void run_cycles(std::uint64_t cycles);
    bool run_until_cycle(std::uint64_t target_cycle);
    std::uint32_t step_cpu_instruction();
    void load_memory(std::uint16_t address, const std::uint8_t* bytes, std::uint32_t size);
    [[nodiscard]] std::uint8_t read_memory(std::uint16_t address) const;

    // Non-perturbing debug seams over the full decode fabric (M13). Unlike
    // read_memory/load_memory (which are DRAM-direct, segment-independent), these
    // route through the SlotBus / IoBus exactly as the CPU does, so tests/tools
    // can inspect slot/sub-slot/mapper resolution without running the CPU. They
    // do not advance the CPU or the clock.
    [[nodiscard]] std::uint8_t debug_bus_read(std::uint16_t address);
    void debug_bus_write(std::uint16_t address, std::uint8_t value);
    [[nodiscard]] std::uint8_t debug_io_read(std::uint16_t port);
    void debug_io_write(std::uint16_t port, std::uint8_t value);
    [[nodiscard]] bool slot_expanded(int primary) const;
    // Direct (non-#FFFF-indirected) sub-slot register readback for a primary
    // slot -- reports the TRUE current page-1 routing regardless of which
    // primary currently occupies page 3 (a #FFFF access is itself indirected
    // through whatever primary answers page 3; this bypasses that). Non-perturbing.
    [[nodiscard]] std::uint8_t debug_sub_slot_register(int primary) const;
    [[nodiscard]] const devices::cpu::Z80aCpu& cpu() const;
    devices::cpu::Z80aCpu& cpu();
    [[nodiscard]] std::uint64_t elapsed_cycles() const;
    [[nodiscard]] std::uint64_t frame_count() const;
    [[nodiscard]] std::uint64_t frame_cycles_per_frame() const;

    // VDP access-timing foundation accessors (M23-S2, backlog C1/D4 partial;
    // additive-only, NON-GATING -- nothing in step_cpu_instruction()/
    // run_cycles()/run_until_cycle() consults these). cycles_since_last_vsync()
    // is elapsed_cycles() relative to the most recent run_frame()'s on_vsync()
    // call, or cycle 0 if run_frame() was never called (tested boundary
    // condition, R-M23-5; every M21/M22 system test drives the CPU purely via
    // step_cpu_instruction()). vdp_cycle_position() wraps
    // vdp_access_timing::vdp_cycle_within_line() over that same relative
    // position. See devices/video/vdp_access_timing.h for the full contract
    // and the non-gating warning.
    [[nodiscard]] std::uint64_t cycles_since_last_vsync() const;
    [[nodiscard]] int vdp_cycle_position() const;

    // Deterministic CPU trace-export facility (M10-S1). Off by default. When
    // enabled, the machine attaches its owned sink as the CPU's per-instruction
    // observer; the sink collects records and serializes a diffable text form.
    // Disabling detaches the observer (zero effect on emulation).
    void set_cpu_trace_enabled(bool enabled);
    [[nodiscard]] bool cpu_trace_enabled() const;
    [[nodiscard]] const CpuTraceSink& cpu_trace() const;
    CpuTraceSink& cpu_trace();

    // Minimum INERT memory region (M10-S2): a pure storage byte buffer sized to
    // the strict Target Machine Specification, deterministically zero-
    // initialized at cold_boot, exposing read/write/load/dump via MemoryRegion.
    // No device behavior (no slot/mapper decoding, no V9958 VDP semantics, no
    // I/O bus).
    //
    // - DRAM = 64 KB main RAM, the same store the CPU sees over the bus;
    //   load_memory/read_memory below are the CPU-visible aliases.
    //
    // NO internal SRAM (M36, DEC-0050): the earlier speculative 8 KB `sram_`
    // region was removed. The HB-F1XV's built-in FM is MSX-MUSIC (OPLL +
    // APRLOPLL BIOS, NO SRAM) -- grounded in
    // references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml (`<MSX-MUSIC>`
    // with no `<sramname>`). Battery SRAM belongs to the external, insertable
    // Panasonic FM-PAC CARTRIDGE (fmpac() / CartridgeFmPacRom), not the bare
    // machine -- so a bare HB-F1XV correctly reports "NO S-RAM AVAILABLE"; the
    // state dump's SRAM section reflects an inserted FM-PAC's SRAM when
    // present, empty otherwise.
    //
    // The 128 KB VRAM MIGRATED to the V9958 VDP device (M14-S1): no longer an
    // inert machine MemoryRegion, the CPU reaches it ONLY through the VDP I/O
    // ports #98/#99 (+ the S1985 #9C/#9D mirror). Its store + authoritative
    // size now live in devices::video::VdpVram (VdpVram::kVramBytes); access
    // via vdp().vram().
    static constexpr std::size_t kDramBytes = 64 * 1024;

    [[nodiscard]] std::size_t dram_size() const;

    [[nodiscard]] const MemoryRegion& dram() const;
    MemoryRegion& dram();

    // The V9958 VDP device (M14). Owns the 128 KB VRAM and answers ports
    // #98-#9B (+ the S1985 #9C-#9F mirror). Debug tooling reaches VRAM via
    // vdp().vram(); tests drive the register/status/interrupt contract here.
    [[nodiscard]] const devices::video::V9958Vdp& vdp() const;
    devices::video::V9958Vdp& vdp();

    // Decoded frame accessor. Signature unchanged since M21; semantics
    // upgraded by M32 (Defect A of DEC-0039, docs/m32-planner-package.md
    // §2.4): Field::Progressive (every production call site) now routes to
    // the raster-true scanline accumulator -- "accumulated past + projected
    // future". Called at a frame boundary (immediately after
    // on_vsync_boundary(), the Sdl3App::run_one_frame() shape) it returns the
    // finalized per-line frame of the frame that just ended; called mid-frame
    // it returns the partial accumulation plus a live-register projection of
    // the lines below the beam. With no mid-frame VDP write it is
    // byte-identical to the legacy snapshot renderer for any call position
    // (AC-4 hard oracle). Even/Odd (test/debug-only interlace fields; no
    // production caller) keep the legacy frozen-snapshot semantics via
    // VdpFrameRenderer::render_frame().
    //
    // Logical constness (documented decision, §2.4): the accumulator member
    // is `mutable` -- committing lines the beam has already passed is
    // memoization (a later hooked write would commit the identical bytes,
    // since only hooked writes can change VDP state between this call and
    // that write). No observable machine state changes.
    [[nodiscard]] devices::video::FrameBuffer render_frame(
        devices::video::Field field = devices::video::Field::Progressive) const;

    // M15 device integration accessors (PSG/RTC/PPI/keyboard/joystick). Tests
    // drive human input and inspect device state through these; the CPU reaches
    // them over the M11 IoBus at #A0-A2 / #B4/B5 / #A8-AB / #F5.
    [[nodiscard]] const devices::audio::PsgYm2149& psg() const;
    devices::audio::PsgYm2149& psg();
    // YM2413 (OPLL) register-accurate device (M17), answering the real
    // MSX-MUSIC I/O ports #7C/#7D alongside the unmodified M13 fmmusic_rom_
    // at slot 3-3 page 1 (A-M17-1/A-M17-2 -- no memory-space register overlay,
    // no SRAM/bank register: this machine's slot-3-3 sound device is the
    // internal MSXMusic pattern, not the external FM-PAC cartridge).
    [[nodiscard]] const devices::audio::Ym2413Opll& ym2413() const;
    devices::audio::Ym2413Opll& ym2413();
    [[nodiscard]] const devices::rtc::Rp5c01& rtc() const;
    devices::rtc::Rp5c01& rtc();
    [[nodiscard]] const devices::chipset::Ppi8255& ppi() const;
    devices::chipset::Ppi8255& ppi();
    [[nodiscard]] const peripherals::KeyboardMatrix& keyboard() const;
    peripherals::KeyboardMatrix& keyboard();
    [[nodiscard]] const peripherals::JoystickPorts& joystick() const;
    peripherals::JoystickPorts& joystick();
    [[nodiscard]] const devices::chipset::S1985Engine& s1985() const;
    devices::chipset::S1985Engine& s1985();

    // M18 peripheral I/O device integration accessors (Kanji font ROM,
    // printer port, cassette interface -- backlog B5/C7). The CPU reaches the
    // Kanji device over #D8-#DB and the printer over #90-#97; the cassette
    // interface has no dedicated CPU-visible port (A-M18-8) -- it is reached
    // only through this accessor / the existing ppi()/joystick() seams.
    [[nodiscard]] const devices::kanji::KanjiFontRom& kanji() const;
    devices::kanji::KanjiFontRom& kanji();
    [[nodiscard]] const peripherals::PrinterPort& printer() const;
    peripherals::PrinterPort& printer();
    [[nodiscard]] const peripherals::CassetteInterface& cassette() const;
    peripherals::CassetteInterface& cassette();

    // M25 Sony MB670836 hardware PAUSE + Speed Controller, and Ren-Sha Turbo
    // autofire (backlog C8). pause_controller() is the machine-level
    // CPU-execution gate consulted at the top of step_cpu_instruction() and
    // driven per-VBlank by run_frame() (§2.3/§2.4 of docs/m25-planner-
    // package.md); rensha_turbo() is the autofire signal generator wired
    // into keyboard_()/joystick_() via the M25-S3 OR-mask attach points.
    // Both mirror the existing s1985()/ppi() accessor style.
    [[nodiscard]] const devices::chipset::Mb670836PauseController& pause_controller() const;
    devices::chipset::Mb670836PauseController& pause_controller();
    [[nodiscard]] const peripherals::RenshaTurbo& rensha_turbo() const;
    peripherals::RenshaTurbo& rensha_turbo();

    // FDC subsystem (M16). The WD2793 core answers the Sony register window
    // 0x7FF8-0x7FFF over the DISK ROM at slot 3-2 page 1; the drive mounts the
    // deterministic 720 KB image. Tests drive commands over the bus and inspect
    // controller/drive state here.
    [[nodiscard]] const devices::fdc::Wd2793& fdc() const;
    devices::fdc::Wd2793& fdc();
    [[nodiscard]] const devices::fdc::DiskDrive& disk_drive() const;
    devices::fdc::DiskDrive& disk_drive();
    [[nodiscard]] const devices::fdc::DiskImage& disk_image() const;
    devices::fdc::DiskImage& disk_image();

    // External cartridge slots (M19, backlog B7). Primary slots 1 and 2 are
    // bare, unexpanded XML `<primary external="true">` bays (A-M19-1); each is
    // wired to ITS OWN CartridgeSlot device at all 4 CPU pages in wire_bus()
    // (no SlotBus::set_expanded call for either -- SlotBus::sub_for_page
    // already returns 0 unconditionally for a non-expanded primary). Loading
    // is additive/explicit: an unloaded slot is byte-for-byte identical to
    // the M13-M18 open-bus default (regression guard). load_cartridge/
    // unload_cartridge are usable directly by tests without any CLI/argv/
    // file-I/O plumbing (mirrors load_memory/set_asset_root).
    [[nodiscard]] devices::cartridge::CartridgeLoadResult load_cartridge(
        int slot_number, devices::cartridge::CartridgeMapperType type, std::vector<std::uint8_t> image);
    void unload_cartridge(int slot_number);
    [[nodiscard]] const devices::cartridge::CartridgeSlot& cartridge_slot1() const;
    devices::cartridge::CartridgeSlot& cartridge_slot1();
    [[nodiscard]] const devices::cartridge::CartridgeSlot& cartridge_slot2() const;
    devices::cartridge::CartridgeSlot& cartridge_slot2();

    // Konami SCC wavetable chip accessor (M29-S4, backlog G1; docs/m29-
    // planner-package.md §2.3). Returns the given bay's owned SccWavetable
    // when it currently holds a `KonamiSCC` cartridge, else nullptr
    // (including invalid slot numbers and every other mapper type). Mirrors
    // psg()'s accessor shape with the M25 default-nullptr precedent: with no
    // SCC cart loaded the frontend/mixer sees nullptr, byte-identical to
    // v1.0.29 (regression null). Both bays are queryable independently (real
    // hardware mixes both slots' sound-in lines). No new clock consumer and
    // no wire_bus() change: reached over the existing M19 slot attachment;
    // its generator advances only via the frontend audio pump
    // (SccWavetable::advance_cycles, A-M29-6).
    [[nodiscard]] devices::audio::SccWavetable* scc_chip(int slot_number);
    [[nodiscard]] const devices::audio::SccWavetable* scc_chip(int slot_number) const;

    // FM-PAC peripheral cartridge accessor (M36, DEC-0050). Returns the given
    // bay's owned CartridgeFmPacRom when that bay currently holds an FmPac
    // cartridge, else nullptr -- mirroring scc_chip()'s shape exactly
    // (type-safe: FmPac is the only mapper reporting mapper_type()==FmPac).
    // With no FM-PAC cart inserted the machine behaves byte-identically to
    // before (regression null). Used by tests, the state dump, and the
    // .sram-persistence plumbing.
    [[nodiscard]] devices::cartridge::CartridgeFmPacRom* fmpac(int slot_number);
    [[nodiscard]] const devices::cartridge::CartridgeFmPacRom* fmpac(int slot_number) const;

    // FM-PAC 8 KB battery-SRAM .sram persistence (M36). Mirrors the M20
    // Halnote set_halnote_sram_path/flush_halnote_sram pattern EXACTLY. Set
    // the path BEFORE load_cartridge(FmPac) so the SRAM loads on insertion
    // (absent file -> deterministic zero state, never fabricated).
    // flush_fmpac_sram() saves whichever inserted FM-PAC's SRAM to the path.
    void set_fmpac_sram_path(std::filesystem::path path);
    [[nodiscard]] const std::filesystem::path& fmpac_sram_path() const;
    bool flush_fmpac_sram() const;

    // S1985 16-byte backup-RAM .sram persistence (M15-S5, backlog C4). Set the
    // file path BEFORE cold_boot to load it (absent -> deterministic zero state,
    // preserving the M11 golden). flush_backup_ram writes the current 16 bytes.
    void set_backup_ram_path(std::filesystem::path path);
    [[nodiscard]] const std::filesystem::path& backup_ram_path() const;
    bool flush_backup_ram() const;

    // Halnote-mapped MSX-JE firmware ROM at primary slot 0, secondary slot 3
    // (M20, closes backlog B6 together with B4). Reads the real
    // bios/f1xvfirm.rom; owns the real 16 KB BatteryBackedSram store gated by
    // the mapper's own SRAM-enable bit. Persistence mirrors set_backup_ram_path/
    // backup_ram_path/flush_backup_ram EXACTLY (no CLI flag, A-M20-12): set the
    // path BEFORE cold_boot to load it (absent -> deterministic zero state).
    [[nodiscard]] const devices::halnote::HalnoteRom& halnote() const;
    devices::halnote::HalnoteRom& halnote();
    void set_halnote_sram_path(std::filesystem::path path);
    [[nodiscard]] const std::filesystem::path& halnote_sram_path() const;
    bool flush_halnote_sram() const;

    // Full-state debug dump + execution-event logging (M10-S3).
    //
    // Deterministic by construction: every serializer is hand-rolled ASCII
    // (fixed field order, fixed-width uppercase hex, '\n' endings), T-state
    // stamps come from the deterministic scheduler clock, and no wall-clock or
    // environment value is ever embedded -- two identical runs produce
    // byte-for-byte identical output. Dumping is non-perturbing: it reads
    // state only and never advances the CPU or clock.
    //
    // Output-file layout (created on demand under the configurable debug root,
    // default "debug/"):
    //   <root>/traces/<name>  — full-state dump (write_state_dump) and flushed
    //                           per-instruction CPU trace (write_cpu_trace).
    //   <root>/logs/<name>    — execution-event log (write_event_log).

    // Execution-event logging is off by default. When enabled, cold_boot records
    // a Reset event, each step_cpu_instruction records an InstructionRetire (and
    // a Halt event on the step that enters the halted state), and write_state_dump
    // records a Dump event. Enable BEFORE cold_boot to capture the Reset event.
    void set_event_logging_enabled(bool enabled);
    [[nodiscard]] bool event_logging_enabled() const;
    [[nodiscard]] const DebugEventLog& debug_event_log() const;
    DebugEventLog& debug_event_log();

    // Configurable base directory for debug output (default "debug").
    void set_debug_root(std::filesystem::path root);
    [[nodiscard]] const std::filesystem::path& debug_root() const;

    // Deterministic full-state dump text (CPU + DRAM + SRAM + VRAM). Pure/const:
    // it does not mutate machine state, the CPU, or the clock.
    [[nodiscard]] std::string serialize_state_dump() const;

    // Write the full-state dump to <root>/traces/<filename>, creating the
    // directory if absent. Records a Dump event when event logging is enabled.
    // Returns true on success. Non-perturbing to CPU/memory/clock state.
    bool write_state_dump(const std::string& filename);

    // Flush the collected S1 per-instruction CPU trace to <root>/traces/<filename>.
    bool write_cpu_trace(const std::string& filename) const;

    // Write the execution-event log to <root>/logs/<filename>.
    bool write_event_log(const std::string& filename) const;

    // Decoded-FrameBuffer -> dump capture (M26-S4, the ONE new debug/testing
    // capability this milestone authorizes -- docs/m26-planner-package.md
    // §2.5). Mirrors write_state_dump()/write_cpu_trace()/write_event_log()
    // exactly (same directory-creation/error-handling pattern, same
    // debug_root_-relative convention); writes to <root>/frames/<filename>.
    // Pure/const, non-perturbing -- render_frame() is itself already a pure,
    // on-demand VRAM/register snapshot (M21).
    [[nodiscard]] std::string serialize_frame_dump(
        devices::video::Field field = devices::video::Field::Progressive) const;
    bool write_frame_dump(const std::string& filename,
                          devices::video::Field field = devices::video::Field::Progressive);

    // --- M36 Phase 3 comprehensive debug SNAPSHOT (DEC-0051,
    //     docs/m36-phase3-planner-package.md). A SEPARATE artifact from the
    //     golden-locked serialize_state_dump() (M10/M13/M14): captures the
    //     exact current state of EVERY machine component (§2.3 inventory) into
    //     a per-component typed dump, versioned "HBF1XV-SNAPSHOT v1" and
    //     designed restore-ready (restore itself is a future milestone).
    //     Capture-only + additive + read-only w.r.t. emulation: advances
    //     neither the CPU nor the scheduler, issues no debug_io_write, and
    //     consumes no RNG/wall-clock, so an identical run to the same frame
    //     produces byte-identical snapshot content (auditable determinism). ---

    // The #FC-#FF memory-mapper segment registers (planner §2.4 item 2). const
    // read-only accessor mirroring s1985()/ppi().
    [[nodiscard]] const devices::chipset::MapperIo& mapper_io() const;

    // Deterministic snapshot id: "f<frame_count>_c<elapsed_cycles>" (both
    // deterministic machine counters). An identical run to the same frame
    // boundary yields the identical id and byte-identical content (§3.2).
    [[nodiscard]] std::string snapshot_id() const;

    // Build the full snapshot bundle (manifest + per-component files) for the
    // given id. `caller_notes` are appended verbatim to the manifest (e.g. the
    // frontend's multi-disk index -- planner Assumption A4: a FRONTEND concept
    // added by the CALLER, not machine state). NOT const: it uses the
    // non-perturbing debug_io_read seam (#F4/#F5/#A8) which is declared
    // non-const, but it is non-perturbing to CPU/memory/clock state.
    [[nodiscard]] debug_snapshot::Snapshot serialize_snapshot(
        const std::string& id, const std::vector<std::string>& caller_notes = {});

    // Write the snapshot bundle to <debug_root>/snapshot/<id>/<file> (one file
    // per component + manifest.txt), creating the directory. Returns true iff
    // every file wrote. Non-perturbing to CPU/memory/clock state.
    bool write_snapshot(const std::string& id, const std::vector<std::string>& caller_notes = {});

    // --- DEC-0052 live STREAM-CAPTURE (crash-trajectory diagnostic; additive,
    //     default-off). Extends the M36 Phase-3 snapshot + the M10/M27
    //     CPU-trace facility to record the trajectory INTO a control-flow crash
    //     (the M36 Bug B YS-II building-entry HALT). Armed at a chosen moment
    //     (F10 in the SDL frontend, or set_stream_capture_enabled() at the
    //     machine level); while armed it
    //       (1) writes a full per-component snapshot bundle at EVERY frame
    //           boundary (end of on_vsync_boundary()) into
    //           <debug_root>/snapshot/stream_<id>/<frame-id>/ -- the
    //           frame-by-frame state evolution, and
    //       (2) keeps a BOUNDED per-instruction trace ring (the most-recent N
    //           records) captured around step_cpu_instruction().
    //     The instant the CPU enters HALT (the crash), it AUTO-STOPS: dumps the
    //     ring oldest->newest to <debug_root>/traces/stream_<id>_trace.txt,
    //     writes ONE final snapshot (stream_<id>/HALT_<frame-id>/), then
    //     disarms. A manual OFF finalizes the same way.
    //
    //     Zero-cost + byte-identical when OFF: every hook is behind an
    //     `if (stream_active_)` guard, consumes no RNG/wall-clock, and does not
    //     advance/perturb the CPU or scheduler (the ring records are built from
    //     the same non-perturbing const getters the Phase-3 snapshot uses; the
    //     #A8 slot select comes from the non-perturbing debug_io_read seam).
    //     The hook lives entirely at the MACHINE level (step_cpu_instruction /
    //     on_vsync_boundary) -- src/devices/cpu/ + src/core/ are untouched
    //     (ZEXALL withheld). ---
    //
    //     DEC-0052 enhancement (M36 Bug B): the finalize trigger ALSO fires on a
    //     STACK RUNAWAY, not only on HALT. The YS-II building-entry crash is not
    //     a clean Z80 HALT -- PC derails into a data region, garbage CALLs push
    //     the stack down ~2 KB/frame until it collapses into an RST-38 loop
    //     (PC=0x0038, HALT=0), which the HALT-only trigger never caught. When
    //     the stack pointer underflows kStreamStackFloor (an address normal
    //     execution never reaches -- the YS-II stack lives ~0xDAxx), the
    //     capture finalizes itself into a distinct CRASH_ bundle (see
    //     finalize_stream_capture). ---
    static constexpr std::size_t kStreamTraceRingCapacity = 1u << 20;  // 1,048,576 records

    // DEC-0052 stack-runaway finalize floor. SP < this is an unambiguous
    // runaway: the YS-II stack lives ~0xDAxx and normal MSX execution never
    // runs the stack below 0x4000, whereas the RST-38 crash loop drives SP
    // down hard right after the derail. Firing here still keeps the derail
    // inside the 1M-record ring (~2.8 s ~= ~170 frames): at the observed
    // ~2 KB/frame drop the RST-38 loop reaches SP<0x4000 within a handful of
    // frames of the derail (frame ~2683), so it stays comfortably retained.
    static constexpr std::uint16_t kStreamStackFloor = 0x4000;

    // --- DEC-0052 "stream-light" mode (M36 Bug B long-session upstream hunt). A
    //     lightweight streaming variant for a long armed session (YS-II game
    //     start -> walking to a building -> entering it) that the heavy
    //     per-frame snapshot bundle would bog down. When light mode is armed:
    //       (a) the per-vsync full-snapshot bundle is suppressed, replaced by a
    //           coarse anchor snapshot every kStreamLightSnapshotInterval
    //           frames (~2 s) plus the existing crash/HALT/manual finalize
    //           snapshot, and
    //       (b) a per-event WATCHLOG (<debug_root>/traces/stream_<id>_watch.log)
    //           records the decisive-but-rare upstream events over the whole
    //           session: CPU memory writes to the IM1/RST-38 ISR-vector bytes
    //           0x0039/0x003A and the sound-arm flag 0xA5E1 (via the mapper-RAM
    //           write observer), and writes to VDP register R#1 (via the VDP
    //           register-write observer). These events are rare, so watchlog
    //           overhead is negligible over a long session.
    //     The heavy per-frame mode remains the default when light is off (§3.3);
    //     the ring + its HALT/SP-underflow auto-finalize are unchanged (the ring
    //     stays at kStreamTraceRingCapacity = 1<<20 ~= 2.8 s -- the watchlog, not
    //     the ring, is the whole-session upstream record, so the ring width need
    //     not grow). ---
    static constexpr std::uint32_t kStreamLightSnapshotInterval = 120;  // frames (~2 s)

    // Arm (enabled=true) / finalize (enabled=false) the stream capture. Arming
    // clears the ring and stamps the stream id (defaults to snapshot_id() at the
    // arm moment when stream_id is empty). `light` selects the lightweight mode
    // above (default false = the heavy per-frame-snapshot mode, byte-for-byte the
    // pre-DEC-0052-light behavior). Finalizing dumps the ring + writes the final
    // snapshot + disarms (a no-op if not currently armed). Arming twice is a
    // no-op.
    void set_stream_capture_enabled(bool enabled, const std::string& stream_id = std::string{},
                                    bool light = false);
    [[nodiscard]] bool stream_capture_active() const;
    // Whether the currently-armed capture is in the lightweight mode (false when
    // disarmed or armed heavy). For tests/diagnostics.
    [[nodiscard]] bool stream_capture_light() const;
    [[nodiscard]] const std::string& stream_capture_id() const;
    // Records currently held in the ring (<= kStreamTraceRingCapacity). For tests.
    [[nodiscard]] std::size_t stream_trace_ring_size() const;
    // Deterministic oldest->newest serialization of the ring -- the SAME text
    // finalize dumps to disk. Pure/const; reuses CpuTraceSink::format_record.
    [[nodiscard]] std::string serialize_stream_trace() const;

private:
    static bool write_text_file(const std::filesystem::path& directory, const std::string& filename,
                                const std::string& text);
    // Like write_text_file but APPENDS (never truncates) -- the DEC-0052 FDC
    // sector-read log accumulates one line per read while streaming. Determinism
    // is preserved by removing any stale log at arm time (set_stream_capture_
    // enabled), so an identical armed run rebuilds a byte-identical log.
    static bool append_text_file(const std::filesystem::path& directory,
                                 const std::string& filename, const std::string& text);

    // --- DEC-0052 stream-capture internals (all genuine no-ops unless armed). ---
    // Standard IEEE-802.3 / zlib CRC-32 (reflected, poly 0xEDB88320, init/final
    // ~0) of a byte range -- the FDC log's integrity anchor for diffing our
    // returned disk bytes against the raw .dsk (matches python zlib.crc32). Pure
    // function of the bytes (no wall-clock/RNG).
    [[nodiscard]] static std::uint32_t crc32(const std::uint8_t* data, std::size_t size);
    // Append one deterministic line describing a completed FDC sector read to
    // <debug_root>/traces/stream_<id>_fdc.log while streaming (called via the
    // FdcStreamObserver installed on fdc_). Non-perturbing: it only inspects the
    // already-read bytes + writes a host file; no emulation/scheduler/CPU effect.
    void log_stream_fdc_read(std::uint8_t command, std::uint8_t track, std::uint8_t side,
                             std::uint8_t sector, const std::uint8_t* data, std::size_t size);
    // DEC-0052 stream-light WATCHLOG appenders (called via the mapper-RAM /
    // VDP register-write observers below while streaming). Each appends one
    // deterministic line to <debug_root>/traces/stream_<id>_watch.log for a
    // WATCHED event only (all others are silently ignored, so the per-write /
    // per-register-write overhead is a few comparisons). Non-perturbing: read
    // deterministic machine counters (frame_count_, elapsed_cycles(), the
    // current-instruction PC stream_pc_) + append a host file; no emulation/
    // scheduler/CPU effect. `address` is the CPU-visible write address.
    void log_stream_mem_write(std::uint16_t address, std::uint8_t value);
    void log_stream_vdp_register(std::uint8_t reg, std::uint8_t value);
    // Build a PRE-execution trace record from the live CPU state, REUSING the M27
    // Z80aTraceRecord convention. Const w.r.t. the machine (read-only getters).
    [[nodiscard]] devices::cpu::Z80aTraceRecord capture_stream_pre_record(
        std::uint16_t pre_pc, std::uint8_t opcode0) const;
    // Push a completed record (+ its #A8 primary-slot select) into the bounded
    // ring, evicting the oldest once full.
    void push_stream_record(const devices::cpu::Z80aTraceRecord& record, std::uint8_t a8);
    // Dump the ring + write one final snapshot, uninstall the FDC observer, then
    // disarm (stream_active_=false). `reason_note` is the verbatim
    // "finalize_reason=..." manifest [NOTES] line (self-evident crash cause);
    // `bundle_prefix` distinguishes the finalize bundle directory (HALT_ vs the
    // stack-runaway CRASH_ vs the manual MANUAL_).
    void finalize_stream_capture(const std::string& reason_note, const std::string& bundle_prefix);

    // Wire the slot/I/O decode fabric and S1985 layer onto the buses. Called
    // once from the constructor (static composition; per-boot volatile state is
    // (re)initialized in cold_boot).
    void wire_bus();

    // Cold-boot RAM power-on content (A-5): the XML `initialContent` alternating
    // 00/FF pattern (Sony_HB-F1XV.xml:129) decoded + repeat-filled across 64 KB,
    // matching openMSX Ram::clear (references/openmsx-21.0/src/memory/Ram.cc:37-78).
    void initialize_dram_pattern();

    // (Re)load the local bios/ ROM images into the ROM devices from asset_root_,
    // applying the deterministic missing-asset policy; records diagnostics.
    void load_rom_assets();

    // Render-sync adapter (M32-S2, docs/m32-planner-package.md §2.3): the
    // machine-side listener behind V9958Vdp's render-sync seam. On every
    // hooked VDP io_write it reads the live raster line L =
    // vdp_.raster_display_line() and commits display lines [watermark, L+1)
    // -- a write while the beam is on line L takes effect from line L+1;
    // lines <= L keep the pre-write state. Line-granular simplification of
    // openMSX's line-accuracy rounding
    // (references/openmsx-21.0/src/video/PixelRenderer.cc:549-571), erring by
    // at most one line vs the pixel-accurate model (§2.3 precision
    // disclosure). `suspended` gates the machine's non-perturbing
    // debug_io_write() seam OUT of the hook (§2.3: scenes built through
    // debug_io_write use the lazy projection/finalize path instead, which
    // stays "non-perturbing" per integration test).
    class VdpRenderSyncAdapter final : public devices::video::VdpRenderSyncListener {
    public:
        explicit VdpRenderSyncAdapter(Hbf1xvMachine& machine) : machine_(machine) {}
        void on_before_state_change() override;
        void set_suspended(const bool suspended) { suspended_ = suspended; }

    private:
        Hbf1xvMachine& machine_;
        bool suspended_ = false;
    };

    // Line-interrupt delivery (M32-S2, the DEC-0039/RESP-M32-001 D-1
    // ratified leg; §2.5). Consulted once per step_cpu_instruction, O(1):
    // an input-fingerprint check (R#19/R#23/R#9/last-vsync value compare)
    // plus one cycle compare against the cached next-match cycle. The cache
    // recomputes only when an input changed (openMSX's own reschedule
    // semantic, VDP.cc:518-524) or after a crossing fired.
    void poll_line_interrupt();
    void recompute_line_interrupt_cache(std::uint8_t r19, std::uint8_t r23, std::uint8_t r9,
                                        std::uint64_t now);

    // Level-held /INT adapter (M14-S4): forwards the V9958's owned interrupt
    // line to the M12 Z80A maskable-interrupt request/clear, REUSING the IM1
    // acceptance path unchanged. Bound to cpu_ (declared below) at construction.
    class CpuIrqAdapter final : public devices::video::IrqLine {
    public:
        explicit CpuIrqAdapter(devices::cpu::Z80aCpu& cpu) : cpu_(cpu) {}
        void set_irq(bool asserted) override;

    private:
        devices::cpu::Z80aCpu& cpu_;
    };

    // DEC-0052 stream-capture FDC observer adapter. Installed on fdc_ only
    // while stream_active_ (set_stream_capture_enabled arms it,
    // finalize_stream_capture removes it); default-null on fdc_ => zero
    // behaviour change. Forwards each completed sector read to
    // log_stream_fdc_read(), which appends one line to the per-stream FDC log
    // so the human can diff our returned disk bytes against the raw .dsk.
    // Non-perturbing (see FdcSectorReadObserver's contract).
    class FdcStreamObserver final : public devices::fdc::FdcSectorReadObserver {
    public:
        explicit FdcStreamObserver(Hbf1xvMachine& machine) : machine_(machine) {}
        void on_sector_read(std::uint8_t command, std::uint8_t track, std::uint8_t side,
                            std::uint8_t sector, const std::uint8_t* data,
                            std::size_t size) override;

    private:
        Hbf1xvMachine& machine_;
    };

    // DEC-0052 stream-light mapper-RAM write observer. Installed on ram_mapper_
    // only while streaming (set_stream_capture_enabled arms it, finalize
    // removes it); default-null => zero behaviour change. Forwards each CPU
    // RAM write to log_stream_mem_write(), which appends a watchlog line for
    // the watched addresses (0x0039/0x003A/0xA5E1) only. Non-perturbing.
    class MemWatchObserver final : public devices::memory::MemWriteObserver {
    public:
        explicit MemWatchObserver(Hbf1xvMachine& machine) : machine_(machine) {}
        void on_mem_write(core::BusAddress address, core::BusData value) override;

    private:
        Hbf1xvMachine& machine_;
    };

    // DEC-0052 stream-light VDP control-register-write observer. Installed on
    // vdp_ only while streaming; default-null => zero behaviour change.
    // Forwards each R#0..R#31 write to log_stream_vdp_register(), which appends
    // a watchlog line for R#1 only. Non-perturbing.
    class VdpRegWatchObserver final : public devices::video::VdpRegisterWriteObserver {
    public:
        explicit VdpRegWatchObserver(Hbf1xvMachine& machine) : machine_(machine) {}
        void on_register_write(std::uint8_t reg, std::uint8_t value) override;

    private:
        Hbf1xvMachine& machine_;
    };

    // Deterministic emulated-cycle clock source for the RTC (X4). Returns the
    // scheduler's total cycles READ-ONLY — the RTC advances its time from this
    // without ever touching CPU T-state accounting or the host wall clock.
    class RtcClock final : public devices::rtc::RtcClockSource {
    public:
        explicit RtcClock(const core::Scheduler& scheduler) : scheduler_(scheduler) {}
        [[nodiscard]] std::uint64_t cpu_cycles() const override;

    private:
        const core::Scheduler& scheduler_;
    };

    // Deterministic emulated-cycle clock source for the FDC (M16, X-pattern of
    // RtcClock). Returns scheduler total cycles read-only; all FDC Busy/DRQ/
    // step/index/motor timing advances off this, never the host wall clock or
    // CPU T-state accounting (protecting the M9/M12 oracles).
    class FdcClock final : public devices::fdc::FdcClockSource {
    public:
        explicit FdcClock(const core::Scheduler& scheduler) : scheduler_(scheduler) {}
        [[nodiscard]] std::uint64_t cpu_cycles() const override;

    private:
        const core::Scheduler& scheduler_;
    };

    // Deterministic emulated-cycle clock source for the cassette interface
    // (M18-S3/S4, X-pattern of RtcClock/FdcClock). Returns scheduler total
    // cycles READ-ONLY; the synthetic-tape input model advances off this,
    // never the host wall clock or CPU T-state accounting (A-M18-12).
    // Consulted pull-style only from CassetteInterface -- never wired into
    // step_cpu_instruction()/run_cycles()/run_frame().
    class CassetteClock final : public peripherals::CassetteClockSource {
    public:
        explicit CassetteClock(const core::Scheduler& scheduler) : scheduler_(scheduler) {}
        [[nodiscard]] std::uint64_t cpu_cycles() const override;

    private:
        const core::Scheduler& scheduler_;
    };

    // Deterministic emulated-cycle clock source for Ren-Sha Turbo (M25,
    // X-pattern of RtcClock/FdcClock/CassetteClock, backlog C8). Returns
    // scheduler total cycles READ-ONLY; the autofire signal advances off
    // this, never the host wall clock or CPU T-state accounting (protecting
    // the M9/M12/M23 zero-tolerance CPU-timing oracles). Consulted
    // pull-style only from KeyboardMatrix/JoystickPorts -- never wired into
    // step_cpu_instruction()/run_cycles()/run_frame().
    class RenshaTurboClock final : public peripherals::RenshaTurboClockSource {
    public:
        explicit RenshaTurboClock(const core::Scheduler& scheduler) : scheduler_(scheduler) {}
        [[nodiscard]] std::uint64_t cpu_cycles() const override;

    private:
        const core::Scheduler& scheduler_;
    };

    // Deterministic emulated-cycle clock source for the YM2413 E2
    // write-timing gate (M28-S1, X-pattern of RtcClock/FdcClock/
    // CassetteClock/RenshaTurboClock, backlog E2). Returns scheduler total
    // cycles read-only; the gate's spacing check advances off this, never
    // the host wall clock or CPU T-state accounting (protecting the
    // M9/M12/M23 zero-tolerance CPU-timing oracles). Consulted pull-style
    // only from Ym2413Opll::write_address()/write_data() -- never wired into
    // step_cpu_instruction()/run_cycles()/run_frame(). Wiring this clock
    // source does not by itself change behaviour: the gate defaults to OFF
    // (Ym2413Opll::write_timing_enforced() == false) until a caller opts in
    // via set_write_timing_enforced(true) (docs/m28-implementation-report.md's
    // regression pre-check).
    class Ym2413Clock final : public devices::audio::Ym2413ClockSource {
    public:
        explicit Ym2413Clock(const core::Scheduler& scheduler) : scheduler_(scheduler) {}
        [[nodiscard]] std::uint64_t cpu_cycles() const override;

    private:
        const core::Scheduler& scheduler_;
    };

    // Deterministic clock source for the VDP's S#2 VR/HR raster-position
    // status bits (bug fix, post-M28: the real BIOS hangs forever polling
    // VR/HR during early boot when they are a hardcoded constant, because
    // real hardware's VR/HR genuinely toggle every frame/line). Unlike the
    // other X-pattern clocks above, this one needs BOTH the scheduler AND
    // last_vsync_cycle_ (declared further below) -- it is declared as a data
    // MEMBER after last_vsync_cycle_ so reference-member initialization order
    // is correct; this nested TYPE definition may appear anywhere relative to
    // that, since attaching it to vdp_ happens in wire_bus() (constructor
    // body), after all members exist.
    class VdpRasterClock final : public devices::video::VdpClockSource {
    public:
        VdpRasterClock(const core::Scheduler& scheduler, const std::uint64_t& last_vsync_cycle)
            : scheduler_(scheduler), last_vsync_cycle_(last_vsync_cycle) {}
        [[nodiscard]] std::uint64_t cpu_tstates_since_vsync() const override {
            return scheduler_.total_cycles() - last_vsync_cycle_;
        }

    private:
        const core::Scheduler& scheduler_;
        const std::uint64_t& last_vsync_cycle_;
    };

    core::Scheduler scheduler_;
    MemoryRegion dram_{kDramBytes};

    // S1985 "MSX-ENGINE" chipset + full system bus (M11). The machine owns the
    // decode fabrics and the residual engine layer; the CPU talks to SystemBus.
    devices::chipset::SlotBus slot_bus_;
    devices::chipset::IoBus io_bus_;
    // Human-input peripherals (M15). Declared before the PPI so the PPI can bind
    // the keyboard matrix by reference at construction.
    peripherals::KeyboardMatrix keyboard_;
    peripherals::JoystickPorts joystick_;
    // Centronics printer port peripheral (M18-S2, part of backlog C7),
    // answering #90-#97 (A-M18-5). Purely combinational (A-M18-4's reasoning
    // applies identically) -- no clock dependency.
    peripherals::PrinterPort printer_;
    devices::chipset::Ppi8255 ppi_{slot_bus_, keyboard_};  // #A8-#AB (+#AC-AF mirror)
    // Cassette interface (M18-S3, part of backlog C7): motor/output derived
    // READ-ONLY from ppi_ (A-M18-9, zero edit to Ppi8255); cassette_clock_
    // feeds its deterministic synthetic-tape input model, pull-style only
    // (A-M18-11/A-M18-12). Declared after ppi_ (binds a const reference to
    // it), mirroring the existing Ppi8255 ppi_{slot_bus_, keyboard_} ordering
    // rule.
    CassetteClock cassette_clock_{scheduler_};
    peripherals::CassetteInterface cassette_{ppi_};

    // Ren-Sha Turbo autofire (M25, backlog C8 sub-item): the signal
    // generator + its X-pattern clock adapter, attached into keyboard_/
    // joystick_ via wire_bus()'s additive OR-mask seams (M25-S3). Owned
    // instance's own default post-reset() is speed_=0 (disabled) --
    // regression-safe.
    RenshaTurboClock rensha_clock_{scheduler_};
    peripherals::RenshaTurbo rensha_turbo_;

    devices::chipset::MapperIo mapper_io_;      // #FC-#FF mapper readback (segment owner)
    devices::chipset::SwitchedIoController switched_io_;  // #40-#4F switched I/O
    devices::chipset::S1985Engine s1985_engine_;  // backup RAM ID 0xFE + M1 wait

    // PSG (YM2149) on #A0-A2 and RTC (RP5C01) on #B4/B5 — the M11 seam ports now
    // answered by real devices (M15). The #F5 system-control register gates the
    // RTC CLOCK-IC. rtc_clock_ feeds the RTC deterministic emulated time.
    devices::audio::PsgYm2149 psg_;
    // YM2413 (OPLL) register-accurate device (M17, backlog B3) on #7C/#7D --
    // an IoDevice attached alongside the unmodified M13 fmmusic_rom_ (below).
    // ym2413_clock_ feeds the M28-S1 (E2) write-timing gate READ-ONLY; the
    // gate itself defaults OFF, so wiring this clock source is a no-op
    // unless a caller explicitly enables it.
    Ym2413Clock ym2413_clock_{scheduler_};
    devices::audio::Ym2413Opll ym2413_;
    devices::chipset::SystemControlF5 system_control_;  // #F5 (RTC clock gate)
    // #F4 reset-status latch (boot-logo fix): the MSX2+ BIOS reads it to
    // distinguish cold power-up (bit 7 clear -> run the animated MSX logo)
    // from a warm restart (bit 7 set -> skip it). Sony_HB-F1XV.xml declares
    // the non-inverted variant on port #F4.
    devices::chipset::ResetStatusRegister reset_status_;  // #F4
    RtcClock rtc_clock_{scheduler_};
    devices::rtc::Rp5c01 rtc_;  // #B4/#B5
    std::filesystem::path backup_ram_path_;
    std::filesystem::path halnote_sram_path_;  // M20, mirrors backup_ram_path_ exactly
    std::filesystem::path fmpac_sram_path_;    // M36, mirrors halnote_sram_path_ exactly

    // CPU-addressable memory devices (M13). The mapper RAM consumes mapper_io_'s
    // live segments; the ROM devices are read-only windows over the loaded images.
    // Window base/size come from the Sony_HB-F1XV.xml `<mem>` placements (§2).
    devices::memory::MemoryMapperRam ram_mapper_{dram_, mapper_io_};  // slot 3-0 p0-3
    devices::memory::RomDevice bios_rom_{0x0000, 0x8000};   // slot 0-0 p0-1 (BIOS+BASIC)
    devices::memory::RomDevice sub_rom_{0x0000, 0x4000};    // slot 3-1 p0   (SUB)
    devices::memory::RomDevice kanji_rom_{0x4000, 0x8000};  // slot 3-1 p1-2 (Kanji driver)
    devices::memory::RomDevice disk_rom_{0x4000, 0x4000};   // slot 3-2 p1   (DISK ROM window)
    // FM-MUSIC ROM presence, slot 3-3 page 1 (M13). Unchanged by M17 (A-M17-2,
    // regression guard): the real MSXMusic device has NO memory-space register
    // overlay, so this plain ROM window needs no wrapping device -- the YM2413
    // (ym2413_, above) is attached SEPARATELY, only on io_bus_ #7C/#7D.
    devices::memory::RomDevice fmmusic_rom_{0x4000, 0x4000};  // slot 3-3 p1 (FM-MUSIC presence)

    // Halnote-mapped MSX-JE firmware ROM, slot 0-3, ALL 4 pages (M20, backlog
    // B6, closes B4 together). Composes the M19 CartridgeRomWindow (main 8-slot
    // window, default block mask, NO Konami-style override) + the M17
    // BatteryBackedSram (real 16 KB SRAM store, A-M20-10/A-M20-11). Pure
    // combinational device (A-M20-... determinism) -- no clock adapter needed.
    devices::halnote::HalnoteRom halnote_;

    // Kanji font ROM I/O device (M18-S1, backlog B5), answering #D8-#DB
    // directly (A-M18-1: MSXKanji, NOT the switched-I/O MSXKanji12). Reads
    // the real 256 KB bios/f1xvkfn.rom (JIS1+JIS2). Pure combinational device
    // (A-M18-4) -- no clock dependency, not slot-attached (I/O-only).
    devices::kanji::KanjiFontRom kanji_font_rom_;

    // FDC (M16): WD2793 core + built-in 720 KB drive + deterministic image, and
    // the SonyFdc decode that wraps disk_rom_ and answers 0x7FF8-0x7FFF. Attached
    // at slot 3-2 page 1 in place of the bare disk_rom_ (planner §3.1).
    FdcClock fdc_clock_{scheduler_};
    devices::fdc::DiskImage disk_image_;
    devices::fdc::DiskDrive disk_drive_;
    devices::fdc::Wd2793 fdc_;
    devices::fdc::SonyFdc sony_fdc_{disk_rom_, fdc_, disk_drive_, fdc_clock_};

    // External cartridge slots (M19, backlog B7): primary slots 1 and 2,
    // each a bare, unexpanded external bay (A-M19-1). Empty by default
    // (nullptr mapper inside CartridgeSlot) -- byte-for-byte M13-M18 open-bus
    // regression guard until load_cartridge() is called.
    devices::cartridge::CartridgeSlot cartridge_slot1_{1};
    devices::cartridge::CartridgeSlot cartridge_slot2_{2};

    devices::chipset::SystemBus bus_{slot_bus_, io_bus_};

    std::filesystem::path asset_root_{"bios"};
    std::vector<std::string> rom_diagnostics_;

    devices::cpu::CpuBusClient cpu_bus_client_;
    devices::cpu::Z80aCpu cpu_;

    // V9958 VDP (M14) + its /INT adapter. cpu_irq_adapter_ binds cpu_ (declared
    // above) and is registered as the VDP's IRQ sink in wire_bus().
    devices::video::V9958Vdp vdp_;
    CpuIrqAdapter cpu_irq_adapter_{cpu_};
    // M21 pixel renderer (additive; binds vdp_ by const reference at
    // construction -- must be declared AFTER vdp_).
    devices::video::VdpFrameRenderer vdp_frame_renderer_{vdp_};
    // M32 scanline accumulator (Defect A; §2.2). Declared AFTER
    // vdp_frame_renderer_ (binds const V9958Vdp& + const VdpFrameRenderer&).
    // `mutable` per the documented logical-constness decision on
    // render_frame() above -- committing already-scanned lines is
    // memoization, not observable state change.
    mutable devices::video::VdpScanlineAccumulator scanline_accumulator_{vdp_, vdp_frame_renderer_};
    // The render-sync listener instance attached to vdp_ in wire_bus().
    VdpRenderSyncAdapter render_sync_adapter_{*this};

    CpuTraceSink cpu_trace_sink_;
    DebugEventLog debug_event_log_;
    bool event_logging_enabled_ = false;
    std::filesystem::path debug_root_{"debug"};
    std::uint64_t frame_count_ = 0;

    // VDP access-timing foundation bookkeeping (M23-S2). Updated by exactly
    // ONE added line inside the existing run_frame(), alongside the existing
    // vdp_.on_vsync() call -- run_frame() is otherwise unchanged. Defaults to
    // 0 (program start), matching the documented "no vsync yet" semantic of
    // cycles_since_last_vsync()/vdp_cycle_position() above.
    std::uint64_t last_vsync_cycle_ = 0;

    // VDP raster-position clock (bug fix, post-M28). Declared AFTER
    // last_vsync_cycle_ (initialization-order requirement, see the class
    // comment above); attached to vdp_ in wire_bus().
    VdpRasterClock vdp_raster_clock_{scheduler_, last_vsync_cycle_};

    // Line-interrupt delivery cache (M32-S2, §2.5). line_int_next_cycle_ is
    // the absolute scheduler cycle at which the raster next ENTERS screen
    // line M = (R#19 - R#23) & 0xFF -- the relation both behavior references
    // agree on (openMSX references/openmsx-21.0/src/video/VDP.cc:527-529:
    // `((controlRegs[19] - controlRegs[23]) & 0xFF)` display lines after
    // display start; fMSX references/fmsx-60/source/fMSX/MSX.c:2094-2100:
    // fires when `(((ScanLine+VScroll)&0xFF)-VDP[19])&0xFF` hits its
    // coincidence value -- algebraically identical). kLineIntNever = the
    // openMSX "never occurs" clamp analogue (VDP.cc:554-559) for M >= the
    // active line count.
    static constexpr std::uint64_t kLineIntNever = ~static_cast<std::uint64_t>(0);
    std::uint64_t line_int_next_cycle_ = kLineIntNever;
    std::uint8_t line_int_r19_ = 0;
    std::uint8_t line_int_r23_ = 0;
    std::uint8_t line_int_r9_ = 0;
    std::uint64_t line_int_vsync_ = 0;
    bool line_int_cache_valid_ = false;

    // M25 Sony MB670836 hardware PAUSE + Speed Controller (backlog C8). A
    // machine-level CPU-execution gate consulted at the very top of
    // step_cpu_instruction() (docs/m25-planner-package.md §2.3/§2.4) -- NOT
    // part of Z80aCpu (zero edit to src/devices/cpu/*). Its VBlank-synced
    // duty-cycle window is advanced by exactly ONE added line inside
    // run_frame(), alongside the existing vdp_.on_vsync() call.
    devices::chipset::Mb670836PauseController pause_controller_;

    // --- DEC-0052 live stream-capture state (default-off; see the public API). ---
    // One ring entry = the reused M27 per-instruction record + the #A8 primary-
    // slot select at that instruction (the slot field is NOT part of the CPU-core
    // record type, so it is carried alongside here -- no src/devices/cpu/ edit).
    struct StreamTraceEntry {
        devices::cpu::Z80aTraceRecord cpu;  // pre-execution register/opcode snapshot
        std::uint8_t a8 = 0;                // #A8 primary-slot select at capture
    };
    bool stream_active_ = false;
    // DEC-0052 stream-light: whether the armed capture is the lightweight
    // variant (per-frame bundles suppressed -> coarse anchors + watchlog).
    bool stream_light_ = false;
    // DEC-0052 stream-light: the PC of the instruction currently executing,
    // stamped at the top of step_cpu_instruction() while armed. The mapper-RAM /
    // VDP register-write observers fire DURING cpu_.step() (before the scheduler
    // ticks), so the watchlog reports this instruction's PC and the cycle count
    // at instruction START -- both deterministic.
    std::uint16_t stream_pc_ = 0;
    std::string stream_id_;
    std::uint64_t stream_seq_ = 0;              // absolute instruction index since arm
    std::vector<StreamTraceEntry> stream_ring_;  // grows to kStreamTraceRingCapacity, then wraps
    std::size_t stream_ring_head_ = 0;           // index of the oldest entry once the ring is full
    // FDC per-sector-read logger adapter (installed on fdc_ only while armed). Only
    // holds a reference to *this, so declaration order vs fdc_ is irrelevant.
    FdcStreamObserver fdc_stream_observer_{*this};
    // DEC-0052 stream-light watchlog observer adapters (installed on ram_mapper_
    // / vdp_ only while armed). Only hold a reference to *this.
    MemWatchObserver mem_watch_observer_{*this};
    VdpRegWatchObserver vdp_reg_watch_observer_{*this};
};

}  // namespace sony_msx::machine
