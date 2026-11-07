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
    0x6E6A62,
    0x1E5E5A,
    0x2F8077,
    0x7DBFB7,
    0xCDEBE7,
    0xCDC6BB,
    0xF1ECE4,
};

namespace studio_metrics {

inline constexpr float radius_banner = 16.0f;
// Native metrics::radius_card is 10 (mockup). Studio radius_card is 12
// (screenshot-derived). They are not interchangeable: studio sites must
// qualify studio_metrics::radius_card, native sites metrics::radius_card.
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

// Wave 2 T-003 additions: screenshot-derived studio rhythm (NOT mockup
// tokens; do not treat as D0 content in a future mockup regen). Gaps are
// spelled by role, not scale letters, so a wrong-namespace swap stays
// visible. gap_inline (8) equals metrics::space_s and gap_block (16)
// equals metrics::space_m by grid coincidence; the studio names mark the
// studio role. No studio twin exists for the 4px hairline gap, so the one
// metrics::space_xs site on a studio page intentionally keeps borrowing
// that grid value; do not edit either side expecting the other not to move.
inline constexpr float gap_micro = 6.0f;    // legend cells, innermost spacing
inline constexpr float gap_inline = 8.0f;   // control-to-control inline gap
inline constexpr float gap_compact = 10.0f; // header-cluster glue, mini rows
inline constexpr float gap_section = 12.0f; // section cadence after headers/tabs
inline constexpr float gap_block = 16.0f;   // block-to-block gap (tabs to cards)

inline constexpr float font_banner_px = 40.0f;
inline constexpr float font_caps_px = 12.0f;
inline constexpr float font_page_title_px = 28.0f;
inline constexpr float font_helper_px = 13.0f;
// Wave 2 T-003 additions (same note as the gaps above: screenshot-derived,
// not mockup). font_card_title_px (15) is the studio twin of
// metrics::font_card_title_px: same value by screenshot coincidence, not by
// mockup derivation. font_heading_px (24) also serves the IconButton glyph
// at the same optical size (32px hit box); split it into a separate glyph
// token only if glyph sizing ever diverges from headings.
inline constexpr float font_card_title_px = 15.0f;
inline constexpr float font_heading_px = 24.0f;
inline constexpr float font_subheading_px = 17.0f;
inline constexpr float font_glyph_px = 10.0f;

inline constexpr int toast_dwell_ms = 5000;
inline constexpr float toast_dwell_s = toast_dwell_ms / 1000.0f;

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
    if (value >= month_max) {
        return 4;
    }
    const std::int64_t quarter = static_cast<std::int64_t>(value) * 4 / month_max;
    return static_cast<int>(quarter > 0 ? quarter : 1);
}

inline std::uint32_t HeatColor(int level) {
    switch (level) {
        case 4:
            return kStudioPalette.teal_700;
        case 3:
            return kStudioPalette.teal_500;
        case 2:
            return kStudioPalette.teal_300;
        case 1:
            return kStudioPalette.teal_100;
        default:
            return kStudioPalette.heat_empty;
    }
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
