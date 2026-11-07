#pragma once

#include <string>
#include <vector>

namespace lwf::ui::widgets {

struct RowListItem {
    std::string title;
    std::string detail;
    std::string trailing;
};

bool PrimaryButton(const char* id, const char* label, float scale);
bool LightButton(const char* id, const char* label, float scale);
bool OutlinePill(const char* id, const char* label, bool enabled, float scale);
bool IconButton(const char* id, const char* glyph, float scale);
bool ToggleSwitch(const char* id, bool value, float scale);
void Keycap(const char* label, float scale);
bool Chip(const char* id, const char* label, bool selected, float scale);
void BadgeBeta(float scale);
void TabRow(const char* id, const char* const* tabs, int count, int* selected, float scale);

void RowList(const char* id, const std::vector<RowListItem>& rows, float scale);
void StatCard(const char* id, const char* title, const char* value, const char* subtitle,
              float width, float scale);
void RadialGauge(const char* id, float value, const char* center_label, const char* sub_label,
                 float scale);
void ProgressPill(const char* id, float value, const char* label, float scale);
void ActivityGrid(const char* id, const std::vector<int>& values, int month_max, float scale);
void Banner(const char* id, const char* headline, const char* body, bool* open, float scale);

}
