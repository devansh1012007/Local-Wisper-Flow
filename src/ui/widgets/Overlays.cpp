#include "ui/widgets/Overlays.h"

#include "ui/theme/StudioPalette.h"
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
    draw.AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), Pack(palette.text),
                       studio_metrics::radius_pill * scale);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    const float text_y = pos.y + (h - text_size.y) * 0.5f;
    draw.AddText(ImVec2(pos.x + 16.0f * scale, text_y), Pack(palette.surface),
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
        // DECISION: action text uses accent (single accent per view) over the
        // text-fill toast; see report for the measured tradeoff. Accent on
        // text measures 2.38 in Ivory and 1.80 in the dark variant, so this
        // does NOT meet the 4.5 text floor; the 6.10 figure cited during
        // design is on_accent on accent, not accent on text. The dark
        // variant stays a runtime choice via the palette param; a mid-brass
        // toast token is the follow-up that would restore 4.5 on this fill.
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
