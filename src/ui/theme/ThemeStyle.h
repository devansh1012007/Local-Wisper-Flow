#pragma once

// ImGui-side helpers for the token system. ThemePalette.h stays free of
// Dear ImGui so the tokens can be unit tested; this header converts tokens
// to packed colors and ImVec4 for drawing code.

#include "ui/theme/ThemePalette.h"

#include "imgui.h"

#include <algorithm>
#include <cstdint>

namespace lwf::ui {

// 0xRRGGBB token to a packed ImGui color with alpha (default 1.0).
inline ImU32 Pack(std::uint32_t rgb, float alpha = 1.0f) {
    const int r = static_cast<int>((rgb >> 16) & 0xFF);
    const int g = static_cast<int>((rgb >> 8) & 0xFF);
    const int b = static_cast<int>(rgb & 0xFF);
    const int a = static_cast<int>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f + 0.5f);
    return IM_COL32(r, g, b, a);
}

// 0xRRGGBB token to an ImVec4 color with alpha (default 1.0).
inline ImVec4 Color(std::uint32_t rgb, float alpha = 1.0f) {
    const float r = static_cast<float>((rgb >> 16) & 0xFF) / 255.0f;
    const float g = static_cast<float>((rgb >> 8) & 0xFF) / 255.0f;
    const float b = static_cast<float>(rgb & 0xFF) / 255.0f;
    return ImVec4(r, g, b, std::clamp(alpha, 0.0f, 1.0f));
}

}  // namespace lwf::ui
