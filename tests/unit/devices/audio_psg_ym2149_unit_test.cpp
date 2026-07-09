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

    // =====================================================================
    // M34 additive take_integrated_sample() cases (DEC-0043 Defect A,
    // docs/m34-planner-package.md §2.6.5). Every oracle below is dwell
    // arithmetic authored by hand BEFORE execution (R-M34-9): a period-1
    // tone from reset toggles at every 16-cycle generator step, and the
    // §2.3.3 boundary convention puts the completing cycle's dwell on the
    // PRE-step level, so level(t) = 31 iff floor((t-1)/16) is odd
    // (cycles 1..16 low, 17..32 high, ...).
    // =====================================================================

    // --- M34: period-1 dwell hand-oracle at W=81 (§2.6.5). Window k covers
    //     cycles [81k+1, 81k+81]. Hand-computed high-dwell per window:
    //       w0: [17-32]+[49-64]+[81]          = 16+16+1 = 33 -> 31*33=1023 -> 13
    //       w1: [82-96]+[113-128]+[145-160]   = 15+16+16 = 47 -> 1457 -> 18
    //       w2: [177-192]+[209-224]+[241-243] = 16+16+3 = 35 -> 1085 -> 13
    //       w3: [244-256]+[273-288]+[305-320] = 13+16+16 = 45 -> 1395 -> 17
    //       w4: [337-352]+[369-384]+[401-405] = 16+16+5 = 37 -> 1147 -> 14
    //       w5: [406-416]+[433-448]+[465-480] = 11+16+16 = 43 -> 1333 -> 16
    //     (dwells 33,47,35,45,37,43 -- inside the package's 33..48 band;
    //     round-half-away-from-zero of sum/81 gives 13,18,13,17,14,16.)
    //     Cross-checked below against an INDEPENDENT per-cycle level model
    //     (pure arithmetic over the ideal square, not chip state). ---
    {
        PsgYm2149 psg;
        psg.reset();
        psg.write_address(0);
        psg.write_data(1);  // R0: period 1
        psg.write_address(8);
        psg.write_data(15);  // R8: fixed volume 15 -> amplitude 31
        psg.write_address(7);
        psg.write_data(0x3E);  // tone A only

        const std::int32_t expected[6] = {13, 18, 13, 17, 14, 16};
        bool sequence_ok = true;
        std::uint64_t cycle = 0;  // cycles consumed so far
        for (int k = 0; k < 6; ++k) {
            psg.advance_cycles(81);
            const auto s = psg.take_integrated_sample(81);
            // Independent per-cycle hand model of the ideal square (31 iff
            // floor((t-1)/16) odd) through an exact 81-cycle box.
            std::int64_t model_sum = 0;
            for (std::uint64_t t = cycle + 1; t <= cycle + 81; ++t) {
                if (((t - 1) / 16) % 2 == 1) {
                    model_sum += 31;
                }
            }
            cycle += 81;
            const auto model = static_cast<std::int32_t>((2 * model_sum + 81) / 162);
            if (s.left != expected[k] || s.right != expected[k] || s.left != model) {
                sequence_ok = false;
            }
        }
        if (!expect_true(sequence_ok, "M34_Period1_W81_DwellHandOracle_13_18_13_17_14_16")) {
            return 1;
        }
    }

    // --- M34: constant fixed-point property (§2.3.4): all generators
    //     disabled (R7=0x3F) at fixed volumes A=15/B=8/C=4 -> constant
    //     levels 31/17/9 -> integrated == point == {48, 40} exactly, for
    //     both an odd (81) and a tie-prone even (16) window. ---
    {
        PsgYm2149 psg;
        psg.reset();
        psg.write_address(7);
        psg.write_data(0x3F);
        psg.write_address(8);
        psg.write_data(15);
        psg.write_address(9);
        psg.write_data(8);
        psg.write_address(10);
        psg.write_data(4);

        psg.advance_cycles(81);
        const auto s81 = psg.take_integrated_sample(81);
        psg.advance_cycles(16);
        const auto s16 = psg.take_integrated_sample(16);
        const auto point = psg.sample();
        if (!expect_true(s81.left == 48 && s81.right == 40 && s16.left == 48 && s16.right == 40 &&
                             point.left == 48 && point.right == 40,
                         "M34_ConstantLevels_FixedPoint_IntegratedEqualsPointExactly")) {
            return 1;
        }
    }

    // --- M34: envelope mid-window segmenting (§2.6.5). R11=1/R12=0 ->
    //     envelope period 2*1=2 generator steps = 32 cycles per envelope
    //     step; shape 0x00 (\__): volume 31, then 30 after cycle 32, 29
    //     after cycle 64. Channel A follows the envelope (R8 bit4), all
    //     generators disabled -> level == envelope volume. One 81-cycle
    //     window integrates 31*32 + 30*32 + 29*17 = 992+960+493 = 2445 ->
    //     round(2445/81) = round(30.19) = 30 (the point sample at cycle 81
    //     would be 29 -- the integral is provably NOT the endpoint). ---
    {
        PsgYm2149 psg;
        psg.reset();
        psg.write_address(7);
        psg.write_data(0x3F);
        psg.write_address(11);
        psg.write_data(1);  // envelope fine period 1 -> internal period 2
        psg.write_address(8);
        psg.write_data(0x10);  // follow envelope
        psg.write_address(13);
        psg.write_data(0x00);  // shape \__ : starts at 31, decays

        psg.advance_cycles(81);
        const auto s = psg.take_integrated_sample(81);
        const auto point_after = psg.sample();
        if (!expect_true(s.left == 30 && s.right == 30,
                         "M34_EnvelopeMidWindow_SegmentedDwell_HandComputed30")) {
            return 1;
        }
        if (!expect_true(point_after.left == 29,
                         "M34_EnvelopeMidWindow_EndpointIs29_IntegralDiscriminates")) {
            return 1;
        }
    }

    // --- M34: W=0 guard (§2.3.5) + integral reset semantics: a take with
    //     window 0 returns silence; accumulated dwell is discarded so the
    //     next window starts clean. ---
    {
        PsgYm2149 psg;
        psg.reset();
        psg.write_address(7);
        psg.write_data(0x3F);
        psg.write_address(8);
        psg.write_data(15);  // constant level 31

        const auto s0 = psg.take_integrated_sample(0);
        if (!expect_true(s0.left == 0 && s0.right == 0, "M34_ZeroWindow_ReturnsSilence")) {
            return 1;
        }
        psg.advance_cycles(40);
        const auto discarded = psg.take_integrated_sample(0);  // discards 40 cycles of dwell
        psg.advance_cycles(81);
        const auto clean = psg.take_integrated_sample(81);
        if (!expect_true(discarded.left == 0 && clean.left == 31 && clean.right == 31,
                         "M34_ZeroWindowTake_DiscardsAccumulation_NextWindowClean")) {
            return 1;
        }
    }

    // --- M34: point sample() is UNTOUCHED by integration bookkeeping --
    //     interleaved take_integrated_sample() calls never perturb the
    //     generator trajectory (state identity vs a twin that never takes). ---
    {
        PsgYm2149 with_takes;
        PsgYm2149 without_takes;
        for (PsgYm2149* psg : {&with_takes, &without_takes}) {
            psg->reset();
            psg->write_address(0);
            psg->write_data(3);
            psg->write_address(8);
            psg->write_data(15);
            psg->write_address(7);
            psg->write_data(0x3E);
        }
        bool identical = true;
        for (int i = 0; i < 200; ++i) {
            with_takes.advance_cycles(81);
            (void)with_takes.take_integrated_sample(81);
            without_takes.advance_cycles(81);
            const auto a = with_takes.sample();
            const auto b = without_takes.sample();
            if (a.left != b.left || a.right != b.right) {
                identical = false;
            }
        }
        if (!expect_true(identical, "M34_TakeCalls_NeverPerturbGeneratorState_PointApiByteKept")) {
            return 1;
        }
    }

    return 0;
}
