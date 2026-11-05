#pragma once

// Palette and metrics for the redesigned GUI (redesign step R1).
//
// Source of truth: docs/design/mockup.html (D0). Do not change values here
// without regenerating the contrast tables in the mockup. Both themes are
// defined even though the app starts in dark; the light theme is part of the
// token system and is covered by the unit tests.
//
// Contrast policy (from the mockup, computed with the WCAG relative-luminance
// formula): every text pair passes 4.5:1 and every meaningful color pair
// passes 3:1, EXCEPT the two decorative hairline pairs (border on bg, border
// on surface) which sit below 3:1 in both themes. They separate surfaces that
// already differ in tone and never carry state on their own; every state is
// also shown in text or shape. These two pairs are exempt from the contrast
// test. tests/test_overlay_settings_ui.cpp enforces the policy on this header.
//
// This header is deliberately free of Dear ImGui so the tokens can be unit
// tested from lwf_tests without a windowing context. The ImGui style mapping
// lives in ThemePalette.cpp.

#include <cstdint>
#include <cmath>

namespace lwf::ui {

// 0xRRGGBB packed colors.
struct Palette {
    std::uint32_t bg;        // window background
    std::uint32_t surface;   // cards, rail, mini badge
    std::uint32_t border;    // 1 px hairlines
    std::uint32_t text;      // primary text
    std::uint32_t muted;     // secondary text
    std::uint32_t accent;    // active nav, primary accent (brass dark / navy light)
    std::uint32_t brass;     // brass as text and icons (matches accent in dark)
    std::uint32_t on_accent; // text on an accent fill (derived: bg in dark, surface in light)
    std::uint32_t ok;        // Ready status
    std::uint32_t warn;      // "applies after restart" notes
    std::uint32_t err;       // error copy
    std::uint32_t err_fill;  // attention dot fill
    std::uint32_t focus;     // focus ring, 2 px
    float surface_alpha;     // mini badge opacity over the desktop (0.88 dark, 0.90 light)
};

// Dark theme, "Hunter and Brass".
inline constexpr Palette kDarkPalette{
    0x1A2A24,  // bg
    0x455A50,  // surface
    0x5A7068,  // border
    0xEDE6D6,  // text
    0xA8B3AA,  // muted
    0xC9A96E,  // accent (brass)
    0xC9A96E,  // brass
    0x1A2A24,  // on_accent
    0x8FB39A,  // ok
    0xD0A65A,  // warn
    0xD98A8F,  // err
    0xB5565F,  // err_fill
    0xC9A96E,  // focus
    0.88f,     // surface_alpha
};

// Light theme, "Ivory and Navy".
inline constexpr Palette kLightPalette{
    0xF6F1E4,  // bg
    0xFFFDF7,  // surface
    0xD9CFBA,  // border
    0x1C2B26,  // text
    0x5B665F,  // muted
    0x1F3A5F,  // accent (navy)
    0x7A5C25,  // brass
    0xFFFDF7,  // on_accent
    0x3F6B52,  // ok
    0x7A5C25,  // warn
    0x7A2E3A,  // err
    0x7A2E3A,  // err_fill
    0x1F3A5F,  // focus
    0.90f,     // surface_alpha
};

// Layout metrics from the mockup, section 5. Values are design pixels at
// 1.0 scale; callers multiply by the window DPI scale.
namespace metrics {

inline constexpr float space_xs = 4.0f;   // hairline gaps inside rows
inline constexpr float space_s = 8.0f;    // inside cards
inline constexpr float space_m = 16.0f;   // card padding, rail gaps
inline constexpr float space_l = 24.0f;   // between groups
inline constexpr float space_xl = 32.0f;  // page padding

inline constexpr float radius_control = 6.0f;   // buttons, inputs, chips
inline constexpr float radius_card = 10.0f;     // cards, windows
inline constexpr float radius_pill = 999.0f;    // dot, pill, switches

inline constexpr float rail_width = 208.0f;      // Full window navigation rail
inline constexpr float content_max = 640.0f;     // widest page content, left aligned
inline constexpr float control_min_height = 32.0f;  // controls are at least 32 px tall

// Full window (native title bar on top of these).
inline constexpr float full_default_w = 960.0f;
inline constexpr float full_default_h = 640.0f;
inline constexpr float full_min_w = 760.0f;
inline constexpr float full_min_h = 520.0f;

// Mini badge. Quarter-size dot with a tiny status label under it; the window
// still reserves room below the dot for the attention hover message. The
// badge stays highly transparent so it never blocks the desktop behind it.
inline constexpr float mini_dot = 22.0f;             // was 44: 1/4 of the area
inline constexpr float mini_message_w = 140.0f;      // was 250
inline constexpr float mini_message_h = 48.0f;       // was 96
inline constexpr float mini_margin = 16.0f;  // above the taskbar
inline constexpr float mini_hover_inflate = 10.0f;  // hit zone around the dot
inline constexpr float mini_text_px = 8.5f;         // tiny status label font
inline constexpr float mini_idle_alpha = 0.35f;     // badge opacity when idle
inline constexpr float mini_active_alpha = 0.65f;   // badge opacity when hovered, dragged or busy

// Motion (seconds). All animations stop at a static pose when
// reduce_motion is on.
inline constexpr float fade_in = 0.12f;        // mini fade in
inline constexpr float ring_breathe = 2.4f;    // listening ring period
inline constexpr float bar_cycle = 1.2f;       // level bar wave period
inline constexpr float think_dot_cycle = 0.9f; // thinking dots period
inline constexpr float think_dot_offset = 0.3f;
inline constexpr float done_dwell = 0.6f;      // Done check before Ready
inline constexpr float attention_dwell = 6.0f; // attention auto-clear
inline constexpr float saved_dwell = 1.6f;     // "Saved" indicator on pages

}  // namespace metrics

// WCAG relative luminance and contrast ratio, kept next to the tokens so the
// unit tests enforce the exact policy the mockup documents. The two
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

// Applies the palette to the current ImGui context's style. Scale is the
// window DPI scale (1.0 = 100%); metrics and the default font are scaled by
// it. Real fonts land with the font-approval change; until then the built-in
// font is scaled, which is deliberate and temporary.
void ApplyImGuiStyle(const Palette& palette, float scale);

}  // namespace lwf::ui
