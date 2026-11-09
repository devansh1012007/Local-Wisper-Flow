#include "ui/widgets/Overlays.h"

#include "ui/theme/ThemePalette.h"
#include "ui/theme/ThemeStyle.h"

#include "imgui.h"

namespace lwf::ui::widgets {

void Modal::Open() {
    open_ = true;
    armed_ = true;
}

void Modal::Close() {
    open_ = false;
    armed_ = false;
}

bool Modal::IsOpen() const {
    return open_;
}

bool Modal::Begin(const char* title, float scale) {
    if (!open_ || title == nullptr) {
        visible_ = false;
        return false;
    }
    if (armed_) {
        ImGui::OpenPopup(title);
        armed_ = false;
    }
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(viewport.x * 0.5f, viewport.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(24.0f * scale, 24.0f * scale));
    const bool visible = ImGui::BeginPopupModal(title, &open_, ImGuiWindowFlags_NoResize);
    ImGui::PopStyleVar();
    visible_ = visible;
    if (visible) {
        // Hairline language: 1 px accent hairline under the title row. The
        // accent comes from ApplyImGuiStyle's style colors (this overlay
        // takes no palette), ImGuiCol_CheckMark the same accent source the
        // style uses for checks.
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        const ImVec2 wp = ImGui::GetWindowPos();
        const float y = wp.y + ImGui::GetFrameHeight();
        draw.AddLine(ImVec2(wp.x, y), ImVec2(wp.x + ImGui::GetWindowWidth(), y),
                     ImGui::ColorConvertFloat4ToU32(
                         ImGui::GetStyle().Colors[ImGuiCol_CheckMark]),
                     1.0f * scale);
    }
    if (visible && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        Close();
    }
    return visible;
}

void Modal::End() {
    if (!visible_) {
        return;
    }
    visible_ = false;
    ImGui::EndPopup();
}

void Sheet::Open() {
    open_ = true;
    armed_ = true;
}

void Sheet::Close() {
    open_ = false;
    armed_ = false;
}

bool Sheet::IsOpen() const {
    return open_;
}

bool Sheet::Begin(const char* title, float width, float scale) {
    if (!open_ || title == nullptr) {
        visible_ = false;
        return false;
    }
    if (armed_) {
        ImGui::OpenPopup(title);
        armed_ = false;
    }
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    const float w = width * scale;
    ImGui::SetNextWindowPos(ImVec2(viewport.x - w, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(w, viewport.y), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(24.0f * scale, 24.0f * scale));
    const bool visible = ImGui::BeginPopupModal(title, &open_,
                                                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                                    ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar();
    visible_ = visible;
    if (visible) {
        // Hairline language: 1 px left hairline (separator style color, the
        // palette border token via ApplyImGuiStyle) inset past the popup
        // border, spanning the sheet height.
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        const ImVec2 wp = ImGui::GetWindowPos();
        const float x = wp.x + 1.0f * scale;
        draw.AddLine(ImVec2(x, wp.y), ImVec2(x, wp.y + ImGui::GetWindowHeight()),
                     ImGui::ColorConvertFloat4ToU32(
                         ImGui::GetStyle().Colors[ImGuiCol_Separator]),
                     1.0f * scale);
    }
    if (visible && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        Close();
    }
    return visible;
}

void Sheet::End() {
    if (!visible_) {
        return;
    }
    visible_ = false;
    ImGui::EndPopup();
}

void Toast::Show(const std::string& message, const std::string& action_label) {
    message_ = message;
    action_label_ = action_label;
    shown_at_ = static_cast<float>(ImGui::GetTime());
    action_pending_ = false;
}

void Toast::Dismiss() {
    shown_at_ = -1.0f;
    action_pending_ = false;
}

bool Toast::IsVisible() const {
    if (shown_at_ < 0.0f) {
        return false;
    }
    return (static_cast<float>(ImGui::GetTime()) - shown_at_) < studio_metrics::toast_dwell_s;
}

void Toast::Draw(float scale, const lwf::ui::Palette& palette) {
    if (!IsVisible()) {
        return;
    }
    const float h = studio_metrics::hit_size * scale;
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    const ImVec2 text_size = ImGui::CalcTextSize(message_.c_str());
    ImGui::PopFont();
    float w = text_size.x + 32.0f * scale;
    if (!action_label_.empty()) {
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        const ImVec2 action_size = ImGui::CalcTextSize(action_label_.c_str());
        ImGui::PopFont();
        w += action_size.x + 32.0f * scale;
    }
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    const ImVec2 pos(viewport.x * 0.5f - w * 0.5f, viewport.y - h - 24.0f * scale);
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    // Full-window toast (hairline language): hairline outline box,
    // transparent fill — message + action text sit on the window background
    // inside the 1 px border-token outline, never on a surface fill. Border
    // is 1 px and never carries state on its own (R9).
    draw.AddRect(pos, ImVec2(pos.x + w, pos.y + h), Pack(palette.border),
                 studio_metrics::radius_pill * scale, 0, 1.0f * scale);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    const float text_y = pos.y + (h - text_size.y) * 0.5f;
    draw.AddText(ImVec2(pos.x + 16.0f * scale, text_y), Pack(palette.text),
                 message_.c_str());
    ImGui::PopFont();
    if (!action_label_.empty()) {
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        const ImVec2 action_size = ImGui::CalcTextSize(action_label_.c_str());
        ImGui::PopFont();
        const ImVec2 button_min(pos.x + 16.0f * scale + text_size.x + 8.0f * scale, pos.y);
        const ImVec2 button_size(action_size.x + 24.0f * scale, h);
        const ImVec2 saved_cursor = ImGui::GetCursorScreenPos();
        const std::string button_label = action_label_ + "##toast_action";
        ImGui::SetCursorScreenPos(button_min);
        // Action text is accent on the window background (single accent per
        // view): the accent token passes 4.5:1 as text on bg in both themes
        // (DESIGN §4: 12.66 Graphite / 6.60 Paper). Focus ring is
        // palette.focus at 2 px (already handled below).
        ImGui::PushStyleColor(ImGuiCol_Text, Color(palette.accent));
        if (ImGui::Button(button_label.c_str(), button_size)) {
            action_pending_ = true;
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemFocused()) {
            const ImVec2 ring_min = ImGui::GetItemRectMin();
            const ImVec2 ring_max = ImGui::GetItemRectMax();
            draw.AddRect(ImVec2(ring_min.x - 2.0f * scale, ring_min.y - 2.0f * scale),
                         ImVec2(ring_max.x + 2.0f * scale, ring_max.y + 2.0f * scale),
                         Pack(palette.focus), h * 0.5f, 0, 2.0f * scale);
        }
        ImGui::SetCursorScreenPos(ImVec2(saved_cursor.x, saved_cursor.y - ImGui::GetStyle().ItemSpacing.y));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }
}

bool Toast::TakeAction() {
    const bool result = action_pending_;
    action_pending_ = false;
    if (result) {
        Dismiss();
    }
    return result;
}

}
