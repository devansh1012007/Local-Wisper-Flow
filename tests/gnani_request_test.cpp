#include <doctest/doctest.h>

#include "stt/cloud/GnaniContract.h"
#include "stt/cloud/GnaniRequest.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string AsString(const std::vector<std::uint8_t>& bytes) {
    return std::string(bytes.begin(), bytes.end());
}

bool Contains(const std::vector<std::uint8_t>& haystack, const std::string& needle) {
    return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end()) !=
           haystack.end();
}

std::size_t Count(const std::string& haystack, char needle) {
    return static_cast<std::size_t>(
        std::count(haystack.begin(), haystack.end(), needle));
}

GnaniRequestOptions Options() {
    GnaniRequestOptions options;
    options.api_key_id = "test-key-id";
    options.model = "prisma-v3.0";
    options.language = "indic-auto";
    return options;
}

}  // namespace

TEST_CASE("gnani request targets the documented endpoint and headers") {
    const std::vector<float> pcm(16, 0.0f);
    const HttpRequest request = BuildGnaniRequest(pcm, 16000, Options());
    CHECK(request.host == GnaniContract::kHost);
    CHECK(request.path == GnaniContract::kPath);
    REQUIRE(request.headers.size() == 1);
    CHECK(request.headers[0].first == "X-API-Key-ID");
    CHECK(request.headers[0].second == "test-key-id");
    CHECK(request.content_type.find("multipart/form-data") == 0);
    CHECK(request.content_type.find(GnaniContract::kBoundary) != std::string::npos);
}

TEST_CASE("gnani request embeds model, language, and wav audio") {
    const std::vector<float> pcm{0.5f, -0.5f, 0.25f};
    const HttpRequest request = BuildGnaniRequest(pcm, 16000, Options());
    CHECK(Contains(request.body, "name=\"model\"\r\n\r\nprisma-v3.0"));
    CHECK(Contains(request.body, "name=\"language\"\r\n\r\nindic-auto"));
    CHECK(Contains(request.body, "name=\"audio\"; filename=\"audio.wav\""));
    CHECK(Contains(request.body, "Content-Type: audio/wav"));
    CHECK(Contains(request.body, "RIFF"));
    CHECK(Contains(request.body, "WAVE"));
    const std::string body = AsString(request.body);
    const std::string expected_tail = "\r\n--" + std::string(GnaniContract::kBoundary) + "--\r\n";
    CHECK(body.substr(body.size() - expected_tail.size()) == expected_tail);
}

TEST_CASE("gnani request omits the keyterms field when none are given") {
    const std::vector<float> pcm(4, 0.0f);
    const HttpRequest request = BuildGnaniRequest(pcm, 16000, Options());
    CHECK(!Contains(request.body, "name=\"keyterms\""));
}

TEST_CASE("gnani request caps and filters keyterms") {
    const std::vector<float> pcm(4, 0.0f);
    GnaniRequestOptions options = Options();
    for (std::size_t i = 0; i < 260; ++i) {
        options.keyterms.push_back("term" + std::to_string(i));
    }
    options.keyterms.push_back("unsafe,term");
    options.keyterms.push_back("unsafe\r\nterm");
    options.keyterms.push_back("");
    const HttpRequest request = BuildGnaniRequest(pcm, 16000, options);
    const std::string body = AsString(request.body);
    const std::size_t field = body.find("name=\"keyterms\"\r\n\r\n");
    REQUIRE(field != std::string::npos);
    const std::size_t value_begin = field + std::string("name=\"keyterms\"\r\n\r\n").size();
    const std::size_t value_end = body.find("\r\n--", value_begin);
    const std::string joined = body.substr(value_begin, value_end - value_begin);
    CHECK(Count(joined, ',') == GnaniContract::kKeytermsCap - 1);
    CHECK(joined.find("term199") != std::string::npos);
    CHECK(joined.find("term200") == std::string::npos);
    CHECK(joined.find("unsafe") == std::string::npos);
}

TEST_CASE("gnani request rejects missing credentials and settings") {
    const std::vector<float> pcm(4, 0.0f);
    GnaniRequestOptions options = Options();
    options.api_key_id = "";
    CHECK_THROWS_AS(BuildGnaniRequest(pcm, 16000, options), std::runtime_error);
    options = Options();
    options.model = "";
    CHECK_THROWS_AS(BuildGnaniRequest(pcm, 16000, options), std::runtime_error);
    options = Options();
    options.language = "";
    CHECK_THROWS_AS(BuildGnaniRequest(pcm, 16000, options), std::runtime_error);
}
