#include "ui/pages/InsightsPage.h"

#include "ui/theme/ThemePalette.h"
#include "ui/theme/ThemeStyle.h"
#include "ui/widgets/Widgets.h"

#include "imgui.h"

#include <algorithm>
#include <optional>
#include <string>

namespace lwf::ui::insights {

namespace {

// DECISION (T10d pattern): live palette on this page; custom draws read the
// explicit palette argument and widget calls pass it through. Each card
// inlines the hairline child chrome on palette.bg with palette.border
// hairlines (ImGuiChildFlags_Borders), matching T10a/T10b. The page shows
// exactly two averages — words spoken per minute (mic-open speaking time)
// and words written per minute (end-to-end session time) — and nothing else.
using lwf::ui::Color;
using lwf::ui::Pack;
namespace studio_metrics = lwf::ui::studio_metrics;

}  // namespace

void InsightsPage::Draw(float scale, const lwf::ui::Palette& palette) {
    SyncIfStale();
    DrawHeader(scale, palette);
    DrawRow(scale, palette);
}

void InsightsPage::DrawHeader(float scale, const lwf::ui::Palette& palette) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    draw.AddText(origin, Pack(palette.text), kPageTitle);
    const ImVec2 title_size = ImGui::CalcTextSize(kPageTitle);
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(
        ImVec2(origin.x, origin.y + title_size.y + studio_metrics::gap_block * scale));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

void InsightsPage::DrawRow(float scale, const lwf::ui::Palette& palette) {
    const float avail = ImGui::GetContentRegionAvail().x;
    const float gap = studio_metrics::gap_block * scale;
    const float card_w = (avail - gap) * 0.5f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    float bottom = origin.y;
    ImGui::SetCursorScreenPos(origin);
    DrawStatCard("##insights_spoken", summary_.spoken_wpm, kSpokenCaption, kSpokenTooltip, card_w,
                 scale, palette);
    bottom = std::max(bottom, ImGui::GetCursorScreenPos().y);
    ImGui::SetCursorScreenPos(ImVec2(origin.x + card_w + gap, origin.y));
    DrawStatCard("##insights_written", summary_.written_wpm, kWrittenCaption, kWrittenTooltip,
                 card_w, scale, palette);
    bottom = std::max(bottom, ImGui::GetCursorScreenPos().y);
    ImGui::SetCursorScreenPos(ImVec2(origin.x, bottom + gap));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    if (!summary_.has_data) {
        ImGui::TextColored(Color(palette.muted), "%s", kEmptyHint);
    } else if (!summary_.spoken_wpm.has_value() || !summary_.written_wpm.has_value()) {
        ImGui::TextColored(Color(palette.muted), "%s", kFloorHint);
    }
    ImGui::PopFont();
}

void InsightsPage::DrawStatCard(const char* id, const std::optional<double>& wpm,
                                const char* caption, const char* tooltip, float width, float scale,
                                const lwf::ui::Palette& palette) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(palette.bg));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, studio_metrics::radius_card * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(studio_metrics::gap_block * scale, studio_metrics::gap_block * scale));
    ImGui::BeginChild(id, ImVec2(width, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    const std::string value = WpmLabel(wpm);
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted(value.c_str());
    ImGui::PopFont();
    ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
    const std::string info_id = std::string(id) + "_info";
    widgets::IconButton(info_id.c_str(), "i", scale, palette);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
    ImGui::PushFont(nullptr, studio_metrics::font_caps_px);
    ImGui::TextColored(Color(palette.muted), "%s", caption);
    ImGui::PopFont();
    ImGui::EndChild();
}

}  // namespace lwf::ui::insights
