#pragma once

#include <cstdint>
#include <string>

namespace lwf::ui {

struct StudioPalette {
    std::uint32_t page_bg;
    std::uint32_t rail_bg;
    std::uint32_t card_bg;
    std::uint32_t card_bg_white;
    std::uint32_t border;
    std::uint32_t ink;
    std::uint32_t muted;
    std::uint32_t teal_700;
    std::uint32_t teal_500;
    std::uint32_t teal_300;
    std::uint32_t teal_100;
    std::uint32_t heat_empty;
    std::uint32_t banner_cream;
};

inline constexpr StudioPalette kStudioPalette{
    0xFAFAF8,
    0xF4F2EE,
    0xF4F2EE,
    0xFFFFFF,
    0xE7E4DE,
    0x1C1C1C,
    0x7A776F,
    0x1E5E5A,
    0x2F8077,
    0x7DBFB7,
    0xCDEBE7,
    0xCDC6BB,
    0xF1ECE4,
};

namespace studio_metrics {

inline constexpr float radius_banner = 16.0f;
inline constexpr float radius_card = 12.0f;
inline constexpr float radius_row = 10.0f;
inline constexpr float radius_input = 10.0f;
inline constexpr float radius_button = 8.0f;
inline constexpr float radius_chip = 6.0f;
inline constexpr float radius_pill = 999.0f;

inline constexpr float page_padding = 32.0f;
inline constexpr float gap = 24.0f;
inline constexpr float row_height = 52.0f;
inline constexpr float hit_size = 32.0f;

inline constexpr float font_banner_px = 40.0f;
inline constexpr float font_caps_px = 12.0f;
inline constexpr float font_page_title_px = 28.0f;
inline constexpr float font_helper_px = 13.0f;

inline constexpr float toast_dwell_s = 5.0f;
inline constexpr int toast_dwell_ms = 5000;

inline constexpr float activity_cell = 14.0f;
inline constexpr float activity_gap = 4.0f;
inline constexpr float activity_corner = 3.0f;
inline constexpr float gauge_stroke = 14.0f;
inline constexpr float progress_height = 28.0f;

}

inline float Clamp01(float value) {
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

inline int HeatLevel(int value, int month_max) {
    if (value <= 0 || month_max <= 0) {
        return 0;
    }
    const float ratio = static_cast<float>(value) / static_cast<float>(month_max);
    if (ratio > 0.75f) {
        return 4;
    }
    if (ratio > 0.5f) {
        return 3;
    }
    if (ratio > 0.25f) {
        return 2;
    }
    return 1;
}

inline std::string UpperSpaced(const std::string& text) {
    std::string out;
    out.reserve(text.size() + text.size());
    bool first = true;
    for (const char c : text) {
        const char upper = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
        if (!first) {
            out.push_back(' ');
        }
        out.push_back(upper);
        first = false;
    }
    return out;
}

void ApplyStudioStyle(const StudioPalette& palette, float scale);

}
