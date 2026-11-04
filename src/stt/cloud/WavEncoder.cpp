#include "stt/cloud/WavEncoder.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

void PutU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

void PutU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
}

}  // namespace

std::vector<std::uint8_t> EncodeWav16Mono(const std::vector<float>& pcm, std::uint32_t sample_rate) {
    const std::uint32_t data_bytes = static_cast<std::uint32_t>(pcm.size()) * 2u;
    std::vector<std::uint8_t> out;
    out.reserve(44 + data_bytes);

    out.insert(out.end(), {'R', 'I', 'F', 'F'});
    PutU32(out, 36u + data_bytes);
    out.insert(out.end(), {'W', 'A', 'V', 'E'});
    out.insert(out.end(), {'f', 'm', 't', ' '});
    PutU32(out, 16u);                     // fmt chunk size
    PutU16(out, 1u);                      // PCM
    PutU16(out, 1u);                      // mono
    PutU32(out, sample_rate);
    PutU32(out, sample_rate * 2u);        // byte rate (mono, 16-bit)
    PutU16(out, 2u);                      // block align
    PutU16(out, 16u);                     // bits per sample
    out.insert(out.end(), {'d', 'a', 't', 'a'});
    PutU32(out, data_bytes);

    for (const float sample : pcm) {
        const float clamped = std::isfinite(sample)
                                  ? std::max(-1.0f, std::min(1.0f, sample))
                                  : 0.0f;
        const auto scaled = static_cast<int>(clamped * 32767.0f);
        PutU16(out, static_cast<std::uint16_t>(scaled));
    }
    return out;
}
