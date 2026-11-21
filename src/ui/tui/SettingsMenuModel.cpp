#include "ui/tui/SettingsMenuModel.h"

#include "os/LinuxKeysym.h"

#include <algorithm>
#include <cstdio>
#include <fstream>

namespace {

// Curated grouping of the schema (plan C07: "TUI lists/persists every
// editable config key"). Keys not listed anywhere land in "Advanced", and a
// unit test proves group coverage == full schema coverage.
const struct {
    const char* group;
    std::vector<const char*> keys;
} kGroupSpecs[] = {
    {"Hotkeys",
     {"hotkey_ptt", "hotkey_keysym", "hotkey_handsfree", "hotkey_cancel", "hotkey_command",
      "hotkey_paste_last", "hotkey_evdev_enabled", "double_tap_handsfree",
      "double_tap_window_ms", "avoid_altgr_conflict"}},
    {"Speech & STT",
     {"whisper_model_path", "whisper_language", "stt_provider", "stt_consent_accepted",
      "stt_language", "stt_timeout_ms", "stt_max_clips", "vad_threshold", "streaming_stt",
      "streaming_min_segment_ms", "streaming_pause_ms", "mic_device"}},
    {"Dictation behavior",
     {"dictation_enabled", "min_hold_ms", "max_recording_ms", "hands_free_silence_ms",
      "sound_enabled", "sound_volume", "inject_delay_ms"}},
    {"Cleanup & commands",
     {"cleanup_enabled", "llm_model_path", "cleanup_min_words", "cleanup_timeout_ms",
      "cleanup_language_policy", "spoken_commands_aggressive", "dictionary_fuzzy",
      "snippet_mode", "command_timeout_ms"}},
    {"History & privacy",
     {"history_enabled", "keep_raw_in_history", "history_max_entries", "history_max_age_days",
      "debug_log_transcripts", "use_remote", "remote_endpoint"}},
    {"Linux daemon",
     {"daemon_toasts"}},
};

std::string HumanizeKey(const std::string& key) {
    std::string label;
    bool upper_next = true;
    for (const char c : key) {
        if (c == '_') {
            label += ' ';
            upper_next = true;
        } else if (upper_next) {
            label += static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
            upper_next = false;
        } else {
            label += c;
        }
    }
    return label;
}

std::string FormatValue(const SettingsStore& store, const SettingsStore::SchemaEntry& entry) {
    char buffer[64];
    switch (entry.type) {
        case SettingsStore::Type::Int:
            return std::to_string(store.GetInt(entry.key));
        case SettingsStore::Type::Double:
            std::snprintf(buffer, sizeof(buffer), "%.3f", store.GetDouble(entry.key));
            return buffer;
        case SettingsStore::Type::Bool:
            return store.GetBool(entry.key) ? "on" : "off";
        case SettingsStore::Type::String:
            return store.GetString(entry.key);
    }
    return std::string();
}

}  // namespace

SettingsMenuModel::SettingsMenuModel(SettingsStore& store) : store_(store) {
    BuildGroups();
    RefreshItemRows();
}

void SettingsMenuModel::BuildGroups() {
    const auto& schema = SettingsStore::Schema();
    std::vector<bool> claimed(schema.size(), false);
    for (const auto& spec : kGroupSpecs) {
        GroupDef def;
        def.name = spec.group;
        for (const char* key : spec.keys) {
            for (std::size_t i = 0; i < schema.size(); ++i) {
                if (schema[i].key == key && !claimed[i]) {
                    claimed[i] = true;
                    def.keys.push_back(key);
                }
            }
        }
        if (!def.keys.empty()) {
            groups_.push_back(std::move(def));
        }
    }
    GroupDef advanced;
    advanced.name = "Advanced";
    for (std::size_t i = 0; i < schema.size(); ++i) {
        if (!claimed[i]) {
            advanced.keys.push_back(schema[i].key);
        }
    }
    if (!advanced.keys.empty()) {
        groups_.push_back(std::move(advanced));
    }
    group_names_.clear();
    for (const GroupDef& def : groups_) {
        group_names_.push_back(def.name);
    }
}

void SettingsMenuModel::RefreshItemRows() {
    item_rows_.clear();
    if (group_cursor_ < 0 || group_cursor_ >= static_cast<int>(groups_.size())) {
        return;
    }
    const auto& schema = SettingsStore::Schema();
    for (const std::string& key : groups_[group_cursor_].keys) {
        for (const auto& entry : schema) {
            if (entry.key == key) {
                ItemRow row;
                row.key = entry.key;
                row.label = HumanizeKey(entry.key);
                row.type = entry.type;
                row.hot_apply = entry.hot_apply;
                row.value = FormatValue(store_, entry);
                item_rows_.push_back(std::move(row));
                break;
            }
        }
    }
}

void SettingsMenuModel::EnterGroup(int index) {
    if (index < 0 || index >= static_cast<int>(groups_.size())) {
        return;
    }
    group_cursor_ = index;
    item_cursor_ = 0;
    RefreshItemRows();
    page_ = Page::Items;
}

void SettingsMenuModel::MoveCursor(int delta, int page_delta) {
    if (page_ == Page::Groups) {
        const int count = static_cast<int>(group_names_.size());
        if (page_delta != 0) {
            group_cursor_ = std::clamp(group_cursor_ + page_delta, 0, count - 1);
        } else {
            group_cursor_ = std::clamp(group_cursor_ + delta, 0, count - 1);
        }
    } else if (page_ == Page::Items) {
        const int count = static_cast<int>(item_rows_.size());
        if (page_delta != 0) {
            item_cursor_ = std::clamp(item_cursor_ + page_delta, 0, count - 1);
        } else {
            item_cursor_ = std::clamp(item_cursor_ + delta, 0, count - 1);
        }
    }
}

bool SettingsMenuModel::ToggleBool(const ItemRow& row) {
    const bool value = !store_.GetBool(row.key);
    store_.SetBool(row.key, value);
    dirty_ = true;
    status_ = row.label + ": " + (value ? "on" : "off");
    RefreshItemRows();
    return true;
}

void SettingsMenuModel::BeginEdit() {
    if (item_cursor_ < 0 || item_cursor_ >= static_cast<int>(item_rows_.size())) {
        return;
    }
    const ItemRow& row = item_rows_[item_cursor_];
    if (row.type == SettingsStore::Type::Bool) {
        ToggleBool(row);
        return;
    }
    editor_key_ = row.key;
    editor_draft_ = row.value;
    page_ = Page::Editor;
}

void SettingsMenuModel::SetEditorText(const std::string& text) {
    editor_draft_ = text;
}

std::optional<std::string> SettingsMenuModel::CommitEditor() {
    const auto& schema = SettingsStore::Schema();
    const SettingsStore::SchemaEntry* entry = nullptr;
    for (const auto& candidate : schema) {
        if (candidate.key == editor_key_) {
            entry = &candidate;
            break;
        }
    }
    if (entry == nullptr) {
        page_ = Page::Items;
        return std::string("unknown key");
    }
    try {
        switch (entry->type) {
            case SettingsStore::Type::Int: {
                const long long parsed = std::stoll(editor_draft_);
                const long long min = static_cast<long long>(entry->min_value);
                const long long max = static_cast<long long>(entry->max_value);
                const long long clamped = std::clamp(parsed, min, max);
                store_.SetInt(entry->key, static_cast<int>(clamped));
                if (clamped != parsed) {
                    status_ = "clamped to [" + std::to_string(min) + ", " + std::to_string(max) +
                              "]";
                } else {
                    status_ = std::string(entry->key) + " set";
                }
                break;
            }
            case SettingsStore::Type::Double: {
                const double parsed = std::stod(editor_draft_);
                const double clamped = std::clamp(parsed, entry->min_value, entry->max_value);
                store_.SetDouble(entry->key, clamped);
                status_ = std::string(entry->key) + " set";
                break;
            }
            case SettingsStore::Type::String:
                store_.SetString(entry->key, editor_draft_);
                status_ = std::string(entry->key) + " set";
                break;
            case SettingsStore::Type::Bool:
                return std::string("bool keys toggle with Enter");
        }
    } catch (const std::exception&) {
        return std::string("invalid value for " + std::string(entry->key));
    }
    dirty_ = true;
    RefreshItemRows();
    page_ = Page::Items;
    return std::nullopt;
}

std::string SettingsMenuModel::Apply(Action action) {
    switch (action) {
        case Action::Up:
            MoveCursor(-1, 0);
            break;
        case Action::Down:
            MoveCursor(1, 0);
            break;
        case Action::PageUp:
            MoveCursor(0, -8);
            break;
        case Action::PageDown:
            MoveCursor(0, 8);
            break;
        case Action::Enter:
            if (page_ == Page::Groups) {
                EnterGroup(group_cursor_);
                status_.clear();
            } else if (page_ == Page::Items) {
                BeginEdit();
            }
            break;
        case Action::Back:
            if (page_ == Page::Items) {
                page_ = Page::Groups;
                status_.clear();
            } else if (page_ == Page::Editor) {
                page_ = Page::Items;
                status_ = "edit cancelled";
            }
            break;
        case Action::Save:
            // The frontend persists (needs the config path + daemon pid) and
            // calls MarkSaved; the model only reports.
            return "save";
        case Action::ReloadFromDisk:
            return "reload";
        case Action::Quit:
            return "quit";
    }
    return status_;
}

void SettingsMenuModel::MarkSaved(bool persisted) {
    if (persisted) {
        dirty_ = false;
        status_ = "saved; daemon reloaded";
    } else {
        status_ = "saved (no daemon running)";
        dirty_ = false;
    }
}

std::string SettingsMenuModel::KeysymHint() const {
    if (page_ != Page::Items || item_cursor_ < 0 ||
        item_cursor_ >= static_cast<int>(item_rows_.size())) {
        return std::string();
    }
    const ItemRow& row = item_rows_[item_cursor_];
    if (row.type != SettingsStore::Type::Int || row.key != "hotkey_keysym") {
        return std::string();
    }
    const int value = store_.GetInt(row.key);
    if (value <= 0) {
        return "0 = unmapped (uses the chord above)";
    }
    return "0x" + [&] {
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "%x", value);
        return std::string(buffer);
    }() + " = " + lwf::linuxkeys::KeysymDisplayName(static_cast<std::uint32_t>(value));
}

std::vector<std::string> SettingsMenuModel::TailFile(const std::string& path,
                                                     std::size_t max_lines) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    std::vector<std::string> tail;
    std::vector<std::string> window(max_lines);
    std::size_t next = 0;
    bool wrapped = false;
    std::string line;
    while (std::getline(file, line)) {
        window[next] = line;
        next = (next + 1) % max_lines;
        if (next == 0) {
            wrapped = true;
        }
    }
    tail.reserve(wrapped ? max_lines : next);
    const std::size_t start = wrapped ? next : 0;
    const std::size_t count = wrapped ? max_lines : next;
    for (std::size_t offset = 0; offset < count; ++offset) {
        tail.push_back(window[(start + offset) % max_lines]);
    }
    return tail;
}
