#include <doctest/doctest.h>

#include "stt/cloud/MultipartBuilder.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string AsString(const std::vector<std::uint8_t>& bytes) {
    return std::string(bytes.begin(), bytes.end());
}

std::size_t Find(const std::vector<std::uint8_t>& haystack, const std::string& needle,
                 std::size_t from = 0) {
    const auto it = std::search(haystack.begin() + static_cast<std::ptrdiff_t>(from),
                                haystack.end(), needle.begin(), needle.end());
    return it == haystack.end() ? std::string::npos
                                : static_cast<std::size_t>(it - haystack.begin());
}

const std::vector<std::uint8_t> kBinaryPayload{0x01, 0x00, 0xFF, 0x0D, 0x0A, 0x00, 0xFE, 0x7F};
const std::string kTerminator = "\r\n--BOUND--\r\n";

}  // namespace

TEST_CASE("multipart builder emits fields in canonical form") {
    MultipartBuilder builder("BOUND");
    builder.AddField("model", "prisma-v3.0");
    const std::vector<std::uint8_t> body = builder.Build();
    CHECK(AsString(body) ==
          "--BOUND\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\nprisma-v3.0\r\n"
          "--BOUND--\r\n");
}

TEST_CASE("multipart builder emits file parts with content type and payload") {
    MultipartBuilder builder("BOUND");
    builder.AddField("language", "indic-auto");
    builder.AddFile("audio", "audio.wav", "audio/wav", kBinaryPayload);
    const std::vector<std::uint8_t> body = builder.Build();
    const std::string expected_field =
        "--BOUND\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\nindic-auto\r\n";
    const std::string expected_header =
        "--BOUND\r\nContent-Disposition: form-data; name=\"audio\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n";
    const std::size_t field_offset = Find(body, expected_field);
    const std::size_t header_offset = Find(body, expected_header);
    REQUIRE(field_offset == 0);
    REQUIRE(header_offset == expected_field.size());
    const std::size_t payload_offset = header_offset + expected_header.size();
    REQUIRE(body.size() == payload_offset + kBinaryPayload.size() + kTerminator.size());
    for (std::size_t i = 0; i < kBinaryPayload.size(); ++i) {
        CHECK(body[payload_offset + i] == kBinaryPayload[i]);
    }
    CHECK(AsString(std::vector<std::uint8_t>(body.end() - static_cast<std::ptrdiff_t>(kTerminator.size()),
                                            body.end())) == kTerminator);
}

TEST_CASE("multipart builder keeps binary payloads byte exact") {
    MultipartBuilder builder("BOUND");
    builder.AddFile("audio", "clip.wav", "audio/wav", kBinaryPayload);
    const std::vector<std::uint8_t> body = builder.Build();
    const std::string header =
        "--BOUND\r\nContent-Disposition: form-data; name=\"audio\"; filename=\"clip.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n";
    const std::size_t payload_offset = Find(body, header) + header.size();
    for (std::size_t i = 0; i < kBinaryPayload.size(); ++i) {
        CHECK(body[payload_offset + i] == kBinaryPayload[i]);
    }
}

TEST_CASE("multipart builder rejects header and value injection") {
    CHECK_THROWS_AS(MultipartBuilder builder(""), std::runtime_error);
    CHECK_THROWS_AS(MultipartBuilder builder("bad\r\nboundary"), std::runtime_error);
    MultipartBuilder builder("BOUND");
    CHECK_THROWS_AS(builder.AddField("name\"; evil", "v"), std::runtime_error);
    CHECK_THROWS_AS(builder.AddField("model", "v\r\nX-Evil: 1"), std::runtime_error);
    CHECK_THROWS_AS(builder.AddField("model", "v--BOUND--"), std::runtime_error);
    CHECK_THROWS_AS(builder.AddFile("audio", "a.wav\r\nEvil", "audio/wav", {}), std::runtime_error);
    CHECK_THROWS_AS(builder.AddFile("audio", "a.wav", "audio/wav \"x\"", {}), std::runtime_error);
}

TEST_CASE("multipart builder with no parts emits only the terminator") {
    MultipartBuilder builder("BOUND");
    const std::vector<std::uint8_t> body = builder.Build();
    CHECK(AsString(body) == "--BOUND--\r\n");
}

TEST_CASE("multipart builder content type carries the boundary") {
    MultipartBuilder builder("lwf-abc123");
    CHECK(builder.ContentType() == "multipart/form-data; boundary=lwf-abc123");
}
