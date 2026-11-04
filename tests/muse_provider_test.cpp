#include <doctest/doctest.h>

#include "stt/cloud/MuseContract.h"
#include "stt/cloud/MuseProvider.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct ScriptedResponse {
    int status = 200;
    std::string body =
        R"({"sessionId":"sess-1","transcript":"clip","audioDurationMs":1600,"turns":[]})";
    bool throw_timeout = false;
    bool throw_network = false;
};

class FakeTransport final : public IHttpTransport {
public:
    explicit FakeTransport(std::vector<ScriptedResponse> script) : script_(std::move(script)) {}

    HttpResponse Post(const HttpRequest& request) override {
        requests_.push_back(request);
        if (index_ >= script_.size()) {
            throw std::runtime_error("FakeTransport: script exhausted");
        }
        const ScriptedResponse& step = script_[index_++];
        if (step.throw_timeout) {
            throw HttpTransportError(HttpTransportErrorKind::Timeout, "timed out");
        }
        if (step.throw_network) {
            throw HttpTransportError(HttpTransportErrorKind::Network, "connection reset");
        }
        HttpResponse response;
        response.status = step.status;
        response.body.assign(step.body.begin(), step.body.end());
        return response;
    }

    const std::vector<HttpRequest>& Requests() const {
        return requests_;
    }

private:
    std::vector<ScriptedResponse> script_;
    std::vector<HttpRequest> requests_;
    std::size_t index_ = 0;
};

MuseProviderSettings Settings() {
    MuseProviderSettings settings;
    settings.api_key = "muse-secret-key";
    settings.max_clips = 40;
    return settings;
}

std::vector<float> ConstantAudio(std::size_t samples) {
    return std::vector<float>(samples, 0.3f);
}

}  // namespace

TEST_CASE("muse provider sends the documented two-part request") {
    FakeTransport transport({ScriptedResponse()});
    MuseProvider provider(transport, Settings());
    provider.Transcribe(ConstantAudio(1600), {});
    REQUIRE(transport.Requests().size() == 1);
    const HttpRequest& request = transport.Requests()[0];
    CHECK(request.host == MuseContract::kHost);
    CHECK(request.path == MuseContract::kPath);
    REQUIRE(request.headers.size() == 1);
    CHECK(request.headers[0].first == "Authorization");
    CHECK(request.headers[0].second == "Bearer muse-secret-key");
    CHECK(request.content_type.find("multipart/form-data") == 0);
    const std::string body(request.body.begin(), request.body.end());
    const std::string request_header =
        "Content-Disposition: form-data; name=\"request\"\r\n"
        "Content-Type: application/json\r\n\r\n";
    const std::size_t request_at = body.find(request_header);
    REQUIRE(request_at != std::string::npos);
    const std::string request_json = body.substr(request_at + request_header.size());
    CHECK(request_json.find("\"mode\":\"PUSH_TO_TALK\"") != std::string::npos);
    CHECK(request_json.find("\"model\":\"muse-voice-transcribe-1.0\"") != std::string::npos);
    CHECK(request_json.find("\"audioEncoding\":\"WAV\"") != std::string::npos);
    CHECK(request_json.find("languageBias") == std::string::npos);
    CHECK(request_json.find("keywords") == std::string::npos);
    const std::string audio_header =
        "Content-Disposition: form-data; name=\"audio\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n";
    const std::size_t audio_at = body.find(audio_header);
    REQUIRE(audio_at != std::string::npos);
    CHECK(body.compare(audio_at + audio_header.size(), 4, "RIFF") == 0);
}

TEST_CASE("muse provider sends language bias and keywords when configured") {
    FakeTransport transport({ScriptedResponse()});
    MuseProviderSettings settings = Settings();
    settings.language_bias = {"Kannada", "Hindi"};
    settings.keywords = {"Acme Mobile", "eSIM", ""};
    MuseProvider provider(transport, settings);
    provider.Transcribe(ConstantAudio(1600), {});
    REQUIRE(transport.Requests().size() == 1);
    const std::string body(transport.Requests()[0].body.begin(),
                           transport.Requests()[0].body.end());
    CHECK(body.find("\"languageBias\":[\"Kannada\",\"Hindi\"]") != std::string::npos);
    CHECK(body.find("\"keywords\":[\"Acme Mobile\",\"eSIM\"]") != std::string::npos);
}

TEST_CASE("muse provider transcribes one clip and records the session id") {
    FakeTransport transport({ScriptedResponse()});
    MuseProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe(ConstantAudio(16000), {});
    CHECK(result.success);
    CHECK(result.text == "clip");
    REQUIRE(result.attempts.size() == 1);
    CHECK(result.attempts[0].provider == "muse");
    CHECK(result.attempts[0].http_status == 200);
    CHECK(result.attempts[0].request_id == "sess-1");
    CHECK(result.attempts[0].bytes_sent > 0);
    CHECK(result.attempts[0].bytes_received > 0);
}

TEST_CASE("muse provider classifies vendor size and format errors") {
    ScriptedResponse too_large;
    too_large.status = 413;
    too_large.body = R"({"message":"request body exceeds 32 MB"})";
    FakeTransport large_transport({too_large});
    MuseProvider large_provider(large_transport, Settings());
    const SttTranscription large_result = large_provider.Transcribe(ConstantAudio(16000), {});
    CHECK(!large_result.success);
    CHECK(large_result.error == SttErrorCategory::PayloadTooLarge);
    CHECK(large_result.attempts[0].detail.find("32 MB") != std::string::npos);

    ScriptedResponse bad_format;
    bad_format.status = 400;
    bad_format.body = R"({"message":"audio is not a supported WAV file"})";
    FakeTransport format_transport({bad_format});
    MuseProvider format_provider(format_transport, Settings());
    const SttTranscription format_result = format_provider.Transcribe(ConstantAudio(16000), {});
    CHECK(!format_result.success);
    CHECK(format_result.error == SttErrorCategory::InvalidRequest);
}

TEST_CASE("muse provider maps transport timeouts and network failures") {
    ScriptedResponse timeout_step;
    timeout_step.throw_timeout = true;
    FakeTransport timeout_transport({timeout_step});
    MuseProvider timeout_provider(timeout_transport, Settings());
    const SttTranscription timeout_result = timeout_provider.Transcribe(ConstantAudio(16000), {});
    CHECK(!timeout_result.success);
    CHECK(timeout_result.error == SttErrorCategory::Timeout);

    ScriptedResponse network_step;
    network_step.throw_network = true;
    FakeTransport network_transport({network_step});
    MuseProvider network_provider(network_transport, Settings());
    const SttTranscription network_result = network_provider.Transcribe(ConstantAudio(16000), {});
    CHECK(!network_result.success);
    CHECK(network_result.error == SttErrorCategory::Network);
}

TEST_CASE("muse provider splits audio past the ten-minute vendor cap") {
    FakeTransport transport({ScriptedResponse{200, R"({"transcript":"one","sessionId":"s1"})"},
                             ScriptedResponse{200, R"({"transcript":"two","sessionId":"s2"})"}});
    MuseProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe(ConstantAudio(9700000), {});
    CHECK(result.success);
    CHECK(result.text == "one two");
    CHECK(transport.Requests().size() == 2);
    REQUIRE(result.attempts.size() == 2);
    CHECK(result.attempts[1].request_id == "s2");
}

TEST_CASE("muse provider aborts remaining clips after an error status") {
    FakeTransport transport(
        {ScriptedResponse{429, R"({"message":"tenant rate limit reached"})"}});
    MuseProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe(ConstantAudio(9700000), {});
    CHECK(!result.success);
    CHECK(result.error == SttErrorCategory::RateLimited);
    CHECK(result.text.empty());
    CHECK(transport.Requests().size() == 1);
}

TEST_CASE("muse provider redacts the api key from failure details") {
    ScriptedResponse echo;
    echo.status = 401;
    echo.body = R"json({"message":"bad bearer muse-secret-key token"})json";
    FakeTransport transport({echo});
    MuseProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe(ConstantAudio(16000), {});
    CHECK(!result.success);
    REQUIRE(result.attempts.size() == 1);
    CHECK(result.attempts[0].detail.find("muse-secret-key") == std::string::npos);
    CHECK(result.attempts[0].detail.find("[redacted]") != std::string::npos);
}

TEST_CASE("muse provider treats an empty transcript as success") {
    FakeTransport transport({ScriptedResponse{200, R"({"transcript":"","sessionId":"s"})"}});
    MuseProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe(ConstantAudio(16000), {});
    CHECK(result.success);
    CHECK(result.text.empty());
    CHECK(result.error == SttErrorCategory::None);
}

TEST_CASE("muse provider honors cancellation between clips") {
    FakeTransport transport({ScriptedResponse{200, R"({"transcript":"one","sessionId":"s1"})"}});
    MuseProvider provider(transport, Settings());
    std::size_t checks = 0;
    const SttCancelCheck cancel = [&checks] { return ++checks >= 2; };
    const SttTranscription result = provider.Transcribe(ConstantAudio(9700000), cancel);
    CHECK(result.cancelled);
    CHECK(result.error == SttErrorCategory::Cancelled);
    CHECK(result.text.empty());
    CHECK(transport.Requests().size() == 1);
}

TEST_CASE("muse provider skips the transport for empty audio") {
    FakeTransport transport({});
    MuseProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe({}, {});
    CHECK(result.success);
    CHECK(result.text.empty());
    CHECK(transport.Requests().empty());
}
