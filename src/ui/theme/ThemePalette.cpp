#include "ui/theme/ThemePalette.h"

#include "imgui.h"

#include <algorithm>

namespace lwf::ui {
namespace {

ImU32 ToPacked(std::uint32_t rgb, float alpha = 1.0f) {
    const int r = static_cast<int>((rgb >> 16) & 0xFF);
    const int g = static_cast<int>((rgb >> 8) & 0xFF);
    const int b = static_cast<int>(rgb & 0xFF);
    const int a = static_cast<int>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f + 0.5f);
    return IM_COL32(r, g, b, a);
}

ImVec4 ToColor(std::uint32_t rgb, float alpha = 1.0f) {
    const float r = static_cast<float>((rgb >> 16) & 0xFF) / 255.0f;
    const float g = static_cast<float>((rgb >> 8) & 0xFF) / 255.0f;
    const float b = static_cast<float>(rgb & 0xFF) / 255.0f;
    return ImVec4(r, g, b, std::clamp(alpha, 0.0f, 1.0f));
}

}  // namespace

void ApplyImGuiStyle(const Palette& palette, float scale) {
    ImGuiStyle& style = ImGui::GetStyle();

    // Geometry from the token set.
    style.WindowRounding = metrics::radius_card;
    style.ChildRounding = metrics::radius_card;
    style.PopupRounding = metrics::radius_control;
    style.FrameRounding = metrics::radius_control;
    style.GrabRounding = metrics::radius_pill;
    style.ScrollbarRounding = metrics::radius_pill;
    style.ScrollbarSize = 10.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(metrics::space_m, metrics::space_m);
    style.FramePadding = ImVec2(metrics::space_s, 6.0f);
    style.ItemSpacing = ImVec2(metrics::space_s, metrics::space_s);
    style.ItemInnerSpacing = ImVec2(metrics::space_s, 4.0f);
    style.IndentSpacing = metrics::space_m;
    style.GrabMinSize = 14.0f;

    // The built-in font is scaled until real fonts are approved; the display
    // sizes on the rail and page titles use SetWindowFontScale at call sites.
    style.FontScaleMain = scale;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = ToColor(palette.text);
    c[ImGuiCol_TextDisabled] = ToColor(palette.muted);
    c[ImGuiCol_WindowBg] = ToColor(palette.bg);
    c[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_PopupBg] = ToColor(palette.surface, 0.97f);
    c[ImGuiCol_Border] = ToColor(palette.border);
    c[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_TitleBg] = ToColor(palette.bg);
    c[ImGuiCol_TitleBgActive] = ToColor(palette.bg);
    c[ImGuiCol_TitleBgCollapsed] = ToColor(palette.bg);
    c[ImGuiCol_MenuBarBg] = ToColor(palette.surface);

    // Frames (inputs, combos, checkboxes): a quiet fill derived from text,
    // never a new hue.
    c[ImGuiCol_FrameBg] = ToColor(palette.text, 0.06f);
    c[ImGuiCol_FrameBgHovered] = ToColor(palette.text, 0.10f);
    c[ImGuiCol_FrameBgActive] = ToColor(palette.text, 0.14f);

    c[ImGuiCol_CheckMark] = ToColor(palette.accent);
    c[ImGuiCol_SliderGrab] = ToColor(palette.accent);
    c[ImGuiCol_SliderGrabActive] = ToColor(palette.accent);

    // Buttons: quiet by default; primary buttons are pushed at call sites
    // with accent + on_accent.
    c[ImGuiCol_Button] = ToColor(palette.text, 0.05f);
    c[ImGuiCol_ButtonHovered] = ToColor(palette.text, 0.10f);
    c[ImGuiCol_ButtonActive] = ToColor(palette.text, 0.15f);

    c[ImGuiCol_Header] = ToColor(palette.text, 0.06f);
    c[ImGuiCol_HeaderHovered] = ToColor(palette.text, 0.10f);
    c[ImGuiCol_HeaderActive] = ToColor(palette.text, 0.14f);

    c[ImGuiCol_Separator] = ToColor(palette.border);
    c[ImGuiCol_SeparatorHovered] = ToColor(palette.muted);
    c[ImGuiCol_SeparatorActive] = ToColor(palette.muted);

    c[ImGuiCol_ResizeGrip] = ToColor(palette.accent, 0.20f);
    c[ImGuiCol_ResizeGripHovered] = ToColor(palette.accent, 0.40f);
    c[ImGuiCol_ResizeGripActive] = ToColor(palette.accent, 0.60f);

    c[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_ScrollbarGrab] = ToColor(palette.text, 0.15f);
    c[ImGuiCol_ScrollbarGrabHovered] = ToColor(palette.text, 0.25f);
    c[ImGuiCol_ScrollbarGrabActive] = ToColor(palette.text, 0.35f);

    c[ImGuiCol_Tab] = ToColor(palette.surface);
    c[ImGuiCol_TabHovered] = ToColor(palette.text, 0.10f);
    c[ImGuiCol_TabSelected] = ToColor(palette.surface);
    c[ImGuiCol_TabDimmed] = ToColor(palette.surface);
    c[ImGuiCol_TabDimmedSelected] = ToColor(palette.surface);
    c[ImGuiCol_PlotLines] = ToColor(palette.accent);
    c[ImGuiCol_PlotHistogram] = ToColor(palette.accent);
    c[ImGuiCol_TableHeaderBg] = ToColor(palette.surface);
    c[ImGuiCol_TableBorderStrong] = ToColor(palette.border);
    c[ImGuiCol_TableBorderLight] = ToColor(palette.border);
    c[ImGuiCol_TextSelectedBg] = ToColor(palette.accent, 0.30f);
    c[ImGuiCol_DragDropTarget] = ToColor(palette.accent);
    c[ImGuiCol_NavCursor] = ToColor(palette.focus);
    c[ImGuiCol_ModalWindowDimBg] = ToColor(palette.bg, 0.60f);

    if (scale != 1.0f) {
        style.ScaleAllSizes(scale);
    }
}

}  // namespace lwf::ui
