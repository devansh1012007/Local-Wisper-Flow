#include <doctest/doctest.h>

#include "stt/cloud/GnaniProvider.h"
#include "stt/cloud/GnaniContract.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct ScriptedResponse {
    int status = 200;
    std::string body = R"({"transcript":"clip"})";
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

GnaniProviderSettings Settings() {
    GnaniProviderSettings settings;
    settings.api_key_id = "key-id-1";
    settings.max_clips = 40;
    return settings;
}

std::vector<float> ConstantAudio(std::size_t samples) {
    return std::vector<float>(samples, 0.3f);
}

}  // namespace

TEST_CASE("provider transcribes one clip through the transport") {
    FakeTransport transport({ScriptedResponse()});
    GnaniProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe(ConstantAudio(16000), {});
    CHECK(result.success);
    CHECK(result.text == "clip");
    REQUIRE(result.attempts.size() == 1);
    CHECK(result.attempts[0].provider == "gnani");
    CHECK(result.attempts[0].http_status == 200);
    CHECK(result.attempts[0].request_id == "req-id");
    CHECK(result.attempts[0].bytes_sent > 0);
    CHECK(result.attempts[0].bytes_received > 0);
    CHECK(result.attempts[0].latency.count() >= 0);
}

TEST_CASE("provider sends the documented request shape") {
    FakeTransport transport({ScriptedResponse()});
    GnaniProvider provider(transport, Settings());
    provider.Transcribe(ConstantAudio(1600), {});
    REQUIRE(transport.Requests().size() == 1);
    const HttpRequest& request = transport.Requests()[0];
    CHECK(request.host == GnaniContract::kHost);
    CHECK(request.path == GnaniContract::kPath);
    REQUIRE(request.headers.size() == 1);
    CHECK(request.headers[0].first == GnaniContract::kApiKeyHeader);
    CHECK(request.headers[0].second == "key-id-1");
    CHECK(request.content_type.find("multipart/form-data") == 0);
    CHECK(request.timeout_ms == Settings().timeout_ms);
    const std::string body(request.body.begin(), request.body.end());
    CHECK(body.find("name=\"model\"\r\n\r\nprisma-v3.0") != std::string::npos);
    CHECK(body.find("name=\"language\"\r\n\r\nindic-auto") != std::string::npos);
    const std::string audio_header =
        "Content-Disposition: form-data; name=\"audio\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n";
    const std::size_t audio_at = body.find(audio_header);
    REQUIRE(audio_at != std::string::npos);
    CHECK(body.compare(audio_at + audio_header.size(), 4, "RIFF") == 0);
}

TEST_CASE("provider splits long audio into clip requests and joins transcripts") {
    FakeTransport transport({ScriptedResponse{200, R"({"transcript":"one"})", "r1"},
                             ScriptedResponse{200, R"({"transcript":"two"})", "r2"},
                             ScriptedResponse{200, R"({"transcript":"three"})", "r3"}});
    GnaniProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe(ConstantAudio(1040000), {});
    CHECK(result.success);
    CHECK(result.text == "one two three");
    CHECK(transport.Requests().size() == 3);
    REQUIRE(result.attempts.size() == 3);
    CHECK(result.attempts[0].request_id == "r1");
    CHECK(result.attempts[2].request_id == "r3");
}

TEST_CASE("provider maps transport timeouts and network failures") {
    ScriptedResponse timeout_step;
    timeout_step.throw_timeout = true;
    FakeTransport timeout_transport({timeout_step});
    GnaniProvider timeout_provider(timeout_transport, Settings());
    SttTranscription result = timeout_provider.Transcribe(ConstantAudio(16000), {});
    CHECK(!result.success);
    CHECK(result.error == SttErrorCategory::Timeout);
    CHECK(result.text.empty());

    ScriptedResponse network_step;
    network_step.throw_network = true;
    FakeTransport network_transport({network_step});
    GnaniProvider network_provider(network_transport, Settings());
    result = network_provider.Transcribe(ConstantAudio(16000), {});
    CHECK(!result.success);
    CHECK(result.error == SttErrorCategory::Network);

    FakeTransport generic_transport({});
    GnaniProvider generic_provider(generic_transport, Settings());
    result = generic_provider.Transcribe(ConstantAudio(16000), {});
    CHECK(!result.success);
    CHECK(result.error == SttErrorCategory::Network);
}

TEST_CASE("provider aborts remaining clips after an error status") {
    FakeTransport transport({ScriptedResponse{401, R"({"error":"key rejected"})", "r1"}});
    GnaniProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe(ConstantAudio(1040000), {});
    CHECK(!result.success);
    CHECK(result.error == SttErrorCategory::Auth);
    CHECK(result.text.empty());
    CHECK(transport.Requests().size() == 1);
}

TEST_CASE("provider treats an empty transcript as success") {
    FakeTransport transport({ScriptedResponse{200, R"({"transcript":""})"}});
    GnaniProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe(ConstantAudio(16000), {});
    CHECK(result.success);
    CHECK(result.text.empty());
    CHECK(result.error == SttErrorCategory::None);
}

TEST_CASE("provider honors cancellation between clips") {
    FakeTransport transport({ScriptedResponse{200, R"({"transcript":"one"})"}});
    GnaniProvider provider(transport, Settings());
    std::size_t checks = 0;
    const SttCancelCheck cancel = [&checks] { return ++checks >= 2; };
    const SttTranscription result = provider.Transcribe(ConstantAudio(1040000), cancel);
    CHECK(result.cancelled);
    CHECK(result.error == SttErrorCategory::Cancelled);
    CHECK(result.text.empty());
    CHECK(transport.Requests().size() == 1);
}

TEST_CASE("provider stops after the configured clip budget") {
    FakeTransport transport({ScriptedResponse{200, R"({"transcript":"one"})"},
                             ScriptedResponse{200, R"({"transcript":"two"})"}});
    GnaniProviderSettings settings = Settings();
    settings.max_clips = 2;
    GnaniProvider provider(transport, settings);
    const SttTranscription result = provider.Transcribe(ConstantAudio(1040000), {});
    CHECK(result.success);
    CHECK(result.text == "one two");
    CHECK(transport.Requests().size() == 2);
}

TEST_CASE("provider skips the transport for empty audio") {
    FakeTransport transport({});
    GnaniProvider provider(transport, Settings());
    const SttTranscription result = provider.Transcribe({}, {});
    CHECK(result.success);
    CHECK(result.text.empty());
    CHECK(transport.Requests().empty());
}
