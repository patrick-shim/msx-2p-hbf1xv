#include "devices/cartridge/cartridge_mirrored_rom.h"

namespace sony_msx::devices::cartridge {

bool CartridgeMirroredRom::is_valid_image_size(const std::size_t size) {
    return size > 0 && size <= 0x10000 && (size % CartridgeRomWindow::kBankSize) == 0;
}

CartridgeMirroredRom::CartridgeMirroredRom(std::vector<std::uint8_t> image) : window_(std::move(image)) {
    reset();
}

void CartridgeMirroredRom::reset() {
    // Bank 0 at 0x0000 (A-M19-8: no placement ambiguity -- the whole 64 KB
    // window IS the external slot). For window-slot s < nrBlocks this lands
    // bank s directly; for s >= nrBlocks it falls through to
    // CartridgeRomWindow::set_bank's default-mask fallback, WHICH IS THE SAME
    // "romPage & (numPages - 1)" bitwise-AND wraparound RomPlain.cc:92-93 uses
    // for its mirrored branch (not a true modulo -- they coincide only when
    // nrBlocks is a power of two). Grounds RomPlain.cc:85-98 exactly (with
    // firstPage == 0, mirrored == true for the Mirrored type).
    for (int s = 0; s < CartridgeRomWindow::kSlots; ++s) {
        window_.set_bank(s, static_cast<unsigned>(s));
    }
}

core::BusData CartridgeMirroredRom::mem_read(const core::BusAddress address) {
    return window_.read(address);
}

void CartridgeMirroredRom::mem_write(const core::BusAddress /*address*/, const core::BusData /*value*/) {
    // Read-only: no bank-switch registers (RomPlain has none for MIRRORED).
}

}  // namespace sony_msx::devices::cartridge
