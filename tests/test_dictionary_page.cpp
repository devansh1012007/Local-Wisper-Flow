#include "ui/pages/DictionaryPage.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string TempDir(const char* name) {
    const std::string dir =
        (std::filesystem::temp_directory_path() / (std::string("lwf_dictpage_") + name)).string();
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

std::int64_t IdOf(const DictionaryStore& store, const std::string& phrase) {
    for (const DictionaryStore::Entry& entry : store.Entries()) {
        if (entry.phrase == phrase) {
            return entry.id;
        }
    }
    return 0;
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

std::string SortedImport() {
    return R"({"schema_version": 2, "entries": [)"
           R"({"id": 1, "phrase": "starr", "starred": true, "source": "manual", "usage_count": 0, "created_at": 100, "updated_at": 100},)"
           R"({"id": 2, "phrase": "newer", "starred": false, "source": "manual", "usage_count": 0, "created_at": 300, "updated_at": 300},)"
           R"({"id": 3, "phrase": "middle", "starred": false, "source": "manual", "usage_count": 0, "created_at": 200, "updated_at": 200}]})";
}

}

TEST_CASE("dictionary page add rejects duplicate with exact message") {
    const std::string dir = TempDir("adddup");
    DictionaryStore store(dir + "/dictionary.json");
    REQUIRE(store.Load());
    std::string error;
    REQUIRE(lwf::ui::pages::SubmitAdd(store, nullptr, "alpha", "", &error));
    REQUIRE(store.Size() == 2);
    CHECK_FALSE(lwf::ui::pages::SubmitAdd(store, nullptr, " ALPHA ", "other", &error));
    CHECK(error == "Word already exists.");
    REQUIRE(store.Size() == 2);
    CHECK_FALSE(lwf::ui::pages::SubmitAdd(store, nullptr, "  ", "", &error));
    CHECK(error == "phrase cannot be empty");
    REQUIRE(store.Size() == 2);
    CHECK(ContainsPhrase(lwf::ui::pages::VisibleEntries(store, lwf::ui::pages::DictionaryPageState{}), "alpha"));
}

TEST_CASE("dictionary page edit duplicates another phrase with exact message") {
    const std::string dir = TempDir("editdup");
    DictionaryStore store(dir + "/dictionary.json");
    REQUIRE(store.Load());
    std::string error;
    REQUIRE(lwf::ui::pages::SubmitAdd(store, nullptr, "alpha", "", &error));
    REQUIRE(lwf::ui::pages::SubmitAdd(store, nullptr, "beta", "", &error));
    const std::int64_t alpha_id = IdOf(store, "alpha");
    REQUIRE(alpha_id != 0);
    CHECK_FALSE(
        lwf::ui::pages::SubmitUpdate(store, nullptr, alpha_id, "BETA", "x", &error));
    CHECK(error == "Word already exists.");
    REQUIRE(ContainsPhrase(store.Entries(), "alpha"));
    REQUIRE(lwf::ui::pages::SubmitUpdate(store, nullptr, alpha_id, "gamma", "delta", &error));
    REQUIRE(ContainsPhrase(store.Entries(), "gamma"));
    CHECK_FALSE(
        lwf::ui::pages::SubmitUpdate(store, nullptr, 999999, "omega", "", &error));
    CHECK(error == "entry not found");
}

TEST_CASE("dictionary page search matches phrase and replacement case insensitively") {
    const std::string dir = TempDir("search");
    DictionaryStore store(dir + "/dictionary.json");
    REQUIRE(store.Load());
    std::string error;
    REQUIRE(lwf::ui::pages::SubmitAdd(store, nullptr, "kubernetes", "", &error));
    lwf::ui::pages::DictionaryPageState state;
    state.search = "WAY";
    const std::vector<DictionaryStore::Entry> by_replacement =
        lwf::ui::pages::VisibleEntries(store, state);
    REQUIRE(by_replacement.size() == 1);
    CHECK(by_replacement[0].phrase == "btw");
    state.search = "KUBE";
    const std::vector<DictionaryStore::Entry> by_phrase =
        lwf::ui::pages::VisibleEntries(store, state);
    REQUIRE(by_phrase.size() == 1);
    CHECK(by_phrase[0].phrase == "kubernetes");
    state.search = "missing-term";
    CHECK(lwf::ui::pages::VisibleEntries(store, state).empty());
    state.search = "";
    CHECK(lwf::ui::pages::VisibleEntries(store, state).size() == store.Size());
}

TEST_CASE("dictionary page starred entries sort first in every sort mode") {
    const std::string dir = TempDir("star");
    DictionaryStore store(dir + "/dictionary.json");
    REQUIRE(store.Load());
    REQUIRE(store.ImportJson(SortedImport(), nullptr));
    for (const lwf::ui::pages::DictionarySort sort :
         {lwf::ui::pages::DictionarySort::NameAsc, lwf::ui::pages::DictionarySort::Newest,
          lwf::ui::pages::DictionarySort::MostUsed}) {
        lwf::ui::pages::DictionaryPageState state;
        state.sort = sort;
        const std::vector<DictionaryStore::Entry> rows =
            lwf::ui::pages::VisibleEntries(store, state);
        REQUIRE(rows.size() == 3);
        CHECK(rows[0].phrase == "starr");
    }
    lwf::ui::pages::DictionaryPageState newest;
    newest.sort = lwf::ui::pages::DictionarySort::Newest;
    const std::vector<DictionaryStore::Entry> newest_rows =
        lwf::ui::pages::VisibleEntries(store, newest);
    CHECK(newest_rows[1].phrase == "newer");
    CHECK(newest_rows[2].phrase == "middle");
}

TEST_CASE("dictionary page sort modes order entries within the starred band") {
    const std::string dir = TempDir("sortmodes");
    DictionaryStore store(dir + "/dictionary.json");
    REQUIRE(store.Load());
    std::string error;
    REQUIRE(store.ImportJson(
        R"({"schema_version": 2, "entries": [)"
        R"({"id": 1, "phrase": "zzz", "starred": false, "source": "manual", "usage_count": 9, "created_at": 300, "updated_at": 300},)"
        R"({"id": 2, "phrase": "aaa", "starred": false, "source": "manual", "usage_count": 1, "created_at": 100, "updated_at": 100},)"
        R"({"id": 3, "phrase": "mmm", "starred": false, "source": "manual", "usage_count": 5, "created_at": 200, "updated_at": 200}]})",
        nullptr));
    lwf::ui::pages::DictionaryPageState state;
    state.sort = lwf::ui::pages::DictionarySort::NameAsc;
    const std::vector<DictionaryStore::Entry> named =
        lwf::ui::pages::VisibleEntries(store, state);
    REQUIRE(named.size() == 3);
    CHECK(named[0].phrase == "aaa");
    CHECK(named[1].phrase == "mmm");
    CHECK(named[2].phrase == "zzz");
    state.sort = lwf::ui::pages::DictionarySort::MostUsed;
    const std::vector<DictionaryStore::Entry> used =
        lwf::ui::pages::VisibleEntries(store, state);
    REQUIRE(used.size() == 3);
    CHECK(used[0].phrase == "zzz");
    CHECK(used[1].phrase == "mmm");
    CHECK(used[2].phrase == "aaa");
}

TEST_CASE("dictionary page top phrases caps at five with the highest usage first") {
    const std::string dir = TempDir("chips");
    DictionaryStore store(dir + "/dictionary.json");
    REQUIRE(store.Load());
    std::string json =
        R"({"schema_version": 2, "entries": [)"
        R"({"id": 1, "phrase": "p1", "starred": false, "source": "manual", "usage_count": 7, "created_at": 1, "updated_at": 1},)"
        R"({"id": 2, "phrase": "p2", "starred": false, "source": "manual", "usage_count": 6, "created_at": 2, "updated_at": 2},)"
        R"({"id": 3, "phrase": "p3", "starred": false, "source": "manual", "usage_count": 5, "created_at": 3, "updated_at": 3},)"
        R"({"id": 4, "phrase": "p4", "starred": false, "source": "manual", "usage_count": 4, "created_at": 4, "updated_at": 4},)"
        R"({"id": 5, "phrase": "p5", "starred": false, "source": "manual", "usage_count": 3, "created_at": 5, "updated_at": 5},)"
        R"({"id": 6, "phrase": "p6", "starred": false, "source": "manual", "usage_count": 2, "created_at": 6, "updated_at": 6},)"
        R"({"id": 7, "phrase": "p7", "starred": false, "source": "manual", "usage_count": 1, "created_at": 7, "updated_at": 7},)"
        R"({"id": 8, "phrase": "unused", "starred": false, "source": "manual", "usage_count": 0, "created_at": 8, "updated_at": 8}]})";
    REQUIRE(store.ImportJson(json, nullptr));
    const std::vector<std::string> chips = lwf::ui::pages::TopPhrases(store, 5);
    REQUIRE(chips.size() == 5);
    CHECK(chips[0] == "p1");
    CHECK(chips[1] == "p2");
    CHECK(chips[4] == "p5");
    CHECK(std::find(chips.begin(), chips.end(), "p6") == chips.end());
    CHECK(std::find(chips.begin(), chips.end(), "unused") == chips.end());
}

TEST_CASE("dictionary page banner reappears when the list becomes empty again") {
    const std::string dir = TempDir("banner");
    DictionaryStore store(dir + "/dictionary.json");
    REQUIRE(store.Load());
    lwf::ui::pages::DictionaryPageState state;
    state.banner_closed = true;
    CHECK_FALSE(lwf::ui::pages::VisibleBanner(store, state));
    REQUIRE(store.Remove(store.Entries()[0].id));
    REQUIRE(store.Size() == 0);
    CHECK(lwf::ui::pages::VisibleBanner(store, state));
}

TEST_CASE("dictionary page undo hides a deleted entry without removing it") {
    const std::string dir = TempDir("undo");
    DictionaryStore store(dir + "/dictionary.json");
    REQUIRE(store.Load());
    const std::int64_t id = store.Entries()[0].id;
    lwf::ui::pages::DictionaryPageState state;
    REQUIRE(lwf::ui::pages::RequestDelete(store, state, id, 100.0f));
    CHECK(lwf::ui::pages::VisibleEntries(store, state).empty());
    REQUIRE(store.Size() == 1);
    CHECK(ContainsPhrase(store.Entries(), "btw"));
    lwf::ui::pages::UndoPendingDelete(state);
    CHECK_FALSE(state.pending_delete);
    const std::vector<DictionaryStore::Entry> rows =
        lwf::ui::pages::VisibleEntries(store, state);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].phrase == "btw");
    REQUIRE(store.Size() == 1);
}

TEST_CASE("dictionary page delete commits only after the five second undo window") {
    CHECK(lwf::ui::studio_metrics::toast_dwell_s == 5.0f);
    const std::string dir = TempDir("expiry");
    DictionaryStore store(dir + "/dictionary.json");
    REQUIRE(store.Load());
    const std::int64_t id = store.Entries()[0].id;
    lwf::ui::pages::DictionaryPageState state;
    REQUIRE(lwf::ui::pages::RequestDelete(store, state, id, 100.0f));
    CHECK(state.delete_at == 105.0f);
    CHECK_FALSE(lwf::ui::pages::CommitExpiredPending(store, state, 104.99f));
    REQUIRE(store.Size() == 1);
    REQUIRE(state.pending_delete);
    CHECK(lwf::ui::pages::CommitExpiredPending(store, state, 105.0f));
    CHECK(store.Size() == 0);
    CHECK_FALSE(state.pending_delete);
}

TEST_CASE("dictionary page second delete commits the first pending delete") {
    const std::string dir = TempDir("twodelete");
    DictionaryStore store(dir + "/dictionary.json");
    REQUIRE(store.Load());
    REQUIRE(store.ImportJson(
        R"({"schema_version": 2, "entries": [)"
        R"({"id": 1, "phrase": "alpha", "starred": false, "source": "manual", "usage_count": 0, "created_at": 1, "updated_at": 1},)"
        R"({"id": 2, "phrase": "beta", "starred": false, "source": "manual", "usage_count": 0, "created_at": 2, "updated_at": 2}]})",
        nullptr));
    lwf::ui::pages::DictionaryPageState state;
    REQUIRE(lwf::ui::pages::RequestDelete(store, state, 1, 100.0f));
    REQUIRE(lwf::ui::pages::RequestDelete(store, state, 2, 101.0f));
    CHECK(store.Size() == 1);
    CHECK_FALSE(ContainsPhrase(store.Entries(), "alpha"));
    CHECK(ContainsPhrase(store.Entries(), "beta"));
    CHECK(lwf::ui::pages::VisibleEntries(store, state).empty());
    CHECK(lwf::ui::pages::CommitExpiredPending(store, state, 106.0f));
    CHECK(store.Size() == 0);
}

TEST_CASE("dictionary page refresh reloads external disk changes") {
    const std::string dir = TempDir("refresh");
    DictionaryStore store(dir + "/dictionary.json");
    REQUIRE(store.Load());
    WriteFile(dir + "/dictionary.json",
              R"({"schema_version": 2, "entries": [{"id": 1, "phrase": "gamma", "source": )"
              R"("manual", "usage_count": 0, "created_at": 1, "updated_at": 1}]})");
    REQUIRE(store.Reload());
    lwf::ui::pages::DictionaryPageState state;
    const std::vector<DictionaryStore::Entry> rows =
        lwf::ui::pages::VisibleEntries(store, state);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].phrase == "gamma");
}

TEST_CASE("dictionary page add saves and reloads the pipeline dictionary") {
    const std::string dir = TempDir("pipeline");
    const std::string path = dir + "/dictionary.json";
    DictionaryStore store(path);
    REQUIRE(store.Load());
    REQUIRE(store.Save());
    PersonalDictionary dict;
    REQUIRE(dict.Load(path));
    std::string error;
    REQUIRE(lwf::ui::pages::SubmitAdd(store, &dict, "gihub", "GitHub", &error));
    bool has_gihub = false;
    for (const std::string& word : dict.Words()) {
        if (word == "gihub") {
            has_gihub = true;
        }
    }
    CHECK(has_gihub);
    CHECK(dict.Enforce("gihub today", true) == "GitHub today");
    CHECK(ReadFile(path).find("gihub") != std::string::npos);
}

TEST_CASE("dictionary page tabs share the local list while sharing is unavailable") {
    const std::string dir = TempDir("tabs");
    DictionaryStore store(dir + "/dictionary.json");
    REQUIRE(store.Load());
    std::string error;
    REQUIRE(lwf::ui::pages::SubmitAdd(store, nullptr, "alpha", "", &error));
    lwf::ui::pages::DictionaryPageState all_state;
    all_state.tab = lwf::ui::pages::DictionaryTab::All;
    lwf::ui::pages::DictionaryPageState personal_state;
    personal_state.tab = lwf::ui::pages::DictionaryTab::Personal;
    CHECK(lwf::ui::pages::VisibleEntries(store, all_state).size() == 2);
    CHECK(lwf::ui::pages::VisibleEntries(store, personal_state).size() == 2);
}

TEST_CASE("dictionary page sources stay off the font scale ban and carry no copied copy") {
    const std::string header = ReadFile("src/ui/pages/DictionaryPage.h");
    const std::string impl = ReadFile("src/ui/pages/DictionaryPage.cpp");
    REQUIRE_FALSE(header.empty());
    REQUIRE_FALSE(impl.empty());
    CHECK(header.find("SetWindowFontScale") == std::string::npos);
    CHECK(impl.find("SetWindowFontScale") == std::string::npos);
    CHECK(header.find("#include \"imgui.h\"") == std::string::npos);
    CHECK(impl.find("Wispr") == std::string::npos);
    CHECK(impl.find("wispr") == std::string::npos);
    CHECK(impl.find("Dictionary") != std::string::npos);
    CHECK(impl.find("Add new") != std::string::npos);
    CHECK(impl.find("Word or phrase") != std::string::npos);
    CHECK(impl.find("Replace with") != std::string::npos);
    CHECK(impl.find("Share with team") != std::string::npos);
    CHECK(impl.find("Not available locally") != std::string::npos);
    CHECK(impl.find("Name (A-Z)") != std::string::npos);
    CHECK(impl.find("Newest") != std::string::npos);
    CHECK(impl.find("Most-used") != std::string::npos);
    CHECK(impl.find("Reload") != std::string::npos);
    CHECK(impl.find("PrimaryButton") != std::string::npos);
    CHECK(impl.find("TabRow") != std::string::npos);
    CHECK(impl.find("OutlinePill") != std::string::npos);
    CHECK(impl.find("LightButton") != std::string::npos);
    CHECK(impl.find("Banner") != std::string::npos);
    CHECK(impl.find("Chip") != std::string::npos);
    CHECK(impl.find("\xE2\x86\x92") != std::string::npos);
}

TEST_CASE("dictionary rows compensate cursor restore by ItemSpacing before Dummy") {
    const std::string dict = ReadFile("src/ui/pages/DictionaryPage.cpp");
    const std::string snips = ReadFile("src/ui/pages/SnippetsPage.cpp");
    const std::string insights = ReadFile("src/ui/pages/InsightsPage.cpp");
    REQUIRE_FALSE(dict.empty());
    REQUIRE_FALSE(snips.empty());
    REQUIRE_FALSE(insights.empty());
    const std::string dummy = "ImGui::Dummy(ImVec2(0.0f, 0.0f))";
    const std::string spacing = "- ImGui::GetStyle().ItemSpacing.y";
    CHECK(dict.find(spacing) != std::string::npos);
    CHECK(snips.find(spacing) != std::string::npos);
    CHECK(insights.find(spacing) != std::string::npos);
    const std::string saved_restore =
        "ImGui::SetCursorScreenPos(ImVec2(saved_cursor.x, saved_cursor.y - ImGui::GetStyle().ItemSpacing.y));";
    const std::string insights_restore =
        "ImGui::SetCursorScreenPos(ImVec2(row_pos.x, row_pos.y + row_h + 8.0f * scale - ImGui::GetStyle().ItemSpacing.y));";
    const std::size_t dict_pos = dict.find(saved_restore);
    const std::size_t snips_pos = snips.find(saved_restore);
    const std::size_t insights_pos = insights.find(insights_restore);
    REQUIRE(dict_pos != std::string::npos);
    REQUIRE(snips_pos != std::string::npos);
    REQUIRE(insights_pos != std::string::npos);
    const auto dummy_within_two_lines =
        [&dummy](const std::string& content, std::size_t restore_pos, const std::string& restore) {
            const std::size_t dummy_pos = content.find(dummy, restore_pos + restore.size());
            if (dummy_pos == std::string::npos) {
                return false;
            }
            std::size_t newlines = 0;
            for (std::size_t i = restore_pos; i < dummy_pos; ++i) {
                if (content[i] == '\n') {
                    ++newlines;
                }
            }
            return newlines <= 2;
        };
    CHECK(dummy_within_two_lines(dict, dict_pos, saved_restore));
    CHECK(dummy_within_two_lines(snips, snips_pos, saved_restore));
    CHECK(dummy_within_two_lines(insights, insights_pos, insights_restore));
}
