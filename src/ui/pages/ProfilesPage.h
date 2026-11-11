#pragma once

#include "core/AppContextResolver.h"
#include "core/SettingsStore.h"
#include "ui/theme/ThemePalette.h"

#include <algorithm>
#include <string>
#include <vector>

namespace lwf::ui::pages {

inline std::vector<std::string> ProfileNames(const AppContextResolver& resolver) {
    std::vector<std::string> names = resolver.ListProfiles();
    std::sort(names.begin(), names.end());
    return names;
}

inline int CategoryIndex(const std::string& category) {
    const auto& names = AppContextResolver::CategoryNames();
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i] == category) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(names.size()) - 1;
}

class ProfilesPage {
public:
    ProfilesPage(AppContextResolver& resolver, SettingsStore& settings,
                 const std::string& data_dir)
        : resolver_(resolver), settings_(settings), data_dir_(data_dir) {}

    ProfilesPage(const ProfilesPage&) = delete;
    ProfilesPage& operator=(const ProfilesPage&) = delete;

    void Draw(float scale, const lwf::ui::Palette& palette = lwf::ui::kDarkPalette);

private:
    AppContextResolver& resolver_;
    SettingsStore& settings_;
    std::string data_dir_;
    bool restart_needed_ = false;
    char search_buffer_[128] = {};
};

}  // namespace lwf::ui::pages