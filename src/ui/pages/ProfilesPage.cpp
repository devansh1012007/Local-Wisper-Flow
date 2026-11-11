#include "ui/pages/ProfilesPage.h"

#include "ui/theme/ThemePalette.h"
#include "ui/theme/ThemeStyle.h"
#include "ui/widgets/Widgets.h"

#include "imgui.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace lwf::ui::pages {

namespace {

using lwf::ui::Color;
using lwf::ui::Pack;

}  // namespace

void ProfilesPage::Draw(float scale, const lwf::ui::Palette& palette) {
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted("Profiles");
    ImGui::PopFont();
    ImGui::SameLine(0.0f, studio_metrics::gap_block * scale);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::TextDisabled("How dictation adapts to each app.");
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_section * scale));
    ImGui::GetWindowDrawList()->AddText(ImGui::GetCursorScreenPos(),
                                       Pack(palette.text), "");
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(palette.bg));
    ImGui::PopStyleColor();

    if (restart_needed_) {
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::TextColored(Color(palette.err), "Applies after restart.");
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap * scale));
    }

    const std::vector<std::string> names = ProfileNames(resolver_);
    if (!resolver_.Loaded() || names.empty()) {
        ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        draw.AddText(ImVec2(pos.x, pos.y), Pack(palette.muted), "No profiles.");
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::row_height * scale));
        ImGui::PopFont();
        return;
    }

    const auto& categories = AppContextResolver::CategoryNames();

    for (const std::string& name : names) {
        const AppProfile& profile = resolver_.ProfileByName(name);

        // Category row.
        {
            const std::string row_id = "##profile_" + name;
            const std::string combo_id = "##profile_cat_" + name;
            lwf::ui::widgets::Row(row_id.c_str(), name.c_str(),
                                  profile.prompt_addendum.empty() ? "\xE2\x80\x94"
                                                                  : profile.prompt_addendum.c_str(),
                                  248.0f * scale, scale, palette);
            int idx = CategoryIndex(profile.category);
            std::string preview = (idx >= 0 && idx < static_cast<int>(categories.size()))
                                      ? categories[static_cast<std::size_t>(idx)]
                                      : categories.back();
            if (ImGui::BeginCombo(combo_id.c_str(), preview.c_str())) {
                for (int i = 0; i < static_cast<int>(categories.size()); ++i) {
                    const bool selected = (i == idx);
                    if (ImGui::Selectable(categories[static_cast<std::size_t>(i)].c_str(),
                                          selected)) {
                        resolver_.AssignCategory(name, categories[static_cast<std::size_t>(i)]);
                        resolver_.Save();
                        restart_needed_ = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            lwf::ui::widgets::RowEnd(scale);
        }

        // Enabled row.
        {
            const std::string row_id = "##profile_enabled_" + name;
            const std::string toggle_id = "##profile_toggle_" + name;
            const bool enabled = resolver_.ProfileEnabled(name);
            lwf::ui::widgets::Row(row_id.c_str(), "Enabled", nullptr, 52.0f * scale, scale,
                                  palette);
            if (lwf::ui::widgets::ToggleSwitch(toggle_id.c_str(), enabled, scale, palette)) {
                resolver_.SetEnabled(name, !enabled);
                resolver_.Save();
                restart_needed_ = true;
            }
            lwf::ui::widgets::RowEnd(scale);
        }

        // hairline separator
        {
            const ImVec2 sep = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(
                sep, ImVec2(sep.x + ImGui::GetContentRegionAvail().x, sep.y),
                Pack(palette.border), 1.0f * scale);
        }
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap * scale));
    }
}

}  // namespace lwf::ui::pages