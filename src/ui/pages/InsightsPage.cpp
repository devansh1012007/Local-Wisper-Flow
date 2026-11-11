#include "ui/pages/InsightsPage.h"

#include "ui/theme/ThemePalette.h"
#include "ui/theme/ThemeStyle.h"
#include "ui/widgets/Widgets.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace lwf::ui::insights {

namespace {

// DECISION (T10d): live palette on this page. Custom draws read the explicit
// palette argument (bg / border / text / muted / accent); widget calls pass
// it through. The HeatColor ramp fills are kept as data viz (the legend
// swatches and grid levels), since HeatColor already tracks the live theme.
// The share badge and its dialog are removed with T10d (no stub remains);
// the file-local BeginCard helpers are gone and each card inlines the
// hairline child chrome on palette.bg with palette.border hairlines
// (ImGuiChildFlags_Borders), matching T10a/T10b.
using lwf::ui::Color;
using lwf::ui::HeatColor;
using lwf::ui::Pack;
namespace studio_metrics = lwf::ui::studio_metrics;

void DrawDivider(float scale, const lwf::ui::Palette& palette) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    draw.AddLine(pos, ImVec2(pos.x + width, pos.y), Pack(palette.border), 1.0f * scale);
    ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_section * scale));
}

void DrawLegend(float scale, const lwf::ui::Palette& palette) {
    const float cell = studio_metrics::activity_cell * scale;
    const float gap = studio_metrics::activity_gap * scale;
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::TextUnformatted("More");
    ImGui::PopFont();
    ImGui::SameLine(0.0f, studio_metrics::gap_micro * scale);
    const int levels[] = {4, 3, 2, 1};
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    for (int level : levels) {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(cell, cell));
        draw.AddRectFilled(pos, ImVec2(pos.x + cell, pos.y + cell), Pack(HeatColor(level)),
                           studio_metrics::activity_corner * scale);
        ImGui::SameLine(0.0f, gap);
    }
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::TextUnformatted("Less");
    ImGui::PopFont();
    ImGui::SameLine(0.0f, studio_metrics::gap_micro * scale);
    const ImVec2 empty_pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(cell, cell));
    draw.AddRectFilled(empty_pos, ImVec2(empty_pos.x + cell, empty_pos.y + cell),
                       Pack(palette.border), studio_metrics::activity_corner * scale);
}

void DrawInfoRow(const char* id, const std::string& text, const char* tooltip, float scale,
                 const lwf::ui::Palette& palette) {
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopFont();
    ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
    widgets::IconButton(id, "i", scale, palette);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
}

}  // namespace

void InsightsPage::Draw(float scale, const lwf::ui::Palette& palette) {
    SyncIfStale();
    DrawHeader(scale, palette);
    DrawRow1(scale, palette);
    DrawRow2(scale, palette);
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

void InsightsPage::DrawRow1(float scale, const lwf::ui::Palette& palette) {
    const float avail = ImGui::GetContentRegionAvail().x;
    const float gap = 16.0f * scale;
    const float card_w = (avail - 2.0f * gap) / 3.0f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    float bottom = origin.y;
    ImGui::SetCursorScreenPos(origin);
    DrawCardA(card_w, scale, palette);
    bottom = std::max(bottom, ImGui::GetCursorScreenPos().y);
    ImGui::SetCursorScreenPos(ImVec2(origin.x + card_w + gap, origin.y));
    DrawCardB(card_w, scale, palette);
    bottom = std::max(bottom, ImGui::GetCursorScreenPos().y);
    ImGui::SetCursorScreenPos(ImVec2(origin.x + 2.0f * (card_w + gap), origin.y));
    DrawCardC(card_w, scale, palette);
    bottom = std::max(bottom, ImGui::GetCursorScreenPos().y);
    ImGui::SetCursorScreenPos(ImVec2(origin.x, bottom + 16.0f * scale));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

void InsightsPage::DrawCardA(float width, float scale, const lwf::ui::Palette& palette) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(palette.bg));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, studio_metrics::radius_card * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * scale, 16.0f * scale));
    ImGui::BeginChild("##insights_card_a", ImVec2(width, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    const std::string wpm = WpmLabel(summary_.wpm);
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted(wpm.c_str());
    ImGui::PopFont();
    ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
    widgets::IconButton("##insights_wpm_info", "i", scale, palette);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", kWpmTooltip);
    }
    ImGui::PushFont(nullptr, studio_metrics::font_caps_px);
    ImGui::TextColored(Color(palette.muted), "%s", kWpmCaption);
    ImGui::PopFont();
    widgets::RadialGauge("##insights_gauge", GaugeFraction(summary_.wpm), kGaugeCenter, wpm.c_str(),
                         scale, palette);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 16.0f * scale);
    ImGui::TextColored(Color(palette.muted), "%s", kGaugeFootnote);
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::EndChild();
}

void InsightsPage::DrawCardB(float width, float scale, const lwf::ui::Palette& palette) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(palette.bg));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, studio_metrics::radius_card * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * scale, 16.0f * scale));
    ImGui::BeginChild("##insights_card_b", ImVec2(width, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted(std::to_string(summary_.fixes_total).c_str());
    ImGui::PopFont();
    ImGui::PushFont(nullptr, studio_metrics::font_caps_px);
    ImGui::TextColored(Color(palette.muted), "%s", kFixesCaption);
    ImGui::PopFont();
    DrawDivider(scale, palette);
    const std::string corrected =
        std::to_string(summary_.words_corrected) + " " + kWordsCorrectedSuffix;
    DrawInfoRow("##insights_corrected_info", corrected, kWordsCorrectedTooltip, scale, palette);
    const std::string dictionary =
        std::to_string(summary_.dictionary_fixes) + " " + kDictionaryFixesSuffix;
    DrawInfoRow("##insights_dictionary_info", dictionary, kDictionaryFixesTooltip, scale, palette);
    ImGui::EndChild();
}

void InsightsPage::DrawCardC(float width, float scale, const lwf::ui::Palette& palette) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(palette.bg));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, studio_metrics::radius_card * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * scale, 16.0f * scale));
    ImGui::BeginChild("##insights_card_c", ImVec2(width, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted(std::to_string(summary_.total_words).c_str());
    ImGui::PopFont();
    ImGui::PushFont(nullptr, studio_metrics::font_caps_px);
    ImGui::TextColored(Color(palette.muted), "%s", kTotalWordsCaption);
    ImGui::PopFont();
    DrawDivider(scale, palette);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::TextUnformatted(DesktopWordsLine(summary_.total_words).c_str());
    ImGui::PopFont();
    widgets::OutlinePill("##insights_mobile", kMobileLabel, false, scale, palette);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("%s", kMobileTooltip);
    }
    ImGui::EndChild();
}

void InsightsPage::DrawRow2(float scale, const lwf::ui::Palette& palette) {
    const float avail = ImGui::GetContentRegionAvail().x;
    const float gap = 16.0f * scale;
    const float left_w = (avail - gap) * 0.55f;
    const float right_w = avail - gap - left_w;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    float bottom = origin.y;
    ImGui::SetCursorScreenPos(origin);
    DrawCardD(left_w, scale, palette);
    bottom = std::max(bottom, ImGui::GetCursorScreenPos().y);
    ImGui::SetCursorScreenPos(ImVec2(origin.x + left_w + gap, origin.y));
    DrawCardE(right_w, scale, palette);
    bottom = std::max(bottom, ImGui::GetCursorScreenPos().y);
    ImGui::SetCursorScreenPos(ImVec2(origin.x, bottom + 16.0f * scale));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

void InsightsPage::DrawCardD(float width, float scale, const lwf::ui::Palette& palette) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(palette.bg));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, studio_metrics::radius_card * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * scale, 16.0f * scale));
    ImGui::BeginChild("##insights_card_d", ImVec2(width, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::PushFont(nullptr, studio_metrics::font_heading_px);
    ImGui::TextUnformatted(kUsageTitle);
    ImGui::PopFont();
    ImGui::PushFont(nullptr, studio_metrics::font_caps_px);
    ImGui::TextColored(Color(palette.muted), "%s", TotalAppsCaption(summary_.total_apps).c_str());
    ImGui::PopFont();
    if (!summary_.has_data) {
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 16.0f * scale);
        ImGui::TextColored(Color(palette.muted), "%s", kDesktopEmpty);
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
        ImGui::EndChild();
        return;
    }
    const std::vector<CategoryRow> rows = CategoryRowsFor(snapshot_);
    for (const CategoryRow& row : rows) {
        const ImVec2 row_pos = ImGui::GetCursorScreenPos();
        const float row_h = studio_metrics::progress_height * scale;
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        const ImVec2 icon_center(row_pos.x + 14.0f * scale, row_pos.y + row_h * 0.5f);
        draw.AddCircle(icon_center, 12.0f * scale, Pack(palette.border), 24, 1.0f * scale);
        ImGui::PushFont(nullptr, studio_metrics::font_glyph_px);
        const ImVec2 icon_size = ImGui::CalcTextSize(row.icon.c_str());
        draw.AddText(ImVec2(icon_center.x - icon_size.x * 0.5f, icon_center.y - icon_size.y * 0.5f),
                     Pack(palette.accent), row.icon.c_str());
        ImGui::PopFont();
        ImGui::SetCursorScreenPos(ImVec2(row_pos.x + 32.0f * scale, row_pos.y));
        const std::string pill_id = std::string("##insights_pill_") + row.key;
        widgets::ProgressPill(pill_id.c_str(), row.fraction,
                              PillLabel(row.words, row.display_label).c_str(), scale);
        ImGui::SetCursorScreenPos(ImVec2(row_pos.x, row_pos.y + row_h + 8.0f * scale - ImGui::GetStyle().ItemSpacing.y));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }
    ImGui::EndChild();
}

void InsightsPage::DrawCardE(float width, float scale, const lwf::ui::Palette& palette) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(palette.bg));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, studio_metrics::radius_card * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * scale, 16.0f * scale));
    ImGui::BeginChild("##insights_card_e", ImVec2(width, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::PushFont(nullptr, studio_metrics::font_heading_px);
    ImGui::TextUnformatted(StreakTitle(summary_.current_streak).c_str());
    ImGui::PopFont();
    ImGui::PushFont(nullptr, studio_metrics::font_caps_px);
    ImGui::TextColored(Color(palette.muted), "%s", LongestCaption(summary_.longest_streak).c_str());
    ImGui::PopFont();
    const std::string month_label = MonthLabelFor(ViewMonth());
    if (!CanPrevMonth()) {
        ImGui::BeginDisabled();
    }
    const bool prev_pressed = widgets::IconButton("##insights_month_prev", "<", scale, palette);
    if (!CanPrevMonth()) {
        ImGui::EndDisabled();
    }
    if (prev_pressed) {
        GoPrevMonth();
    }
    ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::TextUnformatted(month_label.c_str());
    ImGui::PopFont();
    ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
    if (!CanNextMonth()) {
        ImGui::BeginDisabled();
    }
    const bool next_pressed = widgets::IconButton("##insights_month_next", ">", scale, palette);
    if (!CanNextMonth()) {
        ImGui::EndDisabled();
    }
    if (next_pressed) {
        GoNextMonth();
    }
    const MonthValues grid = MonthValuesFor(snapshot_, view_year_, view_month_);
    widgets::ActivityGrid("##insights_grid", grid.values, grid.month_max, scale, palette);
    DrawLegend(scale, palette);
    ImGui::EndChild();
}

}  // namespace lwf::ui::insights
