#include "devices/halnote/halnote_rom.h"

#include <utility>

namespace sony_msx::devices::halnote {

namespace {
constexpr std::uint32_t kSubBankRegionBase = 0x80000;  // last 512 KB, RomHalnote.cc:68
constexpr std::uint32_t kSubBankSize = 0x800;          // 2 KB
constexpr core::BusAddress kSubBankLow = 0x77FF;
constexpr core::BusAddress kSubBankHigh = 0x7FFF;
constexpr core::BusAddress kSubMapperRegionBase = 0x7000;
constexpr core::BusAddress kSubMapperRegionLimit = 0x8000;
constexpr core::BusAddress kSubMapperSplit = 0x7800;
}  // namespace

HalnoteRom::HalnoteRom() : window_(std::vector<std::uint8_t>(kImageBytes, kOpenBus)) {
    reset_bank_state();
}

void HalnoteRom::reset_bank_state() {
    // RomHalnote.cc:48-61 (bank/flag portion only -- sram_ handled by callers).
    sub_banks_ = {0, 0};
    sram_enabled_ = false;
    sub_mapper_enabled_ = false;

    window_.set_unmapped(0);
    window_.set_unmapped(1);
    window_.set_bank(2, 0);
    window_.set_bank(3, 0);
    window_.set_bank(4, 0);
    window_.set_bank(5, 0);
    window_.set_unmapped(6);
    window_.set_unmapped(7);
}

void HalnoteRom::reset() {
    reset_bank_state();
    // A-M20-12: mirrors S1985Engine::reset()'s disclosed, deterministic
    // cold-boot zeroing exactly -- persistence lives entirely in the file
    // load/save the owning machine performs around this call.
    sram_.clear();
}

void HalnoteRom::set_image(std::vector<std::uint8_t> image) {
    image.resize(kImageBytes, kOpenBus);
    window_ = devices::cartridge::CartridgeRomWindow(std::move(image));
    // Re-apply the bank/flag layout only -- sram_ is independent of the ROM
    // image content (A-M20-6/A-M20-12) and must NOT be touched here.
    reset_bank_state();
}

core::BusData HalnoteRom::mem_read(const core::BusAddress address) {
    if (address < kSramLimit) {
        // A-M20-6: direct address-range branch, proven behaviourally
        // identical to openMSX's pointer-indirection into window-slots 0/1.
        return sram_enabled_ ? sram_.read(address) : kOpenBus;
    }
    if (sub_mapper_enabled_ && address >= kSubMapperRegionBase && address < kSubMapperRegionLimit) {
        // RomHalnote.cc:65-68: sub-mapper shadow, ONLY for 0x7000-0x7FFF --
        // 0x6000-0x6FFF (same window-slot 3, outside this narrower range)
        // always falls through to the normal window read below.
        const std::size_t sub_bank_index = (address < kSubMapperSplit) ? 0 : 1;
        const std::size_t offset = kSubBankRegionBase +
                                    static_cast<std::size_t>(sub_banks_[sub_bank_index]) * kSubBankSize +
                                    (address & 0x7FF);
        return window_.image()[offset];
    }
    // Banks 2-5 span 0x4000-0xBFFF; window-slots 6/7 (0xC000-0xFFFF) are
    // permanently unmapped (0xFF reads) by construction (A-M20-9).
    return window_.read(address);
}

void HalnoteRom::mem_write(const core::BusAddress address, const core::BusData value) {
    if (address < kSramLimit) {
        // A-M20-6: writes silently ignored while disabled (RomHalnote.cc:82-86).
        if (sram_enabled_) {
            sram_.write(address, value);
        }
        return;
    }
    if (address >= kBankSwitchLimit) {
        return;  // A-M20-9: 0xC000-0xFFFF is never a trigger target.
    }
    if (address == kSubBankLow || address == kSubBankHigh) {
        // A-M20-7: sub-bank register writes ALWAYS take effect, regardless of
        // sub_mapper_enabled_ -- only READS are gated (RomHalnote.cc:88-97).
        sub_banks_[address == kSubBankLow ? 0 : 1] = value;
        return;
    }
    if ((address & 0x1FFF) == 0x0FFF) {
        // A-M20-3/A-M20-4: normal bank-switch region, bank in {2,3,4,5}.
        const int bank = address >> 13;
        // A-M20-5: pass the RAW byte (including bit 0x80) to set_bank FIRST,
        // unconditionally -- do NOT mask it out. A single write can both
        // resolve a ROM bank AND (for bank 2/3) toggle an enable flag.
        window_.set_bank(bank, value);
        if (bank == 2) {
            sram_enabled_ = (value & 0x80) != 0;
        } else if (bank == 3) {
            sub_mapper_enabled_ = (value & 0x80) != 0;
        }
    }
}

}  // namespace sony_msx::devices::halnote
