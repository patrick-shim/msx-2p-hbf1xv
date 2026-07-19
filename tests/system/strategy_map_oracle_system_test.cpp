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

// Suite: System_Hbf1xvStrategyMapOracle (M58 S8-FIX, DEF-M58-DSKCHG)
//
// Permanent regression oracle for the M58 multi-disk strategy title's
// whole-screen map corruption (DEC-0086): the game's post-swap GRAPHIC6
// campaign-map screen must render as a COHERENT map, not colored noise.
//
// ROOT CAUSE GUARDED (DEF-M58-DSKCHG, src/devices/fdc/sony_fdc.cpp): the
// /DSKCHG sense at 0x7FFD bit2 is a per-drive Shugart-bus output gated by
// Drive Select (fact-sheet "FDC for Sony HB-F1XV.md" §1/§7/§8). The Sony disk
// ROM's DSKCHG probe reads 0x7FFD once BEFORE selecting drive A and again
// after; without the drive-select gate, the unselected first read consumed
// the disk-changed one-shot, the post-select read reported "unchanged", DOS
// kept the previous disk's FAT/DPB, and every post-swap file load resolved
// through a stale FAT -> the scrambled map (right bytes, wrong places).
//
// REPRO (the pinned M58 recipe, debug/m58/repro/newgame_pin.txt +
// docs/m58-investigation-report.md §S1, headless equivalent of
//   --debug-session --frames 14000 --stock --swap-disk-frame 11200):
// stock machine (64 KB, no FM-PAC, accurate disk timing), CTRL-held boot of
// the strategy title's Disk 1, scripted menu navigation to "new game" /
// scenario 1, hot-swap to Disk 2 at frame 11200, run to frame 14000, render
// the frame.
//
// ORACLE (structural, measured on the real corrupt-vs-clean captures,
// debug/m58/s8/): all three must hold at frame 14000 --
//   (1) right-panel (x in [272,511]) horizontal neighbor-equality >= 0.75
//       (clean map = 0.906: the message panel is large flat regions;
//        corrupt noise = 0.587);
//   (2) whole-frame vertical neighbor-equality >= 0.60
//       (clean = 0.687; corrupt = 0.504);
//   (3) non-triviality guard so a blank/monochrome frame cannot pass the
//       coherence metrics trivially: >= 5 distinct colors AND the most
//       common color covers <= 0.80 of the frame (clean = 8 colors, top
//       color 0.611; an undrawn frame is ~1 color at ~1.0).
// Adversarial-revert proof (recorded in the M58 S8 report): reverting the
// DEF-M58-DSKCHG gate makes (1) and (2) FAIL on this exact test.
//
// Game disks are owner-provisioned, untracked assets under games/disks/, so
// this test SKIPS (returns 0) when they are absent -- the same
// skip-when-absent discipline as the m19/m28 asset-gated tests.

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "devices/fdc/disk_image.h"
#include "devices/video/frame_buffer.h"
#include "machine/hbf1xv_machine.h"
#include "machine/input_script.h"

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build"
#endif
#ifndef SONY_MSX_GAMES_DIR
#error "SONY_MSX_GAMES_DIR must be defined by the build"
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
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

// The pinned M58 repro input script, embedded VERBATIM from
// debug/m58/repro/newgame_pin.txt (parsed by the production parser, exactly
// as the headless --input-script path does): LCTRL held from T=0 (the
// single-drive CTRL boot), SPACE/RETURN taps through the publisher intro, "1" =
// new game, "1" = scenario 1, RETURN confirmations across the disk-2 prompt.
constexpr const char* kNewGamePinScript = R"(HBF1XV-INPUT-SCRIPT v1
T=0 KEY=LCTRL DOWN
T=59736000 KEY=LCTRL UP
T=89604000 KEY=SPACE DOWN
T=89962416 KEY=SPACE UP
T=98564400 KEY=RETURN DOWN
T=98922816 KEY=RETURN UP
T=107524800 KEY=SPACE DOWN
T=107883216 KEY=SPACE UP
T=116485200 KEY=RETURN DOWN
T=116843616 KEY=RETURN UP
T=125445600 KEY=SPACE DOWN
T=125804016 KEY=SPACE UP
T=134406000 KEY=RETURN DOWN
T=134764416 KEY=RETURN UP
T=143366400 KEY=SPACE DOWN
T=143724816 KEY=SPACE UP
T=152326800 KEY=RETURN DOWN
T=152685216 KEY=RETURN UP
T=161287200 KEY=SPACE DOWN
T=161645616 KEY=SPACE UP
T=170247600 KEY=RETURN DOWN
T=170606016 KEY=RETURN UP
T=179208000 KEY=SPACE DOWN
T=179566416 KEY=SPACE UP
T=188168400 KEY=RETURN DOWN
T=188526816 KEY=RETURN UP
T=197128800 KEY=SPACE DOWN
T=197487216 KEY=SPACE UP
T=206089200 KEY=RETURN DOWN
T=206447616 KEY=RETURN UP
T=215049600 KEY=SPACE DOWN
T=215408016 KEY=SPACE UP
T=224010000 KEY=RETURN DOWN
T=224368416 KEY=RETURN UP
T=232970400 KEY=SPACE DOWN
T=233328816 KEY=SPACE UP
T=241930800 KEY=RETURN DOWN
T=242289216 KEY=RETURN UP
T=250891200 KEY=SPACE DOWN
T=251249616 KEY=SPACE UP
T=259851600 KEY=RETURN DOWN
T=260210016 KEY=RETURN UP
T=268812000 KEY=SPACE DOWN
T=269170416 KEY=SPACE UP
T=277772400 KEY=RETURN DOWN
T=278130816 KEY=RETURN UP
T=286732800 KEY=SPACE DOWN
T=287091216 KEY=SPACE UP
T=295693200 KEY=RETURN DOWN
T=296051616 KEY=RETURN UP
T=304653600 KEY=SPACE DOWN
T=305012016 KEY=SPACE UP
T=313614000 KEY=RETURN DOWN
T=313972416 KEY=RETURN UP
T=322574400 KEY=SPACE DOWN
T=322932816 KEY=SPACE UP
T=331534800 KEY=RETURN DOWN
T=331893216 KEY=RETURN UP
T=340495200 KEY=SPACE DOWN
T=340853616 KEY=SPACE UP
T=349455600 KEY=RETURN DOWN
T=349814016 KEY=RETURN UP
T=358416000 KEY=SPACE DOWN
T=358774416 KEY=SPACE UP
T=367376400 KEY=RETURN DOWN
T=367734816 KEY=RETURN UP
T=376336800 KEY=SPACE DOWN
T=376695216 KEY=SPACE UP
T=385297200 KEY=RETURN DOWN
T=385655616 KEY=RETURN UP
T=394257600 KEY=SPACE DOWN
T=394616016 KEY=SPACE UP
T=403218000 KEY=RETURN DOWN
T=403576416 KEY=RETURN UP
T=412178400 KEY=SPACE DOWN
T=412536816 KEY=SPACE UP
T=421138800 KEY=RETURN DOWN
T=421497216 KEY=RETURN UP
T=430099200 KEY=SPACE DOWN
T=430457616 KEY=SPACE UP
T=439059600 KEY=RETURN DOWN
T=439418016 KEY=RETURN UP
T=448020000 KEY=SPACE DOWN
T=448378416 KEY=SPACE UP
T=456980400 KEY=RETURN DOWN
T=457338816 KEY=RETURN UP
T=465940800 KEY=SPACE DOWN
T=466299216 KEY=SPACE UP
T=474901200 KEY=RETURN DOWN
T=475259616 KEY=RETURN UP
T=537624000 KEY=1 DOWN
T=538101888 KEY=1 UP
T=555544800 KEY=RETURN DOWN
T=556022688 KEY=RETURN UP
T=597360000 KEY=1 DOWN
T=597837888 KEY=1 UP
T=615280800 KEY=RETURN DOWN
T=615758688 KEY=RETURN UP
T=686964000 KEY=RETURN DOWN
T=687441888 KEY=RETURN UP
T=728779200 KEY=RETURN DOWN
T=729257088 KEY=RETURN UP
T=770594400 KEY=RETURN DOWN
T=771072288 KEY=RETURN UP
T=812409600 KEY=RETURN DOWN
T=812887488 KEY=RETURN UP
T=854224800 KEY=RETURN DOWN
T=854702688 KEY=RETURN UP
[END]
)";

constexpr std::uint32_t kTotalFrames = 14000;
constexpr std::uint32_t kSwapDiskFrame = 11200;

}  // namespace

int main() {
    const std::string disk_dir = std::string(SONY_MSX_GAMES_DIR) + "/disks/strategy";
    const std::string disk1_path = disk_dir + "/strategy-d1.dsk";
    const std::string disk2_path = disk_dir + "/strategy-d2.dsk";
    const std::vector<std::uint8_t> disk1 = read_file(disk1_path);
    const std::vector<std::uint8_t> disk2 = read_file(disk2_path);
    if (disk1.size() != 737280 || disk2.size() != 737280) {
        std::cout << "SKIP: strategy-title disks not present under " << disk_dir
                  << " -- skip-when-absent asset guard (games/ is owner-provisioned, untracked)\n";
        return 0;
    }

    using sony_msx::machine::Hbf1xvMachine;
    using sony_msx::machine::InputScriptPlayer;

    // Stock machine (the pinned repro config): the default Hbf1xvMachine is
    // the bare 64 KB / no-cartridge / accurate-disk-timing machine.
    Hbf1xvMachine machine;
    machine.set_asset_root(SONY_MSX_BIOS_DIR);
    machine.cold_boot();

    machine.disk_image() = sony_msx::devices::fdc::DiskImage(disk1);
    machine.disk_drive().attach_image(&machine.disk_image());

    InputScriptPlayer script_player(
        sony_msx::machine::parse_input_script(kNewGamePinScript));

    // The --debug-session --frames loop shape (src/main.cpp frame-loop mode):
    // step to each frame boundary applying due input edges after each
    // instruction, deliver VBlank at every boundary, hot-swap the medium at
    // the scripted frame (attach + DSKCHG latch, the F11/--swap-disk-frame
    // semantics).
    const std::uint64_t target = machine.frame_cycles_per_frame();
    for (std::uint32_t frame = 0; frame < kTotalFrames; ++frame) {
        if (frame == kSwapDiskFrame) {
            machine.disk_image() = sony_msx::devices::fdc::DiskImage(disk2);
            machine.disk_drive().attach_image(&machine.disk_image());
            machine.disk_drive().set_disk_changed(true);
        }
        const std::uint64_t start = machine.elapsed_cycles();
        while (machine.elapsed_cycles() - start < target) {
            machine.step_cpu_instruction();
            script_player.apply_due(machine.elapsed_cycles(), machine.keyboard());
        }
        machine.on_vsync_boundary();
    }

    const sony_msx::devices::video::FrameBuffer frame = machine.render_frame();
    expect(frame.width == 512 && frame.height == 212,
           "MapFrame_Graphic6Dimensions_512x212");
    if (frame.width != 512 || frame.height != 212) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }

    // --- Oracle metric (1): right-panel horizontal neighbor-equality. ---
    std::uint64_t r_eq = 0;
    std::uint64_t r_tot = 0;
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 272; x < frame.width - 1; ++x) {
            r_eq += (frame.at(x, y) == frame.at(x + 1, y)) ? 1u : 0u;
            ++r_tot;
        }
    }
    const double right_eq = static_cast<double>(r_eq) / static_cast<double>(r_tot);

    // --- Oracle metric (2): whole-frame vertical neighbor-equality. ---
    std::uint64_t v_eq = 0;
    std::uint64_t v_tot = 0;
    for (int y = 0; y < frame.height - 1; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            v_eq += (frame.at(x, y) == frame.at(x, y + 1)) ? 1u : 0u;
            ++v_tot;
        }
    }
    const double vert_eq = static_cast<double>(v_eq) / static_cast<double>(v_tot);

    // --- Oracle metric (3): non-triviality guard. ---
    std::set<std::uint16_t> distinct;
    std::uint64_t top_count = 0;
    {
        // 32768 possible RGB555 values; count via a flat histogram.
        std::vector<std::uint32_t> hist(1u << 15, 0);
        for (const std::uint16_t p : frame.pixels) {
            ++hist[p & 0x7FFF];
        }
        for (std::uint32_t i = 0; i < hist.size(); ++i) {
            if (hist[i] != 0) {
                distinct.insert(static_cast<std::uint16_t>(i));
                if (hist[i] > top_count) {
                    top_count = hist[i];
                }
            }
        }
    }
    const double top_frac =
        static_cast<double>(top_count) / static_cast<double>(frame.pixels.size());

    std::cerr << "[M58 oracle] right_eq=" << right_eq << " (>=0.75)"
              << " vert_eq=" << vert_eq << " (>=0.60)"
              << " distinct_colors=" << distinct.size() << " (>=5)"
              << " top_color_frac=" << top_frac << " (<=0.80)\n";

    expect(right_eq >= 0.75, "MapFrame_RightPanelHorizontalCoherence_CleanNotNoise");
    expect(vert_eq >= 0.60, "MapFrame_VerticalCoherence_CleanNotNoise");
    expect(distinct.size() >= 5, "MapFrame_NonTrivial_AtLeastFiveColors");
    expect(top_frac <= 0.80, "MapFrame_NonTrivial_NotMonochrome");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
