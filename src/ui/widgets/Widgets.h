#pragma once

#include "ui/theme/ThemePalette.h"

#include <string>
#include <vector>

namespace lwf::ui::widgets {

// Status square + word states (DESIGN §6: "8 px square + word, never square
// alone"). The square carries the state color from the passed palette; the
// word is always drawn beside it. Palette-role vocabulary (ok/warn/err/
// accent/muted) rather than the FSM state names, since each value maps 1:1
// to a palette token for the square fill.
enum class StatusKind { Ok, Warn, Err, Accent, Muted };

// DECISION: widgets read every color from Palette (Paper default). The
// trailing palette param defaults to kLightPalette so existing callers keep
// compiling without change; callers that already carry a Palette (Ph5 pages)
// pass it explicitly. No color is spelled as a raw hex literal here.
// AC4 32px = interactive: BadgeBeta (24) and ProgressPill (28) are static
// displays, not controls, and stay below 32 by design.

bool PrimaryButton(const char* id, const char* label, float scale,
                   const lwf::ui::Palette& palette = lwf::ui::kLightPalette);
bool LightButton(const char* id, const char* label, float scale,
                 const lwf::ui::Palette& palette = lwf::ui::kLightPalette);
bool OutlinePill(const char* id, const char* label, bool enabled, float scale,
                 const lwf::ui::Palette& palette = lwf::ui::kLightPalette);
bool IconButton(const char* id, const char* glyph, float scale,
                const lwf::ui::Palette& palette = lwf::ui::kLightPalette);
bool ToggleSwitch(const char* id, bool value, float scale,
                  const lwf::ui::Palette& palette = lwf::ui::kLightPalette);
void Keycap(const char* label, float scale,
            const lwf::ui::Palette& palette = lwf::ui::kLightPalette);
bool Chip(const char* id, const char* label, bool selected, float scale,
          const lwf::ui::Palette& palette = lwf::ui::kLightPalette);
void BadgeBeta(float scale,
               const lwf::ui::Palette& palette = lwf::ui::kLightPalette);
void TabRow(const char* id, const char* const* tabs, int count, int* selected, float scale,
            const lwf::ui::Palette& palette = lwf::ui::kLightPalette);

// Top-strip nav item (DESIGN §6): text label + 2 px accent underline when
// active. Hit box is metrics::topstrip_height tall (>= 32 per AC-G2).
bool TopStripItem(const char* id, const char* label, bool active, float scale,
                  const lwf::ui::Palette& palette = lwf::ui::kLightPalette);

// Status square + word (DESIGN §6): 8x8 square (metrics::status_square) in
// the state color, word always present. Read-only, like Keycap/BadgeBeta.
void StatusSquare(const char* id, StatusKind state, const char* word, float scale,
                  const lwf::ui::Palette& palette = lwf::ui::kLightPalette);

// Ruled ledger row (DESIGN §6): label left, control right, helper below in
// muted, 1 px hairlines top/bottom, studio_metrics::row_height tall. Row
// draws the chrome (both hairlines, label, helper), reserves the row, and
// leaves the cursor at the right control slot (control_w wide, vertically
// centered); the caller draws exactly one control — or, by exception, a
// compound slot of two inline items on the slot line (hotkey chips + Record/
// Clear buttons; model InputText + Browse) — then calls RowEnd to restore
// the cursor below the row (RowEnd uses the last item's rect). Mirrors the
// Modal/Sheet Begin/End idiom.
// control_w must be the control's actual width: an under-width value
// overflows left into the label, which is never truncated. Controls taller
// than 52 px overflow the hairline. Calling RowEnd twice or without Row
// silently teleports the cursor. id is currently unused (reserved).
void Row(const char* id, const char* label, const char* helper, float control_w, float scale,
         const lwf::ui::Palette& palette = lwf::ui::kLightPalette);
void RowEnd(float scale);

void RadialGauge(const char* id, float value, const char* center_label, const char* sub_label,
                 float scale,
                 const lwf::ui::Palette& palette = lwf::ui::kLightPalette);
void ProgressPill(const char* id, float value, const char* label, float scale,
                  const lwf::ui::Palette& palette = lwf::ui::kLightPalette);
void ActivityGrid(const char* id, const std::vector<int>& values, int month_max, float scale,
                  const lwf::ui::Palette& palette = lwf::ui::kLightPalette);
void Banner(const char* id, const char* headline, const char* body, bool* open, float scale,
            const lwf::ui::Palette& palette = lwf::ui::kLightPalette);

}
