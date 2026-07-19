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

// Suite: Frontend_DiskWritableToggleFlush_Integration (M52, DEC-0079,
// docs/m52-planner-package.md §2.4/§5 item 6, S3-3/S3-4/S3-5).
//
// Reproduces the Alt+S disk-writable toggle CONTRACT at the Hbf1xvMachine +
// DiskImage + temp-.dsk level (the hbf1xv_m36_disk_save_persist pattern). The
// toggle is modeled by DiskImage::set_host_path bind/unbind -- the SAME primitive
// the Sdl3App::on_disk_writable_toggle_hotkey() handler uses (ZERO src/devices/fdc
// edits). Three cases:
//   (a) ON (bind host path) -> write sector -> flush() true -> reload byte-identical.
//   (b) bind -> write -> OFF (set_host_path({})) -> flush()==false, file UNWRITTEN,
//       but data_ STILL holds the write (no rollback) -> discarded at exit.
//   (c) write-while-OFF (unbound) -> late-ON (bind) -> flush() persists the WHOLE
//       in-memory image, including the earlier write.
//
// Deterministic oracle: fixed writes -> fixed file bytes, every run.

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

std::vector<std::uint8_t> make_sector(const std::uint8_t seed) {
    std::vector<std::uint8_t> s(DiskImage::kSectorSize);
    for (std::size_t i = 0; i < s.size(); ++i) {
        s[i] = static_cast<std::uint8_t>((i * 3u + seed) & 0xFFu);
    }
    return s;
}

}  // namespace

int main() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "hbf1xv_m52_disk_writable_toggle";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    // --- (a) Alt+S ON: bind -> write -> flush persists -> reload byte-identical. ---
    {
        const std::filesystem::path path = dir / "on_persist.dsk";
        std::filesystem::remove(path);

        Hbf1xvMachine m;
        m.cold_boot();
        // Alt+S -> ON binds the current image's host path (the handler's ON branch).
        m.disk_image().set_host_path(path);
        const std::vector<std::uint8_t> written = make_sector(0x2A);
        m.disk_drive().write_sector(5, written.data());
        expect(m.disk_image().flush(), "ON_FlushOk");
        expect(std::filesystem::exists(path), "ON_HostFileCreated");

        Hbf1xvMachine reloaded;
        reloaded.cold_boot();
        reloaded.disk_image() = DiskImage(read_file(path));
        reloaded.disk_drive().attach_image(&reloaded.disk_image());
        std::vector<std::uint8_t> back(DiskImage::kSectorSize, 0);
        expect(reloaded.disk_drive().read_sector(5, back.data()), "ON_ReloadReadOk");
        expect(back == written, "ON_ReloadByteIdentical");

        std::filesystem::remove(path);
    }

    // --- (b) bind -> write -> Alt+S OFF (unbind) -> flush no-op, file unwritten,
    //     in-memory write KEPT (no rollback), discarded at exit. ---
    {
        const std::filesystem::path path = dir / "off_discard.dsk";
        std::filesystem::remove(path);

        Hbf1xvMachine m;
        m.cold_boot();
        m.disk_image().set_host_path(path);  // ON
        const std::vector<std::uint8_t> written = make_sector(0x77);
        m.disk_drive().write_sector(3, written.data());
        expect(m.disk_image().dirty(), "OFF_DirtyAfterWrite");

        // Alt+S -> OFF unbinds (the handler's OFF branch): set_host_path({}).
        m.disk_image().set_host_path(std::filesystem::path{});
        // The flush is now a no-op and no host file is created.
        expect(!m.disk_image().flush(), "OFF_FlushNoOpAfterUnbind");
        expect(!std::filesystem::exists(path), "OFF_HostFileNeverWritten");
        // The in-memory image STILL holds the write (never rolled back).
        std::vector<std::uint8_t> back(DiskImage::kSectorSize, 0);
        m.disk_drive().read_sector(3, back.data());
        expect(back == written, "OFF_InMemoryWriteKept_NoRollback");

        // Simulate exit: the shutdown flush (unbound) is a no-op -> DISCARDED.
        expect(!m.disk_image().flush(), "OFF_ExitFlushNoOp_Discarded");
        expect(!std::filesystem::exists(path), "OFF_DirtyImageDiscardedAtExit");
    }

    // --- (c) write-while-OFF then late-ON: the whole in-memory image (incl. the
    //     earlier write) is flushed on the late bind. ---
    {
        const std::filesystem::path path = dir / "late_on.dsk";
        std::filesystem::remove(path);

        Hbf1xvMachine m;
        m.cold_boot();
        // OFF (unbound): write accumulates in memory only.
        const std::vector<std::uint8_t> early = make_sector(0x11);
        m.disk_drive().write_sector(2, early.data());
        expect(!m.disk_image().flush(), "LateOn_FlushNoOpWhileUnbound");
        expect(!std::filesystem::exists(path), "LateOn_NoFileWhileOff");

        // A further write still OFF, then Alt+S -> late-ON binds + a flush at exit
        // persists the WHOLE image including the earlier write.
        const std::vector<std::uint8_t> more = make_sector(0x22);
        m.disk_drive().write_sector(6, more.data());
        m.disk_image().set_host_path(path);  // late-ON bind
        expect(m.disk_image().flush(), "LateOn_FlushPersistsAfterBind");

        Hbf1xvMachine reloaded;
        reloaded.cold_boot();
        reloaded.disk_image() = DiskImage(read_file(path));
        reloaded.disk_drive().attach_image(&reloaded.disk_image());
        std::vector<std::uint8_t> b2(DiskImage::kSectorSize, 0);
        std::vector<std::uint8_t> b6(DiskImage::kSectorSize, 0);
        reloaded.disk_drive().read_sector(2, b2.data());
        reloaded.disk_drive().read_sector(6, b6.data());
        expect(b2 == early, "LateOn_EarlyWhileOffWrite_Persisted");
        expect(b6 == more, "LateOn_LaterWrite_Persisted");

        std::filesystem::remove(path);
    }

    // --- Determinism: two identical toggle+write runs -> byte-identical files. ---
    {
        const std::filesystem::path pa = dir / "det_a.dsk";
        const std::filesystem::path pb = dir / "det_b.dsk";
        std::filesystem::remove(pa);
        std::filesystem::remove(pb);
        auto run = [](const std::filesystem::path& p) {
            Hbf1xvMachine m;
            m.cold_boot();
            // write OFF, then late-ON bind + flush (the (c) path).
            for (std::uint8_t sec : {2u, 4u, 7u}) {
                m.disk_drive().write_sector(sec, make_sector(static_cast<std::uint8_t>(sec * 5u)).data());
            }
            m.disk_image().set_host_path(p);
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
    std::cout << "All Frontend_DiskWritableToggleFlush_Integration cases passed\n";
    return 0;
}
