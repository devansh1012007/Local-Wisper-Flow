#include "core/DictionaryStore.h"
#include "core/PersonalDictionary.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string TempDir(const char* name) {
    const std::string dir =
        (std::filesystem::temp_directory_path() / (std::string("lwf_dictstore_") + name)).string();
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

void WriteFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::trunc);
    file << content;
}

std::string ReadFile(const std::string& path) {
    std::ifstream file(path);
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

bool ContainsPhrase(const std::vector<DictionaryStore::Entry>& entries,
                    const std::string& phrase) {
    for (const DictionaryStore::Entry& entry : entries) {
        if (entry.phrase == phrase) {
            return true;
        }
    }
    return false;
}

}

TEST_CASE("migration reads bare string array") {
    const std::string dir = TempDir("mig_array");
    const std::string path = dir + "/dictionary.json";
    WriteFile(path, R"(["alpha", "beta"])");

    DictionaryStore store(path);
    CHECK(store.Load());
    const std::vector<DictionaryStore::Entry> entries = store.Entries();
    REQUIRE(entries.size() == 2);
    CHECK(ContainsPhrase(entries, "alpha"));
    CHECK(ContainsPhrase(entries, "beta"));
    CHECK(store.Save());
    CHECK(ReadFile(path).find("\"schema_version\": 2") != std::string::npos);
    CHECK(ReadFile(path).find("\"phrase\": \"alpha\"") != std::string::npos);
}

TEST_CASE("migration reads words object") {
    const std::string dir = TempDir("mig_words");
    const std::string path = dir + "/dictionary.json";
    WriteFile(path, R"({"words": ["Docker"]})");

    DictionaryStore store(path);
    CHECK(store.Load());
    const std::vector<DictionaryStore::Entry> entries = store.Entries();
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].phrase == "Docker");
    CHECK_FALSE(entries[0].has_replacement);
}

TEST_CASE("migration reads words with replacements") {
    const std::string dir = TempDir("mig_modern");
    const std::string path = dir + "/dictionary.json";
    WriteFile(path,
              R"({"words": ["Kubernetes"], "replacements": [{"from": "gihub", "to": "GitHub"}]})");

    DictionaryStore store(path);
    CHECK(store.Load());
    const std::vector<DictionaryStore::Entry> entries = store.Entries();
    REQUIRE(entries.size() == 2);
    CHECK(ContainsPhrase(entries, "Kubernetes"));
    CHECK(ContainsPhrase(entries, "gihub"));
    for (const DictionaryStore::Entry& entry : entries) {
        if (entry.phrase == "gihub") {
            CHECK(entry.has_replacement);
            CHECK(entry.replacement == "GitHub");
        }
    }
}

TEST_CASE("migration reads v2 round trip") {
    const std::string dir = TempDir("mig_v2");
    const std::string path = dir + "/dictionary.json";
    WriteFile(path,
              R"({"schema_version": 2, "entries": [{"id": 7, "phrase": "btw", )"
              R"("replacement": "by the way", "starred": true, "source": "manual", )"
              R"("usage_count": 3, "created_at": 100, "updated_at": 200}]})");

    DictionaryStore store(path);
    CHECK(store.Load());
    const std::vector<DictionaryStore::Entry> entries = store.Entries();
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].id == 7);
    CHECK(entries[0].starred);
    CHECK(entries[0].usage_count == 3);
    CHECK(entries[0].created_at == 100);
    CHECK(entries[0].updated_at == 200);
    CHECK(store.Save());

    DictionaryStore reloaded(path);
    CHECK(reloaded.Load());
    REQUIRE(reloaded.Size() == 1);
    CHECK(reloaded.Entries()[0].phrase == "btw");
}

TEST_CASE("first run seeds btw") {
    const std::string dir = TempDir("seed");
    const std::string path = dir + "/dictionary.json";
    DictionaryStore store(path);
    CHECK(store.Load());
    REQUIRE(store.Size() == 1);
    const std::vector<DictionaryStore::Entry> entries = store.Entries();
    CHECK(entries[0].phrase == "btw");
    CHECK(entries[0].replacement == "by the way");
    CHECK(entries[0].has_replacement);
    CHECK(entries[0].source == DictionaryStore::Source::Manual);
    CHECK(store.Save());

    DictionaryStore reloaded(path);
    CHECK(reloaded.Load());
    REQUIRE(reloaded.Size() == 1);
    CHECK(reloaded.Entries()[0].phrase == "btw");
}

TEST_CASE("duplicate trimmed case insensitive is rejected") {
    const std::string dir = TempDir("dup");
    const std::string path = dir + "/dictionary.json";
    DictionaryStore store(path);
    CHECK(store.Load());
    REQUIRE(store.Size() == 1);
    std::string error;
    CHECK(store.Add("alpha", "", &error));
    CHECK_FALSE(store.Add(" ALPHA ", "other", &error));
    CHECK(error == "Word already exists.");
    CHECK(store.Add("  beta  ", "", &error));
    CHECK_FALSE(store.Add("beta", "other", &error));
    CHECK(error == "Word already exists.");
    REQUIRE(store.Size() == 2);
    CHECK(ContainsPhrase(store.Entries(), "alpha"));
    CHECK(ContainsPhrase(store.Entries(), "beta"));
}

TEST_CASE("search finds phrase and replacement substring") {
    const std::string dir = TempDir("search");
    const std::string path = dir + "/dictionary.json";
    DictionaryStore store(path);
    CHECK(store.Load());
    std::string error;
    CHECK(store.Add("kubernetes", "", &error));
    CHECK(store.Add("gihub", "GitHub", &error));
    CHECK(store.Search("WAY").size() == 1);
    CHECK(store.Search("WAY")[0].phrase == "btw");
    CHECK(store.Search("kube").size() == 1);
    CHECK(store.Search("KUBE")[0].phrase == "kubernetes");
    CHECK(store.Search("hub").size() == 1);
    CHECK(store.Search("missing-term").empty());
    CHECK(store.Search("").size() == store.Size());
}

TEST_CASE("starred entries sort before newer entries") {
    const std::string dir = TempDir("sort");
    const std::string path = dir + "/dictionary.json";
    DictionaryStore store(path);
    CHECK(store.Load());
    std::string json =
        R"({"schema_version": 2, "entries": [)"
        R"({"id": 1, "phrase": "old", "starred": true, "source": "manual", "usage_count": 0, "created_at": 100, "updated_at": 100},)"
        R"({"id": 2, "phrase": "newer", "starred": false, "source": "manual", "usage_count": 0, "created_at": 300, "updated_at": 300},)"
        R"({"id": 3, "phrase": "middle", "starred": false, "source": "manual", "usage_count": 0, "created_at": 200, "updated_at": 200}])";
    CHECK(store.ImportJson(json, nullptr));
    const std::vector<DictionaryStore::Entry> entries = store.Entries();
    REQUIRE(entries.size() == 3);
    CHECK(entries[0].phrase == "old");
    CHECK(entries[1].phrase == "newer");
    CHECK(entries[2].phrase == "middle");
}

TEST_CASE("max 2000 boundary") {
    const std::string dir = TempDir("max");
    const std::string path = dir + "/dictionary.json";
    DictionaryStore store(path);
    CHECK(store.Load());
    CHECK(store.Remove(store.Entries()[0].id));
    REQUIRE(store.Size() == 0);
    std::string error;
    for (int i = 0; i < 2000; ++i) {
        CHECK(store.Add("term" + std::to_string(i), "", &error));
    }
    REQUIRE(store.Size() == 2000);
    CHECK_FALSE(store.Add("term2000", "", &error));
    CHECK(error == "maximum 2000 entries reached");
    REQUIRE(store.Size() == 2000);
}

TEST_CASE("fix count counts enforced replacements") {
    PersonalDictionary dict;
    dict.SetReplacements({{"gihub", "GitHub"}, {"js", "JavaScript"}});
    std::size_t count = 0;
    CHECK(dict.Enforce("push to gihub now", true, &count) == "push to GitHub now");
    CHECK(count == 1);
    CHECK(dict.Enforce("gihub and gihub", true, &count) == "GitHub and GitHub");
    CHECK(count == 2);
    CHECK(dict.Enforce("unrelated text", true, &count) == "unrelated text");
    CHECK(count == 0);
    CHECK(dict.Enforce("gihub js", true, &count) == "GitHub JavaScript");
    CHECK(count == 2);
    CHECK(dict.Enforce("push to gihub now", true) == "push to GitHub now");
}

TEST_CASE("fix count counts word corrections") {
    PersonalDictionary dict;
    dict.SetWords({"Kubernetes", "PostgreSQL"});
    std::size_t count = 0;
    CHECK(dict.Enforce("we run kubernetes on postgresql", false, &count) ==
          "we run Kubernetes on PostgreSQL");
    CHECK(count == 2);
    CHECK(dict.Enforce("kubernetes", false, &count) == "Kubernetes");
    CHECK(count == 1);
}

TEST_CASE("bak is created once from pre migration file") {
    const std::string dir = TempDir("bak");
    const std::string path = dir + "/dictionary.json";
    const std::string legacy = R"(["alpha"])";
    WriteFile(path, legacy);

    DictionaryStore store(path);
    CHECK(store.Load());
    CHECK(store.Save());
    REQUIRE(std::filesystem::exists(path + ".bak"));
    CHECK(ReadFile(path + ".bak") == legacy);

    std::string error;
    CHECK(store.Add("beta", "", &error));
    CHECK(store.Save());
    CHECK(ReadFile(path + ".bak") == legacy);
    CHECK(ReadFile(path).find("\"schema_version\": 2") != std::string::npos);
}

TEST_CASE("reload picks up external v2 changes") {
    const std::string dir = TempDir("reload");
    const std::string path = dir + "/dictionary.json";
    WriteFile(path, R"(["alpha"])");
    DictionaryStore store(path);
    CHECK(store.Load());
    CHECK(store.Save());
    WriteFile(path,
              R"({"schema_version": 2, "entries": [{"id": 1, "phrase": "gamma", "source": )"
              R"("manual", "usage_count": 0, "created_at": 1, "updated_at": 1}]})");
    CHECK(store.Reload());
    REQUIRE(store.Size() == 1);
    CHECK(store.Entries()[0].phrase == "gamma");
}

TEST_CASE("personal dictionary loads v2 file after migration") {
    const std::string dir = TempDir("pdv2");
    const std::string path = dir + "/dictionary.json";
    WriteFile(path, R"({"words": ["Kubernetes"], "replacements": [{"from": "gihub", "to": "GitHub"}]})");

    DictionaryStore store(path);
    CHECK(store.Load());
    CHECK(store.Save());

    PersonalDictionary dict;
    CHECK(dict.Load(path));
    CHECK(dict.Loaded());
    std::size_t count = 0;
    CHECK(dict.Enforce("push to gihub, hello kubernetes", true, &count) ==
          "push to GitHub, hello Kubernetes");
    CHECK(count == 2);
}

TEST_CASE("personal dictionary reload picks up saved edits") {
    const std::string dir = TempDir("pdreload");
    const std::string path = dir + "/dictionary.json";
    WriteFile(path, R"(["alpha"])");
    DictionaryStore store(path);
    CHECK(store.Load());
    CHECK(store.Save());

    PersonalDictionary dict;
    CHECK(dict.Load(path));
    std::string error;
    CHECK(store.Add("beta", "", &error));
    CHECK(store.Save());
    CHECK(dict.Reload());
    bool has_beta = false;
    for (const std::string& word : dict.Words()) {
        if (word == "beta") {
            has_beta = true;
        }
    }
    CHECK(has_beta);
}

TEST_CASE("export and import json round trip") {
    const std::string dir = TempDir("export");
    const std::string path = dir + "/dictionary.json";
    DictionaryStore store(path);
    CHECK(store.Load());
    std::string error;
    CHECK(store.Add("gihub", "GitHub", &error));
    const std::string exported = store.ExportJson();
    CHECK(exported.find("\"schema_version\": 2") != std::string::npos);

    DictionaryStore other(dir + "/other.json");
    CHECK(other.Load());
    CHECK(other.ImportJson(exported, &error));
    CHECK(other.Size() == store.Size());
    CHECK(ContainsPhrase(other.Entries(), "gihub"));
    CHECK(ContainsPhrase(other.Entries(), "btw"));
}

TEST_CASE("corrupt file is renamed to bad") {
    const std::string dir = TempDir("corrupt");
    const std::string path = dir + "/dictionary.json";
    WriteFile(path, "{not json");
    DictionaryStore store(path);
    CHECK_FALSE(store.Load());
    CHECK(std::filesystem::exists(path + ".bad"));
    CHECK(store.Size() == 0);
}
