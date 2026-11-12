#include "core/TransformStore.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path TestDir(const char* name) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / (std::string("lwf_transformstore_") + name);
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

TEST_CASE("missing file seeds ten empty slots and opt-in off") {
    const std::filesystem::path dir = TestDir("seed");
    const std::string path = (dir / "transforms.json").string();
    TransformStore store;
    REQUIRE(store.Load(path));
    CHECK(store.Entries().size() == 10);
    CHECK_FALSE(store.Enabled());
    // Followups-2: no built-ins ship any more - every slot is empty scaffolding
    // with its default shortcut and nothing is flagged built_in.
    for (const TransformStore::Entry& entry : store.Entries()) {
        CHECK(entry.name.empty());
        CHECK(entry.prompt.empty());
        CHECK_FALSE(entry.built_in);
        CHECK_FALSE(store.IsSlotEnabled(entry.slot));
    }
    CHECK(store.Entries()[0].slot == 1);
    CHECK(store.Entries()[0].shortcut == "Win+Alt+1");
    CHECK(store.Entries()[9].slot == 10);
    CHECK(store.Entries()[9].shortcut == "Win+Alt+0");
    CHECK(store.DiffShortcut() == "Win+Alt+O");
    CHECK(std::filesystem::exists(path));
    CHECK(ReadFile(path).find("\"version\": 2") != std::string::npos);

    TransformStore reloaded;
    REQUIRE(reloaded.Load(path));
    CHECK(reloaded.Entries()[0].name.empty());
    CHECK(reloaded.Entries()[1].name.empty());
    CHECK_FALSE(reloaded.Enabled());
}

TEST_CASE("legacy built-in entries are dropped on load while customs survive") {
    const std::filesystem::path dir = TestDir("dropbuiltins");
    const std::string path = (dir / "transforms.json").string();
    WriteFile(path,
              R"({"version": 2, "enabled": true, "diff_shortcut": "Win+Alt+O", "slots": [)"
              R"({"slot": 1, "name": "Polish", "prompt": "Improve clarity {text}", "shortcut": "Win+Alt+1", "built_in": true, "created_at": 1, "updated_at": 2}, )"
              R"({"slot": 2, "name": "Prompt Engineer", "prompt": "Rewrite as a prompt {text}", "shortcut": "Win+Alt+2", "built_in": true, "created_at": 1, "updated_at": 2}, )"
              R"({"slot": 4, "name": "Terse", "prompt": "Be terse {text}", "shortcut": "Win+Alt+4", "built_in": false, "created_at": 5, "updated_at": 6}]})");
    TransformStore store;
    REQUIRE(store.Load(path));
    // Followups-2 migration: the retired Polish / Prompt Engineer built-ins
    // vanish for existing installs too; user entries keep their slots.
    CHECK(store.Entries().size() == 10);
    CHECK(store.Entries()[0].name.empty());
    CHECK(store.Entries()[1].name.empty());
    CHECK(store.Entries()[3].name == "Terse");
    CHECK(store.Entries()[3].slot == 4);
    CHECK(store.Entries()[3].shortcut == "Win+Alt+4");
    CHECK(store.Enabled());
    bool warned = false;
    for (const std::string& warning : store.Warnings()) {
        if (warning.find("built-ins are retired") != std::string::npos) {
            warned = true;
        }
    }
    CHECK(warned);
}

TEST_CASE("custom prompt auto-appends text placeholder and caps at 2000 chars") {
    const std::filesystem::path dir = TestDir("append");
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
    const std::filesystem::path dir = TestDir("optin");
    const std::string path = (dir / "transforms.json").string();
    TransformStore store;
    REQUIRE(store.Load(path));
    CHECK_FALSE(store.Enabled());
    std::string error;
    REQUIRE(store.SetSlot(1, "Mine", "Do a thing {text}", error));
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
    const std::filesystem::path dir = TestDir("v1");
    const std::string path = (dir / "transforms.json").string();
    WriteFile(path,
              R"([{"name": "Fix Grammar", "prompt": "Fix grammar: {text}"}, {"name": "Summarize", "prompt": "Summarize this: {text}"}])");
    TransformStore store;
    REQUIRE(store.Load(path));
    CHECK(store.Entries().size() == 10);
    // Followups-2: no built-in slots are reserved, so v1 entries fill from 1.
    CHECK(store.Entries()[0].name == "Fix Grammar");
    CHECK(store.Entries()[0].slot == 1);
    CHECK(store.Entries()[0].shortcut == "Win+Alt+1");
    CHECK(store.Entries()[1].name == "Summarize");
    CHECK(store.Entries()[1].slot == 2);
    CHECK(store.Entries()[2].name.empty());
    CHECK_FALSE(store.Enabled());
    const std::string migrated = ReadFile(path);
    CHECK(migrated.find("\"version\": 2") != std::string::npos);
    CHECK(migrated.find("\"slots\"") != std::string::npos);
    CHECK(std::filesystem::exists(path + ".bak"));
    CHECK(ReadFile(path + ".bak").find("Fix Grammar") != std::string::npos);
}

TEST_CASE("v2 file with placeholder missing from a custom prompt gains it on load") {
    const std::filesystem::path dir = TestDir("placeholder");
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
    const std::filesystem::path dir = TestDir("conflict");
    TransformStore store;
    REQUIRE(store.Load((dir / "transforms.json").string()));
    std::string error;
    REQUIRE(store.SetSlot(1, "Custom", "Do a thing {text}", error));
    REQUIRE(store.SetSlot(3, "Third", "Do a third {text}", error));
    Binding exact;
    exact.role = BindingRole::PushToTalk;
    exact.label = "Win+Alt+1";
    store.SetHotkeyBindings({exact});
    CHECK(store.Conflicted(1));
    CHECK_FALSE(store.Conflicted(2));
    CHECK_FALSE(store.Conflicted(3));
    store.SetEnabled(true);
    CHECK_FALSE(store.IsSlotEnabled(1));
    CHECK(store.IsSlotEnabled(3));
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
    std::string other_error;
    REQUIRE(other.SetSlot(1, "First", "Do a thing {text}", other_error));
    REQUIRE(other.SetSlot(2, "Second", "Do another {text}", other_error));
    other.SetHotkeyBindings({*subset});
    CHECK(other.Conflicted(1));
    CHECK(other.Conflicted(2));
}

TEST_CASE("corrupt file does not throw and keeps defaults") {
    const std::filesystem::path dir = TestDir("corrupt");
    const std::string path = (dir / "transforms.json").string();
    WriteFile(path, "not json");
    TransformStore store;
    CHECK_FALSE(store.Load(path));
    CHECK(store.Entries().size() == 10);
    CHECK(store.Entries()[0].name.empty());
    CHECK(ReadFile(path) == "not json");
}

TEST_CASE("validate and set slot reject out of range slots and empty names") {
    const std::filesystem::path dir = TestDir("slotrange");
    TransformStore store;
    REQUIRE(store.Load((dir / "transforms.json").string()));
    CHECK(store.Validate(0, "Name", "prompt {text}") == "Invalid slot");
    CHECK(store.Validate(11, "Name", "prompt {text}") == "Invalid slot");
    CHECK(store.Validate(-1, "Name", "prompt {text}") == "Invalid slot");
    CHECK(store.Validate(3, "  ", "prompt {text}") == "Please enter a name");
    // Followups-2: the name rule is slot-uniform now that slots 1-2 are
    // ordinary user slots.
    CHECK(store.Validate(1, "  ", "prompt {text}") == "Please enter a name");
    CHECK(store.Validate(2, "", "prompt {text}") == "Please enter a name");
    CHECK(store.Validate(3, "", "") == "");
    std::string error;
    CHECK_FALSE(store.SetSlot(0, "Name", "prompt {text}", error));
    CHECK(error == "Invalid slot");
    CHECK_FALSE(store.SetSlot(11, "Name", "prompt {text}", error));
    CHECK(error == "Invalid slot");
    CHECK_FALSE(store.SetSlot(3, "  ", "prompt {text}", error));
    CHECK(error == "Please enter a name");
    CHECK(store.Validate(3, "Name", "") == "");
}

TEST_CASE("duplicate transform names are rejected by the store") {
    const std::filesystem::path dir = TestDir("dupname");
    TransformStore store;
    REQUIRE(store.Load((dir / "transforms.json").string()));
    std::string error;
    REQUIRE(store.SetSlot(3, "Shorten", "Make it short {text}", error));
    CHECK(store.Validate(4, "Shorten", "Other {text}") == "Transform name already exists");
    CHECK(store.Validate(4, "shorten", "Other {text}") == "Transform name already exists");
    CHECK(store.Validate(4, "Lengthen", "Other {text}") == "");
    CHECK(store.Validate(3, "Shorten", "Make it shorter {text}") == "");
    CHECK_FALSE(store.SetSlot(4, "Shorten", "Other {text}", error));
    CHECK(error == "Transform name already exists");
    CHECK(store.Entries()[3].name.empty());
    CHECK(store.SetSlot(4, "Lengthen", "Make it longer {text}", error));
    CHECK(store.Entries()[3].name == "Lengthen");
}

TEST_CASE("empty slot is never slot enabled even when opt-in is on") {
    const std::filesystem::path dir = TestDir("emptyslot");
    TransformStore store;
    REQUIRE(store.Load((dir / "transforms.json").string()));
    std::string error;
    REQUIRE(store.SetSlot(1, "Mine", "Do a thing {text}", error));
    store.SetEnabled(true);
    CHECK_FALSE(store.IsSlotEnabled(3));
    CHECK_FALSE(store.IsSlotEnabled(10));
    CHECK(store.IsSlotEnabled(1));
    CHECK_FALSE(store.IsSlotEnabled(0));
    CHECK_FALSE(store.IsSlotEnabled(11));
}

TEST_CASE("custom diff shortcut persists across save and load") {
    const std::filesystem::path dir = TestDir("diffshortcut");
    const std::string path = (dir / "transforms.json").string();
    TransformStore store;
    REQUIRE(store.Load(path));
    store.SetDiffShortcut("Win+Alt+D");
    CHECK(store.DiffShortcut() == "Win+Alt+D");
    REQUIRE(store.Save(path));
    TransformStore reloaded;
    REQUIRE(reloaded.Load(path));
    CHECK(reloaded.DiffShortcut() == "Win+Alt+D");
}

TEST_CASE("v1 overflow and empty and duplicate names are skipped with warnings") {
    const std::filesystem::path dir = TestDir("v1overflow");
    const std::string path = (dir / "transforms.json").string();
    std::string content = "[";
    for (int i = 0; i < 11; ++i) {
        if (i > 0) {
            content += ", ";
        }
        content += "{\"name\": \"Custom" + std::to_string(i) + "\", \"prompt\": \"Do it: {text}\"}";
    }
    content += "]";
    WriteFile(path, content);
    TransformStore store;
    REQUIRE(store.Load(path));
    // Followups-2: all ten slots are fillable; the eleventh v1 entry is
    // skipped with a warning.
    CHECK(store.Entries().size() == 10);
    CHECK(store.Entries()[0].name == "Custom0");
    CHECK(store.Entries()[9].name == "Custom9");
    TransformStore dup_store;
    const std::string dup_path = (dir / "dup.json").string();
    WriteFile(dup_path,
              R"([{"name": "Custom0", "prompt": "Do it: {text}"}, {"name": "Custom1", "prompt": "Do it: {text}"}, {"name": "", "prompt": "Skip me: {text}"}, {"name": "Custom0", "prompt": "Duplicate: {text}"}, {"name": "Named Like Anything", "prompt": "Clash: {text}"}, {"name": "Custom2", "prompt": "Do it: {text}"}])");
    REQUIRE(dup_store.Load(dup_path));
    CHECK(dup_store.Entries()[0].name == "Custom0");
    CHECK(dup_store.Entries()[1].name == "Custom1");
    CHECK(dup_store.Entries()[2].name == "Named Like Anything");
    CHECK(dup_store.Entries()[3].name == "Custom2");
    bool saw_overflow = false;
    bool saw_duplicate = false;
    for (const std::string& warning : store.Warnings()) {
        if (warning.find("only 10 slots") != std::string::npos) {
            saw_overflow = true;
        }
    }
    for (const std::string& warning : dup_store.Warnings()) {
        if (warning.find("duplicate transform name") != std::string::npos) {
            saw_duplicate = true;
        }
    }
    CHECK(saw_overflow);
    CHECK(saw_duplicate);
    bool any_polish = false;
    for (const auto& entry : store.Entries()) {
        if (entry.name == "Polish") {
            any_polish = true;
        }
    }
    CHECK_FALSE(any_polish);
}

TEST_CASE("v2 file with duplicate names keeps the first and drops the second") {
    const std::filesystem::path dir = TestDir("v2dupname");
    const std::string path = (dir / "transforms.json").string();
    WriteFile(path,
              R"({"version": 2, "enabled": true, "diff_shortcut": "Win+Alt+O", "slots": [{"slot": 3, "name": "Terse", "prompt": "Be terse {text}", "shortcut": "Win+Alt+3", "built_in": false, "created_at": 1, "updated_at": 2}, {"slot": 4, "name": "Terse", "prompt": "Be terse again {text}", "shortcut": "Win+Alt+4", "built_in": false, "created_at": 1, "updated_at": 2}]})");
    TransformStore store;
    REQUIRE(store.Load(path));
    CHECK(store.Entries()[2].name == "Terse");
    CHECK(store.Entries()[3].name.empty());
    bool warned = false;
    for (const std::string& warning : store.Warnings()) {
        if (warning.find("duplicate transform name") != std::string::npos) {
            warned = true;
        }
    }
    CHECK(warned);
    CHECK(store.Entries()[2].created_at == 1);
    CHECK(store.Entries()[2].updated_at == 2);
    CHECK(store.Entries()[3].name.empty());
}

TEST_CASE("prompt cap counts characters not bytes") {
    const std::filesystem::path dir = TestDir("promptchars");
    TransformStore store;
    REQUIRE(store.Load((dir / "transforms.json").string()));
    std::string wide;
    for (int i = 0; i < 2000; ++i) {
        wide += "\xE4\xBD\xA0";
    }
    CHECK(wide.size() == 6000);
    CHECK(store.Validate(3, "Wide", wide) == "");
    CHECK(store.Validate(3, "Wide", wide + "\xE4\xBD\xA0") == "Prompt too long");
}

TEST_CASE("token normalized duplicate shortcuts conflict") {
    const std::filesystem::path dir = TestDir("normshortcut");
    TransformStore store;
    REQUIRE(store.Load((dir / "transforms.json").string()));
    std::string error;
    TransformStore::Entry third;
    third.slot = 3;
    third.name = "Third";
    third.prompt = "Do third {text}";
    third.shortcut = "Ctrl+Alt+3";
    REQUIRE(store.SetSlot(third, error));
    TransformStore::Entry fourth;
    fourth.slot = 4;
    fourth.name = "Fourth";
    fourth.prompt = "Do fourth {text}";
    fourth.shortcut = "Control+Alt+3";
    REQUIRE(store.SetSlot(fourth, error));
    CHECK_FALSE(store.Conflicted(3));
    CHECK(store.Conflicted(4));
}
