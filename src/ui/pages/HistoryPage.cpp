#include "ui/pages/HistoryPage.h"

#include "ui/theme/ThemePalette.h"
#include "ui/theme/ThemeStyle.h"
#include "ui/widgets/Widgets.h"

#include "imgui.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace lwf::ui::pages {

namespace {

using lwf::ui::Color;
using lwf::ui::Pack;

}  // namespace

void HistoryPage::Draw(float scale, const lwf::ui::Palette& palette) {
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted("History");
    ImGui::PopFont();
    ImGui::SameLine(0.0f, studio_metrics::gap_block * scale);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::TextDisabled("What you dictated, kept on this computer.");
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_section * scale));

    // Search row
    ImGui::PushItemWidth(-1.0f);
    ImGui::InputTextWithHint("##history_search", "Search history", search_buffer_,
                            sizeof(search_buffer_), ImGuiInputTextFlags_None);
    ImGui::PopItemWidth();
    ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap * scale));

    // Retention link
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::TextDisabled("Retention: Settings > Privacy");
    ImGui::PopFont();
    ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
    if (widgets::LightButton("##history_open_privacy", "Open privacy settings", scale, palette)) {
        if (open_privacy_) {
            open_privacy_();
        }
    }
    ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_section * scale));

    const std::string search(search_buffer_);
    const std::vector<HistoryEntry> rows = VisibleEntries(store_, search, 200);

    if (store_.Size() == 0) {
        ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        const ImVec2 text_size = ImGui::CalcTextSize("No history yet.");
        draw.AddText(ImVec2(pos.x, pos.y), Pack(palette.muted), "No history yet.");
        (void)text_size;
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::row_height * scale));
        ImGui::PopFont();
    }

    if (!export_error_.empty()) {
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::TextColored(Color(palette.err), "%s", export_error_.c_str());
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap * scale));
    }

    // List
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(palette.bg));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, studio_metrics::radius_row * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##history_rows", ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImDrawList& draw = *ImGui::GetWindowDrawList();
    const float width = ImGui::GetContentRegionAvail().x;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const HistoryEntry& entry = rows[i];
        if (i > 0) {
            const ImVec2 sep = ImGui::GetCursorScreenPos();
            draw.AddLine(ImVec2(sep.x, sep.y), ImVec2(sep.x + width, sep.y),
                         Pack(palette.border), 1.0f * scale);
        }
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::TextColored(Color(palette.muted), "%s", FormatTimestamp(entry.timestamp).c_str());
        ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
        if (!entry.app_profile.empty()) {
            ImGui::TextColored(Color(palette.muted), "%s", entry.app_profile.c_str());
            ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
        }
        ImGui::PopFont();

        ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
        ImGui::PushStyleColor(ImGuiCol_Text, Color(palette.text));
        ImGui::TextWrapped("%s", entry.text.c_str());
        ImGui::PopStyleColor();
        ImGui::PopFont();
        // Pack(palette.text) is the draw-list spelling of the same role.

        if (!entry.raw.empty() && entry.raw != entry.text) {
            ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
            ImGui::TextColored(Color(palette.muted), "%s", entry.raw.c_str());
            ImGui::PopFont();
        }
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap * scale));
    }
    ImGui::EndChild();

    // Export row
    ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_section * scale));
    const bool can_export = store_.Size() > 0;
    if (!can_export) {
        ImGui::BeginDisabled();
    }
    if (widgets::LightButton("##history_export", "Export...", scale, palette)) {
        const std::int64_t ts = HistoryStore::NowSeconds();
        std::filesystem::path dir = std::filesystem::path(store_.Path()).parent_path();
        if (dir.empty()) {
            dir = ".";
        }
        const std::string path = (dir / ("history-export-" + std::to_string(ts) + ".tsv")).string();
        // Try to resolve a data dir from settings? Not available. Keep cwd.
        if (store_.Export(path)) {
            last_export_path_ = path;
            export_error_.clear();
            export_success_ = true;
            export_success_until_ = static_cast<float>(ImGui::GetTime()) + 1.5f;
            if (open_file_) {
                open_file_(path);
            }
        } else {
            export_success_ = false;
            export_error_ = "Could not export history. Check the destination folder and try again.";
        }
    }
    if (!can_export) {
        ImGui::EndDisabled();
    }

    if (export_success_ && ImGui::GetTime() < export_success_until_) {
        ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::TextDisabled("Exported");
        ImGui::PopFont();
    } else if (export_success_ && ImGui::GetTime() >= export_success_until_) {
        export_success_ = false;
    }
}

}  // namespace lwf::ui::pages
