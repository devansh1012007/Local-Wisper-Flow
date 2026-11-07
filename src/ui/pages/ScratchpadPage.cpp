#include "ui/pages/ScratchpadPage.h"

#include "ui/theme/StudioPalette.h"
#include "ui/theme/ThemeStyle.h"
#include "ui/widgets/Widgets.h"

#include "imgui.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

using lwf::ui::Color;
using lwf::ui::Pack;
using lwf::ui::kStudioPalette;
namespace studio_metrics = lwf::ui::studio_metrics;

}

ScratchpadPage::ScratchpadPage(NotesStore& notes, SettingsStore& settings, UiBus& bus)
    : notes_(notes), settings_(settings), bus_(bus) {}

bool ScratchpadPage::HasPendingUndo() const {
    return undo_.HasPending();
}

void ScratchpadPage::ClearUndo() {
    undo_.Clear();
}

std::string ScratchpadPage::ShortcutLabel() const {
    return lwf::ui::scratchpad::ShortcutDisplayLabel(shortcut_);
}

bool ScratchpadPage::IsShortcutArmed() const {
    return shortcut_armed_;
}

void ScratchpadPage::Draw(float scale) {
    const double now = static_cast<double>(ImGui::GetTime());
    if (lwf::ui::scratchpad::IsUndoExpired(undo_, now)) {
        undo_.Clear();
    }
    DrawHeader(scale);
    ImGui::Dummy(ImVec2(0.0f, 12.0f * scale));
    DrawBanner(scale);
    ImGui::Dummy(ImVec2(0.0f, 16.0f * scale));
    DrawRecents(scale);
    DrawUndoToast(scale);
}

void ScratchpadPage::DrawHeader(float scale) {
    const float title_h = 32.0f * scale;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float avail = ImGui::GetContentRegionAvail().x;
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    draw.AddText(origin, Pack(kStudioPalette.ink), "Scratchpad");
    const ImVec2 title_size = ImGui::CalcTextSize("Scratchpad");
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(origin.x + title_size.x + 10.0f * scale, origin.y + 4.0f * scale));
    lwf::ui::widgets::BadgeBeta(scale);
    const float flow_w = 52.0f * scale;
    const float pill_h = 32.0f * scale;
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    const ImVec2 flow_size = ImGui::CalcTextSize("Add to Flow Bar");
    const ImVec2 pill_size = ImGui::CalcTextSize(ShortcutLabel().c_str());
    ImGui::PopFont();
    const float pill_w = pill_size.x + 28.0f * scale;
    const float gap = 10.0f * scale;
    const float controls_w = flow_size.x + gap + flow_w + gap + pill_w;
    const float controls_x = origin.x + avail - controls_w;
    const float controls_y = origin.y;
    ImGui::SetCursorScreenPos(ImVec2(controls_x, controls_y + 4.0f * scale));
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    draw.AddText(ImVec2(controls_x, controls_y + (pill_h - flow_size.y) * 0.5f),
                 Pack(kStudioPalette.muted), "Add to Flow Bar");
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(controls_x + flow_size.x + gap, controls_y));
    const bool flow_on = settings_.GetBool("scratchpad_flow_bar");
    if (lwf::ui::widgets::ToggleSwitch("##scratchpad_flow_bar", flow_on, scale)) {
        settings_.SetBool("scratchpad_flow_bar", !flow_on);
        settings_.Save();
        bus_.PostCommand(UiCommandType::ApplySettings);
    }
    ImGui::SetCursorScreenPos(ImVec2(controls_x + flow_size.x + gap + flow_w + gap, controls_y));
    const bool pill_pressed =
        ImGui::InvisibleButton("##scratchpad_shortcut", ImVec2(pill_w, pill_h), ImGuiButtonFlags_EnableNav);
    draw.AddRectFilled(ImVec2(controls_x + flow_size.x + gap + flow_w + gap, controls_y),
                       ImVec2(controls_x + flow_size.x + gap + flow_w + gap + pill_w,
                              controls_y + pill_h),
                       Pack(kStudioPalette.card_bg), studio_metrics::radius_pill * scale);
    draw.AddRect(ImVec2(controls_x + flow_size.x + gap + flow_w + gap, controls_y),
                 ImVec2(controls_x + flow_size.x + gap + flow_w + gap + pill_w,
                        controls_y + pill_h),
                 Pack(kStudioPalette.border), studio_metrics::radius_pill * scale, 0, 1.0f * scale);
    if (ImGui::IsItemFocused()) {
        draw.AddRect(ImVec2(controls_x + flow_size.x + gap + flow_w + gap - 2.0f * scale, controls_y - 2.0f * scale),
                     ImVec2(controls_x + flow_size.x + gap + flow_w + gap + pill_w + 2.0f * scale,
                            controls_y + pill_h + 2.0f * scale),
                     Pack(kStudioPalette.teal_700), studio_metrics::radius_pill * scale, 0, 2.0f * scale);
    }
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    const std::string label = ShortcutLabel();
    const ImVec2 label_size = ImGui::CalcTextSize(label.c_str());
    draw.AddText(ImVec2(controls_x + flow_size.x + gap + flow_w + gap + (pill_w - label_size.x) * 0.5f,
                        controls_y + (pill_h - label_size.y) * 0.5f),
                 Pack(shortcut_armed_ ? kStudioPalette.teal_700 : kStudioPalette.muted), label.c_str());
    ImGui::PopFont();
    if (pill_pressed) {
        shortcut_armed_ = true;
    }
    const char* helper = shortcut_armed_ ? "Shortcut active" : "Click to enable shortcut";
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    const ImVec2 helper_size = ImGui::CalcTextSize(helper);
    draw.AddText(ImVec2(controls_x + flow_size.x + gap + flow_w + gap + pill_w - helper_size.x,
                        controls_y + pill_h + 4.0f * scale),
                 Pack(kStudioPalette.muted), helper);
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + title_h));
    ImGui::Dummy(ImVec2(0.0f, pill_h + 18.0f * scale));
}

void ScratchpadPage::DrawBanner(float scale) {
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = 196.0f * scale;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(width, height));
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    const ImVec2 bottom(origin.x + width, origin.y + height);
    draw.AddRectFilledMultiColor(origin, bottom, Pack(kStudioPalette.banner_cream),
                                 Pack(kStudioPalette.teal_100), Pack(kStudioPalette.teal_100),
                                 Pack(kStudioPalette.banner_cream));
    draw.AddRect(origin, bottom, Pack(kStudioPalette.border), studio_metrics::radius_banner * scale, 0,
                 1.0f * scale);
    ImGui::PushFont(nullptr, studio_metrics::font_banner_px);
    draw.AddText(ImVec2(origin.x + 16.0f * scale, origin.y + 12.0f * scale),
                 Pack(kStudioPalette.ink), lwf::ui::scratchpad::kBannerHeadline);
    ImGui::PopFont();
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    draw.AddText(ImVec2(origin.x + 16.0f * scale, origin.y + 60.0f * scale),
                 Pack(kStudioPalette.ink), lwf::ui::scratchpad::kBannerBody);
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(origin.x + 16.0f * scale, origin.y + height - 52.0f * scale));
    if (lwf::ui::widgets::LightButton("##start_new_note", "Start new note", scale)) {
        const auto created = notes_.Create("Untitled", "");
        if (created.has_value()) {
            bus_.PostCommand(UiCommandType::OpenScratchpad, created->id);
        }
    }
    const float mock_w = 232.0f * scale;
    const float mock_h = 104.0f * scale;
    const ImVec2 mock_pos(origin.x + width - mock_w - 16.0f * scale, origin.y + height - mock_h - 12.0f * scale);
    ImGui::SetCursorScreenPos(mock_pos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(kStudioPalette.card_bg_white));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, studio_metrics::radius_row * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f * scale, 8.0f * scale));
    ImGui::BeginChild("##mock_note", ImVec2(mock_w, mock_h),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::TextDisabled("x");
    ImGui::SameLine();
    ImGui::TextDisabled("||");
    ImGui::SameLine();
    ImGui::TextDisabled("Untitled");
    ImGui::SameLine();
    ImGui::TextDisabled("+");
    ImGui::Separator();
    const ImVec2 bar_origin = ImGui::GetCursorScreenPos();
    ImDrawList& mock_draw = *ImGui::GetWindowDrawList();
    mock_draw.AddRectFilled(ImVec2(bar_origin.x, bar_origin.y), ImVec2(bar_origin.x + 120.0f * scale, bar_origin.y + 6.0f * scale),
                            Pack(kStudioPalette.muted, 0.35f), 3.0f * scale);
    ImGui::Dummy(ImVec2(0.0f, 10.0f * scale));
    const ImVec2 bar2_origin = ImGui::GetCursorScreenPos();
    mock_draw.AddRectFilled(ImVec2(bar2_origin.x, bar2_origin.y), ImVec2(bar2_origin.x + 90.0f * scale, bar2_origin.y + 6.0f * scale),
                            Pack(kStudioPalette.muted, 0.35f), 3.0f * scale);
    ImGui::Dummy(ImVec2(0.0f, 10.0f * scale));
    ImGui::EndChild();
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + height));
}

void ScratchpadPage::DrawRecents(float scale) {
    ImGui::PushFont(nullptr, 17.0f);
    ImGui::TextUnformatted("Recents");
    ImGui::PopFont();
    ImGui::Separator();
    if (search_focus_pending_) {
        ImGui::SetKeyboardFocusHere();
        search_focus_pending_ = false;
    }
    ImGui::SetNextItemWidth(240.0f * scale);
    ImGui::InputTextWithHint("##scratch_search", "Search", search_buffer_, sizeof(search_buffer_));
    ImGui::SameLine();
    if (lwf::ui::widgets::IconButton("##scratch_search", "?", scale)) {
        search_focus_pending_ = true;
    }
    ImGui::SameLine();
    if (lwf::ui::widgets::IconButton("##scratch_new", "+", scale)) {
        const auto created = notes_.Create("Untitled", "");
        if (created.has_value()) {
            bus_.PostCommand(UiCommandType::OpenScratchpad, created->id);
        }
    }
    ImGui::SameLine();
    if (lwf::ui::widgets::IconButton("##scratch_refresh", "R", scale)) {
        notes_.Reload();
    }
    ImGui::Dummy(ImVec2(0.0f, 8.0f * scale));
    const std::vector<NotesStore::Note> notes = notes_.Search(search_buffer_);
    if (notes.empty()) {
        ImGui::PushFont(nullptr, 15.0f);
        ImGui::TextDisabled("No notes found");
        ImGui::PopFont();
        return;
    }
    for (const NotesStore::Note& note : notes) {
        DrawNoteRow(note, scale);
    }
}

void ScratchpadPage::DrawNoteRow(const NotesStore::Note& note, float scale) {
    const float row_h = 68.0f * scale;
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 row_pos = ImGui::GetCursorScreenPos();
    const std::string row_id = "##note_" + note.id;
    const bool row_pressed =
        ImGui::InvisibleButton(row_id.c_str(), ImVec2(width, row_h), ImGuiButtonFlags_EnableNav);
    const bool row_hovered = ImGui::IsItemHovered();
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    if (row_hovered) {
        draw.AddRectFilled(row_pos, ImVec2(row_pos.x + width, row_pos.y + row_h),
                           Pack(kStudioPalette.card_bg_white), studio_metrics::radius_row * scale);
    }
    if (ImGui::IsItemFocused()) {
        draw.AddRect(ImVec2(row_pos.x - 2.0f * scale, row_pos.y - 2.0f * scale),
                     ImVec2(row_pos.x + width + 2.0f * scale, row_pos.y + row_h + 2.0f * scale),
                     Pack(kStudioPalette.teal_700), studio_metrics::radius_row * scale, 0, 2.0f * scale);
    }
    ImGui::PushFont(nullptr, 15.0f);
    const std::string title = note.title.empty() ? "Untitled" : note.title;
    draw.AddText(ImVec2(row_pos.x + 14.0f * scale, row_pos.y + 8.0f * scale), Pack(kStudioPalette.ink),
                 title.c_str());
    ImGui::PopFont();
    const std::string preview = lwf::ui::scratchpad::PreviewText(note.body);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    draw.AddText(ImVec2(row_pos.x + 14.0f * scale, row_pos.y + 32.0f * scale),
                 Pack(kStudioPalette.muted), preview.c_str());
    ImGui::PopFont();
    const std::string date = lwf::ui::scratchpad::FormatModifiedDate(note.modified_at);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    const ImVec2 date_size = ImGui::CalcTextSize(date.c_str());
    draw.AddText(ImVec2(row_pos.x + width - 14.0f * scale - date_size.x, row_pos.y + 8.0f * scale),
                 Pack(kStudioPalette.muted), date.c_str());
    ImGui::PopFont();
    const float hit = 28.0f * scale;
    const ImVec2 pin_pos(row_pos.x + width - 14.0f * scale - hit - hit - 8.0f * scale, row_pos.y + 36.0f * scale);
    ImGui::SetCursorScreenPos(pin_pos);
    const std::string pin_id = "##pin_" + note.id;
    const bool pin_pressed = ImGui::InvisibleButton(pin_id.c_str(), ImVec2(hit, hit), ImGuiButtonFlags_EnableNav);
    const bool show_pin = note.pinned || row_hovered;
    if (show_pin) {
        draw.AddText(ImVec2(pin_pos.x + 6.0f * scale, pin_pos.y + 4.0f * scale),
                     Pack(note.pinned ? kStudioPalette.teal_700 : kStudioPalette.muted), "P");
    }
    bool delete_pressed = false;
    if (row_hovered) {
        const ImVec2 delete_pos(pin_pos.x + hit + 8.0f * scale, pin_pos.y);
        ImGui::SetCursorScreenPos(delete_pos);
        const std::string delete_id = "##delete_" + note.id;
        delete_pressed =
            ImGui::InvisibleButton(delete_id.c_str(), ImVec2(hit, hit), ImGuiButtonFlags_EnableNav);
        draw.AddText(ImVec2(delete_pos.x + 6.0f * scale, delete_pos.y + 4.0f * scale),
                     Pack(kStudioPalette.muted), "x");
    }
    if (pin_pressed) {
        if (note.pinned) {
            notes_.Unpin(note.id);
        } else {
            notes_.Pin(note.id);
        }
    } else if (delete_pressed) {
        const auto deleted = notes_.Delete(note.id);
        if (deleted.has_value()) {
            const double now = static_cast<double>(ImGui::GetTime());
            undo_.Arm(*deleted, now);
        }
    } else if (row_pressed) {
        bus_.PostCommand(UiCommandType::OpenScratchpad, note.id);
    }
    ImGui::SetCursorScreenPos(ImVec2(row_pos.x, row_pos.y + row_h));
}

void ScratchpadPage::DrawUndoToast(float scale) {
    const double now = static_cast<double>(ImGui::GetTime());
    if (!undo_.IsActive(now)) {
        return;
    }
    const float h = 40.0f * scale;
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    const ImVec2 text_size = ImGui::CalcTextSize("Note deleted");
    ImGui::PopFont();
    const float w = text_size.x + 120.0f * scale;
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    const ImVec2 pos(viewport.x * 0.5f - w * 0.5f, viewport.y - h - 24.0f * scale);
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    draw.AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), Pack(kStudioPalette.ink),
                       studio_metrics::radius_pill * scale);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    draw.AddText(ImVec2(pos.x + 16.0f * scale, pos.y + (h - text_size.y) * 0.5f),
                 Pack(kStudioPalette.card_bg_white), "Note deleted");
    ImGui::PopFont();
    const ImVec2 saved_cursor = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(pos.x + 16.0f * scale + text_size.x + 12.0f * scale, pos.y));
    ImGui::PushStyleColor(ImGuiCol_Text, Color(kStudioPalette.teal_300));
    const bool undo_pressed = ImGui::Button("Undo##undo_delete", ImVec2(64.0f * scale, h));
    ImGui::PopStyleColor();
    ImGui::SetCursorScreenPos(saved_cursor);
    if (undo_pressed) {
        const auto deleted = undo_.Take();
        if (deleted.has_value()) {
            notes_.Restore(*deleted);
        }
    }
}
