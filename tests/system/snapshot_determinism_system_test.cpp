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

#include <cstdint>
#include <iostream>
#include <string>

#include "machine/debug_format.h"
#include "machine/debug_snapshot.h"
#include "machine/hbf1xv_machine.h"

// Suite: Hbf1xvSnapshotDeterminism_System -- the headline snapshot
// determinism gate (DEC-0051).
//
// Two independent, IDENTICAL scripted headless runs to the SAME frame capture a
// snapshot each; the two bundles are asserted BYTE-IDENTICAL file-for-file. A
// THIRD, deliberately-diverged run (one extra CPU step past the same frame
// boundary -- an authentic "the run went further" perturbation, a deterministic
// proxy for one extra scripted keypress) MUST produce a DIFFERENT
// bundle -- proving the byte-compare is non-vacuous.

namespace {

int g_failures = 0;
void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

constexpr int kFrames = 90;

// Drive the machine to a fixed frame boundary via the real production loop shape
// (step_cpu_instruction() to each frame boundary, then on_vsync_boundary()).
void run_to_frame(sony_msx::machine::Hbf1xvMachine& m, int frames) {
#ifdef SONY_MSX_BIOS_DIR
    m.set_asset_root(SONY_MSX_BIOS_DIR);
#endif
    m.cold_boot();
    const std::uint64_t target = m.frame_cycles_per_frame();
    for (int f = 0; f < frames; ++f) {
        const std::uint64_t start = m.elapsed_cycles();
        while (m.elapsed_cycles() - start < target) {
            m.step_cpu_instruction();
        }
        m.on_vsync_boundary();
    }
}

// Flatten a snapshot bundle into one comparable string (name + content per file).
std::string flatten(const sony_msx::machine::debug_snapshot::Snapshot& snap) {
    std::string out = "ID=" + snap.id + "\n";
    for (const auto& f : snap.files) {
        out += "<<" + f.name + ">>\n" + f.content;
    }
    return out;
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;
    namespace ds = sony_msx::machine::debug_snapshot;

    // --- Runs A and B: identical to the same frame. ---
    Hbf1xvMachine a;
    run_to_frame(a, kFrames);
    const ds::Snapshot snap_a = a.serialize_snapshot(a.snapshot_id());
    const std::string flat_a = flatten(snap_a);

    Hbf1xvMachine b;
    run_to_frame(b, kFrames);
    const ds::Snapshot snap_b = b.serialize_snapshot(b.snapshot_id());
    const std::string flat_b = flatten(snap_b);

    // The deterministic id is identical.
    expect(a.snapshot_id() == b.snapshot_id(), "Determinism_SameId");

    // Byte-identical file-for-file.
    bool same_shape = snap_a.files.size() == snap_b.files.size();
    expect(same_shape, "Determinism_SameFileCount");
    bool all_identical = same_shape;
    for (std::size_t i = 0; same_shape && i < snap_a.files.size(); ++i) {
        if (snap_a.files[i].name != snap_b.files[i].name ||
            snap_a.files[i].content != snap_b.files[i].content) {
            all_identical = false;
            std::cerr << "  divergent file: " << snap_a.files[i].name << "\n";
        }
    }
    expect(all_identical && flat_a == flat_b, "Determinism_ByteIdenticalBundle");

    // A deterministic digest of the whole bundle (evidence anchor).
    const std::uint32_t digest_a = ds::checksum(flat_a);
    const std::uint32_t digest_b = ds::checksum(flat_b);
    expect(digest_a == digest_b, "Determinism_SameDigest");
    std::cout << "M36 determinism digest (FNV-1a32) = "
              << sony_msx::machine::debug_format::to_hex(digest_a, 8) << " id=" << snap_a.id << "\n";

    // --- Run C: deliberately diverged (one extra CPU step past the same frame
    //     boundary). The bundle MUST differ -- the non-vacuity proof. ---
    Hbf1xvMachine c;
    run_to_frame(c, kFrames);
    c.step_cpu_instruction();  // the divergence
    const ds::Snapshot snap_c = c.serialize_snapshot(c.snapshot_id());
    const std::string flat_c = flatten(snap_c);
    expect(flat_c != flat_a, "Adversarial_DivergedRun_DiffersFromA");
    expect(ds::checksum(flat_c) != digest_a, "Adversarial_DivergedRun_DifferentDigest");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Hbf1xvSnapshotDeterminism_System cases passed\n";
    return 0;
}
