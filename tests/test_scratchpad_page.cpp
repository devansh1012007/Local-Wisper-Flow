#include "core/ITextInjector.h"
#include "core/NotesStore.h"
#include "core/SettingsStore.h"
#include "ui/ScratchpadWindow.h"
#include "ui/pages/ScratchpadPage.h"

#include <doctest/doctest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <vector>

namespace {

std::filesystem::path ScratchDir(const char* name) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / (std::string("lwf_scratchpad_") + name);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string ReadSourceFrom(const std::string& root, const std::string& relative) {
    std::ifstream file(root + "/" + relative);
    if (!file.is_open()) {
        file.open(relative);
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

// T-004: LWF_SOURCE_DIR first with a bare-relative fallback (pattern from
// tests/test_transforms_page.cpp:30-44) so source scans work from the repo
// root and from build/ alike; previously this helper had no fallback.
std::string ReadSource(const std::string& path) {
#ifdef LWF_SOURCE_DIR
    return ReadSourceFrom(LWF_SOURCE_DIR, path);
#else
    return ReadSourceFrom(".", path);
#endif
}

std::size_t CountHits(const std::string& haystack, const std::string& needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

class CountingInjector final : public ITextInjector {
public:
    void InjectText(const std::string& text) override {
        std::lock_guard<std::mutex> lock(mutex_);
        injected.push_back(text);
    }

    std::size_t Count() {
        std::lock_guard<std::mutex> lock(mutex_);
        return injected.size();
    }

    std::string Last() {
        std::lock_guard<std::mutex> lock(mutex_);
        return injected.empty() ? std::string() : injected.back();
    }

private:
    std::mutex mutex_;
    std::vector<std::string> injected;
};

}

TEST_CASE("scratchpad preview flattens lines and truncates to 80 chars") {
    using lwf::ui::scratchpad::PreviewText;
    CHECK(PreviewText("") == "");
    CHECK(PreviewText("   \n\t") == "");
    CHECK(PreviewText("Hello\r\nWorld\tAgain") == "Hello World Again");
    CHECK(PreviewText("  padded text  ") == "padded text");
    const std::string long_body(120, 'a');
    const std::string cut = PreviewText(long_body);
    CHECK(cut.size() == 80);
    CHECK(cut == std::string(80, 'a'));
    const std::string exact(80, 'b');
    CHECK(PreviewText(exact).size() == 80);
    CHECK(PreviewText("short") == "short");
    const std::string utf = PreviewText("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    CHECK(utf.size() == 80);
}

TEST_CASE("scratchpad modified date formats epoch deterministically") {
    using lwf::ui::scratchpad::FormatModifiedDate;
    CHECK(FormatModifiedDate(0) == "1970-01-01");
    CHECK(FormatModifiedDate(86400) == "1970-01-02");
    CHECK(FormatModifiedDate(1700000000) == "2023-11-14");
    CHECK(FormatModifiedDate(-1) == "1969-12-31");
    CHECK(FormatModifiedDate(1794009600) == "2026-11-07");
}

TEST_CASE("scratchpad shortcut display maps chord and empty helper") {
    using lwf::ui::scratchpad::ShortcutDisplayLabel;
    using lwf::ui::scratchpad::kDefaultScratchpadShortcut;
    using lwf::ui::scratchpad::kShortcutHelper;
    CHECK(std::string(kDefaultScratchpadShortcut) == "Win+Alt+S");
    CHECK(ShortcutDisplayLabel("Win+Alt+S") == "Win Alt S");
    CHECK(ShortcutDisplayLabel(std::string(kDefaultScratchpadShortcut)) == "Win Alt S");
    CHECK(ShortcutDisplayLabel("") == std::string(kShortcutHelper));
    CHECK(ShortcutDisplayLabel("") == "Click to enable shortcut");
    CHECK(ShortcutDisplayLabel("Ctrl+Shift+K") == "Ctrl Shift K");
}

TEST_CASE("scratchpad undo window expires after five seconds") {
    const std::filesystem::path dir = ScratchDir("undo");
    NotesStore store(dir.string());
    const auto created = store.Create("Undo me", "undo body line");
    REQUIRE(created.has_value());
    NotesStore::Note note = *created;
    note.body = "undo body line";
    lwf::ui::scratchpad::UndoWindow window;
    CHECK_FALSE(window.IsActive(10.0));
    CHECK_FALSE(window.HasPending());
    window.Arm(note, 10.0);
    CHECK(window.IsActive(10.0));
    CHECK(window.IsActive(14.999));
    CHECK_FALSE(window.IsActive(15.0));
    CHECK_FALSE(window.IsActive(16.0));
    CHECK(window.HasPending());
    CHECK(lwf::ui::scratchpad::IsUndoExpired(window, 15.0));
    CHECK_FALSE(lwf::ui::scratchpad::IsUndoExpired(window, 14.0));
    const auto taken = window.Take();
    REQUIRE(taken.has_value());
    CHECK(taken->id == note.id);
    CHECK_FALSE(window.HasPending());
    CHECK_FALSE(window.IsActive(11.0));
    window.Clear();
    CHECK_FALSE(window.HasPending());
}

TEST_CASE("scratchpad word count counts whitespace separated tokens") {
    using lwf::ui::scratchpad::CountWords;
    using lwf::ui::scratchpad::CountWordsLabel;
    CHECK(CountWords("") == 0);
    CHECK(CountWords("   \n\t  ") == 0);
    CHECK(CountWords("hello") == 1);
    CHECK(CountWords("hello world") == 2);
    CHECK(CountWords("one\ttwo\r\nthree   four") == 4);
    CHECK(CountWords("  leading and trailing  ") == 3);
    CHECK(CountWords("**bold** text") == 2);
    CHECK(CountWordsLabel("a b c") == "3 words");
    CHECK(CountWordsLabel("") == "0 words");
}

TEST_CASE("scratchpad insert at caret clamps and advances past text") {
    using lwf::ui::scratchpad::InsertAtCaret;
    std::string body = "hello world";
    std::size_t caret = 5;
    InsertAtCaret(body, caret, " brave");
    CHECK(body == "hello brave world");
    CHECK(caret == 11);
    body = "ab";
    caret = 99;
    InsertAtCaret(body, caret, "cd");
    CHECK(body == "abcd");
    CHECK(caret == 4);
    body = "x";
    caret = 0;
    InsertAtCaret(body, caret, "");
    CHECK(body == "x");
    CHECK(caret == 0);
    body = "";
    caret = 0;
    InsertAtCaret(body, caret, "first");
    CHECK(body == "first");
    CHECK(caret == 5);
}

TEST_CASE("scratchpad markdown bold toggles round trip") {
    using lwf::ui::scratchpad::ApplyBold;
    std::string body = "hello world";
    std::string bold = ApplyBold(body, 6, 11);
    CHECK(bold == "hello **world**");
    std::string back = ApplyBold(bold, 8, 13);
    CHECK(back == "hello world");
    std::string whole = ApplyBold(body, 0, 11);
    CHECK(whole == "**hello world**");
    std::string caret_case = ApplyBold(body, 5, 5);
    CHECK(caret_case == "hello**** world");
}

TEST_CASE("scratchpad markdown italic toggles round trip without eating bold") {
    using lwf::ui::scratchpad::ApplyItalic;
    std::string body = "hello world";
    std::string italic = ApplyItalic(body, 6, 11);
    CHECK(italic == "hello *world*");
    std::string back = ApplyItalic(italic, 7, 12);
    CHECK(back == "hello world");
    std::string bold_body = "hello **world**";
    std::string nested = ApplyItalic(bold_body, 8, 13);
    CHECK(nested == "hello ***world***");
}

TEST_CASE("scratchpad markdown bullet numbered and heading toggle round trip") {
    using lwf::ui::scratchpad::ApplyBullet;
    using lwf::ui::scratchpad::ApplyNumbered;
    using lwf::ui::scratchpad::ApplyHeading;
    std::string body = "alpha\nbeta";
    std::string bulleted = ApplyBullet(body, 0, body.size());
    CHECK(bulleted == "- alpha\n- beta");
    std::string unbulleted = ApplyBullet(bulleted, 0, bulleted.size());
    CHECK(unbulleted == "alpha\nbeta");
    std::string one_line = ApplyBullet(body, 7, 7);
    CHECK(one_line == "alpha\n- beta");
    std::string numbered = ApplyNumbered(body, 0, body.size());
    CHECK(numbered == "1. alpha\n2. beta");
    std::string unnumbered = ApplyNumbered(numbered, 0, numbered.size());
    CHECK(unnumbered == "alpha\nbeta");
    std::string headed = ApplyHeading(body, 0, 5);
    CHECK(headed == "# alpha\nbeta");
    std::string completed = ApplyHeading(headed, 0, headed.size());
    CHECK(completed == "# alpha\n# beta");
    std::string unheaded = ApplyHeading(completed, 0, completed.size());
    CHECK(unheaded == "alpha\nbeta");
    std::string heading_multi = ApplyHeading(body, 0, body.size());
    CHECK(heading_multi == "# alpha\n# beta");
    CHECK(ApplyHeading(heading_multi, 0, heading_multi.size()) == "alpha\nbeta");
}

TEST_CASE("scratchpad editor display title trims and falls back to untitled") {
    using lwf::ui::scratchpad::EditorDisplayTitle;
    CHECK(EditorDisplayTitle("") == "Untitled");
    CHECK(EditorDisplayTitle("   ") == "Untitled");
    CHECK(EditorDisplayTitle("  hello  ") == "hello");
    CHECK(EditorDisplayTitle("Shopping") == "Shopping");
    CHECK(EditorDisplayTitle("\ttabbed\t") == "tabbed");
}

TEST_CASE("scratchpad route injector sends dictation only to the focused window") {
    ScratchpadBuffer buffer;
    CountingInjector elsewhere;
    std::atomic<bool> active{true};
    lwf::ui::scratchpad::ScratchpadRouteInjector route(elsewhere, buffer, active);
    route.InjectText("dictated line");
    CHECK(buffer.Text() == "dictated line");
    CHECK(elsewhere.Count() == 0);
    route.InjectText("second line");
    CHECK(buffer.Text() == "dictated line\nsecond line");
    CHECK(elsewhere.Count() == 0);
    active.store(false);
    route.InjectText("outside line");
    CHECK(elsewhere.Count() == 1);
    CHECK(elsewhere.Last() == "outside line");
    CHECK(buffer.Text() == "dictated line\nsecond line");
    active.store(true);
    route.InjectText("back inside");
    CHECK(buffer.Text() == "dictated line\nsecond line\nback inside");
    CHECK(elsewhere.Count() == 1);
}

TEST_CASE("scratchpad store crud pin search and sort order are preserved for the page") {
    const std::filesystem::path dir = ScratchDir("store");
    NotesStore store(dir.string());
    const auto alpha = store.Create("Project Alpha", "hello Scratch first");
    const auto beta = store.Create("Beta", "nothing here");
    const auto gamma = store.Create("Gamma", "gamma body");
    REQUIRE(alpha.has_value());
    REQUIRE(beta.has_value());
    REQUIRE(gamma.has_value());
    REQUIRE(store.Pin(beta->id));
    const std::vector<NotesStore::Note> listed = store.List();
    REQUIRE(listed.size() == 3);
    CHECK(listed[0].id == beta->id);
    CHECK(listed[0].pinned);
    const std::vector<NotesStore::Note> hits = store.Search("HELLO");
    REQUIRE(hits.size() == 1);
    CHECK(hits[0].id == alpha->id);
    CHECK(store.Search("beta").size() == 1);
    CHECK(store.Search("absent term").empty());
    CHECK(store.Search("").size() == 3);
    REQUIRE(store.Update(alpha->id, "Project Alpha edited", "hello Scratch edited"));
    const auto got = store.Get(alpha->id);
    REQUIRE(got.has_value());
    CHECK(got->title == "Project Alpha edited");
    CHECK(got->body == "hello Scratch edited");
    const auto deleted = store.Delete(gamma->id);
    REQUIRE(deleted.has_value());
    CHECK(store.Get(gamma->id) == std::nullopt);
    REQUIRE(store.Restore(*deleted));
    REQUIRE(store.Get(gamma->id).has_value());
    CHECK(store.Get(gamma->id)->body == "gamma body");
    const auto preview = lwf::ui::scratchpad::PreviewText(store.Get(gamma->id)->body);
    CHECK(preview == "gamma body");
    CHECK(lwf::ui::scratchpad::FormatModifiedDate(store.Get(gamma->id)->modified_at).size() == 10);
}

TEST_CASE("scratchpad flow bar key defaults off and persists") {
    const std::filesystem::path dir = ScratchDir("cfg");
    const std::string cfg = (dir / "config.json").string();
    {
        SettingsStore store(cfg);
        CHECK(store.HasKey("scratchpad_flow_bar"));
        CHECK_FALSE(store.GetBool("scratchpad_flow_bar"));
        CHECK(store.IsHotApply("scratchpad_flow_bar"));
        store.SetBool("scratchpad_flow_bar", true);
        store.Save();
    }
    {
        SettingsStore reloaded(cfg);
        CHECK(reloaded.GetBool("scratchpad_flow_bar"));
    }
}

TEST_CASE("scratchpad page source wires shell widgets and notes store") {
    const std::string page_h = ReadSource("src/ui/pages/ScratchpadPage.h");
    REQUIRE_FALSE(page_h.empty());
    CHECK(page_h.find("class ScratchpadPage") != std::string::npos);
    CHECK(page_h.find("NotesStore") != std::string::npos);
    CHECK(page_h.find("scratchpad_flow_bar") == std::string::npos);
    const std::string page_cpp = ReadSource("src/ui/pages/ScratchpadPage.cpp");
    REQUIRE_FALSE(page_cpp.empty());
    CHECK(page_cpp.find("Scratchpad") != std::string::npos);
    CHECK(page_cpp.find("BadgeBeta") != std::string::npos);
    CHECK(page_cpp.find("Add to Flow Bar") != std::string::npos);
    CHECK(page_cpp.find("scratchpad_flow_bar") != std::string::npos);
    CHECK(page_cpp.find("ToggleSwitch") != std::string::npos);
    CHECK(page_cpp.find("Click to enable shortcut") != std::string::npos);
    CHECK(page_cpp.find("ShortcutLabel") != std::string::npos);
    CHECK(page_cpp.find("Start new note") != std::string::npos);
    CHECK(page_cpp.find("Recents") != std::string::npos);
    CHECK(page_cpp.find("No notes found") != std::string::npos);
    CHECK(page_cpp.find("Untitled") != std::string::npos);
    CHECK(page_cpp.find("Search") != std::string::npos);
    CHECK(page_cpp.find("notes_.Search") != std::string::npos);
    CHECK(page_cpp.find("notes_.Create") != std::string::npos);
    CHECK(page_cpp.find("notes_.Delete") != std::string::npos);
    CHECK(page_cpp.find("notes_.Restore") != std::string::npos);
    CHECK(page_cpp.find("notes_.Pin") != std::string::npos);
    CHECK(page_cpp.find("notes_.Unpin") != std::string::npos);
    CHECK(page_cpp.find("notes_.Reload") != std::string::npos);
    CHECK(page_cpp.find("PreviewText") != std::string::npos);
    CHECK(page_cpp.find("FormatModifiedDate") != std::string::npos);
    CHECK(page_cpp.find("OpenScratchpad") != std::string::npos);
    CHECK(page_cpp.find("ApplySettings") != std::string::npos);
    CHECK(page_cpp.find("Undo") != std::string::npos);
    CHECK(page_cpp.find("IconButton") != std::string::npos);
    CHECK(page_cpp.find("LightButton") != std::string::npos);
    // T10e: the live palette is explicit and threads every custom draw and
    // widget; the Undo HeatColor(3) data-viz fill stays (accent-ramp L3).
    CHECK(page_h.find("void Draw(float scale, const lwf::ui::Palette& palette = lwf::ui::kDarkPalette)") !=
          std::string::npos);
    CHECK(page_cpp.find("kLightPalette") == std::string::npos);
    CHECK(page_cpp.find("Pack(palette.text)") != std::string::npos);
    CHECK(page_cpp.find("Pack(palette.muted)") != std::string::npos);
    CHECK(page_cpp.find("Pack(palette.surface)") != std::string::npos);
    CHECK(page_cpp.find("Pack(palette.border)") != std::string::npos);
    CHECK(page_cpp.find("Pack(palette.accent)") != std::string::npos);
    CHECK(page_cpp.find("Color(palette.surface)") != std::string::npos);
    CHECK(page_cpp.find("BadgeBeta(scale, palette)") != std::string::npos);
    CHECK(page_cpp.find("ToggleSwitch(\"##scratchpad_flow_bar\", flow_on, scale, palette)") !=
          std::string::npos);
    CHECK(page_cpp.find("LightButton(\"##start_new_note\", \"Start new note\", scale, palette)") !=
          std::string::npos);
    // Pre-PR review fix: the "?" button id must NOT reuse ##scratch_search
    // (the input's id) — shared ids in one scope collide in ImGui.
    CHECK(page_cpp.find("IconButton(\"##scratch_search_focus\", \"?\", scale, palette)") !=
          std::string::npos);
    CHECK(page_cpp.find("IconButton(\"##scratch_search\", \"?\", scale, palette)") ==
          std::string::npos);
    CHECK(page_cpp.find("HeatColor(3)") != std::string::npos);
    // T-004 pin flip (P-005): PushFont must go through design-px tokens, not
    // digit literals. 15px has a same-value token (metrics::font_card_title_px)
    // so it must be gone; 17px has no token in either palette and stays
    // allowlisted (capped at HEAD count 1) in the overlay ban for Wave 2 T-003.
    const bool has_token_font =
        page_cpp.find("PushFont(nullptr, studio_metrics::") != std::string::npos ||
        page_cpp.find("PushFont(nullptr, metrics::") != std::string::npos ||
        page_cpp.find("PushFont(nullptr, lwf::ui::metrics::") != std::string::npos;
    CHECK(has_token_font);
    CHECK(page_cpp.find("PushFont(nullptr, 15.0f)") == std::string::npos);
    CHECK(CountHits(page_cpp, "PushFont(nullptr, 17.0f)") <= 1);
    CHECK(page_cpp.find("SetWindowFontScale") == std::string::npos);
    CHECK(page_cpp.find("ImGui::CreateContext") == std::string::npos);
}

TEST_CASE("scratchpad editor window source keeps its own context and sizes") {
    const std::string win_h = ReadSource("src/ui/ScratchpadWindow.h");
    REQUIRE_FALSE(win_h.empty());
    CHECK(win_h.find("class ScratchpadWindow") != std::string::npos);
    CHECK(win_h.find("ScratchpadRouteInjector") != std::string::npos);
    CHECK(win_h.find("ToolbarAction") != std::string::npos);
    CHECK(win_h.find("760") != std::string::npos);
    CHECK(win_h.find("620") != std::string::npos);
    CHECK(win_h.find("imgui_context_") != std::string::npos);
    const std::string win_cpp = ReadSource("src/ui/ScratchpadWindow.cpp");
    REQUIRE_FALSE(win_cpp.empty());
    CHECK(win_cpp.find("glfwCreateWindow") != std::string::npos);
    CHECK(win_cpp.find("760") != std::string::npos);
    CHECK(win_cpp.find("620") != std::string::npos);
    CHECK(win_cpp.find("480") != std::string::npos);
    CHECK(win_cpp.find("360") != std::string::npos);
    CHECK(win_cpp.find("glfwSetWindowSizeLimits") != std::string::npos);
    CHECK(win_cpp.find("imgui_context_ = ImGui::CreateContext()") != std::string::npos);
    CHECK(win_cpp.find("ImGui::GetCurrentContext()") == std::string::npos);
    CHECK(win_cpp.find("ImGui::SetCurrentContext") != std::string::npos);
    CHECK(win_cpp.find("glfwMakeContextCurrent") != std::string::npos);
    CHECK(win_cpp.find("ApplyImGuiStyle") != std::string::npos);
    // T2b: style selected via PaletteForTheme.
    CHECK(win_cpp.find("PaletteForTheme") != std::string::npos);
    // T10e: the tab chrome reads the live current_palette_ (never Paper);
    // the queue-only ApplyTheme stays queue-only (span-scoped absence).
    CHECK(win_cpp.find("kLightPalette") == std::string::npos);
    CHECK(win_cpp.find("current_palette_->surface") != std::string::npos);
    CHECK(win_cpp.find("current_palette_->text") != std::string::npos);
    CHECK(win_cpp.find("current_palette_->muted") != std::string::npos);
    CHECK(win_cpp.find("current_palette_->accent") != std::string::npos);
    CHECK(win_cpp.find("current_palette_->bg") != std::string::npos);
    {
        const std::size_t apply = win_cpp.find("void ScratchpadWindow::ApplyTheme");
        REQUIRE(apply != std::string::npos);
        const std::size_t open_note = win_cpp.find("void ScratchpadWindow::OpenNote", apply);
        REQUIRE(open_note != std::string::npos);
        const std::string apply_span = win_cpp.substr(apply, open_note - apply);
        CHECK(apply_span.find("ApplyImGuiStyle(") == std::string::npos);
        CHECK(apply_span.find("pending_theme_palette_ = &palette") != std::string::npos);
    }
    CHECK(win_cpp.find("InputTextMultiline") != std::string::npos);
    CHECK(win_cpp.find("CallbackAlways") != std::string::npos);
    CHECK(win_cpp.find("Bold") != std::string::npos);
    CHECK(win_cpp.find("Italic") != std::string::npos);
    CHECK(win_cpp.find("Bullet") != std::string::npos);
    CHECK(win_cpp.find("Numbered") != std::string::npos);
    CHECK(win_cpp.find("Heading") != std::string::npos);
    CHECK(win_cpp.find("Mic") != std::string::npos);
    CHECK(win_cpp.find("Saved") != std::string::npos);
    CHECK(win_cpp.find("words") != std::string::npos);
    CHECK(win_cpp.find("notes_.Update") != std::string::npos);
    CHECK(win_cpp.find("notes_.Create") != std::string::npos);
    CHECK(win_cpp.find("notes_.Get") != std::string::npos);
    CHECK(win_cpp.find("DrainDictation") != std::string::npos);
    CHECK(win_cpp.find("CountWordsLabel") != std::string::npos);
    CHECK(win_cpp.find("SetWindowFontScale") == std::string::npos);
    const std::string header_source = win_h + win_cpp;
    CHECK(CountHits(header_source, "Scratchpad") >= 3);
}
