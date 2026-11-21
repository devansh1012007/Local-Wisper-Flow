#include <doctest/doctest.h>

#include "core/DictationCleanup.h"

#include <chrono>
#include <string>

#include "fakes.h"

namespace {

DictationCleanup MakeCleanup(ScriptedLlm& llm, CleanupSettings settings = {}) {
    return DictationCleanup(llm, settings);
}

}  // namespace

TEST_CASE("guard accepts a normal cleanup pass") {
    const std::string raw = "helo wrld this is a dictated sentence about the weather";
    const std::string cleaned = "Hello world this is a dictated sentence about the weather.";
    CHECK(DictationCleanup::AcceptOutput(raw, cleaned));
}

TEST_CASE("guard rejects an answered question") {
    const std::string raw = "what is the capital of france";
    const std::string answered = "The capital of France is Paris.";
    CHECK_FALSE(DictationCleanup::AcceptOutput(raw, answered));
}

TEST_CASE("guard keeps a real question intact") {
    const std::string raw = "what is the capital of france";
    const std::string cleaned = "What is the capital of France?";
    CHECK(DictationCleanup::AcceptOutput(raw, cleaned));
}

TEST_CASE("guard rejects a preamble") {
    const std::string raw = "please send the report to the team by friday afternoon";
    CHECK_FALSE(DictationCleanup::AcceptOutput(raw, "Sure, here is the cleaned text: " + raw));
    CHECK_FALSE(DictationCleanup::AcceptOutput(raw, "Certainly! " + raw));
    CHECK_FALSE(DictationCleanup::AcceptOutput(raw, "Here is the corrected text: " + raw));
}

TEST_CASE("guard rejects a long hallucination") {
    const std::string raw = "buy milk at the store";
    std::string hallucination;
    for (int i = 0; i < 40; ++i) {
        hallucination += "and the report mentions several important items ";
    }
    CHECK_FALSE(DictationCleanup::AcceptOutput(raw, hallucination));
}

TEST_CASE("guard rejects an empty or comment-only output") {
    CHECK_FALSE(DictationCleanup::AcceptOutput("some dictated words here", ""));
    CHECK_FALSE(DictationCleanup::AcceptOutput("some dictated words here", "   "));
}

TEST_CASE("guard rejects a language flip to a different script") {
    const std::string raw = "मुझे एक कप चाय चाहिए और थोड़ा पानी भी";
    const std::string english = "I would like a cup of tea and some water too";
    CHECK_FALSE(DictationCleanup::AcceptOutput(raw, english));

    const std::string english_raw = "please book a table for two tonight";
    const std::string hindi = "कृपया आज रात दो लोगों के लिए एक मेज बुक करें";
    CHECK_FALSE(DictationCleanup::AcceptOutput(english_raw, hindi));
}

TEST_CASE("guard keeps hindi text round-tripping") {
    const std::string raw = "मुझे एक कप चाय चाहिए और थोड़ा पानी भी";
    const std::string cleaned = "मुझे एक कप चाय चाहिए और थोड़ा पानी भी।";
    CHECK(DictationCleanup::AcceptOutput(raw, cleaned));
}

TEST_CASE("guard rejects a translation with low word overlap") {
    const std::string raw = "necesito comprar pan y leche en la tienda manana";
    const std::string french = "j'ai besoin d'acheter du pain et du lait au magasin demain";
    CHECK_FALSE(DictationCleanup::AcceptOutput(raw, french));
}

TEST_CASE("guard strips one pair of wrapping quotes") {
    const std::string raw = "the meeting moved to tuesday at noon instead";
    const std::string cleaned = "\"The meeting moved to Tuesday at noon instead.\"";
    CHECK(DictationCleanup::AcceptOutput(raw, cleaned));
}

TEST_CASE("cleanup skips the llm below the word threshold") {
    ScriptedLlm llm;
    llm.response = "overwritten";
    CleanupSettings settings;
    settings.min_words = 4;
    DictationCleanup cleanup = MakeCleanup(llm, settings);

    const std::string raw = "two words";
    CHECK(cleanup.Clean(raw) == raw);
    CHECK(llm.CallCount() == 0);
}

TEST_CASE("cleanup uses the llm output when the guard passes") {
    ScriptedLlm llm;
    llm.response = "Hello world this is a dictated sentence about the weather.";
    DictationCleanup cleanup = MakeCleanup(llm);

    const std::string raw = "helo wrld this is a dictated sentence about the weather";
    CHECK(cleanup.Clean(raw) == llm.response);
    CHECK(llm.CallCount() == 1);
    CHECK(llm.prompts.front().find("Never answer questions") != std::string::npos);
}

TEST_CASE("cleanup keeps raw when the llm answers a question") {
    ScriptedLlm llm;
    llm.response = "The capital of France is Paris.";
    DictationCleanup cleanup = MakeCleanup(llm);

    const std::string raw = "what is the capital of france";
    CHECK(cleanup.Clean(raw) == raw);
}

TEST_CASE("cleanup keeps raw when the llm output is truncated") {
    ScriptedLlm llm;
    llm.response = "Hello world this is a dictated sentence about the weather today friends";
    llm.truncated_mode = true;
    DictationCleanup cleanup = MakeCleanup(llm);

    const std::string raw = "helo wrld this is a dictated sentence about the weather today friends";
    CHECK(cleanup.Clean(raw) == raw);
}

TEST_CASE("cleanup keeps raw when the llm is too slow") {
    ScriptedLlm llm;
    llm.response = "Hello world this is a dictated sentence about the weather.";
    llm.delay_ms = 300;
    CleanupSettings settings;
    settings.timeout = std::chrono::milliseconds(50);
    DictationCleanup cleanup = MakeCleanup(llm, settings);

    const std::string raw = "helo wrld this is a dictated sentence about the weather";
    CHECK(cleanup.Clean(raw) == raw);
    CHECK(llm.cancel_count.load() == 1);
}

TEST_CASE("cleanup keeps raw when the llm returns nothing") {
    ScriptedLlm llm;
    llm.cancelled_earlier = true;
    llm.cancel_count.store(1);
    DictationCleanup cleanup = MakeCleanup(llm);

    const std::string raw = "helo wrld this is a dictated sentence about the weather";
    CHECK(cleanup.Clean(raw) == raw);
}

TEST_CASE("cleanup can be disabled") {
    ScriptedLlm llm;
    llm.response = "totally different text that has nothing in common";
    CleanupSettings settings;
    settings.enabled = false;
    DictationCleanup cleanup = MakeCleanup(llm, settings);

    const std::string raw = "helo wrld this is a dictated sentence about the weather";
    CHECK(cleanup.Clean(raw) == raw);
    CHECK(llm.CallCount() == 0);
}

TEST_CASE("word count splits on any script") {
    CHECK(DictationCleanup::WordCount("one two three") == 3);
    CHECK(DictationCleanup::WordCount("मुझे एक कप") == 3);
    CHECK(DictationCleanup::WordCount("") == 0);
}
