#pragma once

#include "core/DictionaryStore.h"
#include "core/PersonalDictionary.h"
#include "ui/theme/StudioPalette.h"
#include "ui/widgets/Overlays.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace lwf::ui::pages {

enum class DictionarySort { NameAsc, Newest, MostUsed };

enum class DictionaryTab { All, Personal, SharedTeam };

struct DictionaryPageState {
    std::string search;
    DictionarySort sort = DictionarySort::NameAsc;
    DictionaryTab tab = DictionaryTab::All;
    bool banner_closed = false;
    bool load_attempted = false;
    bool pending_delete = false;
    DictionaryStore::Entry pending_entry;
    float delete_at = -1.0f;
};

inline std::string LowerCopy(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        if (c >= 'A' && c <= 'Z') {
            out.push_back(static_cast<char>(c - 'A' + 'a'));
        } else {
            out.push_back(c);
        }
    }
    return out;
}

inline std::vector<DictionaryStore::Entry> VisibleEntries(const DictionaryStore& store,
                                                          const DictionaryPageState& state) {
    std::vector<DictionaryStore::Entry> rows = store.Search(state.search);
    if (state.pending_delete) {
        const std::int64_t pending_id = state.pending_entry.id;
        rows.erase(std::remove_if(rows.begin(), rows.end(),
                                  [pending_id](const DictionaryStore::Entry& entry) {
                                      return entry.id == pending_id;
                                  }),
                   rows.end());
    }
    switch (state.sort) {
        case DictionarySort::NameAsc:
            std::stable_sort(rows.begin(), rows.end(),
                             [](const DictionaryStore::Entry& left,
                                const DictionaryStore::Entry& right) {
                                 if (left.starred != right.starred) {
                                     return left.starred > right.starred;
                                 }
                                 const std::string left_key = LowerCopy(left.phrase);
                                 const std::string right_key = LowerCopy(right.phrase);
                                 if (left_key != right_key) {
                                     return left_key < right_key;
                                 }
                                 return left.id < right.id;
                             });
            break;
        case DictionarySort::Newest:
            std::stable_sort(rows.begin(), rows.end(),
                             [](const DictionaryStore::Entry& left,
                                const DictionaryStore::Entry& right) {
                                 if (left.starred != right.starred) {
                                     return left.starred > right.starred;
                                 }
                                 if (left.updated_at != right.updated_at) {
                                     return left.updated_at > right.updated_at;
                                 }
                                 if (left.created_at != right.created_at) {
                                     return left.created_at > right.created_at;
                                 }
                                 return left.id > right.id;
                             });
            break;
        case DictionarySort::MostUsed:
            std::stable_sort(rows.begin(), rows.end(),
                             [](const DictionaryStore::Entry& left,
                                const DictionaryStore::Entry& right) {
                                 if (left.starred != right.starred) {
                                     return left.starred > right.starred;
                                 }
                                 if (left.usage_count != right.usage_count) {
                                     return left.usage_count > right.usage_count;
                                 }
                                 if (left.updated_at != right.updated_at) {
                                     return left.updated_at > right.updated_at;
                                 }
                                 return left.id > right.id;
                             });
            break;
    }
    return rows;
}

inline std::vector<std::string> TopPhrases(const DictionaryStore& store, std::size_t limit) {
    std::vector<DictionaryStore::Entry> used;
    for (const DictionaryStore::Entry& entry : store.Entries()) {
        if (entry.usage_count > 0) {
            used.push_back(entry);
        }
    }
    std::sort(used.begin(), used.end(),
              [](const DictionaryStore::Entry& left, const DictionaryStore::Entry& right) {
                  if (left.usage_count != right.usage_count) {
                      return left.usage_count > right.usage_count;
                  }
                  const std::string left_key = LowerCopy(left.phrase);
                  const std::string right_key = LowerCopy(right.phrase);
                  if (left_key != right_key) {
                      return left_key < right_key;
                  }
                  return left.id < right.id;
              });
    std::vector<std::string> phrases;
    for (const DictionaryStore::Entry& entry : used) {
        if (phrases.size() >= limit) {
            break;
        }
        phrases.push_back(entry.phrase);
    }
    return phrases;
}

inline bool VisibleBanner(const DictionaryStore& store, const DictionaryPageState& state) {
    if (store.Size() == 0) {
        return true;
    }
    return !state.banner_closed;
}

inline bool SubmitAdd(DictionaryStore& store, PersonalDictionary* dict,
                      const std::string& phrase, const std::string& replacement,
                      std::string* error) {
    if (!store.Add(phrase, replacement, error)) {
        return false;
    }
    if (!store.Save()) {
        if (error != nullptr) {
            *error = "Could not save dictionary.";
        }
        return false;
    }
    if (dict != nullptr) {
        (void)dict->Reload();
    }
    return true;
}

inline bool SubmitUpdate(DictionaryStore& store, PersonalDictionary* dict, std::int64_t id,
                         const std::string& phrase, const std::string& replacement,
                         std::string* error) {
    if (!store.Update(id, phrase, replacement, error)) {
        return false;
    }
    if (!store.Save()) {
        if (error != nullptr) {
            *error = "Could not save dictionary.";
        }
        return false;
    }
    if (dict != nullptr) {
        (void)dict->Reload();
    }
    return true;
}

inline bool CommitPendingDelete(DictionaryStore& store, DictionaryPageState& state,
                                PersonalDictionary* dict) {
    if (!state.pending_delete) {
        return true;
    }
    const std::int64_t id = state.pending_entry.id;
    const bool removed = store.Remove(id);
    const bool saved = store.Save();
    state.pending_delete = false;
    state.delete_at = -1.0f;
    if (removed && saved && dict != nullptr) {
        (void)dict->Reload();
    }
    return removed && saved;
}

inline bool CommitPendingDelete(DictionaryStore& store, DictionaryPageState& state) {
    return CommitPendingDelete(store, state, nullptr);
}

inline bool CommitExpiredPending(DictionaryStore& store, DictionaryPageState& state,
                                 PersonalDictionary* dict, float now) {
    if (!state.pending_delete || now < state.delete_at) {
        return false;
    }
    return CommitPendingDelete(store, state, dict);
}

inline bool CommitExpiredPending(DictionaryStore& store, DictionaryPageState& state, float now) {
    return CommitExpiredPending(store, state, nullptr, now);
}

inline void UndoPendingDelete(DictionaryPageState& state) {
    state.pending_delete = false;
    state.delete_at = -1.0f;
}

inline bool RequestDelete(DictionaryStore& store, DictionaryPageState& state,
                          PersonalDictionary* dict, std::int64_t id, float now) {
    bool found = false;
    for (const DictionaryStore::Entry& entry : store.Entries()) {
        if (entry.id == id) {
            found = true;
            break;
        }
    }
    if (!found) {
        return false;
    }
    if (state.pending_delete) {
        const std::int64_t previous_id = state.pending_entry.id;
        const bool previous_removed = store.Remove(previous_id);
        (void)store.Save();
        if (previous_removed && dict != nullptr) {
            (void)dict->Reload();
        }
        state.pending_delete = false;
        state.delete_at = -1.0f;
    }
    for (const DictionaryStore::Entry& entry : store.Entries()) {
        if (entry.id == id) {
            state.pending_entry = entry;
            break;
        }
    }
    state.pending_delete = true;
    state.delete_at = now + studio_metrics::toast_dwell_s;
    return true;
}

inline bool RequestDelete(DictionaryStore& store, DictionaryPageState& state, std::int64_t id,
                          float now) {
    return RequestDelete(store, state, nullptr, id, now);
}

class DictionaryPage {
public:
    DictionaryPage(DictionaryStore& store, PersonalDictionary* dict);

    void OpenAdd();
    void OpenEdit(std::int64_t id);
    void Draw(float scale);

private:
    void DrawHeader(float scale);
    void DrawTabs(float scale);
    void DrawToolbar(float scale);
    void DrawBanner(float scale);
    void DrawRows(float scale);
    void DrawDialog(float scale);
    bool AttemptSubmit();

    DictionaryStore& store_;
    PersonalDictionary* dict_;
    DictionaryPageState state_;
    widgets::Modal dialog_;
    widgets::Toast toast_;
    char search_buffer_[256] = {};
    char phrase_buffer_[256] = {};
    char replacement_buffer_[256] = {};
    std::string dialog_error_;
    std::int64_t editing_id_ = 0;
    bool share_checked_ = false;
    bool search_focus_pending_ = false;
    bool phrase_focus_pending_ = false;
};

}
