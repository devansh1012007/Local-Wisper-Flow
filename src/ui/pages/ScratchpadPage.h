#pragma once

#include "core/NotesStore.h"
#include "core/SettingsStore.h"
#include "core/UiBus.h"
#include "ui/theme/StudioPalette.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

class UiBus;

namespace lwf::ui::scratchpad {

inline constexpr const char* kBannerHeadline = "Capture ideas without leaving the flow";
inline constexpr const char* kBannerBody =
    "Notes stay on this computer. Dictate straight into the editor while the scratchpad "
    "window is focused.";
inline constexpr const char* kDefaultScratchpadShortcut = "Win+Alt+S";
inline constexpr const char* kShortcutHelper = "Click to enable shortcut";
inline constexpr std::size_t kPreviewChars = 80;

inline std::string FlatPreview(const std::string& body) {
    std::string flat;
    flat.reserve(body.size());
    bool last_space = true;
    for (const char c : body) {
        if (c == '\r' || c == '\n' || c == '\t' || c == ' ') {
            if (!last_space) {
                flat.push_back(' ');
            }
            last_space = true;
        } else {
            flat.push_back(c);
            last_space = false;
        }
    }
    if (!flat.empty() && flat.back() == ' ') {
        flat.pop_back();
    }
    return flat;
}

inline std::string PreviewText(const std::string& body, std::size_t max_chars = kPreviewChars) {
    const std::string flat = FlatPreview(body);
    std::size_t points = 0;
    std::size_t bytes = 0;
    while (bytes < flat.size() && points < max_chars) {
        const unsigned char lead = static_cast<unsigned char>(flat[bytes]);
        std::size_t width = 1;
        if (lead >= 0xF0) {
            width = 4;
        } else if (lead >= 0xE0) {
            width = 3;
        } else if (lead >= 0xC0) {
            width = 2;
        } else if ((lead & 0xC0) == 0x80) {
            width = 1;
        }
        if (bytes + width > flat.size()) {
            break;
        }
        bytes += width;
        ++points;
    }
    return flat.substr(0, bytes);
}

inline std::string FormatModifiedDate(std::int64_t epoch_seconds) {
    std::int64_t days = epoch_seconds / 86400;
    const std::int64_t rem = epoch_seconds % 86400;
    if (rem < 0) {
        days -= 1;
    }
    const std::int64_t z = days + 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const std::int64_t doe = z - era * 146097;
    const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t year = yoe + era * 400;
    const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const std::int64_t mp = (5 * doy + 2) / 153;
    const std::int64_t day = doy - (153 * mp + 2) / 5 + 1;
    const std::int64_t month = mp < 10 ? mp + 3 : mp - 9;
    const std::int64_t full_year = month <= 2 ? year + 1 : year;
    char buffer[16] = {};
    const int y = static_cast<int>(full_year);
    const int m = static_cast<int>(month);
    const int d = static_cast<int>(day);
    const int n = std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", y, m, d);
    if (n <= 0) {
        return {};
    }
    return std::string(buffer, static_cast<std::size_t>(n));
}

inline std::string ShortcutDisplayLabel(const std::string& chord) {
    if (chord.empty()) {
        return std::string(kShortcutHelper);
    }
    std::string out;
    out.reserve(chord.size());
    bool last_space = true;
    for (const char c : chord) {
        if (c == '+') {
            if (!last_space) {
                out.push_back(' ');
            }
            last_space = true;
        } else if (c == ' ' || c == '\t') {
            if (!last_space) {
                out.push_back(' ');
            }
            last_space = true;
        } else {
            out.push_back(c);
            last_space = false;
        }
    }
    if (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

struct UndoWindow {
    std::optional<NotesStore::Note> deleted;
    double expires_at = -1.0;

    void Arm(const NotesStore::Note& note, double now) {
        deleted = note;
        expires_at = now + studio_metrics::toast_dwell_s;
    }

    bool IsActive(double now) const {
        if (!deleted.has_value()) {
            return false;
        }
        return now < expires_at;
    }

    bool HasPending() const {
        return deleted.has_value();
    }

    std::optional<NotesStore::Note> Take() {
        std::optional<NotesStore::Note> out = deleted;
        deleted.reset();
        expires_at = -1.0;
        return out;
    }

    void Clear() {
        deleted.reset();
        expires_at = -1.0;
    }
};

inline bool IsUndoExpired(const UndoWindow& window, double now) {
    return window.HasPending() && !window.IsActive(now);
}

}

class ScratchpadPage {
public:
    ScratchpadPage(NotesStore& notes, SettingsStore& settings, UiBus& bus);

    void Draw(float scale);

    bool HasPendingUndo() const;
    void ClearUndo();
    std::string ShortcutLabel() const;
    bool IsShortcutArmed() const;

private:
    void DrawHeader(float scale);
    void DrawBanner(float scale);
    void DrawRecents(float scale);
    void DrawNoteRow(const NotesStore::Note& note, float scale);
    void DrawUndoToast(float scale);

    NotesStore& notes_;
    SettingsStore& settings_;
    UiBus& bus_;
    char search_buffer_[256] = {};
    bool search_focus_pending_ = false;
    std::string shortcut_ = lwf::ui::scratchpad::kDefaultScratchpadShortcut;
    bool shortcut_armed_ = false;
    lwf::ui::scratchpad::UndoWindow undo_;
};
