#pragma once

#include <cstddef>
#include <vector>

#include "devices/audio/scc_wavetable.h"
#include "devices/cartridge/cartridge_mapper_device.h"
#include "devices/cartridge/cartridge_rom_window.h"

namespace sony_msx::devices::cartridge {

// `KonamiSCC` mapper -- the SCC-chip-bearing sibling of M19's plain Konami
// mapper (M29-S3, backlog G1; spec docs/m29-planner-package.md §2.1).
// Grounds references/openmsx-21.0/src/memory/RomKonamiSCC.cc (behaviour
// reference only, never copied -- GPL isolation) and references/fact-sheets/
// "Konami SCC.md" §2 ("SCC fact-sheet"). Used by the real Konami SCC
// MegaROM carts (Nemesis 2/3, King's Valley 2, Space Manbow, Solid Snake,
// Quarth, ...). Owns a real SccWavetable member (the openMSX "RomKonamiSCC
// owns a real `SCC scc;`" shape).
//
// Geometry (SCC fact-sheet §2): 8 kB banks; bank-select registers decode
// 0x800-WIDE windows -- (addr & 0x1800) == 0x1000 within [0x5000, 0xC000):
//   0x5000-0x57FF -> page 2 (CPU 0x4000), 0x7000-0x77FF -> page 3 (0x6000),
//   0x9000-0x97FF -> page 4 (0x8000), 0xB000-0xB7FF -> page 5 (0xA000).
// Writes below 0x5000 or at/above 0xC000 are ignored entirely (contrast
// plain Konami's 0x6000 lower bound).
//
// MIRRORING IS THE OPPOSITE OF PLAIN KONAMI (RomKonamiSCC.cc:44-58, "the
// mirror behavior is different from RomKonami!"; risk R-M29-1): pages 2/3
// (0x4000-0x7FFF) mirror into window-slots 6/7 (0xC000-0xFFFF); pages 4/5
// (0x8000-0xBFFF) mirror into window-slots 0/1 (0x0000-0x3FFF).
//
// SCC enable latch (SCC fact-sheet §2, A-M29-2/§9.6 arbitration): a write
// anywhere in 0x9000-0x97FF with (value & 0x3F) == 0x3F enables the SCC
// (0xBF enables too -- the bank latch is 6 bits wide, the 512 kB/64-bank
// hardware ceiling's own rationale); any other value disables. THE SAME
// WRITE STILL BANK-SWITCHES PAGE 4 (both-effects rule, RomKonamiSCC.cc:
// 108-123 -- the enable check falls through to the page-selection check).
//
// SCC register window: when enabled, reads AND writes in 0x9800-0x9FFF go
// to the sound generator, addressed as (addr & 0xFF) -- the 256-byte map
// mirrored 8x across the 2 kB window (A-M29-1; fMSX decodes only
// 0x9800-0x98FF, arbitrated to openMSX, SCC fact-sheet §9.5). Reads use
// SccWavetable::read() (NOT peek -- the deform-range read side effect must
// fire). The ROM MIRROR pages never expose the SCC window: the override is
// keyed on the raw CPU range 0x9800-0x9FFF only, so 0x1800-0x1FFF (the
// mirror of that region) serves mirrored ROM (SCC fact-sheet §2).
//
// Block mask: KonamiSCC keeps CartridgeRomWindow's image-derived default
// mask (mask-as-fallback-only, A-M19-6) -- deliberately NO set_block_mask(31)
// (that is plain-Konami's 256 kB rule; SCC fact-sheet §10.4). Images larger
// than the real chips' 512 kB/64-bank ceiling are ACCEPTED (openMSX warns
// non-fatally, RomKonamiSCC.cc:28-34) -- documented, never rejected.
//
// Constructor deliberately does NOT self-reset (M19 convention;
// CartridgeSlot::load() resets once before installing). reset():
// bank_switch(2,0)/(3,1)/(4,2)/(5,3), scc_enabled_ = false, scc_.reset()
// (RomKonamiSCC.cc:60-68; the powerUp-vs-reset collapse is SccWavetable's
// disclosed A-M29-5 simplification).
class CartridgeKonamiScc final : public CartridgeMapperDevice {
public:
    [[nodiscard]] static bool is_valid_image_size(std::size_t size);

    // Precondition: is_valid_image_size(image.size()). Does NOT call reset()
    // (see class comment) -- the caller (CartridgeSlot::load) must reset()
    // before relying on any documented bank layout.
    explicit CartridgeKonamiScc(std::vector<std::uint8_t> image);

    void reset() override;
    [[nodiscard]] CartridgeMapperType mapper_type() const override {
        return CartridgeMapperType::KonamiSCC;
    }

    core::BusData mem_read(core::BusAddress address) override;
    void mem_write(core::BusAddress address, core::BusData value) override;

    // Debug/test seams (mirror the M19 window() seam).
    [[nodiscard]] const CartridgeRomWindow& window() const { return window_; }
    [[nodiscard]] const audio::SccWavetable& scc() const { return scc_; }
    [[nodiscard]] audio::SccWavetable& scc() { return scc_; }
    [[nodiscard]] bool scc_enabled() const { return scc_enabled_; }

private:
    void bank_switch(int page, unsigned block);

    CartridgeRomWindow window_;
    audio::SccWavetable scc_;
    bool scc_enabled_ = false;
};

}  // namespace sony_msx::devices::cartridge
