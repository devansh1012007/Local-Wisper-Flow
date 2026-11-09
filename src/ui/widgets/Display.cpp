#include "ui/widgets/Widgets.h"

#include "ui/theme/ThemePalette.h"
#include "ui/theme/ThemeStyle.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace lwf::ui::widgets {

namespace {

using lwf::ui::HeatColor;
using lwf::ui::Pack;

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
    // Hairline track (1 px outline, transparent) + accent fill bar. The bar
    // fill is the progress data itself, like the ActivityGrid heat cells.
    draw.AddRect(pos, ImVec2(pos.x + w, pos.y + h), Pack(palette.border),
                 studio_metrics::radius_pill * scale, 0, 1.0f * scale);
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
        // Narrow-case badge: hairline outline + text, no surface fill.
        draw.AddRect(badge_pos, ImVec2(badge_pos.x + badge_w, badge_pos.y + badge_h),
                     Pack(palette.border), studio_metrics::radius_pill * scale, 0,
                     1.0f * scale);
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
    // Hairline-top structure (DESIGN §6): 1 px accent hairline with a 1 px
    // hairline below it, no fill body (replaces the surface gradient).
    draw.AddLine(pos, ImVec2(pos.x + w, pos.y), Pack(palette.accent), 1.0f * scale);
    const float hairline_y = pos.y + metrics::space_xs * scale;
    draw.AddLine(ImVec2(pos.x, hairline_y), ImVec2(pos.x + w, hairline_y), Pack(palette.border),
                 1.0f * scale);
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

void StatusSquare(const char* id, StatusKind state, const char* word, float scale,
                  const lwf::ui::Palette& palette) {
    (void)id;
    const char* safe_word = word == nullptr ? "" : word;
    std::uint32_t color = palette.muted;
    switch (state) {
        case StatusKind::Ok:
            color = palette.ok;
            break;
        case StatusKind::Warn:
            color = palette.warn;
            break;
        case StatusKind::Err:
            color = palette.err;
            break;
        case StatusKind::Accent:
            color = palette.accent;
            break;
        case StatusKind::Muted:
            color = palette.muted;
            break;
    }
    // The square IS the color carrier (DESIGN §6); the word is always
    // present beside it, so no state is carried by color alone.
    const float sq = metrics::status_square * scale;
    ImGui::PushFont(nullptr, metrics::font_body_px);
    const ImVec2 word_size = ImGui::CalcTextSize(safe_word);
    ImGui::PopFont();
    const float gap = studio_metrics::gap_inline * scale;
    const float h = std::max(sq, word_size.y);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(sq + gap + word_size.x, h));
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    draw.AddRectFilled(ImVec2(pos.x, pos.y + (h - sq) * 0.5f),
                       ImVec2(pos.x + sq, pos.y + (h - sq) * 0.5f + sq), Pack(color));
    ImGui::PushFont(nullptr, metrics::font_body_px);
    draw.AddText(ImVec2(pos.x + sq + gap, pos.y + (h - word_size.y) * 0.5f), Pack(palette.text),
                 safe_word);
    ImGui::PopFont();
}

void Row(const char* id, const char* label, const char* helper, float control_w, float scale,
         const lwf::ui::Palette& palette) {
    (void)id;
    const char* safe_label = label == nullptr ? "" : label;
    const char* safe_helper = helper == nullptr ? "" : helper;
    const float w = ImGui::GetContentRegionAvail().x;
    const float row_h = studio_metrics::row_height * scale;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    // Ruled ledger row (DESIGN §6): 1 px hairlines top/bottom, label left,
    // helper below in muted, no child, no fill. The caller draws exactly
    // one control at the right slot (control_w wide, vertically centered);
    // RowEnd restores the cursor below the row.
    draw.AddLine(pos, ImVec2(pos.x + w, pos.y), Pack(palette.border), 1.0f * scale);
    draw.AddLine(ImVec2(pos.x, pos.y + row_h), ImVec2(pos.x + w, pos.y + row_h),
                 Pack(palette.border), 1.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_body_px);
    draw.AddText(ImVec2(pos.x + 16.0f * scale, pos.y + 8.0f * scale), Pack(palette.text),
                 safe_label);
    ImGui::PopFont();
    if (safe_helper[0] != '\0') {
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        draw.AddText(ImVec2(pos.x + 16.0f * scale, pos.y + 30.0f * scale), Pack(palette.muted),
                     safe_helper);
        ImGui::PopFont();
    }
    ImGui::Dummy(ImVec2(w, row_h));
    const float slot_w = std::min(control_w, w);
    const float slot_y = pos.y + (row_h - studio_metrics::hit_size * scale) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(pos.x + w - slot_w, slot_y));
}

void RowEnd(float scale) {
    const float row_h = studio_metrics::row_height * scale;
    const ImVec2 item_min = ImGui::GetItemRectMin();
    const ImVec2 item_max = ImGui::GetItemRectMax();
    const float control_h = item_max.y - item_min.y;
    // The slot is vertically centered in the row, so the row bottom is
    // half a control-height above the slot top plus half the row height.
    // With no control drawn, GetItemRect is Row's Dummy (full row height)
    // and the same formula lands on the row bottom.
    const float row_bottom = item_min.y + (row_h + control_h) * 0.5f;
    const float row_left = ImGui::GetCursorScreenPos().x;
    ImGui::SetCursorScreenPos(
        ImVec2(row_left, row_bottom - ImGui::GetStyle().ItemSpacing.y));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

}
