#include "Fakes.h"

#include "core/DictationCleanup.h"

#include <doctest/doctest.h>
#include <atomic>
#include <chrono>
#include <string>

namespace {

using namespace lwf_test;

class QuestionAnswerLlm final : public ILlmProvider {
public:
    std::string Process(const std::string&, const std::string&) override {
        return "The capital of France is Paris";
    }
};

class PreambleLlm final : public ILlmProvider {
public:
    std::string Process(const std::string&, const std::string&) override {
        return "Sure, here is the cleaned text: " + std::string("hello world today");
    }
};

class HallucinatingLlm final : public ILlmProvider {
public:
    std::string Process(const std::string&, const std::string&) override {
        return std::string(500, 'x');
    }
};

class SlowCancellableLlm final : public ILlmProvider {
public:
    std::atomic<bool> cancel_requested{false};
    std::atomic<long long> cancel_set_at{0};
    std::atomic<long long> returned_at{0};

    std::string Process(const std::string&, const std::string&) override {
        const auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - start)
                   .count() < 500) {
            if (cancel_requested.load()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        returned_at.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now().time_since_epoch())
                              .count());
        return "late answer";
    }

    void RequestCancel() override {
        cancel_set_at.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count());
        cancel_requested.store(true);
    }
};

struct Fixture {
    std::unique_ptr<ILlmProvider> llm;
    DictationCleanup cleanup;

    explicit Fixture(ILlmProvider* provider) : llm(provider), cleanup(provider) {}
};

}

TEST_CASE("question in dictation passes through when llm answers it") {
    QuestionAnswerLlm llm;
    DictationCleanup cleanup(&llm);
    const std::string cleaned = cleanup.Process("what is the capital of France");
    CHECK(cleaned == "The capital of France is Paris");
}

TEST_CASE("preamble style answers are rejected") {
    PreambleLlm llm;
    DictationCleanup cleanup(&llm);
    const std::string raw = "this is a longer dictated sentence about trivia";
    const std::string cleaned = cleanup.Process(raw);
    CHECK(cleaned == raw);
    CHECK(cleanup.GuardRejections() == 1);
}

TEST_CASE("long hallucinations are rejected") {
    HallucinatingLlm llm;
    DictationCleanup cleanup(&llm);
    const std::string raw = "a short dictated line of words";
    CHECK(cleanup.Process(raw) == raw);
    CHECK(cleanup.GuardRejections() == 1);
}

TEST_CASE("low word overlap with raw is rejected") {
    ScriptedLlm llm;
    llm.reply = "completely unrelated different words appear here now";
    DictationCleanup cleanup(&llm);
    const std::string raw = "please remember the meeting tomorrow morning";
    CHECK(cleanup.Process(raw) == raw);
    CHECK(cleanup.GuardRejections() == 1);
}

TEST_CASE("wrapping quotes are stripped once") {
    ScriptedLlm llm;
    llm.reply = "\"hello world today then\"";
    DictationCleanup cleanup(&llm);
    CHECK(cleanup.Process("hello world today then") == "hello world today then");
}

TEST_CASE("hindi and spanish text round trips unchanged") {
    ScriptedLlm llm;
    const std::string hindi = "\xE0\xA4\xAF\xE0\xA4\xB9 \xE0\xA4\x8F\xE0\xA4\x95 \xE0\xA4\xAA\xE0\xA4\xB0\xE0\xA5\x80\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7\xE0\xA4\xBE \xE0\xA4\xB5\xE0\xA4\xBE\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xAF \xE0\xA4\xB9\xE0\xA5\x88 \xE0\xA4\x86\xE0\xA4\x9C";
    const std::string spanish = "Esta es una frase de prueba para el dictado";
    llm.reply = spanish;
    DictationCleanup cleanup(&llm);
    CHECK(cleanup.Process(spanish) == spanish);
    llm.reply = hindi;
    CHECK(cleanup.Process(hindi) == hindi);
}

TEST_CASE("short text skips the llm entirely") {
    ScriptedLlm llm;
    DictationCleanup cleanup(&llm);
    CHECK(cleanup.Process("hi there now") == "hi there now");
    CHECK(llm.call_count == 0);
}

TEST_CASE("cleanup timeout falls back to raw and cancels cooperatively") {
    SlowCancellableLlm llm;
    DictationCleanup cleanup(&llm);
    cleanup.SetTimeoutMs(60);
    const std::string raw = "a dictated sentence that will time out during cleanup";
    const auto start = std::chrono::steady_clock::now();
    const std::string result = cleanup.Process(raw);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - start)
                             .count();
    CHECK(result == raw);
    CHECK(elapsed < 400);
    const long long cancel_latency = llm.returned_at.load() - llm.cancel_set_at.load();
    CHECK(cancel_latency >= 0);
    CHECK(cancel_latency < 100);
}

TEST_CASE("disabled cleanup returns raw") {
    ScriptedLlm llm;
    llm.reply = "changed words";
    DictationCleanup cleanup(&llm);
    cleanup.SetEnabled(false);
    CHECK(cleanup.Process("this sentence has enough words") == "this sentence has enough words");
    CHECK(llm.call_count == 0);
}

TEST_CASE("llm exception falls back to raw") {
    ScriptedLlm llm;
    llm.fail = true;
    DictationCleanup cleanup(&llm);
    const std::string raw = "this sentence has enough words to be cleaned";
    CHECK(cleanup.Process(raw) == raw);
}
