#include "stt/cloud/WavEncoder.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

void AppendU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

void AppendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

void AppendTag(std::vector<std::uint8_t>& out, const char* tag) {
    out.insert(out.end(), tag, tag + 4);
}

std::int16_t FloatToPcm16(float sample) {
    const float clamped = std::min(1.0f, std::max(-1.0f, sample));
    return static_cast<std::int16_t>(std::lroundf(clamped * 32767.0f));
}

}  // namespace

std::vector<std::uint8_t> EncodeWav16Mono(const std::vector<float>& audio_pcm,
                                          std::uint32_t sample_rate) {
    if (sample_rate == 0) {
        throw std::runtime_error("WavEncoder: sample rate must be positive");
    }
    if (audio_pcm.size() > (0x7FFFFFFFull - 44ull) / 2ull) {
        throw std::runtime_error("WavEncoder: audio too large for a RIFF file");
    }
    const std::size_t data_size = audio_pcm.size() * 2;
    std::vector<std::uint8_t> out;
    out.reserve(44 + data_size);
    AppendTag(out, "RIFF");
    AppendU32(out, static_cast<std::uint32_t>(36 + data_size));
    AppendTag(out, "WAVE");
    AppendTag(out, "fmt ");
    AppendU32(out, 16);
    AppendU16(out, 1);
    AppendU16(out, 1);
    AppendU32(out, sample_rate);
    AppendU32(out, sample_rate * 2);
    AppendU16(out, 2);
    AppendU16(out, 16);
    AppendTag(out, "data");
    AppendU32(out, static_cast<std::uint32_t>(data_size));
    for (float sample : audio_pcm) {
        AppendU16(out, static_cast<std::uint16_t>(FloatToPcm16(sample)));
    }
    return out;
}
