#include "core/PersonalDictionary.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

void WriteFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::trunc);
    file << content;
}

const char* kCommonWords[] = {
    "the", "be", "to", "of", "and", "a", "in", "that", "have", "it", "for", "not",
    "on", "with", "he", "as", "you", "do", "at", "this", "but", "his", "by", "from",
    "they", "we", "say", "her", "she", "or", "an", "will", "my", "one", "all",
    "would", "there", "their", "what", "so", "up", "out", "if", "about", "who",
    "get", "which", "go", "me", "when",
};

}

TEST_CASE("dictionary loads legacy and modern formats") {
    const std::string dir = (std::filesystem::path("/tmp") / "lwf_dict").string();
    std::filesystem::create_directories(dir);

    PersonalDictionary legacy_array;
    WriteFile(dir + "/bare.json", "[\"Docker\", \"Kubernetes\"]");
    CHECK(legacy_array.Load(dir + "/bare.json"));
    CHECK(legacy_array.Words().size() == 2);

    PersonalDictionary legacy_words;
    WriteFile(dir + "/words.json", R"({"words": ["Docker"]})");
    CHECK(legacy_words.Load(dir + "/words.json"));
    CHECK(legacy_words.Words().size() == 1);
    CHECK(legacy_words.Replacements().empty());

    PersonalDictionary modern;
    WriteFile(dir + "/modern.json",
              R"({"words": ["Kubernetes"], "replacements": [{"from": "gihub", "to": "GitHub"}]})");
    CHECK(modern.Load(dir + "/modern.json"));
    CHECK(modern.Words().size() == 1);
    REQUIRE(modern.Replacements().size() == 1);
    CHECK(modern.Replacements()[0].first == "gihub");

    PersonalDictionary garbage;
    WriteFile(dir + "/bad.json", "{not json");
    CHECK_FALSE(garbage.Load(dir + "/bad.json"));
    CHECK_FALSE(garbage.Load(dir + "/missing.json"));
}

TEST_CASE("replacements force exact spelling case-insensitively") {
    PersonalDictionary dict;
    dict.SetReplacements({{"gihub", "GitHub"}, {"js", "JavaScript"}});
    CHECK(dict.Enforce("push to gihub now", true) == "push to GitHub now");
    CHECK(dict.Enforce("push to Gihub now", true) == "push to GitHub now");
    CHECK(dict.Enforce("the gihubrepo", true) == "the gihubrepo");
}

TEST_CASE("dictionary words force canonical spelling and case") {
    PersonalDictionary dict;
    dict.SetWords({"Kubernetes", "PostgreSQL"});
    CHECK(dict.Enforce("we run kubernetes on postgresql", false) ==
          "we run Kubernetes on PostgreSQL");
    CHECK(dict.Enforce("unrelated text", false) == "unrelated text");
}

TEST_CASE("fuzzy correction maps edit distance one words of length six or more") {
    PersonalDictionary dict;
    dict.SetWords({"kubernetes", "postgres", "deploy"});
    CHECK(dict.Enforce("the kubernets cluster", true) == "the kubernetes cluster");
    CHECK(dict.Enforce("the kubernets cluster", false) == "the kubernets cluster");
    CHECK(dict.Enforce("ship it to postgre", true) == "ship it to postgres");
    CHECK(dict.Enforce("ship it to postgre", false) == "ship it to postgre");
}

TEST_CASE("fuzzy never mangles common words") {
    PersonalDictionary dict;
    dict.SetWords({"kubernetes", "postgres", "docker", "repository", "microservice",
                   "asynchronous", "observability", "deployment", "container", "pipeline",
                   "javascript", "typescript", "python", "workflow", "backlog", "sprint",
                   "kanban", "webpack", "babel", "jest"});
    std::string sentence;
    for (const char* word : kCommonWords) {
        if (!sentence.empty()) {
            sentence += " ";
        }
        sentence += word;
    }
    CHECK(dict.Enforce(sentence, true) == sentence);
}

TEST_CASE("initial prompt respects the character cap and drops the least recent") {
    PersonalDictionary dict;
    std::vector<std::string> words;
    for (int i = 0; i < 80; ++i) {
        words.push_back("term" + std::to_string(100 + i) + "xxxxxxxxxxxx");
    }
    dict.SetWords(words);
    const std::string prompt = dict.InitialPrompt(600);
    CHECK(prompt.size() <= 600);
    CHECK(prompt.find("term100") != std::string::npos);
    CHECK(prompt.find("term179") == std::string::npos);
    CHECK(prompt.rfind("Glossary: ", 0) == 0);

    PersonalDictionary empty;
    CHECK(empty.InitialPrompt(600).empty());
}

TEST_CASE("add word keeps most recent first and dedupes") {
    PersonalDictionary dict;
    dict.AddWord("alpha");
    dict.AddWord("beta");
    dict.AddWord("alpha");
    REQUIRE(dict.Words().size() == 2);
    CHECK(dict.Words()[0] == "alpha");
    CHECK(dict.Words()[1] == "beta");
}

TEST_CASE("edit distance basics") {
    CHECK(PersonalDictionary::EditDistance("kitten", "sitting") == 3);
    CHECK(PersonalDictionary::EditDistance("abc", "abc") == 0);
    CHECK(PersonalDictionary::EditDistance("", "abc") == 3);
}

TEST_CASE("sentinel-expanded text is preserved by enforcement") {
    PersonalDictionary dict;
    dict.SetReplacements({{"gihub", "GitHub"}});
    const std::string text = "push to gihub";
    CHECK(dict.Enforce(text, true) == "push to GitHub");
}
