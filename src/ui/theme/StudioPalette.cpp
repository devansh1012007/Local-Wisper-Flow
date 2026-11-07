#include "ui/theme/StudioPalette.h"

#include "ui/theme/ThemeStyle.h"

#include "imgui.h"

#include <algorithm>

namespace lwf::ui {
namespace {

ImGuiContext* g_studio_last_context = nullptr;
float g_studio_last_scale = 1.0f;

}

void ApplyStudioStyle(const StudioPalette& palette, float scale) {
    ImGuiStyle& style = ImGui::GetStyle();
    const bool applies = ImGui::GetCurrentContext() != g_studio_last_context ||
                         scale != g_studio_last_scale;
    if (applies) {
        const float preserved_font_scale_dpi = style.FontScaleDpi;
        const float preserved_alpha = style.Alpha;
        const float preserved_disabled_alpha = style.DisabledAlpha;
        const float preserved_font_size_base = style.FontSizeBase;
        style = ImGuiStyle();
        style.FontScaleDpi = preserved_font_scale_dpi;
        style.Alpha = preserved_alpha;
        style.DisabledAlpha = preserved_disabled_alpha;
        style.FontSizeBase = preserved_font_size_base;
        g_studio_last_context = ImGui::GetCurrentContext();
        g_studio_last_scale = scale;
        style.WindowRounding = studio_metrics::radius_card;
        style.ChildRounding = studio_metrics::radius_card;
        style.PopupRounding = studio_metrics::radius_button;
        style.FrameRounding = studio_metrics::radius_button;
        style.GrabRounding = studio_metrics::radius_pill;
        style.WindowPadding = ImVec2(studio_metrics::page_padding, studio_metrics::page_padding);
        style.FontScaleMain = scale;
        style.ScaleAllSizes(scale);
    }
    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = Color(palette.ink);
    c[ImGuiCol_TextDisabled] = Color(palette.muted);
    c[ImGuiCol_WindowBg] = Color(palette.page_bg);
    c[ImGuiCol_ChildBg] = Color(palette.card_bg);
    c[ImGuiCol_PopupBg] = Color(palette.card_bg_white, 0.97f);
    c[ImGuiCol_Border] = Color(palette.border);
    c[ImGuiCol_TitleBg] = Color(palette.page_bg);
    c[ImGuiCol_TitleBgActive] = Color(palette.page_bg);
    c[ImGuiCol_MenuBarBg] = Color(palette.rail_bg);
    c[ImGuiCol_FrameBg] = Color(palette.card_bg_white);
    c[ImGuiCol_FrameBgHovered] = Color(palette.teal_100, 0.5f);
    c[ImGuiCol_FrameBgActive] = Color(palette.teal_100);
    c[ImGuiCol_Button] = Color(palette.teal_700);
    c[ImGuiCol_ButtonHovered] = Color(palette.teal_500);
    c[ImGuiCol_ButtonActive] = Color(palette.teal_700);
    c[ImGuiCol_Header] = Color(palette.teal_100, 0.6f);
    c[ImGuiCol_HeaderHovered] = Color(palette.teal_100);
    c[ImGuiCol_HeaderActive] = Color(palette.teal_300, 0.5f);
    c[ImGuiCol_Separator] = Color(palette.border);
    c[ImGuiCol_CheckMark] = Color(palette.card_bg_white);
    c[ImGuiCol_SliderGrab] = Color(palette.teal_700);
    c[ImGuiCol_SliderGrabActive] = Color(palette.teal_500);
    c[ImGuiCol_Tab] = Color(palette.card_bg);
    c[ImGuiCol_TabHovered] = Color(palette.teal_100, 0.6f);
    c[ImGuiCol_TabSelected] = Color(palette.card_bg_white);
    c[ImGuiCol_TableBorderStrong] = Color(palette.border);
    c[ImGuiCol_TableBorderLight] = Color(palette.border);
    c[ImGuiCol_NavCursor] = Color(palette.teal_700);
    c[ImGuiCol_ModalWindowDimBg] = Color(palette.ink, 0.4f);
}

}
