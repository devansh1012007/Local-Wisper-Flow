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

// DECISION: single Ivory palette for this page (Phase 5). The share-badge fill
// uses HeatColor(3) so the brass ramp L3 value survives without any hex
// literal in the page; HeatColor owns the ramp stops.
using lwf::ui::Color;
using lwf::ui::HeatColor;
using lwf::ui::Pack;
using lwf::ui::kLightPalette;
namespace studio_metrics = lwf::ui::studio_metrics;

void BeginCard(const char* id, float width, float scale) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(kLightPalette.surface));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, studio_metrics::radius_card * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * scale, 16.0f * scale));
    ImGui::BeginChild(id, ImVec2(width, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void EndCard() {
    ImGui::EndChild();
}

void DrawDivider(float scale) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    draw.AddLine(pos, ImVec2(pos.x + width, pos.y), Pack(kLightPalette.border), 1.0f * scale);
    ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_section * scale));
}

void DrawLegend(float scale) {
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
                       Pack(kLightPalette.border), studio_metrics::activity_corner * scale);
}

void DrawInfoRow(const char* id, const std::string& text, const char* tooltip, float scale) {
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopFont();
    ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
    widgets::IconButton(id, "i", scale);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
}

}

void InsightsPage::Draw(float scale) {
    SyncIfStale();
    DrawHeader(scale);
    DrawTabs(scale);
    if (IsVoice()) {
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::TextDisabled("%s", kVoiceEmpty);
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_block * scale));
        DrawShareDialog(scale);
        return;
    }
    DrawRow1(scale);
    DrawRow2(scale);
    DrawShareDialog(scale);
}

void InsightsPage::DrawHeader(float scale) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float avail = ImGui::GetContentRegionAvail().x;
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    draw.AddText(origin, Pack(kLightPalette.text), kPageTitle);
    ImGui::PopFont();
    const float badge = 56.0f * scale;
    const ImVec2 badge_pos(origin.x + avail - badge, origin.y);
    ImGui::SetCursorScreenPos(badge_pos);
    const bool pressed = ImGui::InvisibleButton("##insights_share", ImVec2(badge, badge),
                                                ImGuiButtonFlags_EnableNav);
    const ImVec2 center(badge_pos.x + badge * 0.5f, badge_pos.y + badge * 0.5f);
    const float radius = badge * 0.5f;
    draw.AddCircleFilled(center, radius, Pack(HeatColor(3)));
    std::vector<std::string> glyphs;
    for (char step : std::string(kShareBadgeLabel)) {
        glyphs.push_back(std::string(1, step));
    }
    glyphs.push_back(" ");
    glyphs.push_back("\xe2\x80\xa2");
    glyphs.push_back(" ");
    const int repeats = 4;
    const std::size_t total = glyphs.size() * static_cast<std::size_t>(repeats);
    const float ring_radius = radius - 3.0f * scale;
    ImGui::PushFont(nullptr, studio_metrics::font_glyph_px);
    for (std::size_t i = 0; i < total; ++i) {
        const std::string& glyph = glyphs[i % glyphs.size()];
        const float angle = -1.5707963267948966f + static_cast<float>(i) * 6.283185307179586f /
                                                         static_cast<float>(total);
        const ImVec2 glyph_pos(center.x + std::cos(angle) * ring_radius - 3.0f * scale,
                               center.y + std::sin(angle) * ring_radius - 5.0f * scale);
        draw.AddText(glyph_pos, Pack(kLightPalette.text), glyph.c_str());
    }
    ImGui::PopFont();
    const float shaft_top = center.y - 7.0f * scale;
    const float shaft_bottom = center.y + 9.0f * scale;
    draw.AddLine(ImVec2(center.x, shaft_bottom), ImVec2(center.x, shaft_top),
                 Pack(kLightPalette.text), 2.0f * scale);
    draw.AddLine(ImVec2(center.x - 5.0f * scale, center.y - 2.0f * scale), ImVec2(center.x, shaft_top),
                 Pack(kLightPalette.text), 2.0f * scale);
    draw.AddLine(ImVec2(center.x, shaft_top),
                 ImVec2(center.x + 5.0f * scale, center.y - 2.0f * scale), Pack(kLightPalette.text),
                 2.0f * scale);
    draw.AddLine(ImVec2(center.x - 7.0f * scale, center.y + 12.0f * scale),
                 ImVec2(center.x + 7.0f * scale, center.y + 12.0f * scale), Pack(kLightPalette.text),
                 2.0f * scale);
    if (ImGui::IsItemFocused()) {
        draw.AddCircle(center, radius + 2.0f * scale, Pack(kLightPalette.accent), 48, 2.0f * scale);
    }
    if (pressed) {
        OpenShare();
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + badge + 8.0f * scale));
}

void InsightsPage::DrawTabs(float scale) {
    static const char* const kTabs[] = {kTabUsage, kTabVoice};
    int selected = tab_;
    widgets::TabRow("##insights_tabs", kTabs, 2, &selected, scale);
    tab_ = selected;
    ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_block * scale));
}

void InsightsPage::DrawRow1(float scale) {
    const float avail = ImGui::GetContentRegionAvail().x;
    const float gap = 16.0f * scale;
    const float card_w = (avail - 2.0f * gap) / 3.0f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    float bottom = origin.y;
    ImGui::SetCursorScreenPos(origin);
    DrawCardA(card_w, scale);
    bottom = std::max(bottom, ImGui::GetCursorScreenPos().y);
    ImGui::SetCursorScreenPos(ImVec2(origin.x + card_w + gap, origin.y));
    DrawCardB(card_w, scale);
    bottom = std::max(bottom, ImGui::GetCursorScreenPos().y);
    ImGui::SetCursorScreenPos(ImVec2(origin.x + 2.0f * (card_w + gap), origin.y));
    DrawCardC(card_w, scale);
    bottom = std::max(bottom, ImGui::GetCursorScreenPos().y);
    ImGui::SetCursorScreenPos(ImVec2(origin.x, bottom + 16.0f * scale));
}

void InsightsPage::DrawCardA(float width, float scale) {
    BeginCard("##insights_card_a", width, scale);
    const std::string wpm = WpmLabel(summary_.wpm);
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted(wpm.c_str());
    ImGui::PopFont();
    ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
    widgets::IconButton("##insights_wpm_info", "i", scale);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", kWpmTooltip);
    }
    ImGui::PushFont(nullptr, studio_metrics::font_caps_px);
    ImGui::TextColored(Color(kLightPalette.muted), "%s", kWpmCaption);
    ImGui::PopFont();
    widgets::RadialGauge("##insights_gauge", GaugeFraction(summary_.wpm), kGaugeCenter, wpm.c_str(),
                         scale);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 16.0f * scale);
    ImGui::TextColored(Color(kLightPalette.muted), "%s", kGaugeFootnote);
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    EndCard();
}

void InsightsPage::DrawCardB(float width, float scale) {
    BeginCard("##insights_card_b", width, scale);
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted(std::to_string(summary_.fixes_total).c_str());
    ImGui::PopFont();
    ImGui::PushFont(nullptr, studio_metrics::font_caps_px);
    ImGui::TextColored(Color(kLightPalette.muted), "%s", kFixesCaption);
    ImGui::PopFont();
    DrawDivider(scale);
    const std::string corrected =
        std::to_string(summary_.words_corrected) + " " + kWordsCorrectedSuffix;
    DrawInfoRow("##insights_corrected_info", corrected, kWordsCorrectedTooltip, scale);
    const std::string dictionary =
        std::to_string(summary_.dictionary_fixes) + " " + kDictionaryFixesSuffix;
    DrawInfoRow("##insights_dictionary_info", dictionary, kDictionaryFixesTooltip, scale);
    EndCard();
}

void InsightsPage::DrawCardC(float width, float scale) {
    BeginCard("##insights_card_c", width, scale);
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted(std::to_string(summary_.total_words).c_str());
    ImGui::PopFont();
    ImGui::PushFont(nullptr, studio_metrics::font_caps_px);
    ImGui::TextColored(Color(kLightPalette.muted), "%s", kTotalWordsCaption);
    ImGui::PopFont();
    DrawDivider(scale);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::TextUnformatted(DesktopWordsLine(summary_.total_words).c_str());
    ImGui::PopFont();
    widgets::OutlinePill("##insights_mobile", kMobileLabel, false, scale);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("%s", kMobileTooltip);
    }
    EndCard();
}

void InsightsPage::DrawRow2(float scale) {
    const float avail = ImGui::GetContentRegionAvail().x;
    const float gap = 16.0f * scale;
    const float left_w = (avail - gap) * 0.55f;
    const float right_w = avail - gap - left_w;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    float bottom = origin.y;
    ImGui::SetCursorScreenPos(origin);
    DrawCardD(left_w, scale);
    bottom = std::max(bottom, ImGui::GetCursorScreenPos().y);
    ImGui::SetCursorScreenPos(ImVec2(origin.x + left_w + gap, origin.y));
    DrawCardE(right_w, scale);
    bottom = std::max(bottom, ImGui::GetCursorScreenPos().y);
    ImGui::SetCursorScreenPos(ImVec2(origin.x, bottom + 16.0f * scale));
}

void InsightsPage::DrawCardD(float width, float scale) {
    BeginCard("##insights_card_d", width, scale);
    ImGui::PushFont(nullptr, studio_metrics::font_heading_px);
    ImGui::TextUnformatted(kUsageTitle);
    ImGui::PopFont();
    ImGui::PushFont(nullptr, studio_metrics::font_caps_px);
    ImGui::TextColored(Color(kLightPalette.muted), "%s", TotalAppsCaption(summary_.total_apps).c_str());
    ImGui::PopFont();
    if (!summary_.has_data) {
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 16.0f * scale);
        ImGui::TextColored(Color(kLightPalette.muted), "%s", kDesktopEmpty);
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
        EndCard();
        return;
    }
    const std::vector<CategoryRow> rows = CategoryRowsFor(snapshot_);
    for (const CategoryRow& row : rows) {
        const ImVec2 row_pos = ImGui::GetCursorScreenPos();
        const float row_h = studio_metrics::progress_height * scale;
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        const ImVec2 icon_center(row_pos.x + 14.0f * scale, row_pos.y + row_h * 0.5f);
        draw.AddCircle(icon_center, 12.0f * scale, Pack(kLightPalette.border), 24, 1.0f * scale);
        ImGui::PushFont(nullptr, studio_metrics::font_glyph_px);
        const ImVec2 icon_size = ImGui::CalcTextSize(row.icon.c_str());
        draw.AddText(ImVec2(icon_center.x - icon_size.x * 0.5f, icon_center.y - icon_size.y * 0.5f),
                     Pack(kLightPalette.accent), row.icon.c_str());
        ImGui::PopFont();
        ImGui::SetCursorScreenPos(ImVec2(row_pos.x + 32.0f * scale, row_pos.y));
        const std::string pill_id = std::string("##insights_pill_") + row.key;
        widgets::ProgressPill(pill_id.c_str(), row.fraction,
                              PillLabel(row.words, row.display_label).c_str(), scale);
        ImGui::SetCursorScreenPos(ImVec2(row_pos.x, row_pos.y + row_h + 8.0f * scale - ImGui::GetStyle().ItemSpacing.y));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }
    EndCard();
}

void InsightsPage::DrawCardE(float width, float scale) {
    BeginCard("##insights_card_e", width, scale);
    ImGui::PushFont(nullptr, studio_metrics::font_heading_px);
    ImGui::TextUnformatted(StreakTitle(summary_.current_streak).c_str());
    ImGui::PopFont();
    ImGui::PushFont(nullptr, studio_metrics::font_caps_px);
    ImGui::TextColored(Color(kLightPalette.muted), "%s", LongestCaption(summary_.longest_streak).c_str());
    ImGui::PopFont();
    const std::string month_label = MonthLabelFor(ViewMonth());
    if (!CanPrevMonth()) {
        ImGui::BeginDisabled();
    }
    const bool prev_pressed = widgets::IconButton("##insights_month_prev", "<", scale);
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
    const bool next_pressed = widgets::IconButton("##insights_month_next", ">", scale);
    if (!CanNextMonth()) {
        ImGui::EndDisabled();
    }
    if (next_pressed) {
        GoNextMonth();
    }
    const MonthValues grid = MonthValuesFor(snapshot_, view_year_, view_month_);
    widgets::ActivityGrid("##insights_grid", grid.values, grid.month_max, scale);
    DrawLegend(scale);
    EndCard();
}

void InsightsPage::DrawShareDialog(float scale) {
    if (share_open_) {
        if (!share_modal_.IsOpen()) {
            share_modal_.Open();
        }
    } else if (share_modal_.IsOpen()) {
        share_modal_.Close();
    }
    bool close_requested = false;
    if (share_modal_.Begin(kShareDialogTitle, scale)) {
        BeginCard("##insights_share_highlights", 220.0f * scale, scale);
        ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
        ImGui::TextUnformatted(kShareHighlightsTitle);
        ImGui::PopFont();
        const std::vector<std::string> lines = ShareHighlightLines(summary_);
        for (const std::string& line : lines) {
            ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopFont();
        }
        EndCard();
        ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
        BeginCard("##insights_share_streak", 220.0f * scale, scale);
        ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
        ImGui::TextUnformatted(kShareStreakTitle);
        ImGui::PopFont();
        ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
        ImGui::TextUnformatted(StreakTitle(summary_.current_streak).c_str());
        ImGui::PopFont();
        EndCard();
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_inline * scale));
        ImGui::BeginDisabled();
        (void)widgets::PrimaryButton("insights_copy", kShareCopy, scale);
        ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
        (void)widgets::PrimaryButton("insights_download", kShareDownload, scale);
        ImGui::EndDisabled();
        ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::TextColored(Color(kLightPalette.muted), "%s", kComingSoon);
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_inline * scale));
        if (widgets::LightButton("insights_share_close", "Close", scale)) {
            close_requested = true;
        }
        if (close_requested) {
            ImGui::CloseCurrentPopup();
        }
    }
    share_modal_.End();
    if (close_requested) {
        share_modal_.Close();
        share_open_ = false;
    } else if (!share_modal_.IsOpen()) {
        share_open_ = false;
    }
}

}
