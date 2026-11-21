#include "Fakes.h"

#include "core/DictationCleanup.h"
#include "core/SpokenCommandProcessor.h"
#include "core/TextPipeline.h"

#include <doctest/doctest.h>
#include <string>

namespace {

using namespace lwf_test;

}

TEST_CASE("each phrase converts to the right character") {
    SpokenCommandProcessor processor;
    processor.SetAggressive(true);

    struct Row {
        std::string spoken;
        std::string expected;
    };
    const Row rows[] = {
        {"alpha new line beta", "alpha\nbeta"},
        {"alpha new paragraph beta", "alpha\n\nbeta"},
        {"alpha period", "alpha."},
        {"alpha full stop", "alpha."},
        {"alpha beta comma gamma", "alpha beta, gamma"},
        {"alpha question mark", "alpha?"},
        {"alpha exclamation mark", "alpha!"},
        {"alpha exclamation point", "alpha!"},
        {"alpha colon beta", "alpha: beta"},
        {"alpha semicolon beta", "alpha; beta"},
        {"open quote hello close quote", "\"hello\""},
        {"bullet point first item", "- first item"},
    };

    for (const Row& row : rows) {
        INFO("spoken=", row.spoken);
        const std::string sentineled = processor.ToSentinels(row.spoken);
        const std::string expanded = processor.ExpandSentinels(sentineled);
        CHECK(expanded == row.expected);
    }
}

TEST_CASE("phrase inside a normal sentence is not converted in conservative mode") {
    SpokenCommandProcessor processor;
    processor.SetAggressive(false);

    CHECK(processor.ToSentinels("a new line of products") == "a new line of products");
    CHECK(processor.ToSentinels("the period of the pendulum") ==
          "the period of the pendulum");
    CHECK(processor.ToSentinels("please add a comma here") == "please add a comma here");
}

TEST_CASE("conservative mode converts isolated segments only") {
    SpokenCommandProcessor processor;
    processor.SetAggressive(false);
    CHECK(processor.ToSentinels("add, comma, here") == "add, LWF_COMMA here");
    CHECK(processor.ToSentinels("hello world. new line. next") ==
          "hello world. LWF_NL next");
}

TEST_CASE("llm that drops a sentinel falls back to raw with commands") {
    ScriptedLlm llm;
    llm.reply = "alpha beta";
    DictationCleanup cleanup(&llm);
    TextPipeline pipeline(cleanup);
    SpokenCommandProcessor processor;
    processor.SetAggressive(true);
    pipeline.SetSpokenCommands(&processor);

    const std::string result = pipeline.Process("alpha comma beta period");
    CHECK(result == "alpha, beta.");
}

TEST_CASE("llm that keeps sentinels passes them through to expansion") {
    ScriptedLlm llm;
    llm.reply = "alpha LWF_COMMA beta LWF_DOT";
    DictationCleanup cleanup(&llm);
    TextPipeline pipeline(cleanup);
    SpokenCommandProcessor processor;
    processor.SetAggressive(true);
    pipeline.SetSpokenCommands(&processor);

    const std::string result = pipeline.Process("alpha comma beta period");
    CHECK(result == "alpha, beta.");
}

TEST_CASE("scratch that removes everything before it in this utterance") {
    SpokenCommandProcessor processor;
    processor.SetAggressive(true);
    const std::string sentineled =
        processor.ToSentinels("wrong words scratch that correct words");
    const std::string expanded = processor.ExpandSentinels(sentineled);
    CHECK(expanded == "correct words");
}

TEST_CASE("scratch that at the end empties the utterance") {
    SpokenCommandProcessor processor;
    processor.SetAggressive(true);
    const std::string sentineled =
        processor.ToSentinels("everything wrong scratch that");
    const std::string expanded = processor.ExpandSentinels(sentineled);
    CHECK(expanded.empty());
}

TEST_CASE("sentinel count helper") {
    CHECK(SpokenCommandProcessor::CountSentinels("a LWF_NL b LWF_DOT c") == 2);
    CHECK(SpokenCommandProcessor::CountSentinels("no sentinels") == 0);
}

TEST_CASE("sentinel preservation hint exists for the cleanup prompt") {
    CHECK(SpokenCommandProcessor::SentinelPreservationHint().find("LWF_NL") !=
          std::string::npos);
}
