#include "core/SnippetStore.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

void WriteFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::trunc);
    file << content;
}

}

TEST_CASE("whole utterance match with punctuation noise") {
    SnippetStore snippets;
    snippets.SetEntries({{"my email", "john@doe.example\njohn@work.example"}});
    CHECK(snippets.Expand("my email") == "john@doe.example\njohn@work.example");
    CHECK(snippets.Expand("My email.") == "john@doe.example\njohn@work.example");
    CHECK(snippets.Expand("my email?!") == "john@doe.example\njohn@work.example");
    CHECK(snippets.Expand("please send my email") == "please send my email");
}

TEST_CASE("contains mode replaces inline whole words") {
    SnippetStore snippets;
    snippets.SetEntries({{"my email", "john@doe.example"}});
    snippets.SetMode("contains");
    CHECK(snippets.Expand("send it to my email please") ==
          "send it to john@doe.example please");
    CHECK(snippets.Expand("the mymailbox account") == "the mymailbox account");
}

TEST_CASE("duplicate triggers last wins with a warning") {
    const std::string path =
        (std::filesystem::temp_directory_path() / "lwf_snippets_dup.json").string();
    WriteFile(path,
              R"([{"trigger": "sig", "expansion": "first"},
                  {"trigger": "sig", "expansion": "second"}])");
    SnippetStore snippets;
    CHECK(snippets.Load(path));
    REQUIRE(snippets.Warnings().size() == 1);
    CHECK(snippets.Warnings()[0].find("duplicate") != std::string::npos);
    CHECK(snippets.Expand("sig") == "second");
}

TEST_CASE("overlong expansion is rejected") {
    const std::string path =
        (std::filesystem::temp_directory_path() / "lwf_snippets_long.json").string();
    WriteFile(path, R"json([{"trigger": "big", "expansion": "x)json" +
                        std::string(4100, 'y') + R"json("}])json");
    SnippetStore snippets;
    CHECK(snippets.Load(path));
    REQUIRE(snippets.Warnings().size() == 1);
    CHECK(snippets.Warnings()[0].find("4000") != std::string::npos);
    CHECK(snippets.Entries().empty());
    CHECK(snippets.Expand("big") == "big");
}

TEST_CASE("missing or broken file leaves store unloaded") {
    SnippetStore snippets;
    const std::string missing =
        (std::filesystem::temp_directory_path() / "lwf_snippets_missing.json").string();
    CHECK_FALSE(snippets.Load(missing));
    const std::string bad =
        (std::filesystem::temp_directory_path() / "lwf_snippets_bad.json").string();
    WriteFile(bad, "not json");
    CHECK_FALSE(snippets.Load(bad));
    CHECK(snippets.Expand("anything") == "anything");
}
