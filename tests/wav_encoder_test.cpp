#include <doctest/doctest.h>

#include "stt/cloud/WavEncoder.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::uint16_t ReadU16(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset]) |
                                      (static_cast<std::uint16_t>(bytes[offset + 1]) << 8));
}

std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::string TagAt(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return std::string(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                       bytes.begin() + static_cast<std::ptrdiff_t>(offset + 4));
}

std::int16_t PcmAt(const std::vector<std::uint8_t>& wav, std::size_t sample_index) {
    return static_cast<std::int16_t>(ReadU16(wav, 44 + sample_index * 2));
}

}  // namespace

TEST_CASE("wav encoder writes a canonical 44 byte header") {
    const std::vector<float> pcm{0.0f, 0.5f, -0.5f, 1.0f};
    const std::vector<std::uint8_t> wav = EncodeWav16Mono(pcm, 16000);
    REQUIRE(wav.size() == 52);
    CHECK(TagAt(wav, 0) == "RIFF");
    CHECK(TagAt(wav, 8) == "WAVE");
    CHECK(TagAt(wav, 12) == "fmt ");
    CHECK(TagAt(wav, 36) == "data");
    CHECK(ReadU32(wav, 4) == 44);
    CHECK(ReadU32(wav, 16) == 16);
    CHECK(ReadU16(wav, 20) == 1);
    CHECK(ReadU16(wav, 22) == 1);
    CHECK(ReadU32(wav, 24) == 16000);
    CHECK(ReadU32(wav, 28) == 32000);
    CHECK(ReadU16(wav, 32) == 2);
    CHECK(ReadU16(wav, 34) == 16);
    CHECK(ReadU32(wav, 40) == 8);
}

TEST_CASE("wav encoder roundtrips sample values") {
    const std::vector<float> pcm{0.0f, 0.5f, -0.5f, 1.0f, -1.0f, 0.25f};
    const std::vector<std::uint8_t> wav = EncodeWav16Mono(pcm, 16000);
    CHECK(PcmAt(wav, 0) == 0);
    CHECK(PcmAt(wav, 1) == 16384);
    CHECK(PcmAt(wav, 2) == -16384);
    CHECK(PcmAt(wav, 3) == 32767);
    CHECK(PcmAt(wav, 4) == -32767);
    CHECK(PcmAt(wav, 5) == 8192);
}

TEST_CASE("wav encoder clamps out of range samples") {
    const std::vector<float> pcm{2.0f, -2.0f, 100.0f, -100.0f};
    const std::vector<std::uint8_t> wav = EncodeWav16Mono(pcm, 16000);
    CHECK(PcmAt(wav, 0) == 32767);
    CHECK(PcmAt(wav, 1) == -32767);
    CHECK(PcmAt(wav, 2) == 32767);
    CHECK(PcmAt(wav, 3) == -32767);
}

TEST_CASE("wav encoder accepts empty audio") {
    const std::vector<float> pcm;
    const std::vector<std::uint8_t> wav = EncodeWav16Mono(pcm, 16000);
    CHECK(wav.size() == 44);
    CHECK(ReadU32(wav, 4) == 36);
    CHECK(ReadU32(wav, 40) == 0);
}

TEST_CASE("wav encoder rejects a zero sample rate") {
    const std::vector<float> pcm{0.5f};
    CHECK_THROWS_AS(EncodeWav16Mono(pcm, 0), std::runtime_error);
}
