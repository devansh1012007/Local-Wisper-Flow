#pragma once

#include "core/HotkeyBinding.h"
#include "core/TextDiff.h"
#include "core/TransformStore.h"
#include "core/UiBus.h"
#include "ui/theme/ThemePalette.h"
#include "ui/widgets/Overlays.h"

#include <cstddef>
#include <exception>
#include <map>
#include <string>
#include <utility>
#include <vector>

class DictationEngine;

namespace lwf::ui {

class ITransformDiffSource {
public:
    virtual ~ITransformDiffSource() = default;
    virtual bool HasTransformDiff() const = 0;
    virtual lwf::TransformDiff TransformDiffSnapshot() const = 0;
};

class EngineDiffSource : public ITransformDiffSource {
public:
    explicit EngineDiffSource(DictationEngine* engine);
    bool HasTransformDiff() const override;
    lwf::TransformDiff TransformDiffSnapshot() const override;

private:
    DictationEngine* engine_ = nullptr;
};

namespace transforms {

// Followups D3/D4: the page-top hero banner and the opted-out notice strip
// were removed; the header's Opt-in toggle is the single affordance. The
// How-it-works copy stays (the header "i" button opens the modal).
inline constexpr const char* kHowItWorksBody =
    "1. Select the text you want to change. 2. Press the transform shortcut. 3. Review the change "
    "with the diff shortcut.";
inline constexpr const char* kD5SelectionTag = "<selection>{text}</selection>";
inline constexpr const char* kPolishCardCopy = "Improve clarity and conciseness";
inline constexpr const char* kPromptEngineerCardCopy = "Constructs optimal prompts";
inline constexpr int kMaxCustomCards = 8;
inline constexpr std::size_t kCardSubtitleLimit = 60;

inline std::string TrimCopy(const std::string& text) {
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

inline std::string LowerAscii(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

inline std::size_t CountCodePoints(const std::string& text) {
    std::size_t count = 0;
    for (const char c : text) {
        const unsigned char u = static_cast<unsigned char>(c);
        if ((u & 0xC0) != 0x80) {
            ++count;
        }
    }
    return count;
}

inline std::vector<std::string> SplitKeycaps(const std::string& chord) {
    std::vector<std::string> caps;
    std::string current;
    for (const char c : chord) {
        if (c == '+') {
            const std::string token = TrimCopy(current);
            if (!token.empty()) {
                caps.push_back(token);
            }
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    const std::string token = TrimCopy(current);
    if (!token.empty()) {
        caps.push_back(token);
    }
    return caps;
}

inline std::size_t ChangeCount(const lwf::DiffResult& result) {
    return result.replaced + result.inserted + result.deleted;
}

inline std::string ChangeLabel(const lwf::DiffResult& result) {
    const std::size_t changes = ChangeCount(result);
    if (changes == 1) {
        return "1 Change";
    }
    return std::to_string(changes) + " Changes";
}

inline std::string NormalizeToken(std::string token) {
    token = LowerAscii(TrimCopy(token));
    if (token == "control") {
        return "ctrl";
    }
    if (token == "windows" || token == "super") {
        return "win";
    }
    if (token == "esc") {
        return "escape";
    }
    if (token == "ins") {
        return "insert";
    }
    if (token.rfind("0x", 0) == 0) {
        try {
            const unsigned long value = std::stoul(token, nullptr, 16);
            if (value >= 0x30 && value <= 0x39) {
                return std::string(1, static_cast<char>(0x30 + (value - 0x30)));
            }
            if (value >= 0x41 && value <= 0x5A) {
                return LowerAscii(std::string(1, static_cast<char>(value)));
            }
        } catch (const std::exception&) {
            return token;
        }
    }
    return token;
}

inline bool IsModifierToken(const std::string& normalized) {
    return normalized == "win" || normalized == "ctrl" || normalized == "alt" ||
           normalized == "shift" || normalized == "leftalt" || normalized == "rightalt";
}

inline std::vector<std::string> TokenSet(const std::string& chord) {
    std::vector<std::string> tokens;
    for (const std::string& raw : SplitKeycaps(chord)) {
        const std::string normalized = NormalizeToken(raw);
        bool seen = false;
        for (const std::string& existing : tokens) {
            if (existing == normalized) {
                seen = true;
                break;
            }
        }
        if (!seen && !normalized.empty()) {
            tokens.push_back(normalized);
        }
    }
    return tokens;
}

inline bool TokenSetsCollide(const std::vector<std::string>& left,
                             const std::vector<std::string>& right) {
    if (left.empty() || right.empty()) {
        return false;
    }
    const auto contains_all = [](const std::vector<std::string>& outer,
                                 const std::vector<std::string>& inner) {
        for (const std::string& token : inner) {
            bool found = false;
            for (const std::string& candidate : outer) {
                if (candidate == token) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
            }
        }
        return true;
    };
    return contains_all(left, right) || contains_all(right, left);
}

inline std::string ValidateShortcut(const std::string& chord, int slot,
                                    const std::vector<TransformStore::Entry>& entries,
                                    const std::vector<Binding>& bindings,
                                    const std::string& diff_shortcut) {
    if (TrimCopy(chord).empty()) {
        return {};
    }
    const std::vector<std::string> tokens = TokenSet(chord);
    bool has_modifier = false;
    bool has_key = false;
    for (const std::string& token : tokens) {
        if (IsModifierToken(token)) {
            has_modifier = true;
        } else {
            has_key = true;
        }
    }
    if (!has_modifier) {
        return "A shortcut needs a modifier key";
    }
    if (!has_key) {
        return "A shortcut needs a key";
    }
    if (!diff_shortcut.empty() && TokenSetsCollide(tokens, TokenSet(diff_shortcut))) {
        return "That shortcut opens the diff view";
    }
    for (const TransformStore::Entry& entry : entries) {
        if (entry.slot == slot || entry.name.empty() || entry.shortcut.empty()) {
            continue;
        }
        if (TokenSetsCollide(tokens, TokenSet(entry.shortcut))) {
            return "That shortcut is already in use";
        }
    }
    if (TransformStore::ShortcutsConflict(chord, bindings)) {
        return "That shortcut is already in use";
    }
    return {};
}

inline std::string CollapseWhitespace(const std::string& text) {
    std::string out;
    bool in_space = false;
    for (const char c : text) {
        const bool space = c == ' ' || c == '\t' || c == '\r' || c == '\n';
        if (space) {
            if (!in_space && !out.empty()) {
                out.push_back(' ');
            }
            in_space = true;
        } else {
            out.push_back(c);
            in_space = false;
        }
    }
    return out;
}

inline std::string TruncateCodePoints(const std::string& text, std::size_t limit) {
    if (CountCodePoints(text) <= limit) {
        return text;
    }
    std::string prefix;
    std::size_t counted = 0;
    for (std::size_t i = 0; i < text.size() && counted < limit; ++i) {
        const unsigned char u = static_cast<unsigned char>(text[i]);
        if ((u & 0xC0) != 0x80) {
            ++counted;
        }
        prefix.push_back(text[i]);
    }
    return prefix + "...";
}

inline std::string CardDescription(const TransformStore::Entry& entry) {
    // Followups D5: the built-in friendly copy shows only while the stored
    // prompt is still the shipped default; an edited Polish / Prompt
    // Engineer surfaces its real prompt like any custom card, so an edit is
    // visible on the grid (edits always took effect at run time).
    if (entry.slot == 1 && entry.prompt == TransformStore::DefaultPolishPrompt()) {
        return kPolishCardCopy;
    }
    if (entry.slot == 2 && entry.prompt == TransformStore::DefaultPromptEngineerPrompt()) {
        return kPromptEngineerCardCopy;
    }
    return TruncateCodePoints(CollapseWhitespace(entry.prompt), kCardSubtitleLimit);
}

inline int CountCustoms(const std::vector<TransformStore::Entry>& entries) {
    int count = 0;
    for (const TransformStore::Entry& entry : entries) {
        if (entry.slot > 2 && !TrimCopy(entry.name).empty()) {
            ++count;
        }
    }
    return count;
}

inline bool ShowCreateCard(const std::vector<TransformStore::Entry>& entries) {
    return CountCustoms(entries) < kMaxCustomCards;
}

inline std::string BuildChord(bool win, bool ctrl, bool alt, bool shift,
                              const std::string& key) {
    std::vector<std::string> parts;
    if (win) {
        parts.push_back("Win");
    }
    if (ctrl) {
        parts.push_back("Ctrl");
    }
    if (alt) {
        parts.push_back("Alt");
    }
    if (shift) {
        parts.push_back("Shift");
    }
    if (!key.empty()) {
        parts.push_back(key);
    }
    std::string chord;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            chord += "+";
        }
        chord += parts[i];
    }
    return chord;
}

inline std::string FirstEmptyCustomSlot(const std::vector<TransformStore::Entry>& entries) {
    for (const TransformStore::Entry& entry : entries) {
        if (entry.slot > 2 && TrimCopy(entry.name).empty()) {
            return std::to_string(entry.slot);
        }
    }
    return {};
}

}

struct ResetPreview {
    int customs_deleted = 0;
    bool restores_built_ins = true;
};

class TransformsPage {
public:
    TransformsPage(TransformStore& store, UiBus* bus = nullptr,
                   ITransformDiffSource* diff_source = nullptr)
        : store_(store), bus_(bus), diff_source_(diff_source) {}

    TransformsPage(const TransformsPage&) = delete;
    TransformsPage& operator=(const TransformsPage&) = delete;

    // T10c: the live palette is explicit. The default keeps the old
    // MainWindow call site compiling; MainWindow::DrawTransformsPage passes
    // *current_palette_ (wired in the same commit, see docs/PROGRESS.md T10c).
    void Draw(float scale, const Palette& palette = kDarkPalette);

    void SetOptIn(bool enabled) {
        store_.SetEnabled(enabled);
        store_.Save();
    }

    bool OptIn() const {
        return store_.Enabled();
    }

    std::map<int, std::string> ActiveShortcuts() {
        std::map<int, std::string> active;
        if (!store_.Enabled()) {
            return active;
        }
        for (const TransformStore::Entry& entry : store_.Entries()) {
            if (store_.IsSlotEnabled(entry.slot)) {
                active[entry.slot] = entry.shortcut;
            }
        }
        return active;
    }

    std::vector<TransformStore::Entry> Cards() {
        std::vector<TransformStore::Entry> cards;
        for (const TransformStore::Entry& entry : store_.Entries()) {
            if (entry.slot <= 2 || !transforms::TrimCopy(entry.name).empty()) {
                cards.push_back(entry);
            }
        }
        for (std::size_t i = 1; i < cards.size(); ++i) {
            for (std::size_t j = i; j > 0 && cards[j].slot < cards[j - 1].slot; --j) {
                std::swap(cards[j], cards[j - 1]);
            }
        }
        return cards;
    }

    int CountCustoms() {
        return transforms::CountCustoms(store_.Entries());
    }

    bool ShowCreateCard() {
        return transforms::ShowCreateCard(store_.Entries());
    }

    std::string CardDescription(const TransformStore::Entry& entry) {
        return transforms::CardDescription(entry);
    }

    std::vector<std::string> DiffKeycapTokens() {
        return transforms::SplitKeycaps(store_.DiffShortcut());
    }

    void SetHotkeyBindings(std::vector<Binding> bindings) {
        hotkey_bindings_ = std::move(bindings);
    }

    void OpenEditor(int slot) {
        editing_slot_ = slot;
        recording_ = false;
        edit_error_.clear();
        edit_name_.clear();
        edit_prompt_.clear();
        edit_shortcut_.clear();
        editor_open_ = true;
        for (const TransformStore::Entry& entry : store_.Entries()) {
            if (entry.slot == slot) {
                edit_name_ = entry.name;
                edit_prompt_ = entry.prompt;
                edit_shortcut_ = entry.shortcut;
                break;
            }
        }
        SyncBuffers();
    }

    bool IsEditing() const {
        return editing_slot_ != 0;
    }

    int EditSlot() const {
        return editing_slot_;
    }

    const std::string& EditName() const {
        return edit_name_;
    }

    const std::string& EditPrompt() const {
        return edit_prompt_;
    }

    const std::string& EditShortcut() const {
        return edit_shortcut_;
    }

    const std::string& EditError() const {
        return edit_error_;
    }

    void SetEditName(std::string name) {
        edit_name_ = std::move(name);
        SyncBuffers();
    }

    void SetEditPrompt(std::string prompt) {
        edit_prompt_ = std::move(prompt);
        SyncBuffers();
    }

    void SetEditShortcut(std::string shortcut) {
        edit_shortcut_ = std::move(shortcut);
        SyncBuffers();
    }

    void CloseEditor() {
        editing_slot_ = 0;
        editor_open_ = false;
        recording_ = false;
        edit_error_.clear();
    }

    bool SaveEdit() {
        if (!IsEditing()) {
            return false;
        }
        const std::string store_error = store_.Validate(editing_slot_, edit_name_, edit_prompt_);
        if (!store_error.empty()) {
            edit_error_ = store_error;
            return false;
        }
        const std::string shortcut_error = transforms::ValidateShortcut(
            edit_shortcut_, editing_slot_, store_.Entries(), hotkey_bindings_,
            store_.DiffShortcut());
        if (!shortcut_error.empty()) {
            edit_error_ = shortcut_error;
            return false;
        }
        TransformStore::Entry entry;
        entry.slot = editing_slot_;
        entry.name = edit_name_;
        entry.prompt = edit_prompt_;
        entry.shortcut = edit_shortcut_;
        std::string set_error;
        if (!store_.SetSlot(entry, set_error)) {
            edit_error_ = set_error;
            return false;
        }
        if (store_.Conflicted(editing_slot_)) {
            edit_error_ = "That shortcut is already in use";
            return false;
        }
        edit_error_.clear();
        store_.Save();
        CloseEditor();
        return true;
    }

    bool DeleteEdit() {
        if (!IsEditing() || editing_slot_ <= 2) {
            return false;
        }
        std::string set_error;
        if (!store_.SetSlot(editing_slot_, "", "", set_error)) {
            edit_error_ = set_error;
            return false;
        }
        store_.Save();
        CloseEditor();
        return true;
    }

    bool ReturnToDefault() {
        if (!IsEditing()) {
            return false;
        }
        std::string set_error;
        if (!store_.SetSlot(editing_slot_, "", "", set_error)) {
            edit_error_ = set_error;
            return false;
        }
        store_.Save();
        CloseEditor();
        return true;
    }

    void BeginRecord() {
        recording_ = true;
    }

    void CancelRecord() {
        recording_ = false;
    }

    bool IsRecording() const {
        return recording_;
    }

    bool CommitRecorded(const std::string& chord) {
        recording_ = false;
        if (!IsEditing()) {
            return false;
        }
        const std::string error = transforms::ValidateShortcut(
            chord, editing_slot_, store_.Entries(), hotkey_bindings_, store_.DiffShortcut());
        if (!error.empty()) {
            edit_error_ = error;
            return false;
        }
        edit_shortcut_ = chord;
        edit_error_.clear();
        SyncBuffers();
        return true;
    }

    void ClearRecordedShortcut() {
        edit_shortcut_ = TransformStore::DefaultShortcutForSlot(editing_slot_);
        edit_error_.clear();
        SyncBuffers();
    }

    ResetPreview ResetPlan() {
        ResetPreview plan;
        plan.customs_deleted = CountCustoms();
        plan.restores_built_ins = true;
        return plan;
    }

    int PerformReset() {
        const int deleted = CountCustoms();
        std::string set_error;
        for (int slot = 1; slot <= TransformStore::kSlotCount; ++slot) {
            store_.SetSlot(slot, "", "", set_error);
        }
        store_.Save();
        return deleted;
    }

    bool HasDiff() const {
        return diff_source_ != nullptr && diff_source_->HasTransformDiff();
    }

    std::string CurrentChangeLabel() const {
        if (!HasDiff()) {
            return "0 Changes";
        }
        return transforms::ChangeLabel(diff_source_->TransformDiffSnapshot().diff);
    }

    std::string CopyResult() const {
        if (!HasDiff()) {
            return {};
        }
        return diff_source_->TransformDiffSnapshot().after;
    }

    void OpenDiff() {
        if (HasDiff()) {
            diff_modal_.Open();
        }
    }

    bool DiffOpen() const {
        return diff_modal_.IsOpen();
    }

    void CloseDiff() {
        diff_modal_.Close();
    }

    // Banner "Try it out" shortcut: fixed polish slot (slot 1), kept
    // byte-identical for the existing polish pin. Per-card Run buttons go
    // through RunSlot(entry.slot) for their real slot.
    bool TryItOut() {
        if (bus_ == nullptr) {
            return false;
        }
        bus_->PostCommand(UiCommandType::StartTransform, "1");
        return true;
    }

    // T10c per-slot Run path: posts the caller's real slot so
    // engine.RequestTransform(N) reaches slot N (main.cpp stoi consumer).
    bool RunSlot(int slot) {
        if (bus_ == nullptr) {
            return false;
        }
        bus_->PostCommand(UiCommandType::StartTransform, std::to_string(slot));
        return true;
    }

    void OpenHowItWorks() {
        how_modal_.Open();
    }

    void CloseHowItWorks() {
        how_modal_.Close();
    }

    bool HowOpen() const {
        return how_modal_.IsOpen();
    }

    void OpenResetConfirm() {
        reset_modal_.Open();
    }

    bool ResetOpen() const {
        return reset_modal_.IsOpen();
    }

    void CloseResetConfirm() {
        reset_modal_.Close();
    }

private:
    void DrawHeader(float scale, const Palette& palette);
    void DrawMyTransforms(float scale, const Palette& palette);
    void DrawGrid(float scale, const Palette& palette);
    void DrawCard(const TransformStore::Entry& entry, float width, float height, float scale,
                  const Palette& palette);
    void DrawCreateCard(float width, float height, float scale, const Palette& palette);
    void DrawEditor(float scale, const Palette& palette);
    void DrawResetConfirm(float scale, const Palette& palette);
    void DrawDiff(float scale, const Palette& palette);
    void DrawHowItWorks(float scale, const Palette& palette);
    void PollRecording();
    void SyncBuffers() {
        const std::size_t name_cap = sizeof(name_buffer_) - 1;
        const std::size_t prompt_cap = sizeof(prompt_buffer_) - 1;
        for (std::size_t i = 0; i < name_cap; ++i) {
            name_buffer_[i] = i < edit_name_.size() ? edit_name_[i] : '\0';
            if (i >= edit_name_.size()) {
                break;
            }
        }
        name_buffer_[sizeof(name_buffer_) - 1] = '\0';
        for (std::size_t i = 0; i < prompt_cap; ++i) {
            prompt_buffer_[i] = i < edit_prompt_.size() ? edit_prompt_[i] : '\0';
            if (i >= edit_prompt_.size()) {
                break;
            }
        }
        prompt_buffer_[sizeof(prompt_buffer_) - 1] = '\0';
    }

    void RefreshFromBuffers() {
        edit_name_ = name_buffer_;
        edit_prompt_ = prompt_buffer_;
    }

    TransformStore& store_;
    UiBus* bus_ = nullptr;
    ITransformDiffSource* diff_source_ = nullptr;
    std::vector<Binding> hotkey_bindings_;
    int editing_slot_ = 0;
    bool editor_open_ = false;
    bool recording_ = false;
    std::string edit_name_;
    std::string edit_prompt_;
    std::string edit_shortcut_;
    std::string edit_error_;
    char name_buffer_[256] = {};
    char prompt_buffer_[8192] = {};
    widgets::Modal reset_modal_;
    widgets::Sheet editor_sheet_;
    widgets::Modal diff_modal_;
    widgets::Modal how_modal_;
    widgets::Toast toast_;
};

}
