#pragma once

#include "core/SnippetStore.h"
#include "ui/theme/ThemePalette.h"
#include "ui/widgets/Overlays.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace lwf::ui::pages {

inline constexpr const char* kSnippetsBannerHeadline = "Reusable phrases.";
inline constexpr const char* kSnippetsBannerExampleTrigger = "my booking link";
inline constexpr const char* kSnippetsBannerExampleExpansion = "https://example.com/book";
inline constexpr const char* kSnippetsEmptyCopy = "No snippets yet.";
inline constexpr const char* kSnippetsEditorAddTitle = "Add new snippet";
inline constexpr const char* kSnippetsEditorEditTitle = "Edit snippet";
inline constexpr const char* kSnippetsAddLabel = "Add new snippet";
inline constexpr const char* kSnippetsUndoAction = "Undo";
inline constexpr int kSnippetsEditorVisibleLines = 6;

struct SnippetsPageState {
    bool banner_closed = false;
    bool pending_delete = false;
    SnippetStore::Entry pending_entry;
    float delete_at = -1.0f;
};

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

inline std::string RowText(const SnippetStore::Entry& entry) {
    return entry.trigger + " → " + entry.expansion;
}

inline std::vector<SnippetStore::Entry> VisibleEntries(const SnippetStore& store,
                                                       const SnippetsPageState& state) {
    std::vector<SnippetStore::Entry> rows = store.Entries();
    if (state.pending_delete) {
        const std::string pending_id = state.pending_entry.id;
        rows.erase(std::remove_if(rows.begin(), rows.end(),
                                  [&pending_id](const SnippetStore::Entry& entry) {
                                      return entry.id == pending_id;
                                  }),
                   rows.end());
    }
    return rows;
}

inline bool SubmitSnippet(SnippetStore& store, const std::string& trigger,
                          const std::string& expansion, const std::string& exclude_id,
                          std::string* error) {
    const std::string problem = store.Validate(trigger, expansion, exclude_id);
    if (!problem.empty()) {
        if (error != nullptr) {
            *error = problem;
        }
        return false;
    }
    std::string upsert_error;
    if (!store.Upsert(trigger, expansion, exclude_id, upsert_error)) {
        if (error != nullptr) {
            *error = upsert_error;
        }
        return false;
    }
    if (error != nullptr) {
        error->clear();
    }
    (void)store.Save();
    return true;
}

inline bool RequestDelete(SnippetStore& store, SnippetsPageState& state, const std::string& id,
                          float now) {
    bool found = false;
    for (const SnippetStore::Entry& entry : store.Entries()) {
        if (entry.id == id) {
            found = true;
            break;
        }
    }
    if (!found) {
        return false;
    }
    for (const SnippetStore::Entry& entry : store.Entries()) {
        if (entry.id == id) {
            state.pending_entry = entry;
            break;
        }
    }
    store.RemoveById(id);
    (void)store.Save();
    state.pending_delete = true;
    state.delete_at = now + studio_metrics::toast_dwell_s;
    return true;
}

inline bool CommitExpiredPending(SnippetsPageState& state, float now) {
    if (!state.pending_delete || now < state.delete_at) {
        return false;
    }
    state.pending_delete = false;
    state.delete_at = -1.0f;
    return true;
}

inline bool UndoPendingDelete(SnippetStore& store, SnippetsPageState& state, std::string* error) {
    if (!state.pending_delete) {
        return false;
    }
    std::string restore_error;
    if (!store.Upsert(state.pending_entry.trigger, state.pending_entry.expansion,
                      restore_error)) {
        if (error != nullptr) {
            *error = restore_error;
        }
        return false;
    }
    (void)store.Save();
    state.pending_delete = false;
    state.delete_at = -1.0f;
    return true;
}

inline bool VisibleBanner(const SnippetsPageState& state) {
    return !state.banner_closed;
}

class SnippetsPage {
public:
    explicit SnippetsPage(SnippetStore& store) : store_(store) {}

    SnippetsPage(const SnippetsPage&) = delete;
    SnippetsPage& operator=(const SnippetsPage&) = delete;

    void Draw(float scale);

    void OpenAdd() {
        editing_id_.clear();
        edit_trigger_.clear();
        edit_expansion_.clear();
        edit_error_.clear();
        SyncBuffers();
        editing_ = true;
    }

    void OpenEdit(const std::string& id) {
        for (const SnippetStore::Entry& entry : store_.Entries()) {
            if (entry.id != id) {
                continue;
            }
            editing_id_ = id;
            edit_trigger_ = entry.trigger;
            edit_expansion_ = entry.expansion;
            edit_error_.clear();
            SyncBuffers();
            editing_ = true;
            return;
        }
    }

    void CloseEditor() {
        editing_ = false;
        editing_id_.clear();
        edit_error_.clear();
    }

    bool IsEditing() const {
        return editing_;
    }

    const std::string& EditingId() const {
        return editing_id_;
    }

    const std::string& EditTrigger() const {
        return edit_trigger_;
    }

    const std::string& EditExpansion() const {
        return edit_expansion_;
    }

    const std::string& EditError() const {
        return edit_error_;
    }

    void SetEditTrigger(std::string trigger) {
        edit_trigger_ = std::move(trigger);
        SyncBuffers();
    }

    void SetEditExpansion(std::string expansion) {
        edit_expansion_ = std::move(expansion);
        SyncBuffers();
    }

    bool SaveEdit() {
        if (!IsEditing()) {
            return false;
        }
        std::string error;
        if (!SubmitSnippet(store_, edit_trigger_, edit_expansion_, editing_id_, &error)) {
            edit_error_ = error;
            return false;
        }
        edit_trigger_.clear();
        edit_expansion_.clear();
        SyncBuffers();
        CloseEditor();
        return true;
    }

private:
    void DrawHeader(float scale);
    void DrawBanner(float scale);
    void DrawRows(float scale);
    void DrawSheet(float scale);
    bool DeleteEdit();

    void SyncBuffers() {
        const std::size_t trigger_cap = sizeof(trigger_buffer_) - 1;
        const std::size_t expansion_cap = sizeof(expansion_buffer_) - 1;
        for (std::size_t i = 0; i < trigger_cap; ++i) {
            trigger_buffer_[i] = i < edit_trigger_.size() ? edit_trigger_[i] : '\0';
            if (i >= edit_trigger_.size()) {
                break;
            }
        }
        trigger_buffer_[sizeof(trigger_buffer_) - 1] = '\0';
        for (std::size_t i = 0; i < expansion_cap; ++i) {
            expansion_buffer_[i] = i < edit_expansion_.size() ? edit_expansion_[i] : '\0';
            if (i >= edit_expansion_.size()) {
                break;
            }
        }
        expansion_buffer_[sizeof(expansion_buffer_) - 1] = '\0';
    }

    void RefreshFromBuffers() {
        edit_trigger_ = trigger_buffer_;
        edit_expansion_ = expansion_buffer_;
    }

    SnippetStore& store_;
    SnippetsPageState state_;
    widgets::Sheet editor_sheet_;
    widgets::Toast toast_;
    char trigger_buffer_[1024] = {};
    char expansion_buffer_[16384] = {};
    std::string editing_id_;
    std::string edit_trigger_;
    std::string edit_expansion_;
    std::string edit_error_;
    bool editing_ = false;
};

}
