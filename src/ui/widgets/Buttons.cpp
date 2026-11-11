#include "ui/widgets/Widgets.h"

#include "ui/theme/ThemePalette.h"
#include "ui/theme/ThemeStyle.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace lwf::ui::widgets {

namespace {

// DECISION: palette defaults to kLightPalette (Ivory primary) via Widgets.h;
// every draw below reads its colors from that Palette, never from a literal
// or from the deprecated shim. Focus ring stays IsItemFocused + 2px.

using lwf::ui::Color;
using lwf::ui::Pack;

void DrawFocusRing(const ImVec2& min, const ImVec2& max, float scale, const lwf::ui::Palette& palette) {
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    draw.AddRect(ImVec2(min.x - 2.0f * scale, min.y - 2.0f * scale),
                 ImVec2(max.x + 2.0f * scale, max.y + 2.0f * scale), Pack(palette.focus),
                 4.0f * scale, 0, 2.0f * scale);
}

void DrawBorder(const ImVec2& min, const ImVec2& max, float radius, float scale,
                std::uint32_t border_color) {
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    draw.AddRect(min, max, Pack(border_color), radius, 0, 1.0f * scale);
}

bool StyleButton(const char* id, const char* label, const ImVec2& size, const ImVec4& fill,
                 const ImVec4& hover, const ImVec4& active, std::uint32_t text, float scale,
                 const lwf::ui::Palette& palette) {
    ImGui::PushStyleColor(ImGuiCol_Button, fill);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
    ImGui::PushStyleColor(ImGuiCol_Text, Color(text));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, studio_metrics::radius_button * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f * scale, 8.0f * scale));
    const std::string button_label =
        std::string(label == nullptr ? "" : label) + "##" + (id == nullptr ? "" : id);
    const bool pressed = ImGui::Button(button_label.c_str(), size);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    if (ImGui::IsItemFocused()) {
        DrawFocusRing(min, max, scale, palette);
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return pressed;
}

}

bool PrimaryButton(const char* id, const char* label, float scale, const lwf::ui::Palette& palette) {
    const bool pressed = StyleButton(id, label, ImVec2(0.0f, 40.0f * scale), Color(palette.accent),
                                     Color(palette.accent), Color(palette.accent),
                                     palette.on_accent, scale, palette);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    DrawBorder(min, max, studio_metrics::radius_button * scale, scale, palette.accent);
    return pressed;
}

bool LightButton(const char* id, const char* label, float scale, const lwf::ui::Palette& palette) {
    // Hairline ghost button (DESIGN §6): transparent middle, 1 px hairline
    // border; hover/active shift only (a faint border-token wash, never a
    // surface fill).
    const bool pressed = StyleButton(id, label, ImVec2(0.0f, 40.0f * scale),
                                     ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
                                     Color(palette.border, 0.15f), Color(palette.border, 0.25f),
                                     palette.text, scale, palette);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    DrawBorder(min, max, studio_metrics::radius_button * scale, scale, palette.border);
    return pressed;
}

bool OutlinePill(const char* id, const char* label, bool enabled, float scale, const lwf::ui::Palette& palette) {
    const std::uint32_t text = enabled ? palette.text : palette.muted;
    // Hairline outline: enabled draws the palette border token (1 px
    // hairline), not the text color; disabled dims to muted.
    const std::uint32_t border = enabled ? palette.border : palette.muted;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, studio_metrics::radius_pill * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f * scale, 8.0f * scale));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Color(palette.border, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Color(palette.border));
    ImGui::PushStyleColor(ImGuiCol_Text, Color(text));
    bool pressed = false;
    const std::string button_label =
        std::string(label == nullptr ? "" : label) + "##" + (id == nullptr ? "" : id);
    if (enabled) {
        pressed = ImGui::Button(button_label.c_str(), ImVec2(0.0f, 32.0f * scale));
    } else {
        ImGui::BeginDisabled();
        pressed = ImGui::Button(button_label.c_str(), ImVec2(0.0f, 32.0f * scale));
        ImGui::EndDisabled();
    }
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    DrawBorder(min, max, studio_metrics::radius_pill * scale, scale, border);
    if (ImGui::IsItemFocused()) {
        DrawFocusRing(min, max, scale, palette);
    }
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    return pressed;
}

bool IconButton(const char* id, const char* glyph, float scale, const lwf::ui::Palette& palette) {
    const char* safe_id = id == nullptr ? "" : id;
    const char* safe_glyph = glyph == nullptr ? "" : glyph;
    const float hit = studio_metrics::hit_size * scale;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed =
        ImGui::InvisibleButton(safe_id, ImVec2(hit, hit), ImGuiButtonFlags_EnableNav);
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    if (ImGui::IsItemHovered()) {
        // Hairline hover: outline only, no fill.
        draw.AddRect(pos, ImVec2(pos.x + hit, pos.y + hit), Pack(palette.border),
                     studio_metrics::radius_chip * scale, 0, 1.0f * scale);
    }
    if (ImGui::IsItemFocused()) {
        DrawFocusRing(pos, ImVec2(pos.x + hit, pos.y + hit), scale, palette);
    }
    ImGui::PushFont(nullptr, studio_metrics::font_heading_px);
    const ImVec2 text_size = ImGui::CalcTextSize(safe_glyph);
    draw.AddText(ImVec2(pos.x + (hit - text_size.x) * 0.5f, pos.y + (hit - text_size.y) * 0.5f),
                 Pack(palette.text), safe_glyph);
    ImGui::PopFont();
    return pressed;
}

bool ToggleSwitch(const char* id, bool value, float scale, const lwf::ui::Palette& palette) {
    const char* safe_id = id == nullptr ? "" : id;
    const float w = 52.0f * scale;
    const float h = studio_metrics::hit_size * scale;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool toggled = ImGui::InvisibleButton(safe_id, ImVec2(w, h), ImGuiButtonFlags_EnableNav);
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    if (ImGui::IsItemFocused()) {
        DrawFocusRing(p, ImVec2(p.x + w, p.y + h), scale, palette);
    }
    // Hairline track (1 px outline, transparent fill) + square knob sized
    // from the status-square token. On-state: filled accent square (the
    // square is the color carrier); off-state: muted hairline square. The
    // 52x32 hit box, EnableNav and focus ring are unchanged.
    draw.AddRect(p, ImVec2(p.x + w, p.y + h), Pack(palette.border), 0.0f, 0, 1.0f * scale);
    const float knob = metrics::status_square * scale;
    const float pad = 4.0f * scale;
    const float x = value ? p.x + w - pad - knob : p.x + pad;
    const ImVec2 knob_min(x, p.y + (h - knob) * 0.5f);
    const ImVec2 knob_max(x + knob, knob_min.y + knob);
    if (value) {
        draw.AddRectFilled(knob_min, knob_max, Pack(palette.accent));
    } else {
        draw.AddRect(knob_min, knob_max, Pack(palette.muted), 0.0f, 0, 1.0f * scale);
    }
    return toggled;
}

void Keycap(const char* label, float scale, const lwf::ui::Palette& palette) {
    const char* safe_label = label == nullptr ? "" : label;
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    const ImVec2 text_size = ImGui::CalcTextSize(safe_label);
    ImGui::PopFont();
    const float pad_x = 12.0f * scale;
    const float h = studio_metrics::hit_size * scale;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(text_size.x + pad_x * 2.0f, h);
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    // Hairline box outline, transparent fill (no surface fill).
    DrawBorder(pos, ImVec2(pos.x + size.x, pos.y + size.y),
               studio_metrics::radius_chip * scale, scale, palette.border);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    draw.AddText(ImVec2(pos.x + pad_x, pos.y + (size.y - text_size.y) * 0.5f),
                 Pack(palette.text), safe_label);
    ImGui::PopFont();
    ImGui::Dummy(size);
}

bool Chip(const char* id, const char* label, bool selected, float scale, const lwf::ui::Palette& palette) {
    const char* safe_id = id == nullptr ? "" : id;
    const char* safe_label = label == nullptr ? "" : label;
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    const ImVec2 text_size = ImGui::CalcTextSize(safe_label);
    ImGui::PopFont();
    const float pad_x = 14.0f * scale;
    const float h = studio_metrics::hit_size * scale;
    const ImVec2 size(text_size.x + pad_x * 2.0f, h);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(safe_id, size, ImGuiButtonFlags_EnableNav);
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    if (selected) {
        // Selected: 2 px accent underline instead of a border fill.
        draw.AddLine(ImVec2(pos.x + 2.0f * scale, pos.y + size.y - 1.0f * scale),
                     ImVec2(pos.x + size.x - 2.0f * scale, pos.y + size.y - 1.0f * scale),
                     Pack(palette.accent), 2.0f * scale);
    } else {
        // Unselected: hairline outline + text, transparent fill.
        draw.AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), Pack(palette.border),
                     studio_metrics::radius_pill * scale, 0, 1.0f * scale);
    }
    if (ImGui::IsItemFocused()) {
        DrawFocusRing(pos, ImVec2(pos.x + size.x, pos.y + size.y), scale, palette);
    }
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    const std::uint32_t text = selected ? palette.text : palette.muted;
    draw.AddText(ImVec2(pos.x + pad_x, pos.y + (size.y - text_size.y) * 0.5f), Pack(text),
                 safe_label);
    ImGui::PopFont();
    return pressed;
}

void BadgeBeta(float scale, const lwf::ui::Palette& palette) {
    const std::string spaced = UpperSpaced("Beta");
    ImGui::PushFont(nullptr, studio_metrics::font_caps_px);
    const ImVec2 text_size = ImGui::CalcTextSize(spaced.c_str());
    ImGui::PopFont();
    const float pad_x = 10.0f * scale;
    const float h = 24.0f * scale;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(text_size.x + pad_x * 2.0f, h);
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    // Hairline outline pill, no border fill. 24 px static, kept by design.
    draw.AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), Pack(palette.border),
                 studio_metrics::radius_pill * scale, 0, 1.0f * scale);
    ImGui::PushFont(nullptr, studio_metrics::font_caps_px);
    draw.AddText(ImVec2(pos.x + pad_x, pos.y + (size.y - text_size.y) * 0.5f),
                 Pack(palette.accent), spaced.c_str());
    ImGui::PopFont();
    ImGui::Dummy(size);
}

void TabRow(const char* id, const char* const* tabs, int count, int* selected, float scale, const lwf::ui::Palette& palette) {
    if (tabs == nullptr || selected == nullptr || count <= 0) {
        return;
    }
    ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
    const float line = ImGui::GetTextLineHeight();
    const float hit = std::max(studio_metrics::hit_size * scale, line + 12.0f * scale);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float row_w = ImGui::GetContentRegionAvail().x;
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    const std::string base = id == nullptr ? std::string() : std::string(id);
    for (int i = 0; i < count; ++i) {
        if (i > 0) {
            ImGui::SameLine(0.0f, 0.0f);
        }
        const std::string tab_id = base + "_" + std::to_string(i);
        const char* safe_tab = tabs[i] == nullptr ? "" : tabs[i];
        const ImVec2 text_size = ImGui::CalcTextSize(safe_tab);
        const bool pressed =
            ImGui::InvisibleButton(tab_id.c_str(), ImVec2(text_size.x + 12.0f * scale, hit),
                                   ImGuiButtonFlags_EnableNav);
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        if (pressed) {
            *selected = i;
        }
        const bool active = (*selected == i);
        const bool hovered = ImGui::IsItemHovered();
        const std::uint32_t color =
            active ? palette.text : (hovered ? palette.text : palette.muted);
        draw.AddText(ImVec2(min.x + 6.0f * scale, min.y + (hit - text_size.y) * 0.5f),
                     Pack(color), safe_tab);
        if (active) {
            draw.AddLine(ImVec2(min.x + 2.0f * scale, max.y - 1.0f * scale),
                         ImVec2(max.x - 2.0f * scale, max.y - 1.0f * scale),
                         Pack(palette.text), 2.0f * scale);
        }
        if (ImGui::IsItemFocused()) {
            DrawFocusRing(min, max, scale, palette);
        }
    }
    const float row_y = ImGui::GetItemRectMax().y;
    draw.AddLine(ImVec2(origin.x, row_y), ImVec2(origin.x + row_w, row_y),
                 Pack(palette.border), 1.0f * scale);
    ImGui::PopFont();
}

bool TopStripItem(const char* id, const char* label, bool active, float scale,
                  const lwf::ui::Palette& palette) {
    const char* safe_id = id == nullptr ? "" : id;
    const char* safe_label = label == nullptr ? "" : label;
    ImGui::PushFont(nullptr, metrics::font_body_px);
    const ImVec2 text_size = ImGui::CalcTextSize(safe_label);
    ImGui::PopFont();
    // The item is exactly the top-strip height (metrics::topstrip_height),
    // which keeps the hit height >= 32 (studio_metrics::hit_size) per AC-G2.
    // No fills: active state is a 2 px accent underline at the strip bottom
    // (TabRow geometry), text carries the rest of the state.
    const float strip_h = metrics::topstrip_height * scale;
    const ImVec2 size(text_size.x + 12.0f * scale, strip_h);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(safe_id, size, ImGuiButtonFlags_EnableNav);
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    const bool hovered = ImGui::IsItemHovered();
    const std::uint32_t color = active ? palette.text : (hovered ? palette.text : palette.muted);
    ImGui::PushFont(nullptr, metrics::font_body_px);
    draw.AddText(ImVec2(pos.x + 6.0f * scale, pos.y + (strip_h - text_size.y) * 0.5f),
                 Pack(color), safe_label);
    ImGui::PopFont();
    if (active) {
        draw.AddLine(ImVec2(pos.x + 2.0f * scale, pos.y + strip_h - 1.0f * scale),
                     ImVec2(pos.x + size.x - 2.0f * scale, pos.y + strip_h - 1.0f * scale),
                     Pack(palette.accent), 2.0f * scale);
    }
    if (ImGui::IsItemFocused()) {
        DrawFocusRing(pos, ImVec2(pos.x + size.x, pos.y + size.y), scale, palette);
    }
    return pressed;
}

}
