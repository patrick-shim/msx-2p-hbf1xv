#include "devices/cartridge/cartridge_konami_scc_rom.h"

namespace sony_msx::devices::cartridge {

bool CartridgeKonamiScc::is_valid_image_size(const std::size_t size) {
    // Any positive whole number of 8 kB banks. Images beyond the real
    // mapper chips' 512 kB/64-bank ceiling are ACCEPTED (openMSX's own
    // non-fatal warning, RomKonamiSCC.cc:28-34; SCC fact-sheet §2).
    return size > 0 && (size % CartridgeRomWindow::kBankSize) == 0;
}

CartridgeKonamiScc::CartridgeKonamiScc(std::vector<std::uint8_t> image) : window_(std::move(image)) {
    // Deliberately NO set_block_mask override (plain-Konami's 256 kB rule
    // does NOT apply here -- SCC fact-sheet §10.4): the image-derived
    // default fallback mask stands (A-M19-6).
    // Deliberately NO reset() call here (M19 convention, class doc).
}

void CartridgeKonamiScc::bank_switch(const int page, const unsigned block) {
    window_.set_bank(page, block);
    // RomKonamiSCC.cc:44-58 -- "the mirror behavior is different from
    // RomKonami!" (R-M29-1): the OPPOSITE directions of the M19 plain
    // Konami mapper.
    if (page == 2 || page == 3) {
        // [0x4000-0x8000) mirrored into [0xC000-0x10000).
        window_.set_bank(page + 4, block);
    } else if (page == 4 || page == 5) {
        // [0x8000-0xC000) mirrored into [0x0000-0x4000).
        window_.set_bank(page - 4, block);
    }
}

void CartridgeKonamiScc::reset() {
    // RomKonamiSCC.cc:60-68: banks 0,1,2,3; SCC disabled; chip reset.
    bank_switch(2, 0);
    bank_switch(3, 1);
    bank_switch(4, 2);
    bank_switch(5, 3);
    scc_enabled_ = false;
    scc_.reset();
}

core::BusData CartridgeKonamiScc::mem_read(const core::BusAddress address) {
    if (scc_enabled_ && address >= 0x9800 && address < 0xA000) {
        // The 256-byte map mirrored 8x across the window (A-M29-1). read(),
        // not peek(): the deform-range read side effect must fire
        // (SCC fact-sheet §3).
        return scc_.read(static_cast<std::uint8_t>(address & 0xFF));
    }
    // ROM (including the KonamiSCC-specific mirror pages -- which never
    // expose the SCC window; the override above is keyed on the raw CPU
    // address range only).
    return window_.read(address);
}

void CartridgeKonamiScc::mem_write(const core::BusAddress address, const core::BusData value) {
    // RomKonamiSCC.cc:98-123 decode order (SCC fact-sheet §2):
    // 1. Writes outside [0x5000, 0xC000) are ignored entirely.
    if (address < 0x5000 || address >= 0xC000) {
        return;
    }
    // 2. SCC register window (only when enabled).
    if (scc_enabled_ && address >= 0x9800 && address < 0xA000) {
        scc_.write(static_cast<std::uint8_t>(address & 0xFF), value);
        return;
    }
    // 3. SCC enable/disable latch: anywhere in 0x9000-0x97FF; the masked
    //    6-bit compare (0xBF also enables -- A-M29-2, fact-sheet §9.6
    //    arbitration). NO return: the same write also bank-switches
    //    (both-effects rule, RomKonamiSCC.cc:108-123).
    if ((address & 0xF800) == 0x9000) {
        scc_enabled_ = ((value & 0x3F) == 0x3F);
    }
    // 4. Bank selection: the 0x800-wide register windows.
    if ((address & 0x1800) == 0x1000) {
        bank_switch((address >> 13) & 0x07, value);
    }
}

}  // namespace sony_msx::devices::cartridge
