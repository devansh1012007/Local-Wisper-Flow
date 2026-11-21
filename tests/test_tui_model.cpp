#include <doctest/doctest.h>

#include "core/SettingsStore.h"
#include "ui/tui/SettingsMenuModel.h"

#include <cstdio>
#include <fstream>
#include <set>
#include <string>

namespace {

std::string TempConfigPath(const char* tag) {
    return std::string("/tmp/lwf-test-tui-") + tag + ".json";
}

}  // namespace

TEST_CASE("the model groups cover every schema key exactly once") {
    const std::string path = TempConfigPath("coverage");
    std::remove(path.c_str());
    SettingsStore store(path);
    SettingsMenuModel model(store);

    std::set<std::string> seen;
    for (const std::string& group : model.group_names()) {
        CHECK_FALSE(group.empty());
    }
    // Walk every group and collect its rows.
    std::set<std::string> rows;
    for (int group = 0; group < static_cast<int>(model.group_names().size()); ++group) {
        model.Apply(SettingsMenuModel::Action::Down);
    }
    // Enter each group from the top by resetting through a fresh model.
    for (int group = 0; group < static_cast<int>(model.group_names().size()); ++group) {
        SettingsMenuModel walker(store);
        for (int step = 0; step < group; ++step) {
            walker.Apply(SettingsMenuModel::Action::Down);
        }
        walker.Apply(SettingsMenuModel::Action::Enter);
        REQUIRE(walker.page() == SettingsMenuModel::Page::Items);
        for (const auto& row : walker.item_rows()) {
            rows.insert(row.key);
        }
        walker.Apply(SettingsMenuModel::Action::Back);
        CHECK(walker.page() == SettingsMenuModel::Page::Groups);
    }
    for (const auto& entry : SettingsStore::Schema()) {
        CHECK(rows.count(entry.key) == 1);
    }
    CHECK(rows.size() == SettingsStore::Schema().size());
    std::remove(path.c_str());
}

TEST_CASE("navigation moves the cursor and stays in range") {
    const std::string path = TempConfigPath("nav");
    std::remove(path.c_str());
    SettingsStore store(path);
    SettingsMenuModel model(store);

    const int groups = static_cast<int>(model.group_names().size());
    REQUIRE(groups > 1);
    model.Apply(SettingsMenuModel::Action::Up);  // clamped at the top
    CHECK(model.group_cursor() == 0);
    model.Apply(SettingsMenuModel::Action::Down);
    CHECK(model.group_cursor() == 1);
    for (int i = 0; i < groups + 5; ++i) {
        model.Apply(SettingsMenuModel::Action::Down);
    }
    CHECK(model.group_cursor() == groups - 1);
    model.Apply(SettingsMenuModel::Action::PageUp);
    CHECK(model.group_cursor() < groups - 1);
    std::remove(path.c_str());
}

TEST_CASE("bool items toggle through Enter and mark the model dirty") {
    const std::string path = TempConfigPath("bool");
    std::remove(path.c_str());
    SettingsStore store(path);
    const bool original = store.GetBool("daemon_toasts");

    SettingsMenuModel model(store);
    // Navigate to the Linux daemon group, then to daemon_toasts.
    for (int group = 0; group < static_cast<int>(model.group_names().size()); ++group) {
        SettingsMenuModel finder(store);
        for (int step = 0; step < group; ++step) {
            finder.Apply(SettingsMenuModel::Action::Down);
        }
        finder.Apply(SettingsMenuModel::Action::Enter);
        for (int item = 0; item < static_cast<int>(finder.item_rows().size()); ++item) {
            if (finder.item_rows()[item].key == "daemon_toasts") {
                for (int move = 0; move < item; ++move) {
                    finder.Apply(SettingsMenuModel::Action::Down);
                }
                const bool before = store.GetBool("daemon_toasts");
                finder.Apply(SettingsMenuModel::Action::Enter);
                CHECK(store.GetBool("daemon_toasts") == !before);
                CHECK(finder.dirty());
                CHECK(finder.status().find("Daemon Toasts") != std::string::npos);
                group = static_cast<int>(model.group_names().size());  // stop outer loop
                break;
            }
        }
    }
    CHECK(store.GetBool("daemon_toasts") == !original);
    std::remove(path.c_str());
}

TEST_CASE("int editor clamps to the schema range and rejects garbage") {
    const std::string path = TempConfigPath("int");
    std::remove(path.c_str());
    SettingsStore store(path);

    // Find the Speech & STT group and the stt_timeout_ms row.
    SettingsMenuModel model(store);
    bool found = false;
    for (int group = 0; group < static_cast<int>(model.group_names().size()) && !found;
         ++group) {
        SettingsMenuModel walker(store);
        for (int step = 0; step < group; ++step) {
            walker.Apply(SettingsMenuModel::Action::Down);
        }
        walker.Apply(SettingsMenuModel::Action::Enter);
        for (int item = 0; item < static_cast<int>(walker.item_rows().size()); ++item) {
            if (walker.item_rows()[item].key == "stt_timeout_ms") {
                for (int move = 0; move < item; ++move) {
                    walker.Apply(SettingsMenuModel::Action::Down);
                }
                walker.Apply(SettingsMenuModel::Action::Enter);
                REQUIRE(walker.page() == SettingsMenuModel::Page::Editor);
                walker.SetEditorText("999999999");
                auto error = walker.CommitEditor();
                REQUIRE_FALSE(error.has_value());
                // Schema range for stt_timeout_ms is [1000, 60000].
                CHECK(store.GetInt("stt_timeout_ms") == 60000);
                walker.SetEditorText("banana");
                error = walker.CommitEditor();
                CHECK(error.has_value());
                // The invalid draft must not corrupt the store.
                CHECK(store.GetInt("stt_timeout_ms") == 60000);
                found = true;
                break;
            }
        }
    }
    CHECK(found);
    std::remove(path.c_str());
}

TEST_CASE("editor cancel leaves the value untouched") {
    const std::string path = TempConfigPath("cancel");
    std::remove(path.c_str());
    SettingsStore store(path);
    const std::string original = store.GetString("whisper_model_path");

    SettingsMenuModel model(store);
    model.Apply(SettingsMenuModel::Action::Down);  // any group is fine; find the key
    bool found = false;
    for (int group = 0; group < static_cast<int>(model.group_names().size()) && !found;
         ++group) {
        SettingsMenuModel walker(store);
        for (int step = 0; step < group; ++step) {
            walker.Apply(SettingsMenuModel::Action::Down);
        }
        walker.Apply(SettingsMenuModel::Action::Enter);
        for (int item = 0; item < static_cast<int>(walker.item_rows().size()); ++item) {
            if (walker.item_rows()[item].key == "whisper_model_path") {
                for (int move = 0; move < item; ++move) {
                    walker.Apply(SettingsMenuModel::Action::Down);
                }
                walker.Apply(SettingsMenuModel::Action::Enter);
                walker.SetEditorText("/new/path.bin");
                walker.Apply(SettingsMenuModel::Action::Back);
                CHECK(walker.page() == SettingsMenuModel::Page::Items);
                CHECK(store.GetString("whisper_model_path") == original);
                found = true;
                break;
            }
        }
    }
    CHECK(found);
    std::remove(path.c_str());
}

TEST_CASE("mark saved clears the dirty flag") {
    const std::string path = TempConfigPath("saved");
    std::remove(path.c_str());
    SettingsStore store(path);
    SettingsMenuModel model(store);
    CHECK_FALSE(model.dirty());
    store.SetBool("daemon_toasts", !store.GetBool("daemon_toasts"));
    // The model only marks dirty on its own edits; emulate one via the menu.
    model.Apply(SettingsMenuModel::Action::Enter);
    model.Apply(SettingsMenuModel::Action::Enter);  // toggles first item if bool
    model.MarkSaved(true);
    CHECK_FALSE(model.dirty());
    CHECK(model.status().find("saved") != std::string::npos);
    std::remove(path.c_str());
}

TEST_CASE("keysym hint renders for hotkey_keysym and is empty elsewhere") {
    const std::string path = TempConfigPath("hint");
    std::remove(path.c_str());
    SettingsStore store(path);
    store.SetInt("hotkey_keysym", 0xffe9);  // XK_Alt_L
    SettingsMenuModel model(store);

    bool found = false;
    for (int group = 0; group < static_cast<int>(model.group_names().size()) && !found;
         ++group) {
        SettingsMenuModel walker(store);
        for (int step = 0; step < group; ++step) {
            walker.Apply(SettingsMenuModel::Action::Down);
        }
        walker.Apply(SettingsMenuModel::Action::Enter);
        for (int item = 0; item < static_cast<int>(walker.item_rows().size()); ++item) {
            if (walker.item_rows()[item].key == "hotkey_keysym") {
                for (int move = 0; move < item; ++move) {
                    walker.Apply(SettingsMenuModel::Action::Down);
                }
                const std::string hint = walker.KeysymHint();
                CHECK(hint.find("0xffe9") != std::string::npos);
                CHECK(hint.find("LeftAlt") != std::string::npos);
                found = true;
                break;
            }
        }
    }
    CHECK(found);
    // On any other row the hint is empty.
    model.Apply(SettingsMenuModel::Action::Down);
    CHECK(model.KeysymHint().empty());
    std::remove(path.c_str());
}

TEST_CASE("tail file returns the last lines in order") {
    const std::string path = TempConfigPath("tail.log");
    {
        std::ofstream file(path, std::ios::trunc);
        for (int i = 1; i <= 10; ++i) {
            file << "line " << i << "\n";
        }
    }
    const std::vector<std::string> tail = SettingsMenuModel::TailFile(path, 4);
    REQUIRE(tail.size() == 4);
    CHECK(tail[0] == "line 7");
    CHECK(tail[3] == "line 10");
    CHECK(SettingsMenuModel::TailFile("/definitely/not/here.log", 4).empty());
    std::remove(path.c_str());
}
