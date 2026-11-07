#include "ui/pages/DictionaryPage.h"

#include "ui/theme/ThemePalette.h"
#include "ui/theme/ThemeStyle.h"
#include "ui/widgets/Widgets.h"

#include "imgui.h"

#include <cstddef>
#include <string>

namespace lwf::ui::pages {

namespace {

using lwf::ui::Color;
using lwf::ui::Pack;
using lwf::ui::kDarkPalette;
using lwf::ui::kStudioPalette;

void CopyToBuffer(char* dest, std::size_t size, const std::string& text) {
    if (dest == nullptr || size == 0) {
        return;
    }
    const std::size_t count = text.size() < size - 1 ? text.size() : size - 1;
    for (std::size_t i = 0; i < count; ++i) {
        dest[i] = text[i];
    }
    dest[count] = '\0';
}

std::string RowText(const DictionaryStore::Entry& entry) {
    if (entry.has_replacement && !entry.replacement.empty()) {
        return entry.phrase + " " + "→" + " " + entry.replacement;
    }
    return entry.phrase;
}

std::string RowDetail(const DictionaryStore::Entry& entry) {
    std::string detail;
    if (entry.starred) {
        detail = "Starred";
    }
    if (entry.usage_count > 0) {
        if (!detail.empty()) {
            detail += "  ";
        }
        detail += "Used " + std::to_string(entry.usage_count) +
                  (entry.usage_count == 1 ? " time" : " times");
    }
    return detail;
}

const char* SortLabel(DictionarySort sort) {
    switch (sort) {
        case DictionarySort::NameAsc:
            return "Sort: Name (A-Z)";
        case DictionarySort::Newest:
            return "Sort: Newest";
        case DictionarySort::MostUsed:
            return "Sort: Most-used";
    }
    return "Sort: Name (A-Z)";
}

}

DictionaryPage::DictionaryPage(DictionaryStore& store, PersonalDictionary* dict)
    : store_(store), dict_(dict) {}

void DictionaryPage::OpenAdd() {
    editing_id_ = 0;
    phrase_buffer_[0] = '\0';
    replacement_buffer_[0] = '\0';
    dialog_error_.clear();
    share_checked_ = false;
    phrase_focus_pending_ = true;
    dialog_.Open();
}

void DictionaryPage::OpenEdit(std::int64_t id) {
    for (const DictionaryStore::Entry& entry : store_.Entries()) {
        if (entry.id != id) {
            continue;
        }
        editing_id_ = id;
        CopyToBuffer(phrase_buffer_, sizeof(phrase_buffer_), entry.phrase);
        CopyToBuffer(replacement_buffer_, sizeof(replacement_buffer_),
                     entry.has_replacement ? entry.replacement : std::string());
        dialog_error_.clear();
        share_checked_ = false;
        phrase_focus_pending_ = true;
        dialog_.Open();
        return;
    }
}

bool DictionaryPage::AttemptSubmit() {
    std::string error;
    const bool ok = editing_id_ == 0
                        ? SubmitAdd(store_, dict_, phrase_buffer_, replacement_buffer_, &error)
                        : SubmitUpdate(store_, dict_, editing_id_, phrase_buffer_,
                                       replacement_buffer_, &error);
    if (ok) {
        dialog_error_.clear();
        phrase_buffer_[0] = '\0';
        replacement_buffer_[0] = '\0';
        editing_id_ = 0;
        return true;
    }
    dialog_error_ = error;
    return false;
}

void DictionaryPage::Draw(float scale) {
    if (!state_.load_attempted) {
        state_.load_attempted = true;
        (void)store_.Load();
        if (dict_ != nullptr && !dict_->Loaded()) {
            (void)dict_->Load(store_.Path());
        }
    }
    if (state_.pending_delete) {
        if (toast_.TakeAction()) {
            UndoPendingDelete(state_);
        } else if (!toast_.IsVisible()) {
            (void)CommitExpiredPending(store_, state_, dict_, ImGui::GetTime());
        }
    }
    DrawHeader(scale);
    DrawBanner(scale);
    DrawTabs(scale);
    DrawToolbar(scale);
    DrawRows(scale);
    DrawDialog(scale);
    toast_.Draw(scale);
}

void DictionaryPage::DrawHeader(float scale) {
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted("Dictionary");
    ImGui::PopFont();
    ImGui::SameLine(0.0f, metrics::space_m * scale);
    if (widgets::PrimaryButton("dict_add_new", "Add new", scale)) {
        OpenAdd();
    }
    // ALLOWLIST (Wave 2 T-003): 12px section gap has no same-value token; mapping to 16 would shift pixels. Value frozen.
    ImGui::Dummy(ImVec2(0.0f, 12.0f * scale));
}

void DictionaryPage::DrawTabs(float scale) {
    static const char* const kTabs[] = {"All", "Personal"};
    int selected = state_.tab == DictionaryTab::Personal ? 1 : 0;
    widgets::TabRow("##dict_tabs", kTabs, 2, &selected, scale);
    state_.tab = selected == 1 ? DictionaryTab::Personal : DictionaryTab::All;
    ImGui::SameLine(0.0f, metrics::space_s * scale);
    widgets::OutlinePill("##dict_shared_tab", "Shared with team", false, scale);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Shared with team is not available locally");
    }
    // ALLOWLIST (Wave 2 T-003): 12px section gap has no same-value token; mapping to 16 would shift pixels. Value frozen.
    ImGui::Dummy(ImVec2(0.0f, 12.0f * scale));
}

void DictionaryPage::DrawToolbar(float scale) {
    if (!dialog_.IsOpen() && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_F)) {
        search_focus_pending_ = true;
    }
    if (search_focus_pending_) {
        ImGui::SetKeyboardFocusHere();
        search_focus_pending_ = false;
    }
    ImGui::SetNextItemWidth(240.0f * scale);
    if (ImGui::InputTextWithHint("##dict_search", "Search", search_buffer_,
                                 sizeof(search_buffer_))) {
        state_.search = search_buffer_;
    }
    ImGui::SameLine(0.0f, metrics::space_s * scale);
    if (widgets::LightButton("dict_sort", SortLabel(state_.sort), scale)) {
        ImGui::OpenPopup("##dict_sort_menu");
    }
    if (ImGui::BeginPopup("##dict_sort_menu")) {
        if (ImGui::MenuItem("Name (A-Z)", nullptr, state_.sort == DictionarySort::NameAsc)) {
            state_.sort = DictionarySort::NameAsc;
        }
        if (ImGui::MenuItem("Newest", nullptr, state_.sort == DictionarySort::Newest)) {
            state_.sort = DictionarySort::Newest;
        }
        if (ImGui::MenuItem("Most-used", nullptr, state_.sort == DictionarySort::MostUsed)) {
            state_.sort = DictionarySort::MostUsed;
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine(0.0f, metrics::space_s * scale);
    if (widgets::LightButton("dict_refresh", "Refresh", scale)) {
        (void)store_.Reload();
        if (dict_ != nullptr) {
            (void)dict_->Reload();
        }
    }
    // ALLOWLIST (Wave 2 T-003): 12px section gap has no same-value token; mapping to 16 would shift pixels. Value frozen.
    ImGui::Dummy(ImVec2(0.0f, 12.0f * scale));
}

void DictionaryPage::DrawBanner(float scale) {
    if (!VisibleBanner(store_, state_)) {
        return;
    }
    bool open = true;
    widgets::Banner("##dict_banner", "Your words, spelled your way",
                    "Add the names and phrases you say. They will be spelled the way you meant.",
                    &open, scale);
    if (!open) {
        state_.banner_closed = true;
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap * scale));
        return;
    }
    const std::vector<std::string> chips = TopPhrases(store_, 5);
    if (!chips.empty()) {
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap * scale));
        for (std::size_t i = 0; i < chips.size(); ++i) {
            if (i > 0) {
                ImGui::SameLine(0.0f, metrics::space_s * scale);
            }
            const std::string chip_id = std::string("##dict_chip_") + std::to_string(i);
            if (widgets::Chip(chip_id.c_str(), chips[i].c_str(), false, scale)) {
                CopyToBuffer(search_buffer_, sizeof(search_buffer_), chips[i]);
                state_.search = chips[i];
            }
        }
    }
    ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap * scale));
}

void DictionaryPage::DrawRows(float scale) {
    const std::vector<DictionaryStore::Entry> rows = VisibleEntries(store_, state_);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(kStudioPalette.card_bg_white));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, studio_metrics::radius_row * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##dict_rows", ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    if (rows.empty()) {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        const char* empty_text = state_.search.empty() ? "No words yet." : "No matching words.";
        const ImVec2 text_size = ImGui::CalcTextSize(empty_text);
        draw.AddText(ImVec2(pos.x + 16.0f * scale,
                            pos.y + (studio_metrics::row_height * scale - text_size.y) * 0.5f),
                     Pack(kStudioPalette.muted), empty_text);
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::row_height * scale));
        ImGui::PopFont();
    } else {
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        const float width = ImGui::GetContentRegionAvail().x;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const DictionaryStore::Entry& entry = rows[i];
            if (i > 0) {
                const ImVec2 sep = ImGui::GetCursorScreenPos();
                draw.AddLine(ImVec2(sep.x, sep.y), ImVec2(sep.x + width, sep.y),
                             Pack(kStudioPalette.border), 1.0f * scale);
            }
            const float row_h = studio_metrics::row_height * scale;
            const std::string row_id = std::string("##dict_row_") + std::to_string(entry.id);
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
            row_draw.AddText(ImVec2(row_min.x + 16.0f * scale, row_min.y + 8.0f * scale),
                             Pack(kStudioPalette.ink), RowText(entry).c_str());
            ImGui::PopFont();
            const std::string detail = RowDetail(entry);
            if (!detail.empty()) {
                ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
                row_draw.AddText(ImVec2(row_min.x + 16.0f * scale, row_min.y + 30.0f * scale),
                                 Pack(kStudioPalette.muted), detail.c_str());
                ImGui::PopFont();
            }
            if (!hovered && !focused) {
                continue;
            }
            const std::string star_label = entry.starred ? "Unstar" : "Star";
            ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
            const float star_w = ImGui::CalcTextSize(star_label.c_str()).x + 32.0f * scale;
            const float edit_w = ImGui::CalcTextSize("Edit").x + 32.0f * scale;
            const float delete_w = ImGui::CalcTextSize("Delete").x + 32.0f * scale;
            ImGui::PopFont();
            const float button_gap = 8.0f * scale;
            const float total = star_w + edit_w + delete_w + 2.0f * button_gap;
            const float button_h = 40.0f * scale;
            const ImVec2 saved_cursor = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(
                ImVec2(row_max.x - total - 12.0f * scale, row_min.y + (row_h - button_h) * 0.5f));
            const std::string star_id = std::string("dict_star_") + std::to_string(entry.id);
            if (widgets::LightButton(star_id.c_str(), star_label.c_str(), scale)) {
                (void)store_.SetStarred(entry.id, !entry.starred);
                (void)store_.Save();
            }
            ImGui::SameLine(0.0f, button_gap);
            const std::string edit_id = std::string("dict_edit_") + std::to_string(entry.id);
            if (widgets::LightButton(edit_id.c_str(), "Edit", scale)) {
                OpenEdit(entry.id);
            }
            ImGui::SameLine(0.0f, button_gap);
            const std::string delete_id =
                std::string("dict_delete_") + std::to_string(entry.id);
            if (widgets::LightButton(delete_id.c_str(), "Delete", scale)) {
                if (RequestDelete(store_, state_, dict_, entry.id, ImGui::GetTime())) {
                    toast_.Show(std::string("Deleted \"") + entry.phrase + "\"", "Undo");
                }
            }
            ImGui::SetCursorScreenPos(saved_cursor);
            ImGui::Dummy(ImVec2(0.0f, 0.0f));
        }
    }
    ImGui::EndChild();
}

void DictionaryPage::DrawDialog(float scale) {
    if (!dialog_.IsOpen()) {
        return;
    }
    const char* title = editing_id_ == 0 ? "Add new word" : "Edit word";
    bool close_requested = false;
    if (dialog_.Begin(title, scale)) {
        ImGui::TextUnformatted("Word or phrase");
        if (phrase_focus_pending_) {
            ImGui::SetKeyboardFocusHere();
            phrase_focus_pending_ = false;
        }
        ImGui::SetNextItemWidth(360.0f * scale);
        const bool phrase_enter =
            ImGui::InputTextWithHint("##dict_phrase", "Word or phrase", phrase_buffer_,
                                     sizeof(phrase_buffer_),
                                     ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::TextUnformatted("Replace with");
        ImGui::SetNextItemWidth(360.0f * scale);
        const bool replacement_enter =
            ImGui::InputTextWithHint("##dict_replacement", "Replace with", replacement_buffer_,
                                     sizeof(replacement_buffer_),
                                     ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::BeginDisabled();
        (void)ImGui::Checkbox("Share with team", &share_checked_);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("Not available locally");
        if (!dialog_error_.empty()) {
            ImGui::TextColored(Color(kDarkPalette.err), "%s", dialog_error_.c_str());
        }
        if ((phrase_enter || replacement_enter) && AttemptSubmit()) {
            close_requested = true;
        }
        if (widgets::LightButton("dict_cancel", "Cancel", scale)) {
            close_requested = true;
        }
        ImGui::SameLine(0.0f, metrics::space_s * scale);
        if (widgets::PrimaryButton("dict_confirm", editing_id_ == 0 ? "Add" : "Save", scale)) {
            if (AttemptSubmit()) {
                close_requested = true;
            }
        }
        if (close_requested) {
            ImGui::CloseCurrentPopup();
        }
    }
    dialog_.End();
    if (close_requested) {
        dialog_error_.clear();
        dialog_.Close();
    }
}

}
