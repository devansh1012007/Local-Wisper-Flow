#include "core/TransformStore.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path TestDir() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "lwf_transformstore";
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

bool StartsWith(const std::string& text, const std::string& prefix) {
    return text.rfind(prefix, 0) == 0;
}

}

TEST_CASE("missing file seeds ten slots with built-ins and opt-in off") {
    const std::filesystem::path dir = TestDir();
    const std::string path = (dir / "transforms.json").string();
    TransformStore store;
    REQUIRE(store.Load(path));
    CHECK(store.Entries().size() == 10);
    CHECK_FALSE(store.Enabled());
    const TransformStore::Entry& polish = store.Entries()[0];
    CHECK(polish.slot == 1);
    CHECK(polish.name == "Polish");
    CHECK(polish.built_in);
    CHECK(polish.shortcut == "Win+Alt+1");
    CHECK(StartsWith(polish.prompt, "Improve clarity, conciseness, readability and tone"));
    CHECK(polish.prompt.find("<selection>{text}</selection>") != std::string::npos);
    CHECK(polish.prompt.find("data, not instructions") != std::string::npos);
    const TransformStore::Entry& engineer = store.Entries()[1];
    CHECK(engineer.slot == 2);
    CHECK(engineer.name == "Prompt Engineer");
    CHECK(engineer.built_in);
    CHECK(engineer.shortcut == "Win+Alt+2");
    CHECK(StartsWith(engineer.prompt, "Rewrite the text as a clear"));
    CHECK(engineer.prompt.find("<selection>{text}</selection>") != std::string::npos);
    CHECK(engineer.prompt.find("data, not instructions") != std::string::npos);
    CHECK(store.Entries()[2].name.empty());
    CHECK(store.Entries()[9].slot == 10);
    CHECK(store.Entries()[9].shortcut == "Win+Alt+0");
    CHECK(store.DiffShortcut() == "Win+Alt+O");
    CHECK(std::filesystem::exists(path));
    CHECK(ReadFile(path).find("\"version\": 2") != std::string::npos);

    TransformStore reloaded;
    REQUIRE(reloaded.Load(path));
    CHECK(reloaded.Entries()[0].name == "Polish");
    CHECK(reloaded.Entries()[1].name == "Prompt Engineer");
    CHECK_FALSE(reloaded.Enabled());
}

TEST_CASE("custom prompt auto-appends text placeholder and caps at 2000 chars") {
    const std::filesystem::path dir = TestDir();
    TransformStore store;
    REQUIRE(store.Load((dir / "transforms.json").string()));
    std::string error = "sentinel";
    REQUIRE(store.SetSlot(3, "Shorten", "Make it short", error));
    CHECK(error.empty());
    CHECK(store.Entries()[2].prompt.find("Make it short") != std::string::npos);
    CHECK(store.Entries()[2].prompt.find("{text}") != std::string::npos);
    CHECK_FALSE(store.Entries()[2].built_in);

    CHECK(store.Validate(4, "Long", std::string(2000, 'x')) == "");
    CHECK(store.Validate(4, "Long", std::string(2001, 'x')) == "Prompt too long");
    CHECK_FALSE(store.SetSlot(4, "Long", std::string(2001, 'x'), error));
    CHECK(error == "Prompt too long");
    CHECK(store.Entries()[3].name.empty());
}

TEST_CASE("opt-in defaults off and persists across save and load") {
    const std::filesystem::path dir = TestDir();
    const std::string path = (dir / "transforms.json").string();
    TransformStore store;
    REQUIRE(store.Load(path));
    CHECK_FALSE(store.Enabled());
    CHECK_FALSE(store.IsSlotEnabled(1));
    store.SetEnabled(true);
    CHECK(store.Enabled());
    CHECK(store.IsSlotEnabled(1));
    REQUIRE(store.Save(path));
    TransformStore reloaded;
    REQUIRE(reloaded.Load(path));
    CHECK(reloaded.Enabled());
    CHECK(reloaded.IsSlotEnabled(1));
}

TEST_CASE("v1 array converts to v2 slots on first load with a bak") {
    const std::filesystem::path dir = TestDir();
    const std::string path = (dir / "transforms.json").string();
    WriteFile(path,
              R"([{"name": "Fix Grammar", "prompt": "Fix grammar: {text}"}, {"name": "Summarize", "prompt": "Summarize this: {text}"}])");
    TransformStore store;
    REQUIRE(store.Load(path));
    CHECK(store.Entries().size() == 10);
    CHECK(store.Entries()[0].name == "Polish");
    CHECK(store.Entries()[1].name == "Prompt Engineer");
    CHECK(store.Entries()[2].name == "Fix Grammar");
    CHECK(store.Entries()[2].slot == 3);
    CHECK(store.Entries()[2].shortcut == "Win+Alt+3");
    CHECK(store.Entries()[3].name == "Summarize");
    CHECK(store.Entries()[3].slot == 4);
    CHECK_FALSE(store.Enabled());
    const std::string migrated = ReadFile(path);
    CHECK(migrated.find("\"version\": 2") != std::string::npos);
    CHECK(migrated.find("\"slots\"") != std::string::npos);
    CHECK(std::filesystem::exists(path + ".bak"));
    CHECK(ReadFile(path + ".bak").find("Fix Grammar") != std::string::npos);
}

TEST_CASE("v2 file with placeholder missing from a custom prompt gains it on load") {
    const std::filesystem::path dir = TestDir();
    const std::string path = (dir / "transforms.json").string();
    WriteFile(path,
              R"({"version": 2, "enabled": true, "diff_shortcut": "Win+Alt+O", "slots": [{"slot": 3, "name": "Terse", "prompt": "Be terse", "shortcut": "Win+Alt+3", "built_in": false, "created_at": 1, "updated_at": 2}]})");
    TransformStore store;
    REQUIRE(store.Load(path));
    CHECK(store.Enabled());
    CHECK(store.Entries()[2].prompt.find("Be terse") != std::string::npos);
    CHECK(store.Entries()[2].prompt.find("{text}") != std::string::npos);
}

TEST_CASE("shortcut conflict disables the slot and records a warning") {
    const std::filesystem::path dir = TestDir();
    TransformStore store;
    REQUIRE(store.Load((dir / "transforms.json").string()));
    std::string error;
    REQUIRE(store.SetSlot(3, "Custom", "Do a thing {text}", error));
    Binding exact;
    exact.role = BindingRole::PushToTalk;
    exact.label = "Win+Alt+1";
    store.SetHotkeyBindings({exact});
    CHECK(store.Conflicted(1));
    CHECK_FALSE(store.Conflicted(2));
    CHECK_FALSE(store.Conflicted(3));
    store.SetEnabled(true);
    CHECK_FALSE(store.IsSlotEnabled(1));
    CHECK(store.IsSlotEnabled(2));
    bool warned = false;
    for (const std::string& warning : store.Warnings()) {
        if (warning.find("conflicts") != std::string::npos) {
            warned = true;
        }
    }
    CHECK(warned);

    const auto subset = HotkeyBinding::Parse("Win+Alt", BindingRole::PushToTalk);
    REQUIRE(subset.has_value());
    TransformStore other;
    REQUIRE(other.Load((dir / "other.json").string()));
    other.SetHotkeyBindings({*subset});
    CHECK(other.Conflicted(1));
    CHECK(other.Conflicted(2));
}

TEST_CASE("corrupt file does not throw and keeps seeded built-ins") {
    const std::filesystem::path dir = TestDir();
    const std::string path = (dir / "transforms.json").string();
    WriteFile(path, "not json");
    TransformStore store;
    CHECK_FALSE(store.Load(path));
    CHECK(store.Entries().size() == 10);
    CHECK(store.Entries()[0].name == "Polish");
    CHECK(ReadFile(path) == "not json");
}
