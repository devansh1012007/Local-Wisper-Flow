#include "ui/pages/SnippetsPage.h"

#include "ui/theme/ThemePalette.h"
#include "ui/theme/ThemeStyle.h"
#include "ui/widgets/Widgets.h"

#include "imgui.h"

#include <cstddef>
#include <string>
#include <vector>

namespace lwf::ui::pages {

namespace {

using lwf::ui::Color;
using lwf::ui::Pack;
using lwf::ui::kDarkPalette;
using lwf::ui::kStudioPalette;

constexpr float kSheetWidth = 440.0f;
constexpr float kRowTextPadding = 16.0f;
constexpr float kRowButtonGap = 8.0f;
constexpr float kRowActionReserve =
    kRowButtonGap * 2.0f + studio_metrics::hit_size * 2.0f + kRowTextPadding;

std::string TruncateToWidth(const std::string& text, float max_width, float scale) {
    if (max_width <= 0.0f) {
        return "...";
    }
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    const float full_w = ImGui::CalcTextSize(text.c_str()).x;
    if (full_w <= max_width) {
        ImGui::PopFont();
        return text;
    }
    const float ellipsis_w = ImGui::CalcTextSize("...").x;
    const float budget = max_width - ellipsis_w;
    if (budget <= 0.0f) {
        ImGui::PopFont();
        return "...";
    }
    std::vector<std::size_t> code_point_ends;
    code_point_ends.reserve(64);
    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char u = static_cast<unsigned char>(text[i]);
        if ((u & 0xC0) != 0x80) {
            code_point_ends.push_back(i + 1);
        }
    }
    if (!code_point_ends.empty() && code_point_ends.back() != text.size()) {
        code_point_ends.push_back(text.size());
    }
    std::size_t best_bytes = 0;
    if (!code_point_ends.empty()) {
        std::size_t lo = 0;
        std::size_t hi = code_point_ends.size();
        while (lo < hi) {
            const std::size_t mid = lo + (hi - lo) / 2;
            const std::size_t bytes = code_point_ends[mid];
            const float w = ImGui::CalcTextSize(text.data(), text.data() + bytes).x;
            if (w <= budget) {
                best_bytes = bytes;
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
    }
    (void)scale;
    ImGui::PopFont();
    return text.substr(0, best_bytes) + "...";
}

}

bool SnippetsPage::DeleteEdit() {
    if (!IsEditing() || editing_id_.empty()) {
        return false;
    }
    if (!RequestDelete(store_, state_, editing_id_, ImGui::GetTime())) {
        return false;
    }
    toast_.Show(std::string("Deleted \"") + state_.pending_entry.trigger + "\"",
                kSnippetsUndoAction);
    CloseEditor();
    return true;
}

void SnippetsPage::Draw(float scale) {
    if (state_.pending_delete) {
        if (toast_.TakeAction()) {
            std::string undo_error;
            (void)UndoPendingDelete(store_, state_, &undo_error);
        } else if (!toast_.IsVisible()) {
            (void)CommitExpiredPending(state_, ImGui::GetTime());
        }
    }
    DrawHeader(scale);
    DrawBanner(scale);
    DrawRows(scale);
    DrawSheet(scale);
    toast_.Draw(scale);
}

void SnippetsPage::DrawHeader(float scale) {
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted("Snippets");
    ImGui::PopFont();
    ImGui::SameLine(0.0f, metrics::space_m * scale);
    if (widgets::LightButton("snippets_add_new", kSnippetsAddLabel, scale)) {
        OpenAdd();
    }
    // ALLOWLIST (Wave 2 T-003): 12px section gap has no same-value token; mapping to 16 would shift pixels. Value frozen.
    ImGui::Dummy(ImVec2(0.0f, 12.0f * scale));
}

void SnippetsPage::DrawBanner(float scale) {
    if (!VisibleBanner(state_)) {
        return;
    }
    bool open = true;
    widgets::Banner("##snippets_banner", kSnippetsBannerHeadline, "", &open, scale);
    if (!open) {
        state_.banner_closed = true;
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap * scale));
        return;
    }
    ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap * scale));
    (void)widgets::Chip("##snippets_chip_trigger", kSnippetsBannerExampleTrigger, false, scale);
    ImGui::SameLine(0.0f, metrics::space_s * scale);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::TextUnformatted("→");
    ImGui::PopFont();
    ImGui::SameLine(0.0f, metrics::space_s * scale);
    (void)widgets::Chip("##snippets_chip_expansion", kSnippetsBannerExampleExpansion, false, scale);
    ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap * scale));
}

void SnippetsPage::DrawRows(float scale) {
    const std::vector<SnippetStore::Entry> rows = VisibleEntries(store_, state_);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(kStudioPalette.card_bg_white));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, studio_metrics::radius_row * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##snippets_rows", ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    if (rows.empty()) {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        const ImVec2 text_size = ImGui::CalcTextSize(kSnippetsEmptyCopy);
        draw.AddText(ImVec2(pos.x + kRowTextPadding * scale,
                            pos.y + (studio_metrics::row_height * scale - text_size.y) * 0.5f),
                     Pack(kStudioPalette.muted), kSnippetsEmptyCopy);
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::row_height * scale));
        ImGui::PopFont();
    } else {
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        const float width = ImGui::GetContentRegionAvail().x;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const SnippetStore::Entry& entry = rows[i];
            if (i > 0) {
                const ImVec2 sep = ImGui::GetCursorScreenPos();
                draw.AddLine(ImVec2(sep.x, sep.y), ImVec2(sep.x + width, sep.y),
                             Pack(kStudioPalette.border), 1.0f * scale);
            }
            const float row_h = studio_metrics::row_height * scale;
            const std::string row_id = std::string("##snippets_row_") + entry.id;
            const bool pressed = ImGui::InvisibleButton(
                row_id.c_str(), ImVec2(0.0f, row_h),
                ImGuiButtonFlags_EnableNav | ImGuiButtonFlags_AllowOverlap);
            const ImVec2 row_min = ImGui::GetItemRectMin();
            const ImVec2 row_max = ImGui::GetItemRectMax();
            const bool hovered = ImGui::IsItemHovered();
            const bool focused = ImGui::IsItemFocused();
            if (pressed) {
                OpenEdit(entry.id);
            }
            if (hovered) {
                draw.AddRectFilled(row_min, row_max, Pack(kStudioPalette.teal_100, 0.35f),
                                   studio_metrics::radius_row * scale);
            }
            if (focused) {
                draw.AddRect(ImVec2(row_min.x - 2.0f * scale, row_min.y - 2.0f * scale),
                             ImVec2(row_max.x + 2.0f * scale, row_max.y + 2.0f * scale),
                             Pack(kStudioPalette.teal_700), 4.0f * scale, 0, 2.0f * scale);
            }
            ImGui::PushFont(nullptr, metrics::font_card_title_px);
            ImDrawList& row_draw = *ImGui::GetWindowDrawList();
            const std::string shown = TruncateToWidth(RowText(entry), width - kRowActionReserve * scale,
                                                      scale);
            row_draw.AddText(ImVec2(row_min.x + kRowTextPadding * scale,
                                    row_min.y + (row_h - ImGui::CalcTextSize(shown.c_str()).y) * 0.5f),
                             Pack(kStudioPalette.ink), shown.c_str());
            ImGui::PopFont();
            if (!hovered && !focused) {
                continue;
            }
            const float hit = studio_metrics::hit_size * scale;
            const ImVec2 saved_cursor = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(ImVec2(row_max.x - hit * 2.0f - kRowButtonGap * scale - 12.0f * scale,
                                             row_min.y + (row_h - hit) * 0.5f));
            const std::string edit_id = std::string("snippets_edit_") + entry.id;
            if (widgets::IconButton(edit_id.c_str(), "✎", scale)) {
                OpenEdit(entry.id);
            }
            ImGui::SameLine(0.0f, kRowButtonGap * scale);
            const std::string delete_id = std::string("snippets_delete_") + entry.id;
            if (widgets::IconButton(delete_id.c_str(), "x", scale)) {
                if (RequestDelete(store_, state_, entry.id, ImGui::GetTime())) {
                    toast_.Show(std::string("Deleted \"") + entry.trigger + "\"",
                                kSnippetsUndoAction);
                }
            }
            ImGui::SetCursorScreenPos(saved_cursor);
            ImGui::Dummy(ImVec2(0.0f, 0.0f));
        }
    }
    ImGui::EndChild();
}

void SnippetsPage::DrawSheet(float scale) {
    if (!editing_) {
        editor_sheet_.Close();
        return;
    }
    if (!editor_sheet_.IsOpen()) {
        editor_sheet_.Open();
    }
    const char* title = editing_id_.empty() ? kSnippetsEditorAddTitle : kSnippetsEditorEditTitle;
    if (editor_sheet_.Begin(title, kSheetWidth, scale)) {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted(title);
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, metrics::space_s * scale));
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Trigger");
        ImGui::PopFont();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
        const bool trigger_enter =
            ImGui::InputTextWithHint("##snippets_trigger", "What to say", trigger_buffer_,
                                     sizeof(trigger_buffer_),
                                     ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        RefreshFromBuffers();
        ImGui::Dummy(ImVec2(0.0f, metrics::space_s * scale));
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Expansion");
        ImGui::PopFont();
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        const float line_height = ImGui::GetTextLineHeight();
        ImGui::PopFont();
        ImGui::InputTextMultiline("##snippets_expansion", expansion_buffer_,
                                  sizeof(expansion_buffer_),
                                  ImVec2(ImGui::GetContentRegionAvail().x,
                                         line_height * static_cast<float>(kSnippetsEditorVisibleLines)),
                                  ImGuiInputTextFlags_None);
        RefreshFromBuffers();
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        const std::string counter =
            std::to_string(CountCodePoints(edit_expansion_)) + " / " +
            std::to_string(SnippetStore::kMaxExpansionChars);
        ImGui::TextColored(Color(kStudioPalette.muted), "%s", counter.c_str());
        ImGui::PopFont();
        if (!edit_error_.empty()) {
            ImGui::Dummy(ImVec2(0.0f, metrics::space_s * scale));
            ImGui::PushFont(nullptr, metrics::font_card_title_px);
            ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 24.0f * scale);
            ImGui::TextColored(Color(kDarkPalette.err), "%s", edit_error_.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopFont();
        }
        // ALLOWLIST (Wave 2 T-003): 12px section gap has no same-value token; mapping to 16 would shift pixels. Value frozen.
        ImGui::Dummy(ImVec2(0.0f, 12.0f * scale));
        if (widgets::LightButton("snippets_cancel", "Cancel", scale)) {
            CloseEditor();
        }
        ImGui::SameLine(0.0f, metrics::space_s * scale);
        if (!editing_id_.empty()) {
            if (widgets::LightButton("snippets_sheet_delete", "Delete", scale)) {
                DeleteEdit();
            }
            ImGui::SameLine(0.0f, metrics::space_s * scale);
        }
        if (widgets::PrimaryButton("snippets_save", "Save", scale)) {
            if (SaveEdit()) {
                toast_.Show("Saved", "");
            }
        }
        if (trigger_enter) {
            if (SaveEdit()) {
                toast_.Show("Saved", "");
            }
        }
        editor_sheet_.End();
    }
    if (editing_ && !editor_sheet_.IsOpen()) {
        CloseEditor();
    }
}

}
