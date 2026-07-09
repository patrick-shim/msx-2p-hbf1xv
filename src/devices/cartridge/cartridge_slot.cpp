#include "devices/cartridge/cartridge_slot.h"

#include <utility>

#include "devices/cartridge/cartridge_ascii16kb_rom.h"
#include "devices/cartridge/cartridge_ascii8kb_rom.h"
#include "devices/cartridge/cartridge_generic16kb_rom.h"
#include "devices/cartridge/cartridge_generic8kb_rom.h"
#include "devices/cartridge/cartridge_konami_rom.h"
#include "devices/cartridge/cartridge_konami_scc_rom.h"
#include "devices/cartridge/cartridge_mirrored_rom.h"

namespace sony_msx::devices::cartridge {

CartridgeSlot::CartridgeSlot(const int primary_slot_number) : primary_slot_number_(primary_slot_number) {
}

CartridgeLoadResult CartridgeSlot::load(const CartridgeMapperType type, std::vector<std::uint8_t> image) {
    // Validate BEFORE constructing anything (A-M19-7): a failed load leaves
    // the slot's prior state (loaded or empty) completely untouched.
    std::unique_ptr<CartridgeMapperDevice> candidate;
    switch (type) {
        case CartridgeMapperType::Mirrored:
            if (!CartridgeMirroredRom::is_valid_image_size(image.size())) {
                return CartridgeLoadResult::ImageSizeInvalidForMapperType;
            }
            candidate = std::make_unique<CartridgeMirroredRom>(std::move(image));
            break;
        case CartridgeMapperType::Generic8kB:
            if (!CartridgeGeneric8kbRom::is_valid_image_size(image.size())) {
                return CartridgeLoadResult::ImageSizeInvalidForMapperType;
            }
            candidate = std::make_unique<CartridgeGeneric8kbRom>(std::move(image));
            break;
        case CartridgeMapperType::Generic16kB:
            if (!CartridgeGeneric16kbRom::is_valid_image_size(image.size())) {
                return CartridgeLoadResult::ImageSizeInvalidForMapperType;
            }
            candidate = std::make_unique<CartridgeGeneric16kbRom>(std::move(image));
            break;
        case CartridgeMapperType::Ascii8kB:
            if (!CartridgeAscii8kbRom::is_valid_image_size(image.size())) {
                return CartridgeLoadResult::ImageSizeInvalidForMapperType;
            }
            candidate = std::make_unique<CartridgeAscii8kbRom>(std::move(image));
            break;
        case CartridgeMapperType::Ascii16kB:
            if (!CartridgeAscii16kbRom::is_valid_image_size(image.size())) {
                return CartridgeLoadResult::ImageSizeInvalidForMapperType;
            }
            candidate = std::make_unique<CartridgeAscii16kbRom>(std::move(image));
            break;
        case CartridgeMapperType::Konami:
            if (!CartridgeKonamiRom::is_valid_image_size(image.size())) {
                return CartridgeLoadResult::ImageSizeInvalidForMapperType;
            }
            candidate = std::make_unique<CartridgeKonamiRom>(std::move(image));
            break;
        case CartridgeMapperType::KonamiSCC:
            // M29-S3 (backlog G1): size-validate -> construct -> reset ->
            // install, the same uniform contract as the six M19 types.
            if (!CartridgeKonamiScc::is_valid_image_size(image.size())) {
                return CartridgeLoadResult::ImageSizeInvalidForMapperType;
            }
            candidate = std::make_unique<CartridgeKonamiScc>(std::move(image));
            break;
    }

    // Establish a well-defined power-up bank layout for every type uniformly
    // (including Konami, whose own constructor deliberately does not
    // self-reset, RomKonami.cc:33-35) BEFORE installing it as active.
    candidate->reset();
    mapper_ = std::move(candidate);
    return CartridgeLoadResult::Ok;
}

void CartridgeSlot::unload() {
    mapper_.reset();
}

void CartridgeSlot::reset() {
    // A-M19-9: reinitializes bank state; no-op when empty; NEVER unloads.
    if (mapper_) {
        mapper_->reset();
    }
}

CartridgeMapperType CartridgeSlot::mapper_type() const {
    return mapper_->mapper_type();
}

core::BusData CartridgeSlot::mem_read(const core::BusAddress address) {
    if (!mapper_) {
        return kOpenBus;
    }
    return mapper_->mem_read(address);
}

void CartridgeSlot::mem_write(const core::BusAddress address, const core::BusData value) {
    if (!mapper_) {
        return;
    }
    mapper_->mem_write(address, value);
}

}  // namespace sony_msx::devices::cartridge
