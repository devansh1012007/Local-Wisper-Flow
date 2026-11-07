#include <doctest/doctest.h>

#include "llm/OpenRouterContract.h"
#include "llm/OpenRouterProvider.h"

#include "core/DictationCleanup.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct ScriptedStep {
    int status = 200;
    std::string body = R"({"id":"gen-1","choices":[{"message":{"role":"assistant","content":"cleaned text"},"finish_reason":"stop"}]})";
    bool throw_timeout = false;
    bool throw_network = false;
    bool throw_runtime = false;
};

class ScriptedTransport final : public IHttpTransport {
public:
    explicit ScriptedTransport(std::vector<ScriptedStep> script) : script_(std::move(script)) {}

    HttpResponse Post(const HttpRequest& request) override {
        requests_.push_back(request);
        if (index_ >= script_.size()) {
            throw std::runtime_error("ScriptedTransport: script exhausted");
        }
        const ScriptedStep step = script_[index_++];
        if (step.throw_timeout) {
            throw HttpTransportError(HttpTransportErrorKind::Timeout, "timed out");
        }
        if (step.throw_network) {
            throw HttpTransportError(HttpTransportErrorKind::Network, "connection reset");
        }
        if (step.throw_runtime) {
            throw std::runtime_error("unexpected transport failure");
        }
        HttpResponse response;
        response.status = step.status;
        response.body.assign(step.body.begin(), step.body.end());
        return response;
    }

    const std::vector<HttpRequest>& Requests() const { return requests_; }

private:
    std::vector<ScriptedStep> script_;
    std::vector<HttpRequest> requests_;
    std::size_t index_ = 0;
};

class GateTransport final : public IHttpTransport {
public:
    GateTransport(int status, std::string body) : status_(status), body_(std::move(body)) {}

    HttpResponse Post(const HttpRequest& request) override {
        requests_.push_back(request);
        entered_.store(true);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!released_.load()) {
            if (std::chrono::steady_clock::now() > deadline) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        HttpResponse response;
        response.status = status_;
        response.body.assign(body_.begin(), body_.end());
        return response;
    }

    bool Entered() const { return entered_.load(); }
    void Release() { released_.store(true); }
    std::size_t RequestCount() const { return requests_.size(); }

private:
    int status_;
    std::string body_;
    std::atomic<bool> entered_{false};
    std::atomic<bool> released_{false};
    std::vector<HttpRequest> requests_;
};

OpenRouterSettings TestSettings() {
    OpenRouterSettings settings;
    settings.api_key = "or-secret-key-123";
    settings.model = "test/model";
    settings.timeout_ms = 8000;
    return settings;
}

std::string BodyText(const HttpRequest& request) {
    return std::string(request.body.begin(), request.body.end());
}

}  // namespace

TEST_CASE("openrouter request build is host path headers json body") {
    ScriptedTransport transport({ScriptedStep()});
    OpenRouterProvider provider(transport, TestSettings());
    const std::string result = provider.Process("hello there this is a test", "Fix grammar.");
    CHECK(result == "cleaned text");
    REQUIRE(transport.Requests().size() == 1);
    const HttpRequest& request = transport.Requests()[0];
    CHECK(request.host == OpenRouterContract::kHost);
    CHECK(request.path == OpenRouterContract::kPath);
    REQUIRE(request.headers.size() == 1);
    CHECK(request.headers[0].first == "Authorization");
    CHECK(request.headers[0].second == "Bearer or-secret-key-123");
    CHECK(request.content_type == "application/json");
    CHECK(request.timeout_ms == 8000);
    const std::string body = BodyText(request);
    const auto parsed = nlohmann::json::parse(body, nullptr, false);
    REQUIRE_FALSE(parsed.is_discarded());
    CHECK(parsed["model"] == "test/model");
    REQUIRE(parsed["messages"].is_array());
    REQUIRE(parsed["messages"].size() == 2);
    CHECK(parsed["messages"][0]["role"] == "system");
    CHECK(parsed["messages"][0]["content"] == "Fix grammar.");
    CHECK(parsed["messages"][1]["role"] == "user");
    CHECK(parsed["messages"][1]["content"] == "hello there this is a test");
    CHECK_FALSE(parsed.contains("stream"));
    CHECK(provider.LastOutcome().error == OpenRouterError::None);
    CHECK(provider.LastOutcome().http_status == 200);
    CHECK(provider.LastOutcome().request_id == "gen-1");
}

TEST_CASE("openrouter omits system message when prompt empty") {
    ScriptedTransport transport({ScriptedStep()});
    OpenRouterProvider provider(transport, TestSettings());
    provider.Process("some text here", "");
    REQUIRE(transport.Requests().size() == 1);
    const auto parsed = nlohmann::json::parse(BodyText(transport.Requests()[0]), nullptr, false);
    REQUIRE_FALSE(parsed.is_discarded());
    REQUIRE(parsed["messages"].size() == 1);
    CHECK(parsed["messages"][0]["role"] == "user");
    CHECK(parsed["messages"][0]["content"] == "some text here");
}

TEST_CASE("openrouter preserves utf8 in messages") {
    ScriptedTransport transport({ScriptedStep()});
    OpenRouterProvider provider(transport, TestSettings());
    const std::string hindi = "\xE0\xA4\xAF\xE0\xA4\xB9 \xE0\xA4\x8F\xE0\xA4\x95";
    provider.Process(hindi, "keep language");
    REQUIRE(transport.Requests().size() == 1);
    const auto parsed = nlohmann::json::parse(BodyText(transport.Requests()[0]), nullptr, false);
    REQUIRE_FALSE(parsed.is_discarded());
    CHECK(parsed["messages"][1]["content"] == hindi);
}

TEST_CASE("openrouter success outcome is none with content") {
    ScriptedTransport transport({ScriptedStep()});
    OpenRouterProvider provider(transport, TestSettings());
    const std::string result = provider.Process("raw input text here now", "sys");
    CHECK(result == "cleaned text");
    const OpenRouterOutcome outcome = provider.LastOutcome();
    CHECK(outcome.error == OpenRouterError::None);
    CHECK(outcome.http_status == 200);
    CHECK(outcome.bytes_sent > 0);
    CHECK(outcome.bytes_received > 0);
}

TEST_CASE("openrouter maps vendor statuses to typed outcomes") {
    struct Case {
        int status;
        std::string body;
        OpenRouterError error;
    };
    const std::vector<Case> cases = {
        {401, R"({"error":{"code":401,"message":"User not found."}})", OpenRouterError::Auth},
        {402, R"({"error":{"code":402,"message":"Insufficient credits."}})", OpenRouterError::Auth},
        {403, R"({"error":{"code":403,"message":"Forbidden"}})", OpenRouterError::Auth},
        {408, R"({"error":{"code":408,"message":"timed out"}})", OpenRouterError::Timeout},
        {429, R"({"error":{"code":429,"message":"Rate limit exceeded","metadata":{"error_type":"rate_limit_exceeded"}}})", OpenRouterError::RateLimited},
        {502, R"({"error":{"code":502,"message":"model down"}})", OpenRouterError::Server},
        {503, R"({"error":{"code":503,"message":"no provider"}})", OpenRouterError::Server},
        {500, R"({"error":{"code":500,"message":"Internal error"}})", OpenRouterError::Server},
        {504, R"({"error":{"code":504,"message":"timed out"}})", OpenRouterError::Timeout},
        {400, R"({"error":{"code":400,"message":"bad request"}})", OpenRouterError::InvalidRequest},
        {404, R"({"error":{"code":404,"message":"not found"}})", OpenRouterError::InvalidRequest},
        {413, R"({"error":{"code":413,"message":"too large"}})", OpenRouterError::InvalidRequest},
        {412, R"({"error":{"code":412,"message":"precondition failed"}})", OpenRouterError::InvalidRequest},
    };
    for (const Case& entry : cases) {
        ScriptedStep step;
        step.status = entry.status;
        step.body = entry.body;
        ScriptedTransport transport({step});
        OpenRouterProvider provider(transport, TestSettings());
        const std::string result = provider.Process("some dictation text here", "sys");
        CHECK(result.empty());
        CHECK(provider.LastOutcome().error == entry.error);
        CHECK(provider.LastOutcome().http_status == entry.status);
    }
}

TEST_CASE("openrouter error_type takes precedence over status") {
    ScriptedStep step;
    step.status = 500;
    step.body = R"({"error":{"code":500,"message":"masked","metadata":{"error_type":"timeout"}}})";
    ScriptedTransport transport({step});
    OpenRouterProvider provider(transport, TestSettings());
    CHECK(provider.Process("some dictation text here", "sys").empty());
    CHECK(provider.LastOutcome().error == OpenRouterError::Timeout);
}

TEST_CASE("openrouter treats 200 with error object as failure") {
    ScriptedStep step;
    step.status = 200;
    step.body = R"({"id":"gen-e","error":{"code":429,"message":"Rate limit exceeded","metadata":{"error_type":"rate_limit_exceeded"}}})";
    ScriptedTransport transport({step});
    OpenRouterProvider provider(transport, TestSettings());
    CHECK(provider.Process("some dictation text here", "sys").empty());
    CHECK(provider.LastOutcome().error == OpenRouterError::RateLimited);
}

TEST_CASE("openrouter treats choice error as failure even with partial content") {
    ScriptedStep step;
    step.status = 200;
    step.body = R"({"id":"gen-p","choices":[{"message":{"role":"assistant","content":"partial output..."},"finish_reason":"error","error":{"code":502,"message":"Provider disconnected mid-stream","metadata":{"error_type":"provider_unavailable"}}}]})";
    ScriptedTransport transport({step});
    OpenRouterProvider provider(transport, TestSettings());
    CHECK(provider.Process("some dictation text here", "sys").empty());
    CHECK(provider.LastOutcome().error == OpenRouterError::Server);
}

TEST_CASE("openrouter treats choice error without content as typed failure") {
    ScriptedStep step;
    step.status = 200;
    step.body = R"({"id":"gen-p2","choices":[{"message":{"role":"assistant","content":null},"finish_reason":"error","error":{"code":429,"message":"Rate limit exceeded","metadata":{"error_type":"rate_limit_exceeded"}}}]})";
    ScriptedTransport transport({step});
    OpenRouterProvider provider(transport, TestSettings());
    CHECK(provider.Process("some dictation text here", "sys").empty());
    CHECK(provider.LastOutcome().error == OpenRouterError::RateLimited);
}

TEST_CASE("openrouter serialization failure is malformed outcome not a throw") {
    ScriptedTransport transport({ScriptedStep()});
    OpenRouterProvider provider(transport, TestSettings());
    const std::string bad = std::string("hello \xFF world");
    CHECK(provider.Process(bad, "sys").empty());
    CHECK(provider.LastOutcome().error == OpenRouterError::Malformed);
    CHECK(transport.Requests().empty());
}

TEST_CASE("openrouter classifies malformed success bodies") {
    const std::vector<std::string> bodies = {
        "not json at all",
        "[]",
        R"({"id":"gen-m","choices":[]})",
        R"({"id":"gen-m","choices":[{"message":{"role":"assistant","content":null},"finish_reason":"stop"}]})",
        R"({"id":"gen-m","choices":[{"message":{"role":"assistant","content":["a","b"]},"finish_reason":"stop"}]})",
    };
    for (const std::string& body : bodies) {
        ScriptedStep step;
        step.status = 200;
        step.body = body;
        ScriptedTransport transport({step});
        OpenRouterProvider provider(transport, TestSettings());
        CHECK(provider.Process("some dictation text here", "sys").empty());
        CHECK(provider.LastOutcome().error == OpenRouterError::Malformed);
    }
}

TEST_CASE("openrouter maps transport failures without throwing") {
    ScriptedStep timeout;
    timeout.throw_timeout = true;
    ScriptedTransport timeout_transport({timeout});
    OpenRouterProvider timeout_provider(timeout_transport, TestSettings());
    CHECK(timeout_provider.Process("some dictation text here", "sys").empty());
    CHECK(timeout_provider.LastOutcome().error == OpenRouterError::Timeout);

    ScriptedStep network;
    network.throw_network = true;
    ScriptedTransport network_transport({network});
    OpenRouterProvider network_provider(network_transport, TestSettings());
    CHECK(network_provider.Process("some dictation text here", "sys").empty());
    CHECK(network_provider.LastOutcome().error == OpenRouterError::Network);

    ScriptedStep runtime;
    runtime.throw_runtime = true;
    ScriptedTransport runtime_transport({runtime});
    OpenRouterProvider runtime_provider(runtime_transport, TestSettings());
    CHECK(runtime_provider.Process("some dictation text here", "sys").empty());
    CHECK(runtime_provider.LastOutcome().error == OpenRouterError::Network);
}

TEST_CASE("openrouter requires key and model at construction") {
    ScriptedTransport transport({ScriptedStep()});
    OpenRouterSettings no_key = TestSettings();
    no_key.api_key = "";
    CHECK_THROWS_AS(OpenRouterProvider(transport, no_key), std::runtime_error);
    OpenRouterSettings no_model = TestSettings();
    no_model.model = "";
    CHECK_THROWS_AS(OpenRouterProvider(transport, no_model), std::runtime_error);
}

TEST_CASE("openrouter sends no request for empty text") {
    ScriptedTransport transport({});
    OpenRouterProvider provider(transport, TestSettings());
    CHECK(provider.Process("", "sys").empty());
    CHECK(transport.Requests().empty());
    CHECK(provider.LastOutcome().error == OpenRouterError::None);
}

TEST_CASE("openrouter ignores null error field on success") {
    ScriptedStep step;
    step.status = 200;
    step.body = R"({"id":"gen-n","error":null,"choices":[{"message":{"role":"assistant","content":"cleaned text"},"finish_reason":"stop"}]})";
    ScriptedTransport transport({step});
    OpenRouterProvider provider(transport, TestSettings());
    CHECK(provider.Process("some dictation text here", "sys") == "cleaned text");
    CHECK(provider.LastOutcome().error == OpenRouterError::None);
}

TEST_CASE("openrouter redacts key from failure detail") {
    ScriptedStep step;
    step.status = 401;
    step.body = R"({"error":{"code":401,"message":"bad bearer or-secret-key-123 token"}})";
    ScriptedTransport transport({step});
    OpenRouterProvider provider(transport, TestSettings());
    CHECK(provider.Process("some dictation text here", "sys").empty());
    const std::string detail = provider.LastOutcome().detail;
    CHECK(detail.find("or-secret-key-123") == std::string::npos);
    CHECK(detail.find("or-s") == std::string::npos);
    CHECK(detail.find("[redacted]") != std::string::npos);
}

TEST_CASE("openrouter stale cancel does not poison next process") {
    ScriptedTransport transport({ScriptedStep()});
    OpenRouterProvider provider(transport, TestSettings());
    provider.RequestCancel();
    const std::string result =
        provider.Process("some dictation text here", "sys");
    CHECK(result == "cleaned text");
    CHECK(transport.Requests().size() == 1);
    CHECK(provider.LastOutcome().error == OpenRouterError::None);
}

TEST_CASE("openrouter cancel during post is observed") {
    GateTransport transport(
        200,
        R"({"id":"gen-c","choices":[{"message":{"role":"assistant","content":"cleaned text"},"finish_reason":"stop"}]})");
    OpenRouterProvider provider(transport, TestSettings());
    std::string produced;
    std::thread worker([&] { produced = provider.Process("some dictation text here", "sys"); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!transport.Entered() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(transport.Entered());
    provider.RequestCancel();
    transport.Release();
    worker.join();
    CHECK(produced.empty());
    CHECK(provider.LastOutcome().error == OpenRouterError::Cancelled);
    CHECK(transport.RequestCount() == 1);
}

TEST_CASE("openrouter serializes concurrent process calls") {
    GateTransport transport(
        200,
        R"({"id":"gen-s","choices":[{"message":{"role":"assistant","content":"ok text here"},"finish_reason":"stop"}]})");
    OpenRouterProvider provider(transport, TestSettings());
    std::string first;
    std::string second;
    std::thread one([&] { first = provider.Process("first dictation text here", "sys"); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!transport.Entered() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(transport.Entered());
    std::thread two([&] { second = provider.Process("second dictation text here", "sys"); });
    two.join();
    CHECK(second.empty());
    transport.Release();
    one.join();
    CHECK(first == "ok text here");
    CHECK(transport.RequestCount() == 1);
}

TEST_CASE("openrouter second process does not wedge while first post is hung") {
    GateTransport transport(
        200,
        R"({"id":"gen-h","choices":[{"message":{"role":"assistant","content":"cleaned text"},"finish_reason":"stop"}]})");
    OpenRouterProvider provider(transport, TestSettings());
    std::string first;
    std::thread worker([&] { first = provider.Process("some dictation text here", "sys"); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!transport.Entered() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(transport.Entered());
    const auto start = std::chrono::steady_clock::now();
    const std::string second = provider.Process("another dictation text here", "sys");
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    CHECK(second.empty());
    CHECK(provider.LastOutcome().error == OpenRouterError::Busy);
    CHECK(elapsed < std::chrono::milliseconds(5000));
    CHECK(transport.RequestCount() == 1);
    transport.Release();
    worker.join();
    CHECK(first == "cleaned text");
}

TEST_CASE("openrouter success flows through dictation cleanup") {
    ScriptedStep step;
    step.status = 200;
    step.body = R"({"id":"gen-clean","choices":[{"message":{"role":"assistant","content":"this is a cleaned sentence for the test"},"finish_reason":"stop"}]})";
    ScriptedTransport transport({step});
    OpenRouterProvider provider(transport, TestSettings());
    DictationCleanup cleanup(&provider);
    const std::string cleaned = cleanup.Process("this is a cleaned sentence for the test");
    CHECK(cleaned == "this is a cleaned sentence for the test");
    CHECK(transport.Requests().size() == 1);
}

TEST_CASE("openrouter failure falls back to raw and counts as rejected") {
    ScriptedStep step;
    step.status = 401;
    step.body = R"({"error":{"code":401,"message":"User not found."}})";
    ScriptedTransport transport({step});
    OpenRouterProvider provider(transport, TestSettings());
    DictationCleanup cleanup(&provider);
    const std::string raw = "this sentence has enough words to be cleaned";
    CHECK(cleanup.Process(raw) == raw);
    CHECK(cleanup.GuardRejections() == 1);
    CHECK(provider.LastOutcome().error == OpenRouterError::Auth);
}
