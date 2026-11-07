#include "core/SnippetStore.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <utility>

namespace {

std::filesystem::path TestDir(const char* name) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / (std::string("lwf_snippetstore_v2_") + name);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

void WriteFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::trunc);
    file << content;
}

std::string ReadFile(const std::string& path) {
    std::ifstream file(path);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void StoreWith(SnippetStore& store,
               std::initializer_list<std::pair<const char*, const char*>> entries) {
    std::vector<SnippetStore::Entry> items;
    for (const auto& pair : entries) {
        SnippetStore::Entry entry;
        entry.trigger = pair.first;
        entry.expansion = pair.second;
        entry.id = entry.trigger;
        items.push_back(std::move(entry));
    }
    store.SetEntries(items);
}

}

TEST_CASE("v2 validation messages are exact") {
    SnippetStore store;
    StoreWith(store, {{"alpha", "bravo"}});
    CHECK(store.Validate("", "some expansion") == "Please enter a word");
    CHECK(store.Validate("   ", "some expansion") == "Please enter a word");
    CHECK(store.Validate("word", "") == "Please enter a snippet");
    CHECK(store.Validate("word", "   ") == "Please enter a snippet");
    CHECK(store.Validate("word", std::string(4000, 'x')) == "");
    CHECK(store.Validate("word", std::string(4001, 'x')) == "Snippet too long");
    CHECK(store.Validate("word", std::string(4001, ' ')) == "Snippet too long");
    CHECK(store.Validate("bravo", "brand new") == "Snippet already exists");
    CHECK(store.Validate("BRAVO", "brand new") == "Snippet already exists");
    CHECK(store.Validate("brand new", "alpha") == "Snippet already exists");
    CHECK(store.Validate("brand new", "bravo") == "Snippet already exists");
    CHECK(store.Validate("brand new", "ALPHA") == "Snippet already exists");
    CHECK(store.Validate("charlie", "delta") == "");
    CHECK(store.Validate("alpha", "updated expansion") == "");
    std::string error = "sentinel";
    CHECK_FALSE(store.Upsert("", "expansion", error));
    CHECK(error == "Please enter a word");
    CHECK_FALSE(store.Upsert("word", std::string(4001, 'x'), error));
    CHECK(error == "Snippet too long");
    CHECK_FALSE(store.Upsert("bravo", "fresh", error));
    CHECK(error == "Snippet already exists");
}

TEST_CASE("longest trigger wins in contains mode") {
    SnippetStore store;
    StoreWith(store, {{"email", "E"}, {"my email", "ME"}});
    store.SetMode("contains");
    CHECK(store.Expand("send my email please") == "send ME please");
    CHECK(store.Expand("send email please") == "send E please");
}

TEST_CASE("expansion casing is preserved") {
    SnippetStore store;
    StoreWith(store, {{"sign", "Hello World!!"}});
    CHECK(store.Expand("sign") == "Hello World!!");
    CHECK(store.Expand("SIGN") == "Hello World!!");
    store.SetMode("contains");
    CHECK(store.Expand("say Sign please") == "say Hello World!! please");
}

TEST_CASE("punctuation and extra spaces are ignored only when spoken alone") {
    SnippetStore store;
    StoreWith(store, {{"my email", "addr@example.test"}});
    CHECK(store.Expand("My Email!!") == "addr@example.test");
    CHECK(store.Expand("my    email") == "addr@example.test");
    CHECK(store.Expand("please my email") == "please my email");
    store.SetMode("contains");
    CHECK(store.Expand("my, email") == "my, email");
}

TEST_CASE("empty expansion removes the entry after the length check") {
    SnippetStore store;
    StoreWith(store, {{"sign", "sig"}, {"other", "o"}});
    std::string error = "sentinel";
    CHECK(store.Upsert("sign", "", error));
    CHECK(error.empty());
    CHECK(store.Entries().size() == 1);
    CHECK(store.Expand("sign") == "sign");

    CHECK_FALSE(store.Upsert("other", std::string(4001, ' '), error));
    CHECK(error == "Snippet too long");
    CHECK(store.Entries().size() == 1);
    CHECK(store.Expand("other") == "o");
}

TEST_CASE("empty trigger is ignored and whole-word matching is case-insensitive") {
    SnippetStore store;
    StoreWith(store, {{"my email", "addr@example.test"}});
    std::string error = "sentinel";
    CHECK_FALSE(store.Upsert("", "x", error));
    CHECK(store.Entries().size() == 1);
    store.SetMode("contains");
    CHECK(store.Expand("send MY EMAIL now") == "send addr@example.test now");
    CHECK(store.Expand("the mymailbox account") == "the mymailbox account");
    CHECK(store.Expand("emails") == "emails");
    store.SetMode("whole");
    CHECK(store.Expand("MY EMAIL") == "addr@example.test");
}

TEST_CASE("v2 save and load roundtrip keeps ids and timestamps") {
    const std::filesystem::path dir = TestDir("roundtrip");
    const std::string path = (dir / "snippets.json").string();
    SnippetStore store;
    std::string error;
    REQUIRE(store.Upsert("alpha", "one", error));
    REQUIRE(store.Upsert("beta", "two", error));
    REQUIRE(store.Entries().size() == 2);
    CHECK_FALSE(store.Entries()[0].id.empty());
    CHECK(store.Entries()[0].created_at > 0);
    REQUIRE(store.Save(path));
    const std::string text = ReadFile(path);
    CHECK(text.find("\"version\": 2") != std::string::npos);
    CHECK(text.find("\"id\"") != std::string::npos);
    CHECK(text.find("\"created_at\"") != std::string::npos);

    CHECK(store.Save(path));
    CHECK(std::filesystem::exists(path + ".bak"));

    SnippetStore loaded;
    REQUIRE(loaded.Load(path));
    CHECK(loaded.Entries().size() == 2);
    CHECK(loaded.Expand("alpha") == "one");
    CHECK(loaded.Expand("beta") == "two");
    CHECK(loaded.Entries()[0].id == store.Entries()[0].id);
    CHECK(loaded.Entries()[0].created_at == store.Entries()[0].created_at);
}

TEST_CASE("v1 array still loads and gains ids") {
    const std::filesystem::path dir = TestDir("v1");
    const std::string path = (dir / "snippets.json").string();
    WriteFile(path,
              R"([{"trigger": "sig", "expansion": "first"}, {"trigger": "email", "expansion": "e"}])");
    SnippetStore store;
    REQUIRE(store.Load(path));
    CHECK(store.Entries().size() == 2);
    CHECK_FALSE(store.Entries()[0].id.empty());
    CHECK(store.Expand("sig") == "first");
    CHECK(store.Expand("EMAIL") == "e");
}

TEST_CASE("duplicate triggers last wins and missing file is not loaded") {
    const std::filesystem::path dir = TestDir("duplock");
    const std::string path = (dir / "snippets.json").string();
    WriteFile(path,
              R"([{"trigger": "sig", "expansion": "first"}, {"trigger": "SIG", "expansion": "second"}])");
    SnippetStore store;
    REQUIRE(store.Load(path));
    CHECK(store.Entries().size() == 1);
    CHECK(store.Expand("sig") == "second");
    SnippetStore missing;
    CHECK_FALSE(missing.Load((dir / "nope.json").string()));
    CHECK_FALSE(missing.Loaded());
}

TEST_CASE("upsert with exclude id renames only that entry") {
    SnippetStore store;
    StoreWith(store, {{"alpha", "one"}, {"beta", "two"}});
    std::string alpha_id;
    std::string beta_id;
    for (const auto& entry : store.Entries()) {
        if (entry.trigger == "alpha") {
            alpha_id = entry.id;
        }
        if (entry.trigger == "beta") {
            beta_id = entry.id;
        }
    }
    REQUIRE_FALSE(alpha_id.empty());
    REQUIRE_FALSE(beta_id.empty());
    std::string error;
    REQUIRE(store.Upsert("gamma", "three", alpha_id, error));
    CHECK(store.Entries().size() == 2);
    CHECK(store.Expand("gamma") == "three");
    CHECK(store.Expand("alpha") == "alpha");
    CHECK(store.Expand("beta") == "two");
    bool kept_id = false;
    for (const auto& entry : store.Entries()) {
        if (entry.trigger == "gamma" && entry.id == alpha_id) {
            kept_id = true;
        }
    }
    CHECK(kept_id);
}

TEST_CASE("upsert rename onto another trigger does not delete the other entry") {
    SnippetStore store;
    StoreWith(store, {{"alpha", "one"}, {"beta", "two"}});
    std::string alpha_id;
    for (const auto& entry : store.Entries()) {
        if (entry.trigger == "alpha") {
            alpha_id = entry.id;
        }
    }
    REQUIRE_FALSE(alpha_id.empty());
    std::string error;
    CHECK_FALSE(store.Upsert("beta", "changed", alpha_id, error));
    CHECK(error == "Snippet already exists");
    CHECK(store.Entries().size() == 2);
    CHECK(store.Expand("alpha") == "one");
    CHECK(store.Expand("beta") == "two");
    REQUIRE(store.Upsert("beta", "", alpha_id, error));
    CHECK(store.Entries().size() == 1);
    CHECK(store.Expand("beta") == "two");
    CHECK(store.Expand("alpha") == "alpha");
}

TEST_CASE("remove by trigger removes only the matching entry") {
    SnippetStore store;
    StoreWith(store, {{"alpha", "one"}, {"beta", "two"}});
    store.RemoveByTrigger("ALPHA");
    CHECK(store.Entries().size() == 1);
    CHECK(store.Expand("beta") == "two");
    store.RemoveByTrigger("missing");
    CHECK(store.Entries().size() == 1);
}

TEST_CASE("contains mode equal length tie break is alphabetical ascending") {
    SnippetStore store;
    StoreWith(store, {{"bb", "cc"}, {"aa", "bb"}});
    store.SetMode("contains");
    CHECK(store.Expand("aa") == "cc");
}

TEST_CASE("expansion cap counts characters not bytes") {
    SnippetStore store;
    std::string wide;
    for (int i = 0; i < 4000; ++i) {
        wide += "\xE4\xBD\xA0";
    }
    CHECK(wide.size() == 12000);
    CHECK(store.Validate("word", wide) == "");
    std::string too_wide = wide + "\xE4\xBD\xA0";
    CHECK(store.Validate("word", too_wide) == "Snippet too long");
}

TEST_CASE("load generates ids that do not collide with file provided ids") {
    const std::filesystem::path dir = TestDir("idcollision");
    const std::string path = (dir / "snippets.json").string();
    WriteFile(path,
              R"([{"trigger": "first", "expansion": "one"}, {"id": "snippet-1", "trigger": "second", "expansion": "two"}])");
    SnippetStore store;
    REQUIRE(store.Load(path));
    REQUIRE(store.Entries().size() == 2);
    CHECK(store.Entries()[0].id != store.Entries()[1].id);
    store.RemoveById("snippet-1");
    CHECK(store.Entries().size() == 1);
    CHECK(store.Expand("first") == "one");
}

TEST_CASE("load tolerates non string id and timestamp fields") {
    const std::filesystem::path dir = TestDir("badtypes");
    const std::string path = (dir / "snippets.json").string();
    WriteFile(path,
              R"([{"id": 42, "trigger": "sig", "expansion": "first", "created_at": "yesterday"}])");
    SnippetStore store;
    REQUIRE(store.Load(path));
    REQUIRE(store.Entries().size() == 1);
    CHECK_FALSE(store.Entries()[0].id.empty());
    CHECK(store.Entries()[0].id != "42");
    CHECK(store.Expand("sig") == "first");
}
