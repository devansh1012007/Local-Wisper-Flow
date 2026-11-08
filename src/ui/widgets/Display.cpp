#include "ui/widgets/Widgets.h"

#include "ui/theme/StudioPalette.h"
#include "ui/theme/ThemePalette.h"
#include "ui/theme/ThemeStyle.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace lwf::ui::widgets {

namespace {

using lwf::ui::Color;
using lwf::ui::HeatColor;
using lwf::ui::Pack;

}

void RowList(const char* id, const std::vector<RowListItem>& rows, float scale, const lwf::ui::Palette& palette) {
    const char* safe_id = id == nullptr ? "" : id;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(palette.surface));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, studio_metrics::radius_row * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild(safe_id, ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    if (rows.empty()) {
        ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        const ImVec2 text_size = ImGui::CalcTextSize("Not available yet.");
        draw.AddText(ImVec2(pos.x + 16.0f * scale, pos.y + (studio_metrics::row_height * scale -
                                                             text_size.y) * 0.5f),
                     Pack(palette.muted), "Not available yet.");
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::row_height * scale));
        ImGui::PopFont();
    } else {
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        const float width = ImGui::GetContentRegionAvail().x;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (i > 0) {
                const ImVec2 sep = ImGui::GetCursorScreenPos();
                draw.AddLine(ImVec2(sep.x, sep.y), ImVec2(sep.x + width, sep.y),
                             Pack(palette.border), 1.0f * scale);
            }
            const float row_h = studio_metrics::row_height * scale;
            const ImVec2 row_pos = ImGui::GetCursorScreenPos();
            ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
            ImDrawList& inner = *ImGui::GetWindowDrawList();
            inner.AddText(ImVec2(row_pos.x + 16.0f * scale, row_pos.y + 8.0f * scale),
                          Pack(palette.text), rows[i].title.c_str());
            if (!rows[i].detail.empty()) {
                inner.AddText(ImVec2(row_pos.x + 16.0f * scale, row_pos.y + 30.0f * scale),
                              Pack(palette.muted), rows[i].detail.c_str());
            }
            ImGui::PopFont();
            if (!rows[i].trailing.empty()) {
                ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
                const ImVec2 trail_size = ImGui::CalcTextSize(rows[i].trailing.c_str());
                ImGui::PopFont();
                const ImVec2 right = ImVec2(row_pos.x + width - 16.0f * scale - trail_size.x,
                                            row_pos.y + (row_h - trail_size.y) * 0.5f);
                inner.AddText(right, Pack(palette.muted), rows[i].trailing.c_str());
            }
            ImGui::Dummy(ImVec2(0.0f, row_h));
        }
    }
    ImGui::EndChild();
}

void StatCard(const char* id, const char* title, const char* value, const char* subtitle,
              float width, float scale, const lwf::ui::Palette& palette) {
    const char* safe_id = id == nullptr ? "" : id;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(palette.surface));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, studio_metrics::radius_card * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(16.0f * scale, 16.0f * scale));
    ImGui::BeginChild(safe_id, ImVec2(width * scale, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    const std::string caps = UpperSpaced(title == nullptr ? "" : title);
    ImGui::PushFont(nullptr, studio_metrics::font_caps_px);
    ImGui::TextColored(Color(palette.muted), "%s", caps.c_str());
    ImGui::PopFont();
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted(value == nullptr ? "" : value);
    ImGui::PopFont();
    if (subtitle != nullptr && subtitle[0] != '\0') {
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::TextColored(Color(palette.muted), "%s", subtitle);
        ImGui::PopFont();
    }
    ImGui::EndChild();
}

void RadialGauge(const char* id, float value, const char* center_label, const char* sub_label,
                 float scale, const lwf::ui::Palette& palette) {
    const float clamped = Clamp01(value);
    const float w = 220.0f * scale;
    const float h = 150.0f * scale;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(w, h));
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    const ImVec2 center(pos.x + w * 0.5f, pos.y + h - 24.0f * scale);
    const float radius = 84.0f * scale;
    const float stroke = studio_metrics::gauge_stroke * scale;
    const float a0 = 3.14159265358979323846f;
    draw.PathArcTo(center, radius, a0, a0 + 3.14159265358979323846f, 48);
    draw.PathStroke(Pack(palette.border), 0, stroke);
    const float a1 = a0 + clamped * 3.14159265358979323846f;
    draw.PathArcTo(center, radius, a0, a1, 48);
    draw.PathStroke(Pack(palette.accent), 0, stroke);
    const ImVec2 start(center.x - radius, center.y);
    const ImVec2 end(center.x + radius * static_cast<float>(std::cos(a1)),
                     center.y + radius * static_cast<float>(std::sin(a1)));
    draw.AddCircleFilled(start, stroke * 0.5f, Pack(palette.border));
    draw.AddCircleFilled(end, stroke * 0.5f, Pack(palette.accent));
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    const ImVec2 center_size = ImGui::CalcTextSize(center_label == nullptr ? "" : center_label);
    draw.AddText(ImVec2(center.x - center_size.x * 0.5f, center.y - 42.0f * scale),
                 Pack(palette.text), center_label == nullptr ? "" : center_label);
    ImGui::PopFont();
    if (sub_label != nullptr && sub_label[0] != '\0') {
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        const ImVec2 sub_size = ImGui::CalcTextSize(sub_label);
        draw.AddText(ImVec2(center.x - sub_size.x * 0.5f, center.y - 14.0f * scale),
                     Pack(palette.muted), sub_label);
        ImGui::PopFont();
    }
    (void)id;
}

void ProgressPill(const char* id, float value, const char* label, float scale, const lwf::ui::Palette& palette) {
    const float clamped = Clamp01(value);
    const float h = studio_metrics::progress_height * scale;
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(w, h));
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    draw.AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), Pack(palette.border),
                       studio_metrics::radius_pill * scale);
    const float fill_w = w * clamped;
    draw.AddRectFilled(pos, ImVec2(pos.x + fill_w, pos.y + h), Pack(palette.accent),
                       studio_metrics::radius_pill * scale);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    const ImVec2 text_size = ImGui::CalcTextSize(label == nullptr ? "" : label);
    ImGui::PopFont();
    const int percent = static_cast<int>(std::lround(clamped * 100.0f));
    char percent_buf[16];
    std::snprintf(percent_buf, sizeof(percent_buf), "%d%%", percent);
    if (fill_w >= text_size.x + 24.0f * scale) {
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        draw.AddText(ImVec2(pos.x + fill_w * 0.5f - text_size.x * 0.5f,
                            pos.y + (h - text_size.y) * 0.5f),
                     Pack(palette.on_accent), label == nullptr ? "" : label);
        ImGui::PopFont();
    } else {
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        const ImVec2 badge_size = ImGui::CalcTextSize(percent_buf);
        const float badge_w = badge_size.x + 20.0f * scale;
        const float badge_h = 20.0f * scale;
        const ImVec2 badge_pos(pos.x + w - badge_w - 4.0f * scale,
                               pos.y + (h - badge_h) * 0.5f);
        draw.AddRectFilled(badge_pos, ImVec2(badge_pos.x + badge_w, badge_pos.y + badge_h),
                           Pack(palette.surface),
                           studio_metrics::radius_pill * scale);
        draw.AddText(ImVec2(badge_pos.x + 10.0f * scale,
                            badge_pos.y + (badge_h - badge_size.y) * 0.5f),
                     Pack(palette.accent), percent_buf);
        const float label_max_x = badge_pos.x - 4.0f * scale;
        draw.PushClipRect(ImVec2(pos.x + 12.0f * scale, pos.y), ImVec2(label_max_x, pos.y + h),
                         true);
        draw.AddText(ImVec2(pos.x + 12.0f * scale, pos.y + (h - text_size.y) * 0.5f),
                     Pack(palette.accent), label == nullptr ? "" : label);
        draw.PopClipRect();
        ImGui::PopFont();
    }
    (void)id;
}

void ActivityGrid(const char* id, const std::vector<int>& values, int month_max, float scale, const lwf::ui::Palette& palette) {
    static const char* const kDays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const float cell = studio_metrics::activity_cell * scale;
    const float gap = studio_metrics::activity_gap * scale;
    const float corner = studio_metrics::activity_corner * scale;
    const std::size_t weeks = (values.size() + 6) / 7;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    ImGui::PushFont(nullptr, studio_metrics::font_caps_px);
    for (std::size_t row = 0; row < 7; ++row) {
        const ImVec2 label_size = ImGui::CalcTextSize(kDays[row]);
        draw.AddText(ImVec2(pos.x, pos.y + row * (cell + gap) + (cell - label_size.y) * 0.5f),
                     Pack(palette.muted), kDays[row]);
    }
    ImGui::PopFont();
    const float labels_w = 34.0f * scale;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const std::size_t row = i % 7;
        const std::size_t col = i / 7;
        const ImVec2 cell_pos(pos.x + labels_w + col * (cell + gap),
                              pos.y + row * (cell + gap));
        draw.AddRectFilled(cell_pos, ImVec2(cell_pos.x + cell, cell_pos.y + cell),
                           Pack(HeatColor(HeatLevel(values[i], month_max))), corner);
    }
    const float total_w = weeks > 0 ? labels_w + weeks * (cell + gap) - gap : labels_w;
    const float total_h = 7.0f * (cell + gap) - gap;
    ImGui::Dummy(ImVec2(total_w, total_h));
    (void)id;
}

void Banner(const char* id, const char* headline, const char* body, bool* open, float scale, const lwf::ui::Palette& palette) {
    if (open != nullptr && !*open) {
        return;
    }
    const char* safe_id = id == nullptr ? "" : id;
    const float w = ImGui::GetContentRegionAvail().x;
    const float pad = 16.0f * scale;
    ImGui::PushFont(nullptr, studio_metrics::font_banner_px);
    const ImVec2 headline_size =
        ImGui::CalcTextSize(headline == nullptr ? "" : headline);
    ImGui::PopFont();
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    const ImVec2 body_size = ImGui::CalcTextSize(body == nullptr ? "" : body);
    ImGui::PopFont();
    const float h = pad + headline_size.y + 6.0f * scale + body_size.y + pad;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(w, h));
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    const ImVec2 bottom(pos.x + w, pos.y + h);
    draw.AddRectFilledMultiColor(pos, bottom, Pack(palette.surface),
                                 Pack(palette.border), Pack(palette.border),
                                 Pack(palette.surface));
    draw.AddRect(pos, bottom, Pack(palette.border), studio_metrics::radius_banner * scale,
                 0, 1.0f * scale);
    ImGui::PushFont(nullptr, studio_metrics::font_banner_px);
    draw.AddText(ImVec2(pos.x + pad, pos.y + pad), Pack(palette.text),
                 headline == nullptr ? "" : headline);
    ImGui::PopFont();
    if (body != nullptr && body[0] != '\0') {
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        draw.AddText(ImVec2(pos.x + pad, pos.y + pad + headline_size.y + 6.0f * scale),
                     Pack(palette.text), body);
        ImGui::PopFont();
    }
    if (open != nullptr) {
        const float hit = studio_metrics::hit_size * scale;
        ImGui::SetCursorScreenPos(ImVec2(pos.x + w - hit - 8.0f * scale, pos.y + 8.0f * scale));
        if (IconButton((std::string(safe_id) + "_close").c_str(), "x", scale, palette)) {
            *open = false;
        }
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + h - ImGui::GetStyle().ItemSpacing.y));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }
}

}
