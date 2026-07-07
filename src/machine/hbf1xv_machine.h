#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/bus.h"
#include "core/scheduler.h"
#include "devices/audio/psg_ym2149.h"
#include "devices/audio/ym2413_opll.h"
#include "devices/chipset/io_bus.h"
#include "devices/chipset/mapper_io.h"
#include "devices/chipset/mb670836_pause.h"
#include "devices/chipset/ppi_8255.h"
#include "devices/chipset/s1985_engine.h"
#include "devices/chipset/slot_bus.h"
#include "devices/chipset/switched_io.h"
#include "devices/chipset/system_bus.h"
#include "devices/chipset/system_control.h"
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
#include "machine/cpu_trace_sink.h"
#include "machine/debug_event_log.h"
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

    // Test/debug helper (M13-S4, discharges the M11 R-1/R-2 obligation): page the
    // 64 KB mapper RAM (slot 3-0) into all four CPU pages as a FLAT, linear 64 KB
    // view — the exact configuration the BIOS installs and that the M11 bring-up
    // default provided implicitly before the authentic #A8=0 reset flip. It sets
    // #A8 so every page selects primary slot 3, the slot-3 sub-slot register to 0
    // (sub-slot 0 = RAM mapper), and mapper segments {0,1,2,3} for pages {0,1,2,3}
    // (page p -> physical p*0x4000). CPU-over-RAM behaviour tests call this after
    // cold_boot to page RAM in explicitly, rather than relying on a hidden default.
    void map_flat_ram();

    void run_frame();
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
    // slot, for diagnostics/tests that need to know the TRUE current page-1
    // routing regardless of which primary slot currently occupies page 3 (the
    // 0xFFFF access itself is indirected through whatever primary answers page
    // 3 -- this bypasses that indirection). Non-perturbing.
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
    // call (or program start, cycle 0, if run_frame() has never been called --
    // an honest, tested boundary condition, R-M23-5; every M21/M22 system test
    // drives the CPU purely via step_cpu_instruction() and never calls
    // run_frame()). vdp_cycle_position() is a thin wrapper over
    // vdp_access_timing::vdp_cycle_within_line() for that same relative
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

    // Minimum INERT memory regions (M10-S2). These are pure storage byte
    // buffers, sized to the strict Target Machine Specification. They are
    // deterministically zero-initialized at cold_boot and expose read/write/
    // load/dump via their MemoryRegion accessors. They carry NO device
    // behavior (no slot/mapper decoding, no V9958 VDP semantics, no FM-PAC
    // battery/mapper behavior, no I/O bus) — those are separate milestones
    // (planner DP-1/DP-2/DP-3).
    //
    // - DRAM = 64 KB main RAM. This is the same store the CPU sees over the
    //   bus; load_memory/read_memory below are the CPU-visible aliases.
    // - SRAM = FM-PAC battery SRAM inert region.
    //   Assumption: the strict spec table lists no SRAM capacity; the standard
    //   Panasonic FM-PAC carries 8 KB of battery-backed SRAM, so kSramBytes is
    //   set to 8 KB. Verification action: confirm the FM-PAC SRAM capacity
    //   against the real device datasheet / an FM-PAC ROM+SRAM dump when the
    //   FM-PAC device milestone (planner DP-3) is implemented.
    //
    // The 128 KB VRAM MIGRATED to the V9958 VDP device (M14-S1). It is no longer
    // an inert machine MemoryRegion: the CPU reaches it ONLY through the VDP I/O
    // ports #98/#99 (+ the S1985 #9C/#9D mirror). Its store + authoritative size
    // now live in devices::video::VdpVram (VdpVram::kVramBytes); access it via
    // vdp().vram().
    static constexpr std::size_t kDramBytes = 64 * 1024;
    static constexpr std::size_t kSramBytes = 8 * 1024;

    [[nodiscard]] std::size_t dram_size() const;
    [[nodiscard]] std::size_t sram_size() const;

    [[nodiscard]] const MemoryRegion& dram() const;
    MemoryRegion& dram();
    [[nodiscard]] const MemoryRegion& sram() const;
    MemoryRegion& sram();

    // The V9958 VDP device (M14). Owns the 128 KB VRAM and answers ports
    // #98-#9B (+ the S1985 #9C-#9F mirror). Debug tooling reaches VRAM via
    // vdp().vram(); tests drive the register/status/interrupt contract here.
    [[nodiscard]] const devices::video::V9958Vdp& vdp() const;
    devices::video::V9958Vdp& vdp();

    // Deterministic, pull-model VDP pixel renderer accessor (M21, backlog
    // D1/D5/D6/D7-display-path). Additive only: the renderer is a pure,
    // on-demand consumer of vdp_'s stored state (no elapsed_cycles()
    // dependency, no new clock consumer), so no change to wire_bus() or
    // cold_boot() is needed. Mirrors the existing vdp() accessor pattern.
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
    // Determinism is guaranteed by construction: every serializer is hand-rolled
    // ASCII (fixed field order, fixed-width uppercase hex, '\n' endings), event
    // T-state stamps come from the deterministic scheduler clock, and NO
    // wall-clock or environment-dependent value is ever embedded. Two identical
    // runs produce byte-for-byte identical dump/log content. Dumping is
    // non-perturbing: it reads state only and never advances the CPU or clock.
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

private:
    static bool write_text_file(const std::filesystem::path& directory, const std::string& filename,
                                const std::string& text);

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
    // RtcClock). Returns scheduler total cycles READ-ONLY; all FDC Busy/DRQ/step/
    // index/motor timing advances off this, never the host wall clock or CPU
    // T-state accounting (protecting the M9/M12 oracles).
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

    core::Scheduler scheduler_;
    MemoryRegion dram_{kDramBytes};
    MemoryRegion sram_{kSramBytes};

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
    // No time-dependent state (§2.4): needs no elapsed_cycles() clock adapter.
    devices::audio::Ym2413Opll ym2413_;
    devices::chipset::SystemControlF5 system_control_;  // #F5 (RTC clock gate)
    RtcClock rtc_clock_{scheduler_};
    devices::rtc::Rp5c01 rtc_;  // #B4/#B5
    std::filesystem::path backup_ram_path_;
    std::filesystem::path halnote_sram_path_;  // M20, mirrors backup_ram_path_ exactly

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

    // M25 Sony MB670836 hardware PAUSE + Speed Controller (backlog C8). A
    // machine-level CPU-execution gate consulted at the very top of
    // step_cpu_instruction() (docs/m25-planner-package.md §2.3/§2.4) -- NOT
    // part of Z80aCpu (zero edit to src/devices/cpu/*). Its VBlank-synced
    // duty-cycle window is advanced by exactly ONE added line inside
    // run_frame(), alongside the existing vdp_.on_vsync() call.
    devices::chipset::Mb670836PauseController pause_controller_;
};

}  // namespace sony_msx::machine
