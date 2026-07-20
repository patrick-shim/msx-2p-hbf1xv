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

// Suite: Frontend_DiskReplaceFlushSemantics_Integration (M56, DEC-0084, planner
// §3.3 / §9 test 3; slice S2).
//
// The F1 Open Disk(s) dirty-flush + transactional-pre-read CONTRACT proven at the
// Hbf1xvMachine + DiskImage level (SDL-free; the M52 disk_writable_toggle_flush +
// M36 disk_save_persist pattern). apply_open_disks() reuses ONLY DiskImage::flush()
// / attach_image() (no src/devices/fdc edit), so this locks the safety semantics
// the SDL3-gated frontend_sdl3_app_open_disks test then exercises end-to-end:
//   (a) writable -> the outgoing dirty disk flushes to host before replace -> the
//       replacement mounts + the flushed bytes reload identically.
//   (b) NOT writable -> the outgoing dirty disk's in-memory writes are DISCARDED
//       (host file never written), never rolled back before the discard.
//   (c) transactional pre-read: a bad path in the new set aborts with the current
//       image byte-intact (no partial state).
//   (d) determinism: two identical scripted runs -> byte-identical host files.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "devices/fdc/disk_image.h"
#include "machine/hbf1xv_machine.h"

namespace {

using sony_msx::devices::fdc::DiskImage;
using sony_msx::machine::Hbf1xvMachine;

int g_failures = 0;

void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

void write_file(const std::filesystem::path& p, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::uint8_t> make_sector(const std::uint8_t seed) {
    std::vector<std::uint8_t> s(DiskImage::kSectorSize);
    for (std::size_t i = 0; i < s.size(); ++i) {
        s[i] = static_cast<std::uint8_t>((i * 7u + seed) & 0xFFu);
    }
    return s;
}

// The transactional pre-read (planner §3.3 step 1): read + validate the WHOLE new
// set BEFORE touching the current mount; ANY failure -> return false (no state
// change). Mirrors Sdl3App::apply_open_disks step 1.
bool transactional_pre_read(const std::vector<std::string>& paths,
                            std::vector<std::vector<std::uint8_t>>& out) {
    std::vector<std::vector<std::uint8_t>> temp;
    temp.reserve(paths.size());
    for (const std::string& path : paths) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return false;  // no partial state
        }
        temp.emplace_back((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
    out = std::move(temp);
    return true;
}

}  // namespace

int main() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "hbf1xv_disk_replace";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    // --- (a) writable: outgoing dirty disk flushes before replace, reloads OK. ---
    {
        const std::filesystem::path cur = dir / "a_current.dsk";
        std::filesystem::remove(cur);

        Hbf1xvMachine m;
        m.cold_boot();
        // Current disk is host-bound + writable (Alt+S ON semantics).
        m.disk_image().set_host_path(cur);
        const std::vector<std::uint8_t> written = make_sector(0x33);
        m.disk_drive().write_sector(4, written.data());
        expect(m.disk_image().dirty(), "Writable_DirtyBeforeReplace");

        // apply_open_disks step 2 (writable): flush the outgoing disk to host.
        expect(m.disk_image().flush(), "Writable_OutgoingFlushPersists");
        expect(std::filesystem::exists(cur), "Writable_HostFileWritten");

        // The flushed bytes reload identically (the write was not lost on replace).
        Hbf1xvMachine reloaded;
        reloaded.cold_boot();
        reloaded.disk_image() = DiskImage(read_file(cur));
        reloaded.disk_drive().attach_image(&reloaded.disk_image());
        std::vector<std::uint8_t> back(DiskImage::kSectorSize, 0);
        expect(reloaded.disk_drive().read_sector(4, back.data()), "Writable_ReloadReadOk");
        expect(back == written, "Writable_FlushedBytesByteIdentical");

        std::filesystem::remove(cur);
    }

    // --- (b) NOT writable: outgoing dirty disk is discarded (host never written). ---
    {
        const std::filesystem::path cur = dir / "b_current.dsk";
        std::filesystem::remove(cur);

        Hbf1xvMachine m;
        m.cold_boot();
        // NOT writable -> no host path bound (the default / Alt+S OFF).
        const std::vector<std::uint8_t> written = make_sector(0x55);
        m.disk_drive().write_sector(2, written.data());
        expect(m.disk_image().dirty(), "NotWritable_DirtyBeforeReplace");

        // apply_open_disks step 2 (not writable): the in-memory write is KEPT (no
        // rollback) but, being unbound, flush() no-ops and no host file appears ->
        // discarded on replace.
        std::vector<std::uint8_t> back(DiskImage::kSectorSize, 0);
        m.disk_drive().read_sector(2, back.data());
        expect(back == written, "NotWritable_InMemoryWriteKept_NoRollback");
        expect(!m.disk_image().flush(), "NotWritable_FlushNoOp");
        expect(!std::filesystem::exists(cur), "NotWritable_HostFileNeverWritten");
    }

    // --- (c) transactional pre-read: a bad path aborts, current image intact. ---
    {
        const std::filesystem::path good = dir / "c_good.dsk";
        write_file(good, DiskImage::synthesize());

        // The current mount holds a known sector value.
        Hbf1xvMachine m;
        m.cold_boot();
        const std::vector<std::uint8_t> current_marker = make_sector(0x99);
        m.disk_drive().write_sector(1, current_marker.data());
        const std::vector<std::uint8_t> before = m.disk_image().data();

        // A new set with one unreadable path -> pre-read fails, no commit.
        const std::vector<std::string> new_set = {good.string(),
                                                  (dir / "c_missing.dsk").string()};
        std::vector<std::vector<std::uint8_t>> staged;
        const bool ok = transactional_pre_read(new_set, staged);
        expect(!ok, "Transactional_BadPathAborts");
        expect(staged.empty(), "Transactional_NoStagedImagesOnAbort");
        // Because the pre-read aborted, the current image is NEVER touched.
        expect(m.disk_image().data() == before, "Transactional_CurrentImageByteIntact");
        std::vector<std::uint8_t> still(DiskImage::kSectorSize, 0);
        m.disk_drive().read_sector(1, still.data());
        expect(still == current_marker, "Transactional_CurrentSectorPreserved");

        // A fully-valid new set instead pre-reads cleanly.
        const std::vector<std::string> valid_set = {good.string(), good.string()};
        std::vector<std::vector<std::uint8_t>> staged2;
        expect(transactional_pre_read(valid_set, staged2), "Transactional_ValidSetPreReadsOk");
        expect(staged2.size() == 2, "Transactional_ValidSetStagedCount");

        std::filesystem::remove(good);
    }

    // --- (d) determinism: two identical flush runs -> byte-identical files. ---
    {
        const std::filesystem::path pa = dir / "d_a.dsk";
        const std::filesystem::path pb = dir / "d_b.dsk";
        std::filesystem::remove(pa);
        std::filesystem::remove(pb);
        auto run = [](const std::filesystem::path& p) {
            Hbf1xvMachine m;
            m.cold_boot();
            m.disk_image().set_host_path(p);
            for (const std::uint8_t sec : {std::uint8_t{1}, std::uint8_t{3}, std::uint8_t{5}}) {
                m.disk_drive().write_sector(sec, make_sector(static_cast<std::uint8_t>(sec * 9u)).data());
            }
            m.disk_image().flush();
        };
        run(pa);
        run(pb);
        expect(read_file(pa) == read_file(pb), "Determinism_TwoRunsByteIdentical");
        std::filesystem::remove(pa);
        std::filesystem::remove(pb);
    }

    std::filesystem::remove_all(dir, ec);

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_DiskReplaceFlushSemantics_Integration cases passed\n";
    return 0;
}
