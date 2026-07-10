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

#include "frontend/psg_audio_dump.h"

#include <algorithm>
#include <fstream>
#include <system_error>
#include <vector>

#include "frontend/psg_audio_pump.h"
#include "machine/debug_dump.h"
#include "machine/debug_format.h"

namespace sony_msx::frontend {

namespace {

// X-pattern precedent (wd2793.h/disk_drive.h/rp5c01.h/rensha_turbo.h/
// sdl3_app.h/sdl3_audio_presenter.h all independently declare this same
// constant): declared locally so this file stays headless-buildable with
// zero SDL3 dependency (A-M27-7/A-M27-8) -- never pulled from an SDL3-gated
// header.
constexpr std::uint64_t kSystemClockHz = 3579545;

}  // namespace

std::int16_t psg_raw_sum_to_pcm16(const std::int32_t raw_sum) {
    // A-M27-4: PsgYm2149::sample() sums two 5-bit resolved_amplitude()
    // values (each in [0,31]) per stereo channel component, so raw_sum is in
    // [0, 62] under normal operation (psg_ym2149.cpp, re-confirmed this
    // cycle). Documented linear map: 0 -> -32768, 62 -> +32767. Clamped
    // (never overflowed) for any out-of-range input.
    constexpr std::int32_t kMaxRaw = 62;
    constexpr std::int32_t kPcmRange = 65535;
    constexpr std::int32_t kPcmOffset = 32768;

    const std::int32_t clamped = std::clamp(raw_sum, 0, kMaxRaw);
    const std::int32_t scaled = (clamped * kPcmRange) / kMaxRaw - kPcmOffset;
    return static_cast<std::int16_t>(scaled);
}

std::string serialize_psg_audio_dump(devices::audio::PsgYm2149& psg, const std::uint64_t sample_rate_hz,
                                      const std::size_t sample_count) {
    const std::uint64_t cycles_per_sample = sample_rate_hz == 0 ? 1 : std::max<std::uint64_t>(1, kSystemClockHz / sample_rate_hz);
    const PsgAudioPump pump(cycles_per_sample);

    std::vector<std::uint8_t> pcm;
    pcm.reserve(sample_count * 4);
    for (std::size_t i = 0; i < sample_count; ++i) {
        const devices::audio::PsgYm2149::StereoSample sample = pump.pump_one_sample(psg);
        const auto left = static_cast<std::uint16_t>(psg_raw_sum_to_pcm16(sample.left));
        const auto right = static_cast<std::uint16_t>(psg_raw_sum_to_pcm16(sample.right));
        // Little-endian, per the header doc comment.
        pcm.push_back(static_cast<std::uint8_t>(left & 0xFF));
        pcm.push_back(static_cast<std::uint8_t>((left >> 8) & 0xFF));
        pcm.push_back(static_cast<std::uint8_t>(right & 0xFF));
        pcm.push_back(static_cast<std::uint8_t>((right >> 8) & 0xFF));
    }

    std::string out;
    out += kAudioDumpFormatTag;
    out.push_back('\n');
    out += "[AUDIO] sample_rate=" + machine::debug_format::to_dec(sample_rate_hz) + " channels=2 bits=16 samples=" +
           machine::debug_format::to_dec(static_cast<std::uint64_t>(sample_count)) + "\n";
    out += machine::debug_dump::serialize_region("PCM", pcm.empty() ? nullptr : pcm.data(), pcm.size());
    out += "[END]\n";
    return out;
}

bool write_psg_audio_dump(const std::filesystem::path& debug_root, const std::string& filename,
                           devices::audio::PsgYm2149& psg, const std::uint64_t sample_rate_hz,
                           const std::size_t sample_count) {
    const std::string text = serialize_psg_audio_dump(psg, sample_rate_hz, sample_count);

    const std::filesystem::path directory = debug_root / "sounds";
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        return false;
    }

    // Binary mode so '\n' is written verbatim (no CRLF translation) --
    // mirrors Hbf1xvMachine::write_text_file()'s exact discipline.
    std::ofstream file(directory / filename, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(file);
}

}  // namespace sony_msx::frontend
