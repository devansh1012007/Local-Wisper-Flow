#include <doctest/doctest.h>

#include "core/PersonalDictionary.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

const char* kDictionaryPath = "tests/data/dictionary_test.json";

void WriteDictionary(const std::string& body) {
    std::filesystem::create_directories("tests/data");
    std::ofstream file(kDictionaryPath);
    file << body;
}

}  // namespace

TEST_CASE("dictionary loads the object format with words and replacements") {
    WriteDictionary(R"({
        "words": ["Alpha", "Mango", "zebra"],
        "replacements": [{"from": "teh", "to": "the"}]
    })");
    PersonalDictionary dictionary;
    dictionary.Load(kDictionaryPath);
    REQUIRE(dictionary.Words().size() == 3);
    CHECK(dictionary.Words()[0] == "Alpha");
    CHECK(dictionary.Words()[1] == "Mango");
    CHECK(dictionary.Words()[2] == "zebra");
    REQUIRE(dictionary.Replacements().size() == 1);
    CHECK(dictionary.Replacements()[0].from == "teh");
}

TEST_CASE("dictionary accepts the legacy bare array format") {
    WriteDictionary(R"(["Alpha", "Mango"])");
    PersonalDictionary dictionary;
    dictionary.Load(kDictionaryPath);
    CHECK(dictionary.Words().size() == 2);
}

TEST_CASE("dictionary dedupes case variants and keeps insertion order") {
    WriteDictionary(R"(["zebra", "Alpha", "alpha", "Mango", "ALPHA"])");
    PersonalDictionary dictionary;
    dictionary.Load(kDictionaryPath);
    REQUIRE(dictionary.Words().size() == 3);
    CHECK(dictionary.Words()[0] == "zebra");
    CHECK(dictionary.Words()[1] == "Alpha");
    CHECK(dictionary.Words()[2] == "Mango");

    dictionary.AddWord("mango");
    CHECK(dictionary.Words().size() == 3);
    dictionary.AddWord("new word");
    REQUIRE(dictionary.Words().size() == 4);
    CHECK(dictionary.Words()[3] == "new word");
    CHECK(dictionary.RemoveWord("ZEBRA"));
    CHECK(dictionary.Words().size() == 3);
}

TEST_CASE("dictionary missing file yields an empty dictionary") {
    PersonalDictionary dictionary;
    dictionary.Load("tests/data/definitely_missing_dictionary.json");
    CHECK(dictionary.Words().empty());
    CHECK(dictionary.Replacements().empty());
    CHECK(dictionary.PromptFragment().empty());
}

TEST_CASE("dictionary prompt keeps insertion order and caps length") {
    WriteDictionary(R"({"words": ["zebra", "Alpha", "Mango"]})");
    PersonalDictionary dictionary;
    dictionary.Load(kDictionaryPath);
    const std::string fragment = dictionary.PromptFragment();
    const std::size_t zebra_pos = fragment.find("zebra");
    const std::size_t alpha_pos = fragment.find("Alpha");
    const std::size_t mango_pos = fragment.find("Mango");
    REQUIRE(zebra_pos != std::string::npos);
    REQUIRE(alpha_pos != std::string::npos);
    REQUIRE(mango_pos != std::string::npos);
    CHECK(zebra_pos < alpha_pos);
    CHECK(alpha_pos < mango_pos);
    CHECK(fragment.size() <= PersonalDictionary::kPromptCharCap);
    CHECK_FALSE(dictionary.TruncatedLastPrompt());
}

TEST_CASE("dictionary prompt truncation drops oldest words not alphabetical ones") {
    PersonalDictionary dictionary;
    for (int i = 0; i < 200; ++i) {
        dictionary.AddWord("word" + std::to_string(i));
    }
    const std::string fragment = dictionary.PromptFragment();
    CHECK(fragment.size() <= PersonalDictionary::kPromptCharCap);
    CHECK(dictionary.TruncatedLastPrompt());
    CHECK(fragment.find("word0") != std::string::npos);
    CHECK(fragment.find("word199") == std::string::npos);
}

TEST_CASE("dictionary enforcement applies whole word case-insensitive replacements") {
    WriteDictionary(R"({
        "words": [],
        "replacements": [{"from": "Teh", "to": "the"}, {"from": "wispr", "to": "Wispr"}]
    })");
    PersonalDictionary dictionary;
    dictionary.Load(kDictionaryPath);
    CHECK(dictionary.Enforce("teh meeting about teh Wispr flow") ==
          "the meeting about the Wispr flow");
    CHECK(dictionary.Enforce("tehee") == "tehee");
    CHECK(dictionary.Enforce("absolutely") == "absolutely");
    CHECK(dictionary.Enforce("Teh start") == "the start");
}

TEST_CASE("dictionary malformed file is tolerated") {
    WriteDictionary("this is not json");
    PersonalDictionary dictionary;
    dictionary.Load(kDictionaryPath);
    CHECK(dictionary.Words().empty());
}
