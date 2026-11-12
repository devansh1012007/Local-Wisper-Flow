#define DOCTEST_CONFIG_IMPLEMENT_WITH_KEYWORDS
#include <doctest/doctest.h>
#include "ui/pages/TransformsPage.h"

#include "core/HotkeyBinding.h"
#include "core/TextDiff.h"
#include "core/TransformStore.h"
#include "core/UiBus.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

namespace {

class FakeDiffSource : public lwf::ui::ITransformDiffSource {
public:
    bool has = false;
    lwf::TransformDiff snap{};

    bool HasTransformDiff() const override {
        return has;
    }
    lwf::TransformDiff TransformDiffSnapshot() const override {
        return snap;
    }
};

std::string ReadSourceFrom(const std::string& root, const std::string& relative) {
    std::ifstream file(root + "/" + relative);
    if (!file.is_open()) {
        file.open(relative);
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::string ReadPageFile(const std::string& relative) {
#ifdef LWF_SOURCE_DIR
    return ReadSourceFrom(LWF_SOURCE_DIR, relative);
#else
    return ReadSourceFrom(".", relative);
#endif
}

void FillCustom(TransformStore& store, int slot, const std::string& name, const std::string& prompt) {
    std::string error;
    REQUIRE(store.SetSlot(slot, name, prompt, error));
    REQUIRE(error.empty());
}

std::vector<Binding> ParseBindings(const std::string& chord, BindingRole role) {
    std::vector<Binding> bindings;
    if (auto parsed = HotkeyBinding::Parse(chord, role)) {
        bindings.push_back(*parsed);
    }
    return bindings;
}

}

TEST_CASE("opt-in off registers nothing at the page gate") {
    TransformStore store;
    lwf::ui::TransformsPage page(store);
    CHECK(false == store.Enabled());
    CHECK(page.ActiveShortcuts().empty());
    // Followups-2: nothing is seeded, so the gate test fills its own slots.
    FillCustom(store, 1, "Email", "Make it formal. {text}");
    FillCustom(store, 2, "Note", "Make it a note. {text}");
    FillCustom(store, 3, "Chat", "Make it chatty. {text}");
    CHECK(page.ActiveShortcuts().empty());
    CHECK(false == store.IsSlotEnabled(3));
    page.SetOptIn(true);
    CHECK(store.Enabled());
    const auto active = page.ActiveShortcuts();
    CHECK(active.size() == 3);
    CHECK(active.at(1) == "Win+Alt+1");
    CHECK(active.at(2) == "Win+Alt+2");
    CHECK(active.at(3) == "Win+Alt+3");
}

TEST_CASE("editing works while opt-in is off but shortcuts stay inactive") {
    TransformStore store;
    lwf::ui::TransformsPage page(store);
    CHECK(false == store.Enabled());
    page.OpenEditor(3);
    CHECK(page.IsEditing());
    CHECK(page.EditSlot() == 3);
    page.SetEditName("Summary");
    page.SetEditPrompt("Summarize in one line. {text}");
    page.SetEditShortcut("Win+Alt+S");
    CHECK(page.SaveEdit());
    const auto entries = store.Entries();
    const auto found = std::find_if(entries.begin(), entries.end(),
                                    [](const TransformStore::Entry& entry) {
                                        return entry.slot == 3;
                                    });
    REQUIRE(found != entries.end());
    CHECK(found->name == "Summary");
    CHECK(found->shortcut == "Win+Alt+S");
    CHECK(page.ActiveShortcuts().empty());
    CHECK(false == store.IsSlotEnabled(3));
    page.SetOptIn(true);
    CHECK(store.IsSlotEnabled(3));
    CHECK(page.ActiveShortcuts().at(3) == "Win+Alt+S");
}

TEST_CASE("diff change counts pin one and many") {
    CHECK(lwf::ui::transforms::ChangeCount(lwf::Diff("same text here", "same text here")) == 0);
    CHECK(lwf::ui::transforms::ChangeLabel(lwf::Diff("same text here", "same text here")) ==
          "0 Changes");
    CHECK(lwf::ui::transforms::ChangeCount(lwf::Diff("hello world", "hello brave world")) == 1);
    CHECK(lwf::ui::transforms::ChangeLabel(lwf::Diff("hello world", "hello brave world")) ==
          "1 Change");
    CHECK(lwf::ui::transforms::ChangeCount(lwf::Diff("hello brave world", "hello world")) == 1);
    CHECK(lwf::ui::transforms::ChangeLabel(lwf::Diff("hello brave world", "hello world")) ==
          "1 Change");
    CHECK(lwf::ui::transforms::ChangeCount(lwf::Diff("a cat sat", "a dog sat")) == 1);
    CHECK(lwf::ui::transforms::ChangeCount(lwf::Diff("one two", "one two three four")) == 2);
    CHECK(lwf::ui::transforms::ChangeLabel(lwf::Diff("one two", "one two three four")) ==
          "2 Changes");
    CHECK(lwf::ui::transforms::ChangeCount(lwf::Diff("a b c", "x y z")) == 3);
    CHECK(lwf::ui::transforms::ChangeLabel(lwf::Diff("a b c", "x y z")) == "3 Changes");
}

TEST_CASE("reset confirmation lists how many customs are deleted") {
    TransformStore store;
    lwf::ui::TransformsPage page(store);
    FillCustom(store, 3, "Email", "Formal. {text}");
    FillCustom(store, 4, "Short", "Brief. {text}");
    std::string error;
    REQUIRE(store.SetSlot(1, "Polished Edit", "Custom polish. {text}", error));
    // Followups-2: every named slot counts (no built-ins are restored).
    const auto plan = page.ResetPlan();
    CHECK(plan.customs_deleted == 3);
    const int deleted = page.PerformReset();
    CHECK(deleted == 3);
    const auto entries = store.Entries();
    for (const auto& entry : entries) {
        CHECK(entry.name.empty());
        CHECK(entry.prompt.empty());
        CHECK_FALSE(entry.built_in);
    }
    CHECK(page.ResetPlan().customs_deleted == 0);
}

TEST_CASE("shortcut recorder requires a modifier and a real key") {
    TransformStore store;
    const auto entries = store.Entries();
    const std::string diff = store.DiffShortcut();
    const std::vector<Binding> no_bindings;
    CHECK(lwf::ui::transforms::ValidateShortcut("", 3, entries, no_bindings, diff).empty());
    CHECK(lwf::ui::transforms::ValidateShortcut("1", 3, entries, no_bindings, diff) ==
          "A shortcut needs a modifier key");
    CHECK(lwf::ui::transforms::ValidateShortcut("Win+Alt", 3, entries, no_bindings, diff) ==
          "A shortcut needs a key");
    CHECK(lwf::ui::transforms::ValidateShortcut("Alt", 3, entries, no_bindings, diff) ==
          "A shortcut needs a key");
    CHECK(lwf::ui::transforms::ValidateShortcut("Win+Alt+9", 3, entries, no_bindings, diff).empty());
    CHECK(lwf::ui::transforms::ValidateShortcut("Ctrl+Shift+T", 3, entries, no_bindings, diff)
              .empty());
}

TEST_CASE("shortcut recorder rejects conflicts with slots bindings and the diff chord") {
    TransformStore store;
    FillCustom(store, 5, "Email", "Formal. {text}");
    // Followups-2: fill slot 1 so its default chord is held by a named entry
    // (nothing is seeded any more).
    FillCustom(store, 1, "First", "One. {text}");
    const auto entries = store.Entries();
    const std::string diff = store.DiffShortcut();
    const std::vector<Binding> no_bindings;
    CHECK(lwf::ui::transforms::ValidateShortcut("Win+Alt+1", 3, entries, no_bindings, diff) ==
          "That shortcut is already in use");
    CHECK(lwf::ui::transforms::ValidateShortcut("win+alt+o", 3, entries, no_bindings, diff) ==
          "That shortcut opens the diff view");
    CHECK(lwf::ui::transforms::ValidateShortcut("Win+Alt+5", 3, entries, no_bindings, diff) ==
          "That shortcut is already in use");
    CHECK(lwf::ui::transforms::ValidateShortcut("Win+Alt+5", 5, entries, no_bindings, diff).empty());
    const auto bindings = ParseBindings("ctrl+shift+0x54", BindingRole::PushToTalk);
    REQUIRE(!bindings.empty());
    CHECK(lwf::ui::transforms::ValidateShortcut("Ctrl+Shift+T", 3, entries, bindings, diff) ==
          "That shortcut is already in use");
    CHECK(lwf::ui::transforms::ValidateShortcut("Ctrl+Shift+U", 3, entries, bindings, diff).empty());
}

TEST_CASE("commit recorded chord validates before storing") {
    TransformStore store;
    lwf::ui::TransformsPage page(store);
    page.OpenEditor(3);
    page.BeginRecord();
    CHECK(page.IsRecording());
    CHECK(false == page.CommitRecorded("1"));
    CHECK(page.EditError() == "A shortcut needs a modifier key");
    CHECK(page.EditShortcut() == "Win+Alt+3");
    CHECK(false == page.CommitRecorded("Win+Alt+O"));
    CHECK(page.EditError() == "That shortcut opens the diff view");
    CHECK(true == page.CommitRecorded("Win+Alt+9"));
    CHECK(page.EditShortcut() == "Win+Alt+9");
    CHECK(page.EditError().empty());
    CHECK(false == page.IsRecording());
    page.BeginRecord();
    page.CancelRecord();
    CHECK(false == page.IsRecording());
}

TEST_CASE("keycaps split the live diff shortcut") {
    TransformStore store;
    lwf::ui::TransformsPage page(store);
    CHECK(page.DiffKeycapTokens() ==
          std::vector<std::string>({"Win", "Alt", "O"}));
    CHECK(lwf::ui::transforms::SplitKeycaps("Win+Alt+1") ==
          std::vector<std::string>({"Win", "Alt", "1"}));
    CHECK(lwf::ui::transforms::SplitKeycaps("Win") == std::vector<std::string>({"Win"}));
    CHECK(lwf::ui::transforms::SplitKeycaps("").empty());
    CHECK(lwf::ui::transforms::BuildChord(true, false, true, false, "1") == "Win+Alt+1");
    CHECK(lwf::ui::transforms::BuildChord(false, true, false, true, "T") == "Ctrl+Shift+T");
    CHECK(lwf::ui::transforms::BuildChord(false, false, false, false, "") == "");
    store.SetDiffShortcut("Ctrl+Shift+D");
    CHECK(page.DiffKeycapTokens() ==
          std::vector<std::string>({"Ctrl", "Shift", "D"}));
}

TEST_CASE("cards list named slots in order and the create card hides at ten") {
    TransformStore store;
    lwf::ui::TransformsPage page(store);
    // Followups-2: no built-ins ship, so an empty store shows no cards.
    auto cards = page.Cards();
    REQUIRE(cards.empty());
    CHECK(page.ShowCreateCard());
    FillCustom(store, 5, "Fifth", "Prompt five. {text}");
    FillCustom(store, 3, "Third", "Prompt three. {text}");
    FillCustom(store, 1, "First", "Prompt one. {text}");
    cards = page.Cards();
    REQUIRE(cards.size() == 3);
    CHECK(cards.at(0).slot == 1);
    CHECK(cards.at(1).slot == 3);
    CHECK(cards.at(2).slot == 5);
    FillCustom(store, 2, "Two", "Prompt two. {text}");
    FillCustom(store, 4, "Four", "Prompt four. {text}");
    FillCustom(store, 6, "Six", "Prompt six. {text}");
    FillCustom(store, 7, "Seven", "Prompt seven. {text}");
    FillCustom(store, 8, "Eight", "Prompt eight. {text}");
    FillCustom(store, 9, "Nine", "Prompt nine. {text}");
    FillCustom(store, 10, "Ten", "Prompt ten. {text}");
    CHECK(page.CountCustoms() == 10);
    CHECK(false == page.ShowCreateCard());
    REQUIRE(page.Cards().size() == 10);
}

TEST_CASE("card descriptions show the real prompt and truncate long ones") {
    TransformStore store;
    lwf::ui::TransformsPage page(store);
    // Followups-2: with the built-ins retired every card shows its real
    // prompt; there is no shipped default copy any more.
    std::string set_error;
    REQUIRE(store.SetSlot(1, "Mine", "Make it crisp. {text}", set_error));
    const auto edited = page.Cards().at(0);
    CHECK(edited.slot == 1);
    CHECK(page.CardDescription(edited) == "Make it crisp. {text}");
    FillCustom(store, 3, "Custom",
               "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz0123456789 rest");
    const auto custom = page.Cards().at(1);
    CHECK(custom.slot == 3);
    CHECK(page.CardDescription(custom).size() == 63);
    CHECK(page.CardDescription(custom).back() == '.');
}

TEST_CASE("prompt counter counts codepoints like the store cap") {
    CHECK(lwf::ui::transforms::CountCodePoints("") == 0);
    CHECK(lwf::ui::transforms::CountCodePoints("abc") == 3);
    CHECK(lwf::ui::transforms::CountCodePoints("h\xc3\xa9llo") == 5);
    CHECK(lwf::ui::transforms::CountCodePoints("{text}") == 6);
}

TEST_CASE("editor save rejects duplicate names empty custom names and long prompts") {
    TransformStore store;
    lwf::ui::TransformsPage page(store);
    FillCustom(store, 1, "Polish", "Make it polish. {text}");
    page.OpenEditor(3);
    page.SetEditName("Polish");
    page.SetEditPrompt("Whatever. {text}");
    CHECK(false == page.SaveEdit());
    CHECK(page.EditError() == "Transform name already exists");
    page.SetEditName("  ");
    CHECK(false == page.SaveEdit());
    CHECK(page.EditError() == "Please enter a name");
    page.SetEditName("Good");
    page.SetEditPrompt(std::string(2001, 'a'));
    CHECK(false == page.SaveEdit());
    CHECK(page.EditError() == "Prompt too long");
    page.SetEditPrompt("Fine. {text}");
    CHECK(page.SaveEdit());
    CHECK(store.Entries().at(2).name == "Good");
}

TEST_CASE("editor delete applies to every slot") {
    TransformStore store;
    lwf::ui::TransformsPage page(store);
    // Followups-2: slots 1-2 are ordinary user slots, so Delete clears them
    // like any other (the old guard and ReturnToDefault went with the
    // built-ins).
    FillCustom(store, 1, "First", "One. {text}");
    page.OpenEditor(1);
    CHECK(page.DeleteEdit());
    CHECK(store.Entries().at(0).name.empty());
    CHECK(false == page.IsEditing());
    FillCustom(store, 3, "Temp", "Temporary. {text}");
    page.OpenEditor(3);
    CHECK(page.DeleteEdit());
    CHECK(store.Entries().at(2).name.empty());
}

TEST_CASE("opt-in toggle reads live store state") {
    TransformStore store;
    lwf::ui::TransformsPage page(store);
    CHECK(false == page.OptIn());
    page.SetOptIn(true);
    CHECK(true == page.OptIn());
    CHECK(store.Enabled());
    page.SetOptIn(false);
    CHECK(false == page.OptIn());
    CHECK(false == store.Enabled());
}

TEST_CASE("diff viewer label and copy payload consume the engine snapshot") {
    TransformStore store;
    FakeDiffSource diff;
    lwf::ui::TransformsPage page(store, nullptr, &diff);
    CHECK(false == page.HasDiff());
    CHECK(page.CurrentChangeLabel() == "0 Changes");
    CHECK(page.CopyResult().empty());
    diff.has = true;
    diff.snap.before = "hello world";
    diff.snap.after = "hello brave world";
    diff.snap.diff = lwf::Diff(diff.snap.before, diff.snap.after);
    diff.snap.valid = true;
    CHECK(page.HasDiff());
    CHECK(page.CurrentChangeLabel() == "1 Change");
    CHECK(page.CopyResult() == "hello brave world");
    diff.snap.before = "one two";
    diff.snap.after = "one two three four";
    diff.snap.diff = lwf::Diff(diff.snap.before, diff.snap.after);
    CHECK(page.CurrentChangeLabel() == "2 Changes");
}

// T10c per-slot Run: RunSlot posts the caller's real slot (plan AC: "Run on
// slot N reaches engine.RequestTransform(N)"). Followups-2: the fixed-slot
// TryItOut helper is retired with the built-ins, so this is the only run
// path left.
TEST_CASE("per-slot run posts the real slot") {
    TransformStore store;
    UiBus bus;
    lwf::ui::TransformsPage page(store, &bus, nullptr);
    CHECK(page.RunSlot(3));
    UiCommand command{UiCommandType::Quit, {}};
    REQUIRE(bus.PollCommand(command));
    CHECK(command.type == UiCommandType::StartTransform);
    CHECK(command.payload == "3");
    CHECK(page.RunSlot(7));
    REQUIRE(bus.PollCommand(command));
    CHECK(command.type == UiCommandType::StartTransform);
    CHECK(command.payload == "7");
    CHECK(page.RunSlot(1));
    REQUIRE(bus.PollCommand(command));
    CHECK(command.payload == "1");
    lwf::ui::TransformsPage unwired(store);
    CHECK(false == unwired.RunSlot(4));
    CHECK(false == bus.PollCommand(command));
}

// T10c F1: every custom draw and widget call threads the live palette;
// edit_error renders in palette.err (byte-identical string).
TEST_CASE("transforms page threads the live palette and err for edit_error") {
    const std::string header = ReadPageFile("src/ui/pages/TransformsPage.h");
    const std::string source = ReadPageFile("src/ui/pages/TransformsPage.cpp");
    REQUIRE(!header.empty());
    REQUIRE(!source.empty());
    CHECK(header.find("Draw(float scale, const Palette& palette = kDarkPalette)") !=
          std::string::npos);
    CHECK(header.find("kLightPalette") == std::string::npos);
    CHECK(source.find("kLightPalette") == std::string::npos);
    CHECK(source.find("palette.") != std::string::npos);
    CHECK(source.find("Color(palette.surface)") != std::string::npos);
    CHECK(source.find("Color(palette.muted)") != std::string::npos);
    CHECK(source.find("Pack(palette.accent)") != std::string::npos);
    CHECK(source.find("Pack(palette.border)") != std::string::npos);
    // T10c link-hover pair retired with the banner's TextLink (D3); the
    // live-palette threading stays pinned by the rows above.
    CHECK(source.find("palette.err") != std::string::npos);
    CHECK(source.find("Color(palette.err), \"%s\", edit_error_.c_str()") != std::string::npos);
    CHECK(source.find("Color(palette.muted), \"%s\", edit_error_.c_str()") == std::string::npos);
    CHECK(source.find("FLAG: edit_error") == std::string::npos);
    // RunSlot posts the real slot.
    CHECK(header.find("std::to_string(slot)") != std::string::npos);
    CHECK(source.find("RunSlot(entry.slot)") != std::string::npos);
    CHECK(source.find("transforms_run_") != std::string::npos);
    // Followups D3: the banner button copy is retired with the hero banner;
    // followups-2 retired the fixed-slot TryItOut API with the built-ins.
    // T10c review CRITICAL (hit-test): the card overlay must carry AllowOverlap
    // so the later Run button wins hover/click (first-submitted-wins);
    // scoped to the DrawCard span so a stray flag elsewhere cannot pass this.
    const std::size_t card_begin = source.find("void TransformsPage::DrawCard");
    const std::size_t card_end = source.find("void TransformsPage::DrawCreateCard");
    REQUIRE(card_begin != std::string::npos);
    REQUIRE(card_end != std::string::npos);
    REQUIRE(card_end > card_begin);
    const std::string card_span = source.substr(card_begin, card_end - card_begin);
    CHECK(card_span.find("ImGuiButtonFlags_AllowOverlap") != std::string::npos);
}

TEST_CASE("page copy keeps how-it-works text; banners are retired") {
    // Followups D3/D4: the hero banner and the opted-out notice strip were
    // removed (constants gone); the How-it-works modal copy stays and keeps
    // the third-party-name ban.
    CHECK(std::string(lwf::ui::transforms::kHowItWorksBody).size() > 40);
    CHECK(std::string(lwf::ui::transforms::kHowItWorksBody).find("Slack") == std::string::npos);
    CHECK(std::string(lwf::ui::transforms::kHowItWorksBody).find("Gmail") == std::string::npos);
    CHECK(std::string(lwf::ui::transforms::kHowItWorksBody).find("LinkedIn") ==
          std::string::npos);
}

TEST_CASE("transforms page sources respect the font and hygiene rules") {
    const std::string header = ReadPageFile("src/ui/pages/TransformsPage.h");
    const std::string source = ReadPageFile("src/ui/pages/TransformsPage.cpp");
    REQUIRE(!header.empty());
    REQUIRE(!source.empty());
    for (const std::string& text : {header, source}) {
        CHECK(text.find("SetWindowFontScale") == std::string::npos);
        CHECK(text.find("windows.h") == std::string::npos);
        CHECK(text.find("TODO") == std::string::npos);
        CHECK(text.find("FIXME") == std::string::npos);
        CHECK(text.find("Slack") == std::string::npos);
        CHECK(text.find("Gmail") == std::string::npos);
        CHECK(text.find("LinkedIn") == std::string::npos);
        CHECK(text.find("Log::") == std::string::npos);
    }
    CHECK(header.find("PushFont") == std::string::npos);
    CHECK(source.find("PushFont(nullptr, studio_metrics::font_") != std::string::npos);
    CHECK(source.find("widgets::BadgeBeta") != std::string::npos);
    CHECK(source.find("widgets::ToggleSwitch") != std::string::npos);
    CHECK(source.find("widgets::PrimaryButton") != std::string::npos);
    CHECK(source.find("widgets::LightButton") != std::string::npos);
    CHECK(source.find("My Transforms") != std::string::npos);
    CHECK(source.find("Create New") != std::string::npos);
    CHECK(source.find("Reset all") != std::string::npos);
    // Followups D3: the banner "Try it out" button is gone with the hero
    // banner; "How it works" stays (the header "i" button opens the modal).
    CHECK(source.find("Try it out") == std::string::npos);
    CHECK(source.find("How it works") != std::string::npos);
    CHECK(source.find("Copy result") != std::string::npos);
    CHECK(header.find("<selection>{text}</selection>") != std::string::npos);
}
