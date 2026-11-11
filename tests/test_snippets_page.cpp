#include "ui/pages/SnippetsPage.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace {

std::filesystem::path TestDir(const char* name) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / (std::string("lwf_snippets_page_") + name);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string ReadFile(const std::string& path) {
    std::ifstream file(path);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::string ReadSourceFrom(const std::string& root, const std::string& relative) {
    std::ifstream file(root + "/" + relative);
    if (!file.is_open()) {
        file.open(relative);
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

// T-004: LWF_SOURCE_DIR first with a bare-relative fallback (pattern from
// tests/test_transforms_page.cpp:30-44) for source scans, so they work from
// the repo root and from build/ alike; temp-file tests above keep using the
// bare ReadFile because their paths are absolute.
std::string ReadSource(const std::string& relative) {
#ifdef LWF_SOURCE_DIR
    return ReadSourceFrom(LWF_SOURCE_DIR, relative);
#else
    return ReadSourceFrom(".", relative);
#endif
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

bool HasTrigger(const std::vector<SnippetStore::Entry>& entries, const std::string& trigger) {
    for (const SnippetStore::Entry& entry : entries) {
        if (entry.trigger == trigger) {
            return true;
        }
    }
    return false;
}

}

TEST_CASE("snippets page banner copy pins the example pair and existing subtitle") {
    CHECK(std::string(lwf::ui::pages::kSnippetsBannerExampleTrigger) == "my booking link");
    CHECK(std::string(lwf::ui::pages::kSnippetsBannerExampleExpansion) ==
          "https://example.com/book");
    CHECK(std::string(lwf::ui::pages::kSnippetsBannerHeadline) == "Reusable phrases.");
    CHECK(std::string(lwf::ui::pages::kSnippetsAddLabel) == "Add new snippet");
    CHECK(lwf::ui::pages::VisibleBanner(lwf::ui::pages::SnippetsPageState{}));
    lwf::ui::pages::SnippetsPageState closed;
    closed.banner_closed = true;
    CHECK_FALSE(lwf::ui::pages::VisibleBanner(closed));
}

TEST_CASE("snippets page row text joins trigger and expansion with the arrow") {
    SnippetStore::Entry entry;
    entry.trigger = "my email";
    entry.expansion = "addr@example.test";
    CHECK(lwf::ui::pages::RowText(entry) == "my email \xE2\x86\x92 addr@example.test");
    CHECK(lwf::ui::pages::RowText(entry).find("\xE2\x86\x92") != std::string::npos);
}

TEST_CASE("snippets page truncation counts codepoints and keeps the ellipsis") {
    CHECK(lwf::ui::pages::CountCodePoints("") == 0);
    CHECK(lwf::ui::pages::CountCodePoints("abc") == 3);
    CHECK(lwf::ui::pages::CountCodePoints("h\xc3\xa9llo") == 5);
    CHECK(lwf::ui::pages::TruncateCodePoints("hello world", 8) == "hello wo...");
    CHECK(lwf::ui::pages::TruncateCodePoints("short", 8) == "short");
    CHECK(lwf::ui::pages::TruncateCodePoints("exactfit", 8) == "exactfit");
}

TEST_CASE("snippets page submit surfaces the four spec validation messages exactly") {
    SnippetStore store;
    StoreWith(store, {{"alpha", "bravo"}});
    std::string error = "sentinel";
    CHECK_FALSE(lwf::ui::pages::SubmitSnippet(store, "", "expansion", "", &error));
    CHECK(error == "Please enter a word");
    CHECK_FALSE(lwf::ui::pages::SubmitSnippet(store, "word", "", "", &error));
    CHECK(error == "Please enter a snippet");
    CHECK_FALSE(lwf::ui::pages::SubmitSnippet(store, "word", std::string(4001, 'x'), "", &error));
    CHECK(error == "Snippet too long");
    CHECK_FALSE(lwf::ui::pages::SubmitSnippet(store, "bravo", "fresh", "", &error));
    CHECK(error == "Snippet already exists");
    CHECK_FALSE(lwf::ui::pages::SubmitSnippet(store, "fresh", "alpha", "", &error));
    CHECK(error == "Snippet already exists");
    CHECK(store.Entries().size() == 1);
    CHECK(lwf::ui::pages::SubmitSnippet(store, "charlie", "delta", "", &error));
    CHECK(error.empty());
    CHECK(store.Entries().size() == 2);
}

TEST_CASE("snippets page validation order puts the length message before the empty expansion") {
    SnippetStore store;
    std::string error;
    CHECK_FALSE(lwf::ui::pages::SubmitSnippet(store, "word", std::string(4001, ' '), "", &error));
    CHECK(error == "Snippet too long");
    CHECK_FALSE(lwf::ui::pages::SubmitSnippet(store, "   ", "", "", &error));
    CHECK(error == "Please enter a word");
}

TEST_CASE("snippets page editor validates through the store and persists the shared instance") {
    SnippetStore store;
    lwf::ui::pages::SnippetsPage page(store);
    CHECK_FALSE(page.IsEditing());
    page.OpenAdd();
    CHECK(page.IsEditing());
    CHECK(page.EditingId().empty());
    page.SetEditTrigger("my email");
    page.SetEditExpansion("addr@example.test");
    CHECK(page.SaveEdit());
    CHECK_FALSE(page.IsEditing());
    CHECK(store.Expand("my email") == "addr@example.test");
    CHECK(store.Expand("MY EMAIL") == "addr@example.test");
    REQUIRE(store.Entries().size() == 1);
    const std::string id = store.Entries()[0].id;
    page.OpenEdit(id);
    CHECK(page.IsEditing());
    CHECK(page.EditingId() == id);
    CHECK(page.EditTrigger() == "my email");
    CHECK(page.EditExpansion() == "addr@example.test");
    page.SetEditTrigger("my inbox");
    CHECK(page.SaveEdit());
    CHECK(store.Expand("my inbox") == "addr@example.test");
    CHECK(store.Expand("my email") == "my email");
    page.OpenEdit(store.Entries()[0].id);
    page.SetEditTrigger("");
    CHECK_FALSE(page.SaveEdit());
    CHECK(page.EditError() == "Please enter a word");
    CHECK(page.IsEditing());
}

TEST_CASE("snippets page editor rejects duplicates but allows editing the entry itself") {
    SnippetStore store;
    StoreWith(store, {{"alpha", "one"}, {"beta", "two"}});
    const std::string alpha_id = store.Entries()[0].id;
    lwf::ui::pages::SnippetsPage page(store);
    page.OpenEdit(alpha_id);
    page.SetEditTrigger("ALPHA");
    page.SetEditExpansion("one updated");
    CHECK(page.SaveEdit());
    CHECK(store.Expand("alpha") == "one updated");
    page.OpenEdit(store.Entries()[0].id);
    page.SetEditTrigger("beta");
    CHECK_FALSE(page.SaveEdit());
    CHECK(page.EditError() == "Snippet already exists");
    CHECK(store.Expand("alpha") == "one updated");
    CHECK(store.Expand("beta") == "two");
}

TEST_CASE("snippets page longest match is delegated to the store in contains mode") {
    SnippetStore store;
    StoreWith(store, {{"email", "E"}, {"my email", "ME"}});
    store.SetMode("contains");
    CHECK(store.Expand("send my email please") == "send ME please");
    CHECK(store.Expand("send email please") == "send E please");
}

TEST_CASE("snippets page casing is preserved while triggers match case-insensitively") {
    SnippetStore store;
    StoreWith(store, {{"sign", "Hello World!!"}});
    CHECK(store.Expand("sign") == "Hello World!!");
    CHECK(store.Expand("SIGN") == "Hello World!!");
    store.SetMode("contains");
    CHECK(store.Expand("say Sign please") == "say Hello World!! please");
}

TEST_CASE("snippets page punctuation rules delegate to the store") {
    SnippetStore store;
    StoreWith(store, {{"my email", "addr@example.test"}});
    CHECK(store.Expand("My Email!!") == "addr@example.test");
    CHECK(store.Expand("my    email") == "addr@example.test");
    CHECK(store.Expand("please my email") == "please my email");
    store.SetMode("contains");
    CHECK(store.Expand("my, email") == "my, email");
}

TEST_CASE("snippets page delete removes now and undo restores the content") {
    SnippetStore store;
    StoreWith(store, {{"my booking link", "https://example.com/book"}});
    const std::string id = store.Entries()[0].id;
    lwf::ui::pages::SnippetsPageState state;
    REQUIRE(lwf::ui::pages::RequestDelete(store, state, id, 100.0f));
    CHECK(state.pending_delete);
    CHECK(state.pending_entry.trigger == "my booking link");
    CHECK(state.pending_entry.expansion == "https://example.com/book");
    CHECK(store.Entries().empty());
    CHECK(state.delete_at == 105.0f);
    std::string error;
    REQUIRE(lwf::ui::pages::UndoPendingDelete(store, state, &error));
    CHECK_FALSE(state.pending_delete);
    REQUIRE(store.Entries().size() == 1);
    CHECK(store.Entries()[0].trigger == "my booking link");
    CHECK(store.Entries()[0].expansion == "https://example.com/book");
    CHECK(store.Expand("my booking link") == "https://example.com/book");
    CHECK_FALSE(lwf::ui::pages::UndoPendingDelete(store, state, &error));
}

TEST_CASE("snippets page delete stash expires after the five second undo window") {
    CHECK(lwf::ui::studio_metrics::toast_dwell_s == 5.0f);
    SnippetStore store;
    StoreWith(store, {{"alpha", "om"}, {"beta", "two"}});
    const std::string id = store.Entries()[0].id;
    lwf::ui::pages::SnippetsPageState state;
    REQUIRE(lwf::ui::pages::RequestDelete(store, state, id, 100.0f));
    CHECK(state.delete_at == 105.0f);
    CHECK_FALSE(lwf::ui::pages::CommitExpiredPending(state, 104.99f));
    CHECK(state.pending_delete);
    CHECK(lwf::ui::pages::CommitExpiredPending(state, 105.0f));
    CHECK_FALSE(state.pending_delete);
    CHECK(store.Entries().size() == 1);
    CHECK_FALSE(lwf::ui::pages::CommitExpiredPending(state, 105.0f));
}

TEST_CASE("snippets page second delete replaces the first stash") {
    SnippetStore store;
    StoreWith(store, {{"alpha", "one"}, {"beta", "two"}});
    const std::string alpha_id = store.Entries()[0].id;
    const std::string beta_id = store.Entries()[1].id;
    lwf::ui::pages::SnippetsPageState state;
    REQUIRE(lwf::ui::pages::RequestDelete(store, state, alpha_id, 100.0f));
    REQUIRE(lwf::ui::pages::RequestDelete(store, state, beta_id, 101.0f));
    CHECK(state.pending_entry.trigger == "beta");
    CHECK(store.Entries().empty());
    std::string error;
    REQUIRE(lwf::ui::pages::UndoPendingDelete(store, state, &error));
    REQUIRE(store.Entries().size() == 1);
    CHECK(store.Entries()[0].trigger == "beta");
    CHECK_FALSE(HasTrigger(store.Entries(), "alpha"));
}

TEST_CASE("snippets page delete of an unknown id changes nothing") {
    SnippetStore store;
    StoreWith(store, {{"alpha", "one"}});
    lwf::ui::pages::SnippetsPageState state;
    CHECK_FALSE(lwf::ui::pages::RequestDelete(store, state, "missing-id", 100.0f));
    CHECK_FALSE(state.pending_delete);
    CHECK(store.Entries().size() == 1);
}

TEST_CASE("snippets page visible entries hide the pending delete") {
    SnippetStore store;
    StoreWith(store, {{"alpha", "om"}, {"beta", "two"}});
    const std::string id = store.Entries()[0].id;
    lwf::ui::pages::SnippetsPageState state;
    REQUIRE(lwf::ui::pages::RequestDelete(store, state, id, 100.0f));
    const std::vector<SnippetStore::Entry> rows = lwf::ui::pages::VisibleEntries(store, state);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].trigger == "beta");
}

TEST_CASE("snippets page mutations save through the loaded store path") {
    const std::filesystem::path dir = TestDir("savenow");
    const std::string path = (dir / "snippets.json").string();
    SnippetStore store;
    REQUIRE(store.Save(path));
    REQUIRE(store.Load(path));
    std::string error;
    REQUIRE(lwf::ui::pages::SubmitSnippet(store, "my link", "https://example.com/join", "", &error));
    CHECK(ReadFile(path).find("my link") != std::string::npos);
    SnippetStore reloaded;
    REQUIRE(reloaded.Load(path));
    CHECK(reloaded.Expand("my link") == "https://example.com/join");
    lwf::ui::pages::SnippetsPageState state;
    REQUIRE(lwf::ui::pages::RequestDelete(store, state, reloaded.Entries()[0].id, 200.0f));
    CHECK(ReadFile(path).find("my link") == std::string::npos);
    REQUIRE(lwf::ui::pages::UndoPendingDelete(store, state, &error));
    CHECK(ReadFile(path).find("my link") != std::string::npos);
}

TEST_CASE("snippets page save and load roundtrip keeps ids and shape") {
    const std::filesystem::path dir = TestDir("roundtrip");
    const std::string path = (dir / "snippets.json").string();
    SnippetStore store;
    std::string error;
    REQUIRE(lwf::ui::pages::SubmitSnippet(store, "alpha", "one", "", &error));
    REQUIRE(lwf::ui::pages::SubmitSnippet(store, "beta", "two", "", &error));
    REQUIRE(store.Save(path));
    const std::string text = ReadFile(path);
    CHECK(text.find("\"version\": 2") != std::string::npos);
    CHECK(text.find("\"trigger\"") != std::string::npos);
    SnippetStore loaded;
    REQUIRE(loaded.Load(path));
    CHECK(loaded.Entries().size() == 2);
    CHECK(loaded.Expand("alpha") == "one");
    CHECK(loaded.Entries()[0].id == store.Entries()[0].id);
}

TEST_CASE("snippets page sources stay off the font scale ban and carry the brief copy") {
    const std::string header = ReadSource("src/ui/pages/SnippetsPage.h");
    const std::string impl = ReadSource("src/ui/pages/SnippetsPage.cpp");
    REQUIRE_FALSE(header.empty());
    REQUIRE_FALSE(impl.empty());
    for (const std::string& text : {header, impl}) {
        CHECK(text.find("SetWindowFontScale") == std::string::npos);
        CHECK(text.find("windows.h") == std::string::npos);
        CHECK(text.find("TODO") == std::string::npos);
        CHECK(text.find("FIXME") == std::string::npos);
        CHECK(text.find("Log::") == std::string::npos);
        CHECK(text.find("Wispr") == std::string::npos);
    }
    CHECK(header.find("#include \"imgui.h\"") == std::string::npos);
    CHECK(header.find("PushFont") == std::string::npos);
    // T-004 pin flip (P-005) + W2-IMPL-02: typography must go through
    // design-px tokens. The 15px row text maps to
    // studio_metrics::font_card_title_px and the 13px detail text to
    // studio_metrics::font_helper_px (same-value tokens), so the old
    // metrics requirement is reversed and studio fonts are required.
    // Anchored form: a loose "metrics::font_" substring would match
    // studio_metrics:: alone and prove nothing.
    CHECK(impl.find("PushFont(nullptr, studio_metrics::") != std::string::npos);
    const bool has_studio_font =
        impl.find("PushFont(nullptr, studio_metrics::font_") != std::string::npos ||
        impl.find("PushFont(nullptr, lwf::ui::studio_metrics::font_") !=
            std::string::npos;
    CHECK(has_studio_font);
    CHECK(impl.find("PushFont(nullptr, 15.0f)") == std::string::npos);
    CHECK(impl.find("PushFont(nullptr, 13.0f)") == std::string::npos);
    CHECK(header.find("my booking link") != std::string::npos);
    CHECK(header.find("https://example.com/book") != std::string::npos);
    CHECK(header.find("Add new snippet") != std::string::npos);
    CHECK(header.find("Edit snippet") != std::string::npos);
    CHECK(header.find("No snippets yet.") != std::string::npos);
    CHECK(header.find("store.Validate(") != std::string::npos);
    CHECK(header.find("store.Upsert(") != std::string::npos);
    CHECK(header.find("store.Save()") != std::string::npos);
    CHECK(impl.find("Snippets") != std::string::npos);
    CHECK(impl.find("What to say") != std::string::npos);
    CHECK(impl.find("Trigger") != std::string::npos);
    CHECK(impl.find("Expansion") != std::string::npos);
    CHECK(impl.find("\xE2\x86\x92") != std::string::npos);
    CHECK(impl.find("\xE2\x9C\x8E") != std::string::npos);
    CHECK(impl.find("LightButton") != std::string::npos);
    CHECK(impl.find("PrimaryButton") != std::string::npos);
    CHECK(impl.find("Chip") != std::string::npos);
    CHECK(impl.find("IconButton") != std::string::npos);
    CHECK(impl.find("editor_sheet_") != std::string::npos);
    CHECK(impl.find("kSnippetsUndoAction") != std::string::npos);
    CHECK(impl.find("IsItemClicked") == std::string::npos);
    // T10b F1: every custom draw and widget call threads the live palette.
    CHECK(header.find("Draw(float scale, const lwf::ui::Palette") != std::string::npos);
    CHECK(impl.find("kLightPalette") == std::string::npos);
    CHECK(impl.find("palette.") != std::string::npos);
    CHECK(impl.find("Color(palette.bg)") != std::string::npos);
    CHECK(impl.find("Pack(palette.border)") != std::string::npos);
    CHECK(impl.find("Pack(palette.muted)") != std::string::npos);
    CHECK(impl.find("Pack(palette.text)") != std::string::npos);
    CHECK(impl.find("Pack(palette.accent)") != std::string::npos);
    CHECK(impl.find("Color(palette.err)") != std::string::npos);
    CHECK(impl.find("Color(kLightPalette.err)") == std::string::npos);
}
