#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/cartridge/cartridge_rom_window.h"

// Suite: Devices_CartridgeRomWindow_Unit (M19-S1, backlog B7)
//
// Byte-exact RomBlocks<BANK_SIZE>::setRom resolution (A-M19-6, behaviour
// reference references/openmsx-21.0/src/memory/RomBlocks.cc:107-118, never
// copied -- GPL isolation):
//   block = (block < nrBlocks) ? block : block & blockMask;
//   if (block < nrBlocks) { real bank } else { unmapped }
// CRITICAL SUBTLETY under test (R-M19-1): the mask is a FALLBACK consulted
// ONLY when the requested block is already out of range -- never an
// unconditional AND-mask. Covers both the default mask (nrBlocks-1, "wraps at
// end of ROM image") and an overridden (Konami-style, mask=31) mask,
// including the "oversized image, mask never engages" subtlety.

namespace {

using sony_msx::devices::cartridge::CartridgeRomWindow;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

// A `num_banks`-bank synthetic image where bank N's every byte equals N
// (mod 256) -- a clearly distinguishable marker per bank.
std::vector<std::uint8_t> make_marker_image(const unsigned num_banks) {
    std::vector<std::uint8_t> image(static_cast<std::size_t>(num_banks) * CartridgeRomWindow::kBankSize);
    for (unsigned bank = 0; bank < num_banks; ++bank) {
        const std::uint8_t marker = static_cast<std::uint8_t>(bank & 0xFF);
        for (std::uint32_t i = 0; i < CartridgeRomWindow::kBankSize; ++i) {
            image[static_cast<std::size_t>(bank) * CartridgeRomWindow::kBankSize + i] = marker;
        }
    }
    return image;
}

}  // namespace

int main() {
    // --- Construction derives nrBlocks from the image and defaults the mask. ---
    {
        CartridgeRomWindow window(make_marker_image(4));
        expect(window.num_blocks() == 4, "Construct_NumBlocks_FromImageSize");
        expect(window.block_mask() == 3, "Construct_DefaultBlockMask_IsNumBlocksMinusOne");
    }

    // --- Default mask: in-range request lands directly. ---
    {
        CartridgeRomWindow window(make_marker_image(4));
        window.set_bank(0, 2);
        expect(window.slot_mapped(0) && window.slot_bank(0) == 2, "DefaultMask_InRange_LandsDirect");
        expect(window.read(0x0000) == 2, "DefaultMask_InRange_ReadReturnsMarker");
    }

    // --- Default mask: out-of-range request wraps via the fallback mask and
    //     (since default mask always < nrBlocks) always resolves to a real
    //     bank -- "wraps at end of ROM image", RomBlocks.cc:46. ---
    {
        CartridgeRomWindow window(make_marker_image(4));
        window.set_bank(1, 5);  // 5 >= 4 -> 5 & 3 = 1
        expect(window.slot_mapped(1) && window.slot_bank(1) == 1, "DefaultMask_OutOfRange_WrapsToValidBank");
        window.set_bank(2, 100);  // 100 >= 4 -> 100 & 3 = 0
        expect(window.slot_mapped(2) && window.slot_bank(2) == 0, "DefaultMask_FarOutOfRange_StillWraps");
    }

    // --- Overridden mask: out-of-range request wraps but the masked result
    //     can STILL be out of range -> unmapped (only possible when the
    //     override mask itself is >= nrBlocks, unlike the default mask). ---
    {
        CartridgeRomWindow window(make_marker_image(3));  // nrBlocks = 3
        window.set_block_mask(7);
        window.set_bank(0, 5);  // 5 >= 3 -> 5 & 7 = 5 -> 5 >= 3 -> unmapped
        expect(!window.slot_mapped(0), "OverriddenMask_OutOfRangeAfterWrap_Unmapped");
        expect(window.read(0x0000) == CartridgeRomWindow::kOpenBus,
               "OverriddenMask_Unmapped_ReadsOpenBus");
        window.set_bank(1, 2);  // 2 < 3 -> direct, mask irrelevant
        expect(window.slot_mapped(1) && window.slot_bank(1) == 2, "OverriddenMask_InRange_StillDirect");
    }

    // --- Konami-style mask=31 (RomKonami.cc:24), 256 KB image (32 banks):
    //     a request with the mask engaging (>=32) but still resolving to a
    //     real bank after the AND. ---
    {
        CartridgeRomWindow window(make_marker_image(32));
        window.set_block_mask(31);
        window.set_bank(0, 50);  // 50 >= 32 -> 50 & 31 = 18 -> 18 < 32 -> mapped
        expect(window.slot_mapped(0) && window.slot_bank(0) == 18, "KonamiMask_OutOfRange_WrapsToValidBank");
    }

    // --- Konami-style mask=31, 16-bank (128 KB) image: the wrap can still
    //     land out of range (16 <= result < 32) -> unmapped. ---
    {
        CartridgeRomWindow window(make_marker_image(16));
        window.set_block_mask(31);
        window.set_bank(0, 50);  // 50 >= 16 -> 50 & 31 = 18 -> 18 >= 16 -> unmapped
        expect(!window.slot_mapped(0), "KonamiMask_SmallImage_OutOfRangeAfterWrap_Unmapped");
    }

    // --- CRITICAL SUBTLETY (A-M19-6/R-M19-1): a Konami-typed image LARGER
    //     than 256 KB (e.g. 320 KB = 40 banks) is NOT capped to 32 reachable
    //     banks -- every byte-value bank request 0..255 satisfies
    //     block < nrBlocks(40) directly for values < 40 and is used UNMASKED;
    //     the mask=31 fallback never engages for those. ---
    {
        CartridgeRomWindow window(make_marker_image(40));
        window.set_block_mask(31);
        window.set_bank(0, 35);  // 35 < 40 -> DIRECT, even though 35 > 31.
        expect(window.slot_mapped(0) && window.slot_bank(0) == 35,
               "KonamiMask_OversizedImage_MaskNeverEngages_BeyondMaskValue");
        expect(window.read(0x0000) == 35, "KonamiMask_OversizedImage_ReadReturnsUnmaskedBank");
    }

    // --- set_unmapped forces open-bus reads regardless of prior state. ---
    {
        CartridgeRomWindow window(make_marker_image(4));
        window.set_bank(3, 1);
        expect(window.slot_mapped(3), "SetUnmapped_PriorState_WasMapped");
        window.set_unmapped(3);
        expect(!window.slot_mapped(3), "SetUnmapped_NowUnmapped");
        expect(window.read(0x6000) == CartridgeRomWindow::kOpenBus, "SetUnmapped_ReadsOpenBus");
    }

    // --- Address -> window-slot selection spans the whole 64 KB (8 x 8 KB). ---
    {
        CartridgeRomWindow window(make_marker_image(8));
        for (int s = 0; s < CartridgeRomWindow::kSlots; ++s) {
            window.set_bank(s, static_cast<unsigned>(s));
        }
        bool all_ok = true;
        for (int s = 0; s < CartridgeRomWindow::kSlots; ++s) {
            const std::uint16_t addr = static_cast<std::uint16_t>(s * CartridgeRomWindow::kBankSize);
            if (window.read(addr) != static_cast<std::uint8_t>(s)) {
                all_ok = false;
            }
            const std::uint16_t addr_end =
                static_cast<std::uint16_t>(addr + CartridgeRomWindow::kBankSize - 1);
            if (window.read(addr_end) != static_cast<std::uint8_t>(s)) {
                all_ok = false;
            }
        }
        expect(all_ok, "AddressToSlot_SpansWholeSixtyFourKB");
    }

    // --- Two-run determinism. ---
    {
        auto run = [] {
            CartridgeRomWindow window(make_marker_image(4));
            window.set_bank(2, 1);
            window.set_bank(3, 9);  // wraps
            std::vector<std::uint8_t> reads;
            for (std::uint32_t addr = 0; addr < 0x10000; addr += 0x1000) {
                reads.push_back(window.read(static_cast<std::uint16_t>(addr)));
            }
            return reads;
        };
        expect(run() == run(), "TwoRunDeterminism_IdenticalSequence_IdenticalResults");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_CartridgeRomWindow_Unit cases passed\n";
    return 0;
}
