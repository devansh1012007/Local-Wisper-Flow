#pragma once

#include "core/HistoryStore.h"
#include "core/SettingsStore.h"
#include "ui/theme/ThemePalette.h"

#include <cstddef>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

namespace lwf::ui::pages {

inline std::vector<HistoryEntry> VisibleEntries(const HistoryStore& store,
                                               const std::string& search,
                                               std::size_t limit) {
    return store.Search(search, limit);
}

inline std::string FormatTimestamp(std::int64_t ts) {
    if (ts <= 0) {
        return "\xE2\x80\x94";
    }
    std::time_t value = static_cast<std::time_t>(ts);
    std::tm* local = std::localtime(&value);
    if (local == nullptr) {
        return "\xE2\x80\x94";
    }
    char buffer[32] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", local);
    return std::string(buffer);
}

class HistoryPage {
public:
    HistoryPage(HistoryStore& store, SettingsStore& settings,
                std::function<void(const std::string&)> open_file,
                std::function<void()> open_privacy)
        : store_(store),
          settings_(settings),
          open_file_(std::move(open_file)),
          open_privacy_(std::move(open_privacy)) {}

    HistoryPage(const HistoryPage&) = delete;
    HistoryPage& operator=(const HistoryPage&) = delete;

    void Draw(float scale, const lwf::ui::Palette& palette = lwf::ui::kDarkPalette);

private:
    HistoryStore& store_;
    SettingsStore& settings_;
    std::function<void(const std::string&)> open_file_;
    std::function<void()> open_privacy_;
    char search_buffer_[256] = {};
    std::string last_export_path_;
    std::string export_error_;
    bool export_success_ = false;
    float export_success_until_ = -1.0f;
};

}  // namespace lwf::ui::pages