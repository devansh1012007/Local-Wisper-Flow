#pragma once

// StudioPalette — DEPRECATED SHIM (Phase 2 Ivory unify, 2026-11-08).
//
// The teal visual identity is retired: every field below now maps to the
// Ivory-primary kLightPalette (ThemePalette.h) so pages/widgets keep
// compiling unchanged while Phase 3-5 migrates them onto Palette directly.
// Do NOT add new StudioPalette fields, and do NOT re-introduce teal values
// here — the struct + kStudioPalette + studio_metrics + HeatColor +
// ApplyStudioStyle are kept only so Ph3/Ph5 migration stays additive.
//
// Field mapping (Ivory primary; dark variant is a runtime choice, not a
// second Studio palette):
//   page_bg       = bg      0xF6F1E4  (page ground)
//   rail_bg       = surface 0xFFFDF7  (rail is a card surface, not a tint)
//   card_bg       = surface 0xFFFDF7
//   card_bg_white = surface 0xFFFDF7  (no separate white; single surface)
//   border        = border  0xD9CFBA
//   ink           = text    0x1C2B26
//   muted         = muted   0x5B665F
//   banner_cream  = surface 0xFFFDF7  (hero/banner is a surface, not cream)
//   teal_700      = accent  0x7A5C25  (primary action / darkest heat)
//   teal_500      = accent  0x7A5C25  (hover/active today; Ph3 will split
//                                       the ramp, so equal today by design)
//   teal_300      = ramp3   0xA28C64  (DECISION: mockup §4B ramp stage 3,
//                                       not accent, so the middle of the
//                                       heat ramp stays distinct; equals
//                                       neither accent nor empty)
//   teal_100      = border  0xD9CFBA  (lightest ramp step is empty in the
//                                       mockup ramp's low end; equals border)
//   heat_empty    = border  0xD9CFBA  (nothing recorded == hairline ground)
// The teal_* field NAMES are historical and must not be renamed until the
// Ph3-5 call sites are migrated (breaking-API deprecation, out of scope).

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
    0xF6F1E4,  // page_bg = kLightPalette.bg
    0xFFFDF7,  // rail_bg = kLightPalette.surface
    0xFFFDF7,  // card_bg = kLightPalette.surface
    0xFFFDF7,  // card_bg_white = kLightPalette.surface
    0xD9CFBA,  // border = kLightPalette.border
    0x1C2B26,  // ink = kLightPalette.text
    0x5B665F,  // muted = kLightPalette.muted
    0x7A5C25,  // teal_700 = kLightPalette.accent (brass)
    0x7A5C25,  // teal_500 = kLightPalette.accent (brass)
    0xA28C64,  // teal_300 = brass ramp stage 3 (mockup §4B)
    0xD9CFBA,  // teal_100 = kLightPalette.border (empty ramp low end)
    0xD9CFBA,  // heat_empty = kLightPalette.border
    0xFFFDF7,  // banner_cream = kLightPalette.surface
};

namespace studio_metrics {

inline constexpr float radius_banner = 16.0f;
// radius_card is ONE value: 10 (metrics::radius_card). The old studio 12
// (screenshot-derived) is collapsed — the mockup has no 12 card radius and
// a shadow value next to the canonical 10 only drifted Call sites. Studio
// sites keep spelling studio_metrics::radius_card until Phase 3-5 migrates
// them onto metrics::radius_card; the VALUE is the single 10.
inline constexpr float radius_card = 10.0f;
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
// font_helper_px stays 13: helper-text semantic, deliberately NOT the
// bumped metrics::font_body_px=14. Phase 3 will replace studio_metrics::
// font_* with metrics::font_* (mapping title/body separately); until then
// studio helper text keeps its 13px visual so no page silently reflows.
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

// Brass heat ramp over the Ivory surface (mockup §4B, Phase 2 unify).
// HeatLevel math is UNCHANGED; only the color stops changed (teal ramp
// retired). State is never color-alone: every cell also carries its
// numeric count and the legend strip, so levels 0-1 which sit near the
// surface luminance are still readable as numbers/legend entries.
//   L0 0xD9CFBA (empty)  L1 0xE4DDCD  L2 0xC3B599  L3 0xA28C64  L4 0x7A5C25
// NOTE: teal_300 (0xA28C64) equals L3 by coincidence of the ramp, NOT by
// HeatColor routing through teal fields — HeatColor owns these literals so
// a future teal-field edit cannot silently shift the heat stops.
inline std::uint32_t HeatColor(int level) {
    switch (level) {
        case 4:
            return 0x7A5C25u;
        case 3:
            return 0xA28C64u;
        case 2:
            return 0xC3B599u;
        case 1:
            return 0xE4DDCDu;
        default:
            return 0xD9CFBAu;
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
