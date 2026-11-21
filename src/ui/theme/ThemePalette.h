#pragma once

// Palette and metrics for the Hairline GUI (redesign step R1 + Phase 2
// token unify, 2026-11-08; Hairline token swap T2, 2026-11-09).
//
// Source of truth: docs/design/DESIGN.md (Hairline spec, T1 signed off
// 2026-11-09). Do not change values here without regenerating the
// contrast tables in DESIGN.md. Two palettes, ONE accent per theme:
//   - Graphite (kDarkPalette) is the runtime default. Paper
//     (kLightPalette) is fully supported via the theme switcher. Both
//     themes are defined even though the app starts in dark; both are
//     covered by the unit tests with the same exactness.
//
// Contrast policy (from DESIGN.md §2, computed with the WCAG relative-luminance
// formula): every TEXT pair passes 4.5:1 — text/muted/accent/ok/warn/err/
// brass/on_accent on their fills — and every meaningful non-text pair
// passes 3:1 (focus ring, err_fill). EXCEPTIONS: the two decorative
// hairline pairs (border on bg, border on surface) sit below 3:1 in both
// themes. They separate surfaces that already differ in tone and never
// carry state on their own; every state is also shown in text or shape.
// These two pairs are exempt from the contrast test.
// err_fill is FILL-ONLY, never text: assert it at >=3.0, never >=4.5 as
// text. A dot with that fill must always pair with a visible word/shape
// so no state is carried by color alone (DESIGN §2).
// tests/test_overlay_settings_ui.cpp enforces this policy on this header.
//
// This header is deliberately free of Dear ImGui so the tokens can be unit
// tested from lwf_tests without a windowing context. The ImGui style mapping
// lives in ThemePalette.cpp.

#include <cstdint>
#include <cmath>
#include <string>

namespace lwf::ui {

// 0xRRGGBB packed colors.
struct Palette {
    std::uint32_t bg;        // window background
    std::uint32_t surface;   // cards, rail, mini badge
    std::uint32_t border;    // 1 px hairlines
    std::uint32_t text;      // primary text
    std::uint32_t muted;     // secondary text
    std::uint32_t accent;    // active nav, primary accent (brass in both themes)
    std::uint32_t brass;     // brass as text and icons (matches accent)
    std::uint32_t on_accent; // text on an accent fill (derived: bg in dark, surface in light)
    std::uint32_t ok;        // Ready status
    std::uint32_t warn;      // "applies after restart" notes
    std::uint32_t err;       // error copy
    std::uint32_t err_fill;  // attention dot fill
    std::uint32_t focus;     // focus ring, 2 px
    float surface_alpha;     // mini badge opacity over the desktop (0.88 dark, 0.90 light)
};

// Dark theme, "Graphite" (Hairline runtime default, DESIGN.md §3.1).
inline constexpr Palette kDarkPalette{
    0x101214,  // bg
    0x17191C,  // surface
    0x2A2D31,  // border
    0xE8E9EA,  // text
    0x9A9DA1,  // muted
    0xB8E356,  // accent (lime A)
    0xB8E356,  // brass
    0x101214,  // on_accent
    0x8FD1A0,  // ok
    0xE0B35A,  // warn
    0xF08A8F,  // err
    0xC65A61,  // err_fill
    0xB8E356,  // focus
    0.88f,     // surface_alpha
};

// Canonical theme key values (src/core/SettingsStore.cpp `theme` schema
// default "graphite", T2b). Single source so no call site hard-codes the
// strings.
inline constexpr const char* kThemeGraphite = "graphite";
inline constexpr const char* kThemePaper = "paper";

// Light theme, "Paper" (DESIGN.md §3.2).
// Single-accent invariant: accent == brass == focus == 0x3F6212. Hairline
// accent A is per-theme derived (dark 0xB8E356, light 0x3F6212), so no
// light-theme surface, rail, focus ring or text accent may fall back to
// a single shared hex. Dark keeps its own lime 0xB8E356 (different
// value, same role).
inline constexpr Palette kLightPalette{
    0xF7F7F5,  // bg
    0xFFFFFF,  // surface
    0xE2E2DF,  // border
    0x1A1A18,  // text
    0x6B6B66,  // muted
    0x3F6212,  // accent (lime A light)
    0x3F6212,  // brass
    0xFFFFFF,  // on_accent
    0x2F6B45,  // ok
    0x7A5500,  // warn
    0xA61E28,  // err
    0xA61E28,  // err_fill
    0x3F6212,  // focus
    0.90f,     // surface_alpha
};

// Selects the runtime palette for a `theme` config value (T2b).
// INVARIANT: unknown/empty falls back to kDarkPalette (Graphite), matching
// the schema default "graphite" — only an exact "paper" opts into
// kLightPalette. Returns a reference to a constexpr object so callers can
// compare by pointer identity to detect an actual change.
inline const Palette& PaletteForTheme(const std::string& theme) {
    return theme == kThemePaper ? kLightPalette : kDarkPalette;
}

// Layout metrics from DESIGN.md §3.3/§3.4. Values are design pixels at
// 1.0 scale; callers multiply by the window DPI scale. Style is applied
// once per actual change (init or live content-scale move).
namespace metrics {

inline constexpr float space_xs = 4.0f;   // hairline gaps inside rows
inline constexpr float space_s = 8.0f;    // inside cards
inline constexpr float space_m = 16.0f;   // card padding, rail gaps
inline constexpr float space_l = 24.0f;   // between groups
inline constexpr float space_xl = 32.0f;  // page padding (vertical)

// Followups D8: the page's left/right gutter. Wider than space_xl so page
// content breathes against the window edge; the top strip uses the same
// token so the wordmark aligns with the page content below it. Vertical
// page padding stays space_xl.
inline constexpr float page_gutter = 48.0f;

inline constexpr float radius_control = 6.0f;   // buttons, inputs, chips
inline constexpr float radius_card = 10.0f;     // cards, windows
inline constexpr float radius_pill = 999.0f;    // dot, pill, switches

inline constexpr float content_max = 640.0f;     // widest page content, left aligned
inline constexpr float control_min_height = 32.0f;  // controls are at least 32 px tall
inline constexpr float topstrip_height = 48.0f;   // Hairline top-strip height (DESIGN §3.3)
inline constexpr float status_square = 8.0f;      // Hairline status square 8x8 (DESIGN §3.3)

// Font sizes in design pixels (Hairline type scale, DESIGN.md §3.4:
// title 24 / section 14 / body 13 / helper 12). Values are
// design px at 1.0 scale; callers apply typography through scoped fonts
// (ImGui::PushFont(nullptr, design_px) + ImGui::PopFont) and the window
// DPI factor once via style.FontScaleMain in ApplyImGuiStyle.
inline constexpr float font_wordmark_px = 24.0f;   // wordmark "Echo flow" (title)
inline constexpr float font_page_title_px = 24.0f; // page title (title)
inline constexpr float font_card_title_px = 14.0f; // card title (section)
inline constexpr float font_body_px = 13.0f;       // body text (body)
inline constexpr float font_mini_text_px = 12.0f;  // tiny status label font (helper)

// Full window (native title bar on top of these).
inline constexpr float full_default_w = 960.0f;
inline constexpr float full_default_h = 640.0f;
inline constexpr float full_min_w = 760.0f;
inline constexpr float full_min_h = 520.0f;

// Mini badge. Quarter-size dot with a tiny status label under it. The
// window hugs the icons (followups D6): the normal extent is the dot and
// the status strip, each with ring_pad clearance for the listening ring;
// the attention state expands the window to the message reserve below the
// dot. The badge stays semi-transparent so it never blocks the desktop
// behind it.
inline constexpr float mini_dot = 22.0f;             // was 44: 1/4 of the area
inline constexpr float mini_ring_pad = 8.0f;   // clearance around the dot (listening ring)
inline constexpr float mini_status_h = 18.0f;  // status word strip under the dot
inline constexpr float mini_message_w = 250.0f;      // attention expansion width
inline constexpr float mini_message_h = 48.0f;       // attention message reserve under the dot
inline constexpr float mini_margin = 16.0f;  // above the taskbar
inline constexpr float mini_hover_inflate = 10.0f;  // hit zone around the dot
inline constexpr float mini_idle_alpha = 0.55f;     // badge opacity when idle (mockup: 55%)
inline constexpr float mini_active_alpha = 1.0f;    // badge opacity when hovered, dragged or busy (mockup: 100%)

// Motion (seconds). All animations stop at a static pose when
// reduce_motion is on.
inline constexpr float fade_in = 0.15f;        // mini fade in (functional motion band; was 0.12)
inline constexpr float ring_breathe = 2.4f;    // listening ring period
inline constexpr float bar_cycle = 1.2f;       // level bar wave period
inline constexpr float think_dot_cycle = 0.9f; // thinking dots period
inline constexpr float think_dot_offset = 0.3f;
inline constexpr float done_dwell = 0.6f;      // Done check before Ready
inline constexpr float attention_dwell = 6.0f; // attention auto-clear
inline constexpr float saved_dwell = 1.5f;     // "Saved" indicator on pages (mockup motion table: 1.5 s)

}  // namespace metrics

// WCAG relative luminance and contrast ratio, kept next to the tokens so the
// unit tests enforce the exact policy DESIGN.md documents. The two
// decorative border pairs are the only allowed sub-3:1 combinations.
inline double RelativeLuminance(std::uint32_t rgb) {
    auto channel = [](std::uint32_t v) {
        const double c = static_cast<double>(v & 0xFFu) / 255.0;
        return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    };
    const double r = channel((rgb >> 16) & 0xFFu);
    const double g = channel((rgb >> 8) & 0xFFu);
    const double b = channel(rgb & 0xFFu);
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

inline double ContrastRatio(std::uint32_t a, std::uint32_t b) {
    const double la = RelativeLuminance(a);
    const double lb = RelativeLuminance(b);
    const double hi = la > lb ? la : lb;
    const double lo = la > lb ? lb : la;
    return (hi + 0.05) / (lo + 0.05);
}

// Studio rhythm/typography tokens, migrated verbatim from the deleted
// shim header (single-palette unify; names kept so call sites and the
// `studio_metrics::` source pins stay valid — header stays ImGui-free).
namespace studio_metrics {

inline constexpr float radius_banner = 16.0f;
// radius_card is ONE value: 10 (metrics::radius_card). The old studio 12
// (screenshot-derived) is collapsed — the mockup has no 12 card radius and
// a shadow value next to the canonical 10 only drifted call sites.
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

inline constexpr float font_banner_px = 24.0f;    // Hairline title (DESIGN §3.4)
inline constexpr float font_caps_px = 12.0f;      // Hairline helper (DESIGN §3.4)
inline constexpr float font_page_title_px = 24.0f;  // Hairline title (DESIGN §3.4)
// Hairline helper is 12 (DESIGN §3.4); metrics::font_body_px is body 13.
inline constexpr float font_helper_px = 12.0f;
// Wave 2 T-003 additions (same note as the gaps above: screenshot-derived,
// not mockup). font_card_title_px (14) is the studio twin of
// metrics::font_card_title_px: same Hairline section value (DESIGN §3.4),
// not by mockup derivation. font_heading_px (24) also serves the IconButton glyph
// at the same optical size (32px hit box); split it into a separate glyph
// token only if glyph sizing ever diverges from headings.
inline constexpr float font_card_title_px = 14.0f;
inline constexpr float font_heading_px = 24.0f;
inline constexpr float font_subheading_px = 14.0f;
inline constexpr float font_glyph_px = 10.0f;

inline constexpr int toast_dwell_ms = 5000;
inline constexpr float toast_dwell_s = toast_dwell_ms / 1000.0f;

inline constexpr float activity_cell = 14.0f;
inline constexpr float activity_gap = 4.0f;
inline constexpr float activity_corner = 3.0f;
inline constexpr float gauge_stroke = 14.0f;
inline constexpr float progress_height = 28.0f;

}  // namespace studio_metrics

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

// Lime heat ramp over the Paper surface (Hairline DESIGN §10, re-derived
// for T2). HeatLevel math is UNCHANGED; only the color stops changed
// (brass ramp retired). State is never color-alone: every cell also carries
// its numeric count and the legend strip, so levels 0-1 which sit near the
// surface luminance are still readable as numbers/legend entries.
//   L0 0xE2E2DF (empty)  L1 0xC9CCC4  L2 0xABB1A4  L3 0x839178  L4 0x3F6212
// Re-derivation: uniform interpolation border 0xE2E2DF -> accent 0x3F6212
// in linear-light RGB at t=0, 0.25, 0.5, 0.75, 1.0 (half-up rounding).
// sRGB-linear at t=0.75 would give 0x688245 (vs text 4.04, FAIL 4.5);
// linear-light gives L3 0x839178 (vs text 5.22, vs surface 3.34, both
// pass) and keeps L0==border, L4==accent, L3/L4 >=3.0 vs surface.
// NOTE: HeatColor owns these literals so a future palette edit cannot
// silently shift the heat stops.
inline std::uint32_t HeatColor(int level) {
    switch (level) {
        case 4:
            return 0x3F6212u;
        case 3:
            return 0x839178u;
        case 2:
            return 0xABB1A4u;
        case 1:
            return 0xC9CCC4u;
        default:
            return 0xE2E2DFu;
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

// Applies the palette and the window DPI scale to the current ImGui context's
// style. Scale is the window DPI scale (1.0 = 100%). Display typography is not
// part of the style: it is scoped per call site with ImGui::PushFont(nullptr,
// design_px) + ImGui::PopFont using the metrics::font_*_px design tokens, so
// the DPI factor (style.FontScaleMain) applies exactly once. The scale is
// applied once per actual change (init or live content-scale move); re-applying
// an unchanged scale does not re-run the metrics scaling. Real fonts land
// with the font-approval change.
void ApplyImGuiStyle(const Palette& palette, float scale);

}  // namespace lwf::ui
