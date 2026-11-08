#pragma once

#include "ui/theme/ThemePalette.h"

#include <string>
#include <vector>

namespace lwf::ui::widgets {

struct RowListItem {
    std::string title;
    std::string detail;
    std::string trailing;
};

// DECISION: widgets read every color from Palette (Ivory primary). The
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

void RowList(const char* id, const std::vector<RowListItem>& rows, float scale,
             const lwf::ui::Palette& palette = lwf::ui::kLightPalette);
void StatCard(const char* id, const char* title, const char* value, const char* subtitle,
              float width, float scale,
              const lwf::ui::Palette& palette = lwf::ui::kLightPalette);
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
