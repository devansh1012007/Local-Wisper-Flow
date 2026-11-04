#include <doctest/doctest.h>

#include "stt/cloud/ElevenLabsContract.h"
#include "stt/cloud/ElevenLabsProvider.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct ScriptedResponse {
    int status = 200;
    std::string body = R"({"language_code":"en","language_probability":0.98,"text":"clip"})";
    std::string request_id_header = "req-id";
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
        if (!step.request_id_header.empty()) {
            response.headers.emplace_back("X-Request-Id", step.request_id_header);
        }
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

ElevenLabsProviderSettings Settings() {
    ElevenLabsProviderSettings settings;
    settings.api_key = "xi-secret-key";
    settings.max_clips = 40;
    return settings;
}

std::vector<float> ConstantAudio(std::size_t samples) {
    return std::vector<float>(samples, 0.3f);
}

std::size_t CountParts(const HttpRequest& request, const std::string& name) {
    const std::string body(request.body.begin(), request.body.end());
    const std::string needle = "name=\"" + name + "\"";
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = body.find(needle, pos)) != std::string::npos) {
        ++count;
        ++pos;
    }
    return count;
}

}  // namespace

TEST_CASE("elevenlabs provider transcribes one clip through the transport") {
    FakeTransport transport({ScriptedResponse()});
    ElevenLabsProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe(ConstantAudio(16000), {});
    CHECK(result.success);
    CHECK(result.text == "clip");
    REQUIRE(result.attempts.size() == 1);
    CHECK(result.attempts[0].provider == "elevenlabs");
    CHECK(result.attempts[0].http_status == 200);
    CHECK(result.attempts[0].request_id == "req-id");
    CHECK(result.attempts[0].bytes_sent > 0);
    CHECK(result.attempts[0].bytes_received > 0);
}

TEST_CASE("elevenlabs provider sends the documented request shape") {
    FakeTransport transport({ScriptedResponse()});
    ElevenLabsProvider provider(transport, Settings());
    provider.Transcribe(ConstantAudio(1600), {});
    REQUIRE(transport.Requests().size() == 1);
    const HttpRequest& request = transport.Requests()[0];
    CHECK(request.host == ElevenLabsContract::kHost);
    CHECK(request.path == ElevenLabsContract::kPath);
    REQUIRE(request.headers.size() == 1);
    CHECK(request.headers[0].first == "xi-api-key");
    CHECK(request.headers[0].second == "xi-secret-key");
    CHECK(request.content_type.find("multipart/form-data") == 0);
    const std::string body(request.body.begin(), request.body.end());
    CHECK(body.find("name=\"model_id\"\r\n\r\nscribe_v2") != std::string::npos);
    CHECK(body.find("language_code") == std::string::npos);
    CHECK(body.find("keyterms") == std::string::npos);
    const std::string audio_header =
        "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n";
    const std::size_t audio_at = body.find(audio_header);
    REQUIRE(audio_at != std::string::npos);
    CHECK(body.compare(audio_at + audio_header.size(), 4, "RIFF") == 0);
}

TEST_CASE("elevenlabs provider sends language and keyterms when configured") {
    FakeTransport transport({ScriptedResponse()});
    ElevenLabsProviderSettings settings = Settings();
    settings.language_code = "kn";
    settings.keyterms = {"Acme", "bad,key", "ok term"};
    ElevenLabsProvider provider(transport, settings);
    provider.Transcribe(ConstantAudio(1600), {});
    REQUIRE(transport.Requests().size() == 1);
    const std::string body(transport.Requests()[0].body.begin(),
                           transport.Requests()[0].body.end());
    CHECK(body.find("name=\"language_code\"\r\n\r\nkn") != std::string::npos);
    CHECK(CountParts(transport.Requests()[0], "keyterms") == 2);
    CHECK(body.find("bad,key") == std::string::npos);
}

TEST_CASE("elevenlabs provider drops over-long and over-word keyterms") {
    FakeTransport transport({ScriptedResponse()});
    ElevenLabsProviderSettings settings = Settings();
    settings.keyterms = {std::string(51, 'a'), "one two three four five six",
                         "one two three four five"};
    ElevenLabsProvider provider(transport, settings);
    provider.Transcribe(ConstantAudio(1600), {});
    REQUIRE(transport.Requests().size() == 1);
    const std::string body(transport.Requests()[0].body.begin(),
                           transport.Requests()[0].body.end());
    CHECK(CountParts(transport.Requests()[0], "keyterms") == 1);
    CHECK(body.find("one two three four five\r") != std::string::npos);
}

TEST_CASE("elevenlabs provider splits long audio and joins transcripts") {
    FakeTransport transport({ScriptedResponse{200, R"({"text":"one"})", "r1"},
                             ScriptedResponse{200, R"({"text":"two"})", "r2"},
                             ScriptedResponse{200, R"({"text":"three"})", "r3"}});
    ElevenLabsProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe(ConstantAudio(1040000), {});
    CHECK(result.success);
    CHECK(result.text == "one two three");
    CHECK(transport.Requests().size() == 3);
    REQUIRE(result.attempts.size() == 3);
    CHECK(result.attempts[0].request_id == "r1");
    CHECK(result.attempts[2].request_id == "r3");
}

TEST_CASE("elevenlabs provider maps transport timeouts and network failures") {
    ScriptedResponse timeout_step;
    timeout_step.throw_timeout = true;
    FakeTransport timeout_transport({timeout_step});
    ElevenLabsProvider timeout_provider(timeout_transport, Settings());
    const SttTranscription timeout_result = timeout_provider.Transcribe(ConstantAudio(16000), {});
    CHECK(!timeout_result.success);
    CHECK(timeout_result.error == SttErrorCategory::Timeout);
    CHECK(timeout_result.text.empty());

    ScriptedResponse network_step;
    network_step.throw_network = true;
    FakeTransport network_transport({network_step});
    ElevenLabsProvider network_provider(network_transport, Settings());
    const SttTranscription network_result = network_provider.Transcribe(ConstantAudio(16000), {});
    CHECK(!network_result.success);
    CHECK(network_result.error == SttErrorCategory::Network);
}

TEST_CASE("elevenlabs provider classifies error statuses and keeps detail") {
    ScriptedResponse unprocessable;
    unprocessable.status = 422;
    unprocessable.body = R"json({"detail":{"status":"invalid_audio","message":"bad audio"}})json";
    FakeTransport transport({unprocessable});
    ElevenLabsProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe(ConstantAudio(16000), {});
    CHECK(!result.success);
    CHECK(result.error == SttErrorCategory::InvalidRequest);
    REQUIRE(result.attempts.size() == 1);
    CHECK(result.attempts[0].detail.find("invalid_audio") != std::string::npos);
    CHECK(result.attempts[0].detail.find("bad audio") != std::string::npos);
}

TEST_CASE("elevenlabs provider aborts remaining clips after an error status") {
    FakeTransport transport({ScriptedResponse{401, R"({"detail":"key rejected"})"}});
    ElevenLabsProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe(ConstantAudio(1040000), {});
    CHECK(!result.success);
    CHECK(result.error == SttErrorCategory::Auth);
    CHECK(result.text.empty());
    CHECK(transport.Requests().size() == 1);
}

TEST_CASE("elevenlabs provider redacts the api key from failure details") {
    ScriptedResponse echo;
    echo.status = 401;
    echo.body = R"json({"detail":"rejected xi-api-key xi-secret-key"})json";
    FakeTransport transport({echo});
    ElevenLabsProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe(ConstantAudio(16000), {});
    CHECK(!result.success);
    REQUIRE(result.attempts.size() == 1);
    CHECK(result.attempts[0].detail.find("xi-secret-key") == std::string::npos);
    CHECK(result.attempts[0].detail.find("[redacted]") != std::string::npos);
}

TEST_CASE("elevenlabs provider treats an empty transcript as success") {
    FakeTransport transport({ScriptedResponse{200, R"({"text":""})"}});
    ElevenLabsProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe(ConstantAudio(16000), {});
    CHECK(result.success);
    CHECK(result.text.empty());
    CHECK(result.error == SttErrorCategory::None);
}

TEST_CASE("elevenlabs provider honors cancellation between clips") {
    FakeTransport transport({ScriptedResponse{200, R"({"text":"one"})"}});
    ElevenLabsProvider provider(transport, Settings());
    std::size_t checks = 0;
    const SttCancelCheck cancel = [&checks] { return ++checks >= 2; };
    const SttTranscription result = provider.Transcribe(ConstantAudio(1040000), cancel);
    CHECK(result.cancelled);
    CHECK(result.error == SttErrorCategory::Cancelled);
    CHECK(result.text.empty());
    CHECK(transport.Requests().size() == 1);
}

TEST_CASE("elevenlabs provider stops after the configured clip budget") {
    FakeTransport transport({ScriptedResponse{200, R"({"text":"one"})"},
                             ScriptedResponse{200, R"({"text":"two"})"}});
    ElevenLabsProviderSettings settings = Settings();
    settings.max_clips = 1;
    ElevenLabsProvider provider(transport, settings);
    const SttTranscription result = provider.Transcribe(ConstantAudio(1040000), {});
    CHECK(result.success);
    CHECK(result.text == "one");
    CHECK(transport.Requests().size() == 1);
}

TEST_CASE("elevenlabs provider skips the transport for empty audio") {
    FakeTransport transport({});
    ElevenLabsProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe({}, {});
    CHECK(result.success);
    CHECK(result.text.empty());
    CHECK(transport.Requests().empty());
}
