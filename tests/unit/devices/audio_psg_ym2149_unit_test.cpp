#include <cstdint>
#include <iostream>

#include "devices/audio/psg_ym2149.h"

// Suite: Devices_AudioPsgYm2149_Unit  (M15-S1, backlog B1)
//
// YM2149 register model: address-latch mask, tone/noise/envelope period decode,
// R7 mixer/IO-direction MSX masking, YM register-readback-as-written, 5-bit
// envelope shape stepping, resolved amplitude, and the S1985 stereo mix
// (A=Center, B=Left, C=Right). Grounding: fact-sheet §2; AY8910.cc.

namespace {

using sony_msx::devices::audio::PsgPortSource;
using sony_msx::devices::audio::PsgYm2149;

bool expect_true(const bool ok, const char* name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << name << "\n";
    return false;
}

// Records port A/B traffic so R14/R15 routing can be asserted.
class StubSource final : public PsgPortSource {
public:
    std::uint8_t read_port_a() override {
        ++reads;
        return a_value;
    }
    void write_port_b(std::uint8_t value) override {
        ++writes;
        last_b = value;
    }
    std::uint8_t a_value = 0x5A;
    std::uint8_t last_b = 0;
    int reads = 0;
    int writes = 0;
};

}  // namespace

int main() {
    // --- Address-latch mask 0x0F (mirrored registers). ---
    {
        PsgYm2149 psg;
        psg.reset();
        psg.write_address(0x1A);
        if (!expect_true(psg.selected_register() == 0x0A, "AddressLatch_MaskedTo0F")) {
            return 1;
        }
        psg.io_write(0xA0, 0x2C);  // via the bus port
        if (!expect_true(psg.selected_register() == 0x0C, "AddressLatch_ViaBus_Masked")) {
            return 1;
        }
    }

    // --- Tone period decode (12-bit), noise (5-bit), envelope (16-bit). ---
    {
        PsgYm2149 psg;
        psg.reset();
        psg.write_address(0);
        psg.write_data(0x34);  // R0 fine
        psg.write_address(1);
        psg.write_data(0x02);  // R1 coarse (only low nibble)
        if (!expect_true(psg.tone_period(0) == 0x234, "TonePeriod_DecodedFromR0R1")) {
            return 1;
        }
        psg.write_address(6);
        psg.write_data(0xFF);  // R6 noise (5-bit)
        if (!expect_true(psg.noise_period() == 0x1F, "NoisePeriod_5BitMasked")) {
            return 1;
        }
        psg.write_address(11);
        psg.write_data(0x11);
        psg.write_address(12);
        psg.write_data(0x22);
        if (!expect_true(psg.envelope_period() == 0x2211, "EnvelopePeriod_16Bit")) {
            return 1;
        }
    }

    // --- R7 mixer/IO-direction MSX masking (bit6 forced 0, bit7 forced 1). ---
    {
        PsgYm2149 psg;
        psg.reset();
        psg.write_address(7);
        psg.write_data(0x00);
        if (!expect_true(psg.register_value(7) == 0x80, "R7_ForcesPortBOutput_PortAInput")) {
            return 1;
        }
        psg.write_data(0xFF);
        if (!expect_true(psg.register_value(7) == 0xBF, "R7_Mask_ClearsBit6_SetsBit7")) {
            return 1;
        }
    }

    // --- YM2149 register readback returns values AS WRITTEN (AY would mask). ---
    {
        PsgYm2149 psg;
        psg.reset();
        psg.write_address(1);
        psg.write_data(0xFF);  // R1 coarse: AY masks to 0x0F, YM keeps 0xFF
        if (!expect_true(psg.register_value(1) == 0xFF, "YmReadback_R1_AsWritten")) {
            return 1;
        }
        psg.write_address(1);
        if (!expect_true(psg.read_data() == 0xFF, "YmReadback_ViaData_AsWritten")) {
            return 1;
        }
    }

    // --- 5-bit / 32-step envelope shape stepping. ---
    {
        PsgYm2149 psg;
        psg.reset();
        // Shape 0x00 (\___): attack=0, hold, no alternate. Volume starts 0x1F.
        psg.write_address(13);
        psg.write_data(0x00);
        if (!expect_true(psg.envelope_volume() == 0x1F, "EnvShape00_StartsFull")) {
            return 1;
        }
        psg.debug_step_envelope(31);
        if (!expect_true(psg.envelope_volume() == 0x00, "EnvShape00_DecaysToZero")) {
            return 1;
        }
        psg.debug_step_envelope(4);  // holds at 0
        if (!expect_true(psg.envelope_volume() == 0x00, "EnvShape00_HoldsAtZero")) {
            return 1;
        }
        // Shape 0x0C (////): attack=0x1F. Volume starts 0 and rises to 0x1F.
        psg.write_address(13);
        psg.write_data(0x0C);
        if (!expect_true(psg.envelope_volume() == 0x00, "EnvShape0C_StartsZero")) {
            return 1;
        }
        psg.debug_step_envelope(31);
        if (!expect_true(psg.envelope_volume() == 0x1F, "EnvShape0C_RisesToFull")) {
            return 1;
        }
    }

    // --- Resolved amplitude + envelope-enable bit. ---
    {
        PsgYm2149 psg;
        psg.reset();
        psg.write_address(8);
        psg.write_data(0x0F);  // fixed level 15 -> 2*15+1 = 31
        if (!expect_true(psg.channel_amplitude(0) == 31, "Amplitude_Fixed15_Is31")) {
            return 1;
        }
        psg.write_data(0x00);  // level 0 -> 0
        if (!expect_true(psg.channel_amplitude(0) == 0, "Amplitude_FixedZero")) {
            return 1;
        }
        psg.write_data(0x10);  // follow envelope
        psg.write_address(13);
        psg.write_data(0x00);  // envelope full
        if (!expect_true(psg.channel_amplitude(0) == psg.envelope_volume(),
                         "Amplitude_FollowEnvelope")) {
            return 1;
        }
    }

    // --- Stereo mix: A=both, B=left only, C=right only. ---
    {
        PsgYm2149 psg;
        psg.reset();
        // Disable tone+noise on all channels (R7 bits0-5 = 1) -> DC amplitude.
        psg.write_address(7);
        psg.write_data(0x3F);
        psg.write_address(8);
        psg.write_data(0x0F);  // A -> 31
        psg.write_address(9);
        psg.write_data(0x08);  // B -> 17
        psg.write_address(10);
        psg.write_data(0x04);  // C -> 9
        const auto s = psg.sample();
        if (!expect_true(s.left == 31 + 17, "Stereo_Left_IsAPlusB")) {
            return 1;
        }
        if (!expect_true(s.right == 31 + 9, "Stereo_Right_IsAPlusC")) {
            return 1;
        }
    }

    // --- R14 read / R15 write route to the injected source. ---
    {
        PsgYm2149 psg;
        psg.reset();
        StubSource src;
        psg.attach_port_source(&src);
        psg.write_address(14);
        if (!expect_true(psg.read_data() == 0x5A && src.reads == 1, "R14_ReadsPortSource")) {
            return 1;
        }
        psg.write_address(15);
        psg.write_data(0x40);  // select port 2
        if (!expect_true(src.writes == 1 && src.last_b == 0x40, "R15_WritesPortSource")) {
            return 1;
        }
    }

    // --- Determinism: two identical write+advance sequences match. ---
    {
        auto run = [] {
            PsgYm2149 psg;
            psg.reset();
            psg.write_address(0);
            psg.write_data(0x10);
            psg.write_address(13);
            psg.write_data(0x0A);  // \/\/ shape
            psg.advance_cycles(100000);
            return psg.envelope_volume();
        };
        if (!expect_true(run() == run(), "Determinism_TwoRunsIdenticalEnvelope")) {
            return 1;
        }
    }

    return 0;
}
