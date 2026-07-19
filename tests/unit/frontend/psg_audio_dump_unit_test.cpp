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

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "devices/audio/psg_ym2149.h"
#include "frontend/psg_audio_dump.h"
#include "frontend/psg_audio_pump.h"
#include "machine/debug_dump.h"

// Suite: Frontend_PsgAudioDump_Unit (M27-S5, "Production Hardening +
// Debug/Test Tooling" item 2, docs/m27-planner-package.md §2.3).
//
// Proves the NEW psg_raw_sum_to_pcm16()/serialize_psg_audio_dump()/
// write_psg_audio_dump() wiring is genuinely correct against a hand-computed
// oracle -- reusing the REAL PsgAudioPump/PsgYm2149 machinery (never a
// parallel/duplicate synthesis implementation), mirroring
// tests/unit/frontend/psg_audio_pump_unit_test.cpp's own oracle-construction
// discipline (the SAME program_channel_a_square_wave() fixture).

namespace {

using sony_msx::devices::audio::PsgYm2149;
using sony_msx::frontend::PsgAudioPump;

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// Mirrors psg_audio_pump_unit_test.cpp's own program_channel_a_square_wave()
// exactly (period=1, fixed max volume 15, channels B/C fully muted).
void program_channel_a_square_wave(PsgYm2149& psg) {
    psg.write_address(0);
    psg.write_data(1);  // R0 (A fine) = 1
    psg.write_address(1);
    psg.write_data(0);  // R1 (A coarse) = 0 -> tone_period == 1
    psg.write_address(8);
    psg.write_data(15);  // R8 (A volume) = fixed level 15 -> resolved_amplitude() == 31
    psg.write_address(7);
    psg.write_data(0x3E);  // R7: tone A on, tone B/C off, all noise off
}

}  // namespace

int main() {
    using sony_msx::frontend::kAudioDumpFormatTag;
    using sony_msx::frontend::psg_raw_sum_to_pcm16;
    using sony_msx::frontend::serialize_psg_audio_dump;
    using sony_msx::frontend::write_psg_audio_dump;

    // --- Case 1: psg_raw_sum_to_pcm16()'s documented linear scale, hand-
    // computed against A-M27-4's grounded [0,62] range (R-M27-3). ---
    {
        expect(psg_raw_sum_to_pcm16(0) == -32768, "PcmScale_ZeroRaw_MapsToInt16Min");
        expect(psg_raw_sum_to_pcm16(62) == 32767, "PcmScale_MaxRaw62_MapsToInt16Max");
        // raw=31 (half of 62): (31*65535)/62 - 32768 = 32767 - 32768 = -1
        // (integer division truncates 32767.5 -> 32767; hand-computed).
        expect(psg_raw_sum_to_pcm16(31) == -1, "PcmScale_HalfRaw31_HandComputedOracle");
        expect(psg_raw_sum_to_pcm16(-5) == -32768, "PcmScale_NegativeRaw_ClampedToZero");
        expect(psg_raw_sum_to_pcm16(9999) == 32767, "PcmScale_OverMaxRaw_ClampedTo62");
    }

    // --- Case 2 (M34-RE-DERIVED, docs/m34-planner-package.md §2.5 row 2;
    // dwell math authored BEFORE execution): sample_rate_hz == kSystemClockHz
    // (3579545, A-M27-5's own constant) -> cycles_per_sample == 1 exactly. A
    // W=1 box window degenerates to the DURING-cycle level (§2.3.6). With
    // tone period=1 and kCyclesPerGeneratorStep=16, the first toggle
    // completes AT cycle 16, and under the §2.3.3 boundary convention the
    // completing cycle's dwell belongs to the PRE-step level -- so sample
    // index i carries the level of cycle i+1: indices 0..15 (cycles 1..16)
    // are silent and index 16 (cycle 17) is the first audible sample. The
    // pre-M34 point sampler read the POST-toggle level at cycle 16, putting
    // the first audible sample one index earlier (index 15) -- the §2.3.3
    // one-index shift, re-derived here, not rebaselined blindly. ---
    {
        constexpr std::uint64_t kSampleRateHz = 3579545;
        constexpr std::size_t kSampleCount = 17;

        PsgYm2149 psg;
        psg.reset();
        program_channel_a_square_wave(psg);

        const std::string dump = serialize_psg_audio_dump(psg, kSampleRateHz, kSampleCount);

        std::istringstream iss(dump);
        std::string first_line;
        std::getline(iss, first_line);
        expect(first_line == kAudioDumpFormatTag, "Header_FirstLine_MatchesFormatTag");

        std::string audio_line;
        std::getline(iss, audio_line);
        expect(audio_line == "[AUDIO] sample_rate=3579545 channels=2 bits=16 samples=17",
               "Header_AudioLine_MatchesHandComputedOracle");

        // Independently reconstruct the expected raw StereoSample sequence
        // on a FRESH, identically-programmed PsgYm2149, applying the SAME
        // documented pcm16 formula -- a strong oracle, not a tautology,
        // since it genuinely re-derives generator state via PsgAudioPump.
        PsgYm2149 oracle_psg;
        oracle_psg.reset();
        program_channel_a_square_wave(oracle_psg);
        const PsgAudioPump pump(1);
        const std::vector<PsgYm2149::StereoSample> samples = pump.pump_samples(oracle_psg, kSampleCount);
        expect(samples.size() == 17, "Oracle_SampleCount_Matches");

        std::vector<std::int32_t> expected_raw(17, 0);
        expected_raw[16] = 31;  // cycle 17 -- the first cycle AFTER the toggle at cycle 16
        bool oracle_matches = true;
        for (std::size_t i = 0; i < samples.size(); ++i) {
            if (samples[i].left != expected_raw[i] || samples[i].right != expected_raw[i]) {
                oracle_matches = false;
            }
        }
        expect(oracle_matches, "Oracle_RawStereoSequence_MatchesHandComputedExpectation_16SilentThen1Audible");

        std::vector<std::uint8_t> expected_pcm;
        expected_pcm.reserve(samples.size() * 4);
        for (const auto& s : samples) {
            const auto left = static_cast<std::uint16_t>(psg_raw_sum_to_pcm16(s.left));
            const auto right = static_cast<std::uint16_t>(psg_raw_sum_to_pcm16(s.right));
            expected_pcm.push_back(static_cast<std::uint8_t>(left & 0xFF));
            expected_pcm.push_back(static_cast<std::uint8_t>((left >> 8) & 0xFF));
            expected_pcm.push_back(static_cast<std::uint8_t>(right & 0xFF));
            expected_pcm.push_back(static_cast<std::uint8_t>((right >> 8) & 0xFF));
        }
        const std::string expected_pcm_section =
            sony_msx::machine::debug_dump::serialize_region("PCM", expected_pcm.data(), expected_pcm.size());

        const std::string expected_full = std::string(kAudioDumpFormatTag) + "\n" +
                                           "[AUDIO] sample_rate=3579545 channels=2 bits=16 samples=17\n" +
                                           expected_pcm_section + "[END]\n";
        expect(dump == expected_full, "FullDump_MatchesHandAssembledOracle_ByteForByte");
    }

    // --- Case 3: determinism -- two identical serialize_psg_audio_dump()
    // calls (on two freshly reset+programmed PSGs) produce byte-identical
    // output (this project's own core determinism claim, re-proven here for
    // the new audio-dump capability). ---
    {
        PsgYm2149 psg_a;
        psg_a.reset();
        program_channel_a_square_wave(psg_a);
        PsgYm2149 psg_b;
        psg_b.reset();
        program_channel_a_square_wave(psg_b);

        const std::string dump_a = serialize_psg_audio_dump(psg_a, 44100, 32);
        const std::string dump_b = serialize_psg_audio_dump(psg_b, 44100, 32);
        expect(dump_a == dump_b, "Determinism_TwoIdenticalRuns_ByteIdenticalDumps");
    }

    // --- Case 4: write_psg_audio_dump() writes a real file under
    // <debug_root>/sounds/<filename>, content matching
    // serialize_psg_audio_dump() exactly (mirrors write_frame_dump's own
    // directory-creation/content-parity contract, M26-S4). ---
    {
        const std::filesystem::path temp_root =
            std::filesystem::temp_directory_path() / "sony-msx-hbf1xv-m27-psg-audio-dump-unit-test";
        std::error_code ec;
        std::filesystem::remove_all(temp_root, ec);

        PsgYm2149 psg_for_file;
        psg_for_file.reset();
        program_channel_a_square_wave(psg_for_file);
        const bool ok = write_psg_audio_dump(temp_root, "tone.dump", psg_for_file, 44100, 8);
        expect(ok, "WritePsgAudioDump_ReturnsTrue");

        const std::filesystem::path expected_path = temp_root / "sounds" / "tone.dump";
        expect(std::filesystem::exists(expected_path), "WritePsgAudioDump_CreatesFileUnderSoundsSubdir");

        std::string on_disk;
        {
            // Scoped so the file handle closes BEFORE remove_all() below.
            std::ifstream file(expected_path, std::ios::binary);
            on_disk.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        }

        PsgYm2149 psg_reference;
        psg_reference.reset();
        program_channel_a_square_wave(psg_reference);
        const std::string expected = serialize_psg_audio_dump(psg_reference, 44100, 8);
        expect(on_disk == expected, "WritePsgAudioDump_ContentMatchesSerializePsgAudioDump");

        std::filesystem::remove_all(temp_root, ec);
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_PsgAudioDump_Unit cases passed\n";
    return 0;
}
