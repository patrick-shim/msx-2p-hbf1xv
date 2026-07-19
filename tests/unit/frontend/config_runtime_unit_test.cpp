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

#include <iostream>
#include <string>
#include <vector>

#include "frontend/config_runtime.h"
#include "frontend/sdl3_cli.h"
#include "machine/emulator_config.h"

// Suite: Frontend_ConfigRuntime_Unit (M50-S2, docs/m50-planner-package.md
// §4.1/§4.6, AC-S2-1/AC-S2-2).
//
// Pure, SDL-free tests for the CLI-default RESOLUTION seam:
//   * the determinism gate config_should_load() (§4.6);
//   * the precedence CLI > XML > built-in default in resolve_runtime_config()
//     (§4.1), proven per knob with CONCRETE asserted values (non-tautological:
//     each case asserts a specific effective value, never that the resolver
//     echoes its input).

namespace {

using sony_msx::frontend::config_should_load;
using sony_msx::frontend::parse_sdl3_cli;
using sony_msx::frontend::PhosphorMode;
using sony_msx::frontend::resolve_runtime_config;
using sony_msx::frontend::TextureFilter;
using sony_msx::machine::EmulatorConfig;

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// A config whose EVERY S2 knob differs from the built-in default, so a
// "config-only" resolution that echoes any built-in default would be caught.
EmulatorConfig non_default_config() {
    EmulatorConfig c;
    c.fast_disk = false;              // default true
    c.fmpac_autoload = false;         // default true
    c.fmpac_slot = 1;                 // default 2
    c.border_enabled = true;          // default false
    c.disk_writable = true;           // default false
    c.speed_level = 3;                // default 0
    c.video_scale = 5;                // default 3
    c.video_filter = "nearest";       // default "linear"
    c.persistence_percent = 40;       // default 0
    c.persistence_mode = "peak";      // default "avg"
    c.fullscreen = true;              // default false
    c.capture_enabled = true;         // default false
    c.ram_kb = 128;                   // default 512
    c.master_volume = 40;             // M52 (DEC-0079): default 100
    return c;
}

}  // namespace

int main() {
    // ================================================================
    // Part A -- the determinism gate config_should_load() (§4.6, AC-S2-2).
    // ================================================================
    {
        // Headless / SDL3 --hidden-window (interactive=false), no --config:
        // NEVER load -> the deterministic ctest path.
        expect(!config_should_load(/*interactive=*/false, /*explicit_config=*/false),
               "Gate_HeadlessOrHidden_NoConfig_DoesNotLoad");
        // A genuinely interactive SDL3 launch auto-loads.
        expect(config_should_load(/*interactive=*/true, /*explicit_config=*/false),
               "Gate_Interactive_AutoLoads");
        // --config forces a load even headless/hidden (the opt-in).
        expect(config_should_load(/*interactive=*/false, /*explicit_config=*/true),
               "Gate_HeadlessWithConfig_ForcesLoad");
        expect(config_should_load(/*interactive=*/true, /*explicit_config=*/true),
               "Gate_InteractiveWithConfig_Loads");
    }

    // ================================================================
    // Part B -- (a) NO config -> every knob is the BUILT-IN default.
    // The resolver receives all-built-in-defaults EmulatorConfig{} + no CLI
    // flags; the result must equal the concrete pre-M50 defaults (byte-identity
    // basis). Concrete values, not echoes.
    // ================================================================
    {
        const EmulatorConfig defaults;  // all built-in defaults
        const auto parsed = parse_sdl3_cli({});
        const auto r = resolve_runtime_config(defaults, parsed);
        expect(r.ram_bytes == 512u * 1024u, "NoConfig_Ram512KB");
        expect(r.fast_disk, "NoConfig_FastDiskOn");
        expect(r.fmpac_autoload, "NoConfig_FmpacAutoloadOn");
        expect(r.fmpac_autoload_slot == 2, "NoConfig_FmpacSlot2");
        expect(!r.is_stock, "NoConfig_NotStock");
        expect(!r.border_enabled, "NoConfig_BorderOff");
        // M52 (DEC-0079, §4.5 enumerated flip): the built-in EmulatorConfig
        // disk_writable default is now TRUE, so a no-config SDL3 resolution is ON
        // (headless never reaches this resolver with a loaded config -- it stays
        // OFF because it never auto-loads). This is the intended default flip, NOT
        // a regression.
        expect(r.disk_writable, "NoConfig_DiskWritableOn");
        // M52 (DEC-0079): master volume defaults to 100 (unity, byte-identical to
        // pre-M52) with no config + no CLI.
        expect(r.master_volume == 100, "NoConfig_MasterVolume100");
        expect(r.speed_level == 0, "NoConfig_Speed0");
        expect(r.scale == 3, "NoConfig_Scale3");
        expect(r.filter == TextureFilter::Linear, "NoConfig_FilterLinear");
        expect(r.persistence == 0, "NoConfig_Persistence0");
        expect(r.persistence_mode == PhosphorMode::Average, "NoConfig_PersistenceModeAverage");
        expect(!r.fullscreen, "NoConfig_FullscreenOff");
        expect(!r.capture_enabled, "NoConfig_CaptureOff");
    }

    // ================================================================
    // Part C -- (b) config-only (no CLI) -> every knob is the XML value.
    // Non-tautology: the config is non_default_config() so EVERY assertion is
    // the OPPOSITE of the built-in default -- an echo-of-default bug would fail.
    // ================================================================
    {
        const EmulatorConfig cfg = non_default_config();
        const auto parsed = parse_sdl3_cli({});  // no CLI flags
        const auto r = resolve_runtime_config(cfg, parsed);
        expect(r.ram_bytes == 128u * 1024u, "ConfigOnly_Ram128KB_XMLWins");
        expect(!r.fast_disk, "ConfigOnly_FastDiskOff_XMLWins");
        expect(!r.fmpac_autoload, "ConfigOnly_FmpacAutoloadOff_XMLWins");
        expect(r.fmpac_autoload_slot == 1, "ConfigOnly_FmpacSlot1_XMLWins");
        expect(r.border_enabled, "ConfigOnly_BorderOn_XMLWins");
        expect(r.disk_writable, "ConfigOnly_DiskWritableOn_XMLWins");
        expect(r.master_volume == 40, "ConfigOnly_MasterVolume40_XMLWins");  // M52 (DEC-0079)
        expect(r.speed_level == 3, "ConfigOnly_Speed3_XMLWins");
        expect(r.scale == 5, "ConfigOnly_Scale5_XMLWins");
        expect(r.filter == TextureFilter::Nearest, "ConfigOnly_FilterNearest_XMLWins");
        expect(r.persistence == 40, "ConfigOnly_Persistence40_XMLWins");
        expect(r.persistence_mode == PhosphorMode::Peak, "ConfigOnly_PersistenceModePeak_XMLWins");
        expect(r.fullscreen, "ConfigOnly_FullscreenOn_XMLWins");
        expect(r.capture_enabled, "ConfigOnly_CaptureOn_XMLWins");
    }

    // ================================================================
    // Part D -- (c) config + explicit CLI override -> CLI wins per knob.
    // The XML sets each knob one way; an explicit CLI flag sets the OPPOSITE, and
    // the resolved value must be the CLI value (never the XML). Values are chosen
    // so CLI != XML != built-in-default where possible, so neither a
    // config-echo nor a default-echo bug could pass.
    // ================================================================
    {
        const EmulatorConfig cfg = non_default_config();  // ram128, fast-disk off, border on, ...
        // CLI: flip each knob away from the XML value.
        const auto parsed = parse_sdl3_cli({
            "--ram", "256",                 // XML 128 -> CLI 256
            "--fast-disk",                  // XML off -> CLI on
            "--no-border",                  // XML on  -> CLI off
            "--filter", "linear",           // XML nearest -> CLI linear
            "--persistence", "10",          // XML 40 -> CLI 10
            "--persistence-mode", "avg",    // XML peak -> CLI avg
            "--scale", "2",                 // XML 5 -> CLI 2
            "--speed", "7",                 // XML 3 -> CLI 7
            "--capture", "off",             // XML on -> CLI off
        });
        expect(parsed.errors.empty(), "ConfigPlusCli_ParseNoErrors");
        const auto r = resolve_runtime_config(cfg, parsed);
        expect(r.ram_bytes == 256u * 1024u, "ConfigPlusCli_Ram256_CliWins");
        expect(r.fast_disk, "ConfigPlusCli_FastDiskOn_CliWins");
        expect(!r.border_enabled, "ConfigPlusCli_BorderOff_CliWins");
        expect(r.filter == TextureFilter::Linear, "ConfigPlusCli_FilterLinear_CliWins");
        expect(r.persistence == 10, "ConfigPlusCli_Persistence10_CliWins");
        expect(r.persistence_mode == PhosphorMode::Average, "ConfigPlusCli_ModeAvg_CliWins");
        expect(r.scale == 2, "ConfigPlusCli_Scale2_CliWins");
        expect(r.speed_level == 7, "ConfigPlusCli_Speed7_CliWins");
        expect(!r.capture_enabled, "ConfigPlusCli_CaptureOff_CliWins");
        // Knobs the CLI did NOT override still take the XML value (mixed session).
        expect(!r.fmpac_autoload, "ConfigPlusCli_UnoverriddenFmpac_StaysXML");
        expect(r.fmpac_autoload_slot == 1, "ConfigPlusCli_UnoverriddenSlot_StaysXML");
        expect(r.disk_writable, "ConfigPlusCli_UnoverriddenDiskWritable_StaysXML");
        expect(r.fullscreen, "ConfigPlusCli_UnoverriddenFullscreen_StaysXML");
    }

    // ================================================================
    // Part E -- AC-4 (RAM precedence) spelled out as the three-way ladder:
    // (a) no config, no CLI -> 512; (b) config 128, no CLI -> 128;
    // (c) config 128 + --ram 256 -> 256.
    // ================================================================
    {
        EmulatorConfig cfg128;
        cfg128.ram_kb = 128;
        expect(resolve_runtime_config(EmulatorConfig{}, parse_sdl3_cli({})).ram_bytes == 512u * 1024u,
               "RamLadder_NoConfigNoCli_512");
        expect(resolve_runtime_config(cfg128, parse_sdl3_cli({})).ram_bytes == 128u * 1024u,
               "RamLadder_Config128NoCli_128");
        expect(resolve_runtime_config(cfg128, parse_sdl3_cli({"--ram", "256"})).ram_bytes ==
                   256u * 1024u,
               "RamLadder_Config128Cli256_256");
    }

    // ================================================================
    // Part F -- --stock still forces the bare machine OVER an XML base (CLI
    // preset wins over XML): config wants fast-disk ON + FM-PAC ON + 512, but
    // --stock forces 64 KB / fast-disk OFF / no FM-PAC.
    // ================================================================
    {
        EmulatorConfig cfg;
        cfg.fast_disk = true;
        cfg.fmpac_autoload = true;
        cfg.ram_kb = 512;
        const auto r = resolve_runtime_config(cfg, parse_sdl3_cli({"--stock"}));
        expect(r.ram_bytes == 64u * 1024u, "Stock_OverXML_Ram64");
        expect(!r.fast_disk, "Stock_OverXML_FastDiskOff");
        expect(!r.fmpac_autoload, "Stock_OverXML_NoFmpac");
        expect(r.is_stock, "Stock_OverXML_IsStockLabel");
    }

    // ================================================================
    // Part G (M52, DEC-0079) -- master-volume precedence 5-case matrix
    // (CLI > XML > built-in default), mirroring the persistence template.
    // ================================================================
    {
        EmulatorConfig cfg30;
        cfg30.master_volume = 30;  // XML value
        // (1) no config, no CLI -> built-in default 100 (unity).
        expect(resolve_runtime_config(EmulatorConfig{}, parse_sdl3_cli({})).master_volume == 100,
               "VolLadder_NoConfigNoCli_100");
        // (2) XML-only (no CLI) -> the XML value.
        expect(resolve_runtime_config(cfg30, parse_sdl3_cli({})).master_volume == 30,
               "VolLadder_Config30NoCli_30");
        // (3) CLI overrides XML.
        expect(resolve_runtime_config(cfg30, parse_sdl3_cli({"--volume", "70"})).master_volume == 70,
               "VolLadder_Config30Cli70_70");
        // (4) CLI-only over the built-in default (no config loaded).
        expect(resolve_runtime_config(EmulatorConfig{}, parse_sdl3_cli({"--volume", "0"})).master_volume == 0,
               "VolLadder_NoConfigCli0_0");
        // (5) XML value survives when the CLI overrides a DIFFERENT knob (mixed).
        expect(resolve_runtime_config(cfg30, parse_sdl3_cli({"--scale", "2"})).master_volume == 30,
               "VolLadder_Config30CliOtherKnob_StaysXML30");
    }

    // ================================================================
    // Part H (M52, DEC-0079) -- disk-writable precedence with the flipped default:
    // no config resolves ON; --no-disk-writable forces OFF over the ON default;
    // --disk-writable forces ON; last-wins on the CLI scan.
    // ================================================================
    {
        // No config, no CLI -> ON (flipped built-in default).
        expect(resolve_runtime_config(EmulatorConfig{}, parse_sdl3_cli({})).disk_writable,
               "DiskWr_NoConfigNoCli_On");
        // --no-disk-writable forces OFF over the ON default.
        expect(!resolve_runtime_config(EmulatorConfig{}, parse_sdl3_cli({"--no-disk-writable"})).disk_writable,
               "DiskWr_NoDiskWritable_ForcesOff");
        // --disk-writable forces ON (redundant but explicit).
        expect(resolve_runtime_config(EmulatorConfig{}, parse_sdl3_cli({"--disk-writable"})).disk_writable,
               "DiskWr_DiskWritable_ForcesOn");
        // Last-wins: --disk-writable then --no-disk-writable -> OFF.
        expect(!resolve_runtime_config(EmulatorConfig{},
                                       parse_sdl3_cli({"--disk-writable", "--no-disk-writable"}))
                    .disk_writable,
               "DiskWr_WritableThenNo_LastWinsOff");
        // Last-wins: --no-disk-writable then --disk-writable -> ON.
        expect(resolve_runtime_config(EmulatorConfig{},
                                      parse_sdl3_cli({"--no-disk-writable", "--disk-writable"}))
                   .disk_writable,
               "DiskWr_NoThenWritable_LastWinsOn");
        // XML OFF is honored when no CLI flag is present (XML overrides the flipped default).
        EmulatorConfig cfg_off;
        cfg_off.disk_writable = false;
        expect(!resolve_runtime_config(cfg_off, parse_sdl3_cli({})).disk_writable,
               "DiskWr_XmlOff_NoCli_StaysOff");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_ConfigRuntime_Unit cases passed\n";
    return 0;
}
