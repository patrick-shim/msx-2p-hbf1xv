#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/kanji/kanji_font_rom.h"

// Suite: Devices_KanjiFontRom_Unit  (M18-S1, backlog B5)
//
// Byte-exact address-latch + auto-increment protocol over #D8-#DB, grounded
// in references/openmsx-21.0/src/MSXKanji.cc:32-88 (A-M18-3). Uses a
// synthetic, byte-distinguishable 256 KB image -- no bios/ file dependency
// (the real bios/f1xvkfn.rom content check is a machine-level concern,
// tests/integration/machine/hbf1xv_m18_peripheral_io_integration_test.cpp).

namespace {

using sony_msx::devices::kanji::KanjiFontRom;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

std::vector<std::uint8_t> make_synthetic_image() {
    std::vector<std::uint8_t> image(KanjiFontRom::kImageSize);
    for (std::size_t i = 0; i < image.size(); ++i) {
        // Distinguishable per-address pattern (not a real glyph -- pure
        // protocol test): every address maps to a unique-ish byte.
        image[i] = static_cast<std::uint8_t>((i * 37 + 11) & 0xFF);
    }
    return image;
}

// Select adr1_ via #D8 (low, bits5-10) then #D9 (high, bits11-16). Per
// A-M18-3, BOTH writes clear adr1_'s low 5 bits, so the resulting counter is
// exactly `addr & ~0x1F` (block-aligned), regardless of write order.
void select_adr1(KanjiFontRom& dev, const std::uint32_t addr) {
    dev.io_write(0xD8, static_cast<std::uint8_t>((addr >> 5) & 0x3F));
    dev.io_write(0xD9, static_cast<std::uint8_t>((addr >> 11) & 0x3F));
}

// Select adr2_ via #DA (low, bits5-10) then #DB (high, bits11-16). Per
// A-M18-3, the #DB write mask (0x207E0) NEVER touches bit 17 (0x20000) --
// it is preserved from whatever adr2_ already held (0x20000 after reset).
void select_adr2(KanjiFontRom& dev, const std::uint32_t addr) {
    dev.io_write(0xDA, static_cast<std::uint8_t>((addr >> 5) & 0x3F));
    dev.io_write(0xDB, static_cast<std::uint8_t>((addr >> 11) & 0x3F));
}

}  // namespace

int main() {
    const std::vector<std::uint8_t> image = make_synthetic_image();

    // --- reset() defaults (MSXKanji.cc:28-29). ---
    {
        KanjiFontRom dev;
        expect(dev.address1() == KanjiFontRom::kResetAdr1, "Construct_Adr1_DefaultsToZero");
        expect(dev.address2() == KanjiFontRom::kResetAdr2, "Construct_Adr2_DefaultsTo0x20000");
    }

    // --- #D8/#D9 writes land adr1_ block-aligned (low 5 bits cleared). ---
    {
        KanjiFontRom dev;
        select_adr1(dev, 0x01234);
        expect(dev.address1() == (0x01234u & ~0x1Fu), "Adr1Write_LandsBlockAligned");
    }

    // --- #D9 sequential reads return the synthetic image and auto-increment
    //     wraps within the 32-byte glyph block (A-M18-3/R-M18-2). ---
    {
        KanjiFontRom dev;
        dev.set_image(image);
        const std::uint32_t base = 0x00100;  // already 32-byte aligned
        select_adr1(dev, base);
        bool all_ok = true;
        for (int i = 0; i < 40; ++i) {  // exceeds 32 -> exercises the wrap
            const std::uint32_t expected_addr = base + static_cast<std::uint32_t>(i % 32);
            const std::uint8_t got = dev.io_read(0xD9);
            if (got != image[expected_addr]) {
                all_ok = false;
            }
        }
        expect(all_ok, "Adr1SequentialRead_MatchesImageWithWrap");
        // After exactly 32 reads (40 % 32 == 8 past a full wrap), the low 5
        // bits equal (0 + 40) & 0x1F.
        expect(dev.address1() == (base + (40u & 0x1Fu)), "Adr1AutoIncrement_LowFiveBitsWrapped");
    }

    // --- #DB sequential reads (JIS2 half) mirror the same protocol. ---
    {
        KanjiFontRom dev;
        dev.set_image(image);
        const std::uint32_t base = 0x20000 + 0x0200;
        select_adr2(dev, base);
        bool all_ok = true;
        for (int i = 0; i < 34; ++i) {
            const std::uint32_t expected_addr = base + static_cast<std::uint32_t>(i % 32);
            const std::uint8_t got = dev.io_read(0xDB);
            if (got != image[expected_addr]) {
                all_ok = false;
            }
        }
        expect(all_ok, "Adr2SequentialRead_MatchesImageWithWrap");
    }

    // --- adr2_ bit 17 (JIS2-half selector) is NEVER lost across #DA/#DB
    //     writes, even when the requested address's own bit17 is 0
    //     (A-M18-3/R-M18-2 -- the write path structurally cannot clear it). ---
    {
        KanjiFontRom dev;
        select_adr2(dev, 0x00456);  // deliberately a JIS1-shaped address
        expect((dev.address2() & 0x20000u) != 0, "Adr2Write_Bit17NeverLost_EvenWithJis1ShapedInput");
        expect(dev.address2() == (0x20000u | (0x00456u & ~0x1Fu)), "Adr2Write_Bit17Preserved_RestMatchesRequest");
    }
    {
        // A representative genuine JIS2-half address, spanning a realistic
        // selection (mirrors the A/B probe's representative addresses).
        KanjiFontRom dev;
        select_adr2(dev, 0x2FFE0);
        expect(dev.address2() == 0x2FFE0u, "Adr2Write_GenuineJis2Address_LandsExactly");
    }

    // --- #D8/#DA reads are ALWAYS open-bus 0xFF, with NO side effect on
    //     adr1_/adr2_ (A-M18-3). ---
    {
        KanjiFontRom dev;
        dev.set_image(image);
        select_adr1(dev, 0x01000);
        select_adr2(dev, 0x21000);
        const std::uint32_t adr1_before = dev.address1();
        const std::uint32_t adr2_before = dev.address2();
        bool all_ff = true;
        for (int i = 0; i < 5; ++i) {
            if (dev.io_read(0xD8) != 0xFF) all_ff = false;
            if (dev.io_read(0xDA) != 0xFF) all_ff = false;
        }
        expect(all_ff, "D8AndDA_Reads_AlwaysOpenBus");
        expect(dev.address1() == adr1_before, "D8Read_NoSideEffectOnAdr1");
        expect(dev.address2() == adr2_before, "DARead_NoSideEffectOnAdr2");
    }

    // --- reset() restores the documented defaults after writes/reads. ---
    {
        KanjiFontRom dev;
        dev.set_image(image);
        select_adr1(dev, 0x01A20);
        select_adr2(dev, 0x21A20);
        dev.io_read(0xD9);
        dev.io_read(0xDB);
        dev.reset();
        expect(dev.address1() == KanjiFontRom::kResetAdr1, "Reset_RestoresAdr1Default");
        expect(dev.address2() == KanjiFontRom::kResetAdr2, "Reset_RestoresAdr2Default");
    }

    // --- Two-run determinism: identical write/read sequence -> identical
    //     results (both address counters and every returned byte). ---
    {
        auto run = [&image]() {
            KanjiFontRom dev;
            dev.set_image(image);
            select_adr1(dev, 0x00A20);
            select_adr2(dev, 0x20A20);
            std::vector<std::uint8_t> reads;
            for (int i = 0; i < 20; ++i) {
                reads.push_back(dev.io_read(0xD9));
                reads.push_back(dev.io_read(0xDB));
            }
            reads.push_back(static_cast<std::uint8_t>(dev.address1() & 0xFF));
            reads.push_back(static_cast<std::uint8_t>(dev.address2() & 0xFF));
            return reads;
        };
        const std::vector<std::uint8_t> a = run();
        const std::vector<std::uint8_t> b = run();
        expect(a == b, "TwoRunDeterminism_IdenticalSequence_IdenticalResults");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_KanjiFontRom_Unit cases passed\n";
    return 0;
}
