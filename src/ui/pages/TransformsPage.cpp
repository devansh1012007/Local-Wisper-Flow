#include "ui/pages/TransformsPage.h"

#include "core/DictationEngine.h"
#include "ui/theme/StudioPalette.h"
#include "ui/theme/ThemePalette.h"
#include "ui/theme/ThemeStyle.h"
#include "ui/widgets/Widgets.h"

#include "imgui.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace lwf::ui {

EngineDiffSource::EngineDiffSource(DictationEngine* engine) : engine_(engine) {}

bool EngineDiffSource::HasTransformDiff() const {
    return engine_ != nullptr && engine_->HasTransformDiff();
}

lwf::TransformDiff EngineDiffSource::TransformDiffSnapshot() const {
    if (engine_ == nullptr) {
        return {};
    }
    return engine_->TransformDiffSnapshot();
}

namespace {

using lwf::ui::Color;
using lwf::ui::kLightPalette;
using lwf::ui::Pack;

constexpr float kCardPadding = 20.0f;
constexpr float kCardGap = 16.0f;
constexpr float kCardHeight = 172.0f;
constexpr float kLinkUnderlineGap = 2.0f;

std::string KeyLabel(ImGuiKey key) {
    if (key >= ImGuiKey_A && key <= ImGuiKey_Z) {
        return std::string(1, static_cast<char>('A' + (key - ImGuiKey_A)));
    }
    if (key >= ImGuiKey_0 && key <= ImGuiKey_9) {
        return std::string(1, static_cast<char>('0' + (key - ImGuiKey_0)));
    }
    if (key >= ImGuiKey_F1 && key <= ImGuiKey_F12) {
        return "F" + std::to_string(static_cast<int>(key - ImGuiKey_F1) + 1);
    }
    if (key >= ImGuiKey_Keypad0 && key <= ImGuiKey_Keypad9) {
        return std::string(1, static_cast<char>('0' + (key - ImGuiKey_Keypad0)));
    }
    switch (key) {
        case ImGuiKey_Space:
            return "Space";
        case ImGuiKey_Tab:
            return "Tab";
        case ImGuiKey_Insert:
            return "Insert";
        case ImGuiKey_Delete:
            return "Delete";
        case ImGuiKey_Home:
            return "Home";
        case ImGuiKey_End:
            return "End";
        case ImGuiKey_PageUp:
            return "PageUp";
        case ImGuiKey_PageDown:
            return "PageDown";
        case ImGuiKey_Backspace:
            return "Backspace";
        case ImGuiKey_Enter:
            return "Enter";
        case ImGuiKey_KeypadEnter:
            return "Enter";
        case ImGuiKey_Escape:
        case ImGuiKey_LeftCtrl:
        case ImGuiKey_RightCtrl:
        case ImGuiKey_LeftShift:
        case ImGuiKey_RightShift:
        case ImGuiKey_LeftAlt:
        case ImGuiKey_RightAlt:
        case ImGuiKey_LeftSuper:
        case ImGuiKey_RightSuper:
        default:
            return {};
    }
}

void DrawKeycapRow(const std::vector<std::string>& tokens, float scale) {
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) {
            ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
        }
        widgets::Keycap(tokens[i].c_str(), scale);
    }
}

float KeycapRowWidth(const std::vector<std::string>& tokens, float scale) {
    float width = 0.0f;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) {
            width += studio_metrics::gap_inline * scale;
        }
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        const float text_w = ImGui::CalcTextSize(tokens[i].c_str()).x;
        ImGui::PopFont();
        width += text_w + 24.0f * scale;
    }
    return width;
}

bool TextLink(const char* id, const char* label, float scale) {
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    const ImVec2 text_size = ImGui::CalcTextSize(label);
    ImGui::PopFont();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, text_size, ImGuiButtonFlags_EnableNav);
    const bool hovered = ImGui::IsItemHovered();
    const bool focused = ImGui::IsItemFocused();
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    draw.AddText(pos, Pack(hovered ? kLightPalette.text : kLightPalette.accent), label);
    ImGui::PopFont();
    if (focused) {
        draw.AddRect(ImVec2(pos.x - 2.0f * scale, pos.y - 2.0f * scale),
                     ImVec2(pos.x + text_size.x + 2.0f * scale, pos.y + text_size.y + 2.0f * scale),
                     Pack(kLightPalette.accent), 2.0f * scale, 0, 2.0f * scale);
    }
    if (hovered) {
        const float y = pos.y + text_size.y + kLinkUnderlineGap * scale;
        draw.AddLine(ImVec2(pos.x, y), ImVec2(pos.x + text_size.x, y),
                     Pack(kLightPalette.accent), 1.0f * scale);
    }
    return pressed;
}

void DrawBannerBox(const ImVec2& pos, float width, float height, float scale) {
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    const ImVec2 bottom(pos.x + width, pos.y + height);
    draw.AddRectFilledMultiColor(pos, bottom, Pack(kLightPalette.surface),
                                 Pack(kLightPalette.border), Pack(kLightPalette.border),
                                 Pack(kLightPalette.surface));
    draw.AddRect(pos, bottom, Pack(kLightPalette.border), studio_metrics::radius_banner * scale,
                 0, 1.0f * scale);
}

}

void TransformsPage::Draw(float scale) {
    DrawHeader(scale);
    DrawBanner(scale);
    DrawMyTransforms(scale);
    DrawGrid(scale);
    DrawEditor(scale);
    DrawResetConfirm(scale);
    DrawDiff(scale);
    DrawHowItWorks(scale);
    toast_.Draw(scale);
}

void TransformsPage::DrawHeader(float scale) {
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted("Transforms");
    ImGui::PopFont();
    const ImVec2 title_min = ImGui::GetItemRectMin();
    const ImVec2 title_max = ImGui::GetItemRectMax();
    ImGui::SameLine(0.0f, studio_metrics::gap_section * scale);
    widgets::BadgeBeta(scale);

    const std::vector<std::string> tokens = DiffKeycapTokens();
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    const float label_w = ImGui::CalcTextSize("Opt in").x;
    const float caption_w = ImGui::CalcTextSize("to view changes").x;
    ImGui::PopFont();
    const float info_w = studio_metrics::hit_size * scale;
    const float toggle_w = 52.0f * scale;
    const float caps_w = KeycapRowWidth(tokens, scale);
    float view_w = 0.0f;
    if (HasDiff()) {
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        view_w = ImGui::CalcTextSize("View changes").x + 28.0f * scale;
        ImGui::PopFont();
    }
    const float cluster_w = label_w + studio_metrics::gap_compact * scale + info_w + studio_metrics::gap_inline * scale + toggle_w +
                            studio_metrics::gap_section * scale + caps_w + studio_metrics::gap_inline * scale + caption_w +
                            (HasDiff() ? (studio_metrics::gap_section * scale + view_w) : 0.0f);
    const float avail = ImGui::GetContentRegionAvail().x;
    const float cluster_x = start.x + std::max(0.0f, avail - cluster_w);
    ImGui::SetCursorScreenPos(ImVec2(cluster_x, title_min.y));
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::TextUnformatted("Opt in");
    ImGui::PopFont();
    ImGui::SameLine(0.0f, studio_metrics::gap_compact * scale);
    if (widgets::IconButton("transforms_info", "i", scale)) {
        how_modal_.Open();
    }
    ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
    if (widgets::ToggleSwitch("transforms_optin", OptIn(), scale)) {
        SetOptIn(!OptIn());
    }
    ImGui::SameLine(0.0f, studio_metrics::gap_section * scale);
    DrawKeycapRow(tokens, scale);
    ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::TextUnformatted("to view changes");
    ImGui::PopFont();
    if (HasDiff()) {
        ImGui::SameLine(0.0f, studio_metrics::gap_section * scale);
        if (widgets::LightButton("transforms_view_diff", "View changes", scale)) {
            OpenDiff();
        }
    }
    ImGui::SetCursorScreenPos(ImVec2(start.x, title_max.y + 16.0f * scale));
}

void TransformsPage::DrawBanner(float scale) {
    const float pad = kCardPadding * scale;
    const float avail = ImGui::GetContentRegionAvail().x;
    if (!OptIn()) {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float h = 48.0f * scale;
        ImGui::Dummy(ImVec2(avail, h));
        DrawBannerBox(pos, avail, h, scale);
        ImGui::SetCursorScreenPos(ImVec2(pos.x + pad, pos.y + (h - 18.0f * scale) * 0.5f));
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::TextColored(Color(kLightPalette.muted), "%s", transforms::kOptInOffBanner);
        ImGui::PopFont();
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + h + 16.0f * scale));
        return;
    }
    if (!banner_open_) {
        return;
    }
    float headline_h = 0.0f;
    float body_h = 0.0f;
    {
        ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
        headline_h = ImGui::CalcTextSize(transforms::kBannerHeadline).y;
        ImGui::PopFont();
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        const float body_w = ImGui::CalcTextSize(transforms::kBannerBody).x;
        const float inner = std::max(avail - pad * 2.0f, 1.0f);
        std::size_t lines = 1;
        while (static_cast<float>(lines) * inner < body_w) {
            ++lines;
        }
        body_h = ImGui::CalcTextSize(transforms::kBannerBody).y *
                 static_cast<float>(lines);
        ImGui::PopFont();
    }
    const float h = pad + headline_h + 6.0f * scale + body_h + 12.0f * scale +
                    40.0f * scale + pad;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(avail, h));
    DrawBannerBox(pos, avail, h, scale);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + pad, pos.y + pad));
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted(transforms::kBannerHeadline);
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(pos.x + pad, pos.y + pad + headline_h + 6.0f * scale));
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::PushTextWrapPos(pos.x + avail - pad);
    ImGui::TextUnformatted(transforms::kBannerBody);
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(
        ImVec2(pos.x + pad, pos.y + pad + headline_h + 6.0f * scale + body_h + 12.0f * scale));
    if (widgets::LightButton("transforms_try", "Try it out", scale)) {
        TryItOut();
    }
    ImGui::SameLine(0.0f, studio_metrics::gap_block * scale);
    if (TextLink("transforms_how", "How it works", scale)) {
        how_modal_.Open();
    }
    const float hit = studio_metrics::hit_size * scale;
    ImGui::SetCursorScreenPos(ImVec2(pos.x + avail - hit - 8.0f * scale, pos.y + 8.0f * scale));
    if (widgets::IconButton("transforms_banner_close", "x", scale)) {
        banner_open_ = false;
    }
    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + h + 16.0f * scale));
}

void TransformsPage::DrawMyTransforms(float scale) {
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted("My Transforms");
    ImGui::PopFont();
    const ImVec2 title_max = ImGui::GetItemRectMax();
    float create_w = 0.0f;
    if (ShowCreateCard()) {
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        create_w = ImGui::CalcTextSize("Create New").x + 32.0f * scale;
        ImGui::PopFont();
    }
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    const float reset_w = ImGui::CalcTextSize("Reset to defaults").x + 32.0f * scale;
    ImGui::PopFont();
    const float avail = ImGui::GetContentRegionAvail().x;
    float cursor_x = start.x + avail - reset_w -
                     (ShowCreateCard() ? (studio_metrics::gap_section * scale + create_w) : 0.0f);
    ImGui::SetCursorScreenPos(ImVec2(cursor_x, start.y));
    if (widgets::OutlinePill("transforms_reset", "Reset to defaults", true, scale)) {
        reset_modal_.Open();
    }
    ImGui::SameLine(0.0f, studio_metrics::gap_section * scale);
    if (ShowCreateCard()) {
        if (widgets::PrimaryButton("transforms_create", "Create New", scale)) {
            const std::string free_slot =
                transforms::FirstEmptyCustomSlot(store_.Entries());
            if (!free_slot.empty()) {
                OpenEditor(std::stoi(free_slot));
            }
        }
    }
    ImGui::SetCursorScreenPos(ImVec2(start.x, title_max.y + 16.0f * scale));
}

void TransformsPage::DrawGrid(float scale) {
    const std::vector<TransformStore::Entry> cards = Cards();
    const bool create = ShowCreateCard();
    const int total = static_cast<int>(cards.size()) + (create ? 1 : 0);
    const float gap = kCardGap * scale;
    const float h = kCardHeight * scale;
    const float avail = ImGui::GetContentRegionAvail().x;
    const float w = (avail - 2.0f * gap) / 3.0f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    if (!OptIn()) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    }
    for (int i = 0; i < total; ++i) {
        const int col = i % 3;
        const int row = i / 3;
        ImGui::SetCursorScreenPos(
            ImVec2(origin.x + col * (w + gap), origin.y + row * (h + gap)));
        if (i < static_cast<int>(cards.size())) {
            DrawCard(cards[static_cast<std::size_t>(i)], w, h, scale);
        } else {
            DrawCreateCard(w, h, scale);
        }
    }
    if (!OptIn()) {
        ImGui::PopStyleVar();
    }
    int rows = (total + 2) / 3;
    if (rows < 1) {
        rows = 1;
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + rows * (h + gap) - ImGui::GetStyle().ItemSpacing.y));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

void TransformsPage::DrawCard(const TransformStore::Entry& entry, float width, float height,
                              float scale) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const std::string child_id = "transforms_card_" + std::to_string(entry.slot);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(kLightPalette.surface));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, studio_metrics::radius_card * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(kCardPadding * scale, kCardPadding * scale));
    ImGui::BeginChild(child_id.c_str(), ImVec2(width, height), ImGuiChildFlags_Borders);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
    ImGui::TextUnformatted(entry.name.c_str());
    ImGui::PopFont();
    const std::string subtitle = transforms::CardDescription(entry);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - kCardPadding * scale);
    ImGui::TextColored(Color(kLightPalette.muted), "%s", subtitle.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_inline * scale));
    const std::vector<std::string> tokens = transforms::SplitKeycaps(entry.shortcut);
    DrawKeycapRow(tokens, scale);
    if (store_.Conflicted(entry.slot)) {
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_inline * scale));
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::TextColored(Color(kLightPalette.muted), "%s", "Shortcut conflict");
        ImGui::PopFont();
    }
    ImGui::EndChild();
    ImGui::SetCursorScreenPos(pos);
    const std::string edit_id = "transforms_edit_" + std::to_string(entry.slot);
    if (ImGui::InvisibleButton(edit_id.c_str(), ImVec2(width, height),
                               ImGuiButtonFlags_EnableNav)) {
        OpenEditor(entry.slot);
    }
    if (ImGui::IsItemFocused()) {
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        draw.AddRect(ImVec2(pos.x - 2.0f * scale, pos.y - 2.0f * scale),
                     ImVec2(pos.x + width + 2.0f * scale, pos.y + height + 2.0f * scale),
                     Pack(kLightPalette.accent), studio_metrics::radius_card * scale, 0, 2.0f * scale);
    }
}

void TransformsPage::DrawCreateCard(float width, float height, float scale) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(kLightPalette.surface));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, studio_metrics::radius_card * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(kCardPadding * scale, kCardPadding * scale));
    ImGui::BeginChild("transforms_card_create", ImVec2(width, height), ImGuiChildFlags_Borders);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted("+");
    ImGui::PopFont();
    ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
    ImGui::TextUnformatted("Create your own");
    ImGui::PopFont();
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::TextColored(Color(kLightPalette.muted), "%s", "Upload your own prompt");
    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::SetCursorScreenPos(pos);
    if (ImGui::InvisibleButton("transforms_edit_create", ImVec2(width, height),
                               ImGuiButtonFlags_EnableNav)) {
        const std::string free_slot =
            transforms::FirstEmptyCustomSlot(store_.Entries());
        if (!free_slot.empty()) {
            OpenEditor(std::stoi(free_slot));
        }
    }
}

void TransformsPage::PollRecording() {
    const bool win = ImGui::IsKeyDown(ImGuiKey_LeftSuper) || ImGui::IsKeyDown(ImGuiKey_RightSuper);
    const bool ctrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
    const bool alt = ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt);
    const bool shift =
        ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
    for (int k = 0; k < ImGuiKey_COUNT; ++k) {
        if (!ImGui::IsKeyPressed(static_cast<ImGuiKey>(k), false)) {
            continue;
        }
        const std::string label = KeyLabel(static_cast<ImGuiKey>(k));
        if (label.empty()) {
            continue;
        }
        CommitRecorded(transforms::BuildChord(win, ctrl, alt, shift, label));
        return;
    }
}

void TransformsPage::DrawEditor(float scale) {
    if (editing_slot_ == 0) {
        editor_open_ = false;
        return;
    }
    if (editor_open_ && !editor_sheet_.IsOpen()) {
        editor_sheet_.Open();
    }
    if (editor_sheet_.Begin("Edit transform", 440.0f, scale)) {
        if (recording_) {
            PollRecording();
        }
        ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
        ImGui::TextUnformatted("Transform");
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_inline * scale));
        if (recording_) {
            ImGui::BeginDisabled();
        }
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::TextUnformatted("Name");
        ImGui::PopFont();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputText("##transforms_name", name_buffer_, sizeof(name_buffer_));
        ImGui::PopItemWidth();
        RefreshFromBuffers();
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_inline * scale));
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::TextUnformatted("Prompt");
        ImGui::PopFont();
        ImGui::InputTextMultiline("##transforms_prompt", prompt_buffer_,
                                  sizeof(prompt_buffer_),
                                  ImVec2(ImGui::GetContentRegionAvail().x, 120.0f * scale));
        RefreshFromBuffers();
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        const std::string counter =
            std::to_string(transforms::CountCodePoints(edit_prompt_)) + " / " +
            std::to_string(TransformStore::kMaxPromptChars);
        ImGui::TextColored(Color(kLightPalette.muted), "%s", counter.c_str());
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_inline * scale));
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::TextUnformatted("Shortcut");
        ImGui::PopFont();
        if (recording_) {
            ImGui::EndDisabled();
            ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
            ImGui::TextUnformatted("Press a shortcut, or Escape to close.");
            ImGui::PopFont();
            if (widgets::LightButton("transforms_record_cancel", "Cancel", scale)) {
                CancelRecord();
            }
        } else {
            const std::vector<std::string> tokens =
                transforms::SplitKeycaps(edit_shortcut_);
            DrawKeycapRow(tokens, scale);
            ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
            if (widgets::LightButton("transforms_record", "Record", scale)) {
                BeginRecord();
            }
            ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
            if (widgets::LightButton("transforms_record_clear", "Use default", scale)) {
                ClearRecordedShortcut();
            }
        }
        if (recording_) {
            ImGui::BeginDisabled();
        }
        // FLAG: edit_error renders muted (not err); err token is a follow-up, out of scope here.
        if (!edit_error_.empty()) {
            ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_inline * scale));
            ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
            ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 24.0f * scale);
            ImGui::TextColored(Color(kLightPalette.muted), "%s", edit_error_.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopFont();
        }
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_inline * scale));
        const int sheet_slot = editing_slot_;
        if (widgets::PrimaryButton("transforms_save", "Save", scale)) {
            if (SaveEdit()) {
                toast_.Show("Saved", "");
            }
        }
        ImGui::SameLine(0.0f, studio_metrics::gap_section * scale);
        if (sheet_slot > 2) {
            if (widgets::LightButton("transforms_delete", "Delete", scale)) {
                DeleteEdit();
            }
        } else {
            if (widgets::LightButton("transforms_restore", "Restore default", scale)) {
                ReturnToDefault();
            }
        }
        if (recording_) {
            ImGui::EndDisabled();
        }
        editor_sheet_.End();
    }
    if (editing_slot_ == 0 && editor_sheet_.IsOpen()) {
        editor_sheet_.Close();
    }
    if (editor_open_ && !editor_sheet_.IsOpen()) {
        editor_open_ = false;
    }
    if (editing_slot_ != 0 && !editor_sheet_.IsOpen()) {
        CloseEditor();
    }
}

void TransformsPage::DrawResetConfirm(float scale) {
    if (reset_modal_.Begin("Reset transforms?", scale)) {
        const ResetPreview plan = ResetPlan();
        ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
        ImGui::TextUnformatted("Reset to defaults?");
        ImGui::PopFont();
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        std::string deleted;
        if (plan.customs_deleted == 0) {
            deleted = "No custom transforms will be deleted.";
        } else if (plan.customs_deleted == 1) {
            deleted = "1 custom transform will be deleted.";
        } else {
            deleted = std::to_string(plan.customs_deleted) + " custom transforms will be deleted.";
        }
        ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 24.0f * scale);
        ImGui::TextUnformatted(deleted.c_str());
        ImGui::PopTextWrapPos();
        // Intentional native grid reuse: metrics::space_xs (4px) has no studio twin; this site alone keeps the native token.
        ImGui::Dummy(ImVec2(0.0f, metrics::space_xs * scale));
        ImGui::TextUnformatted("Polish and Prompt Engineer return to their defaults.");
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_section * scale));
        if (widgets::LightButton("transforms_reset_cancel", "Cancel", scale)) {
            reset_modal_.Close();
        }
        ImGui::SameLine(0.0f, studio_metrics::gap_section * scale);
        if (widgets::PrimaryButton("transforms_reset_confirm", "Reset", scale)) {
            PerformReset();
            reset_modal_.Close();
            toast_.Show("Transforms reset", "");
        }
        reset_modal_.End();
    }
}

void TransformsPage::DrawDiff(float scale) {
    if (diff_modal_.Begin("Transform result", scale)) {
        ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
        ImGui::TextUnformatted("Transform result");
        ImGui::PopFont();
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::TextColored(Color(kLightPalette.muted), "%s",
                           CurrentChangeLabel().c_str());
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_inline * scale));
        if (!HasDiff()) {
            ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
            ImGui::TextUnformatted("No recent change.");
            ImGui::PopFont();
        } else {
            const lwf::TransformDiff snapshot = diff_source_->TransformDiffSnapshot();
            const float width = ImGui::GetContentRegionAvail().x;
            ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
            const float line_h = ImGui::GetTextLineHeight();
            const float space_w = ImGui::CalcTextSize(" ").x;
            const ImVec2 left = ImGui::GetCursorScreenPos();
            float right = left.x + width;
            float cursor_x = left.x;
            float cursor_y = left.y;
            bool line_fresh = true;
            ImDrawList& draw = *ImGui::GetWindowDrawList();
            for (const lwf::DiffToken& token : snapshot.diff.tokens) {
                const ImVec2 token_size = ImGui::CalcTextSize(token.text.c_str());
                if (!line_fresh && cursor_x + token_size.x > right) {
                    cursor_x = left.x;
                    cursor_y += line_h + 4.0f * scale;
                    line_fresh = true;
                }
                if (!line_fresh) {
                    cursor_x += space_w;
                }
                if (token.op == lwf::DiffOp::Insert) {
                    draw.AddRectFilled(
                        ImVec2(cursor_x, cursor_y),
                        ImVec2(cursor_x + token_size.x, cursor_y + token_size.y),
                        Pack(kLightPalette.border), 4.0f * scale);
                }
                const std::uint32_t text_color =
                    token.op == lwf::DiffOp::Insert ? kLightPalette.accent
                    : (token.op == lwf::DiffOp::Delete ? kLightPalette.muted
                                                       : kLightPalette.text);
                draw.AddText(ImVec2(cursor_x, cursor_y), Pack(text_color),
                             token.text.c_str());
                if (token.op == lwf::DiffOp::Delete) {
                    const float mid = cursor_y + token_size.y * 0.5f;
                    draw.AddLine(ImVec2(cursor_x, mid),
                                 ImVec2(cursor_x + token_size.x, mid),
                                 Pack(kLightPalette.muted), 1.0f * scale);
                }
                cursor_x += token_size.x;
                line_fresh = false;
            }
            ImGui::PopFont();
            ImGui::SetCursorScreenPos(ImVec2(left.x, cursor_y + line_h + 12.0f * scale));
            if (widgets::PrimaryButton("transforms_copy", "Copy result", scale)) {
                ImGui::SetClipboardText(CopyResult().c_str());
                toast_.Show("Copied", "");
            }
        }
        if (widgets::LightButton("transforms_diff_close", "Close", scale)) {
            diff_modal_.Close();
        }
        diff_modal_.End();
    }
}

void TransformsPage::DrawHowItWorks(float scale) {
    if (how_modal_.Begin("How it works", scale)) {
        ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
        ImGui::TextUnformatted("How it works");
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_inline * scale));
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 24.0f * scale);
        ImGui::TextUnformatted(transforms::kHowItWorksBody);
        ImGui::PopTextWrapPos();
        const std::string hint = "The diff view opens with " + store_.DiffShortcut() + ".";
        ImGui::TextUnformatted(hint.c_str());
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_section * scale));
        if (widgets::PrimaryButton("transforms_how_close", "Close", scale)) {
            how_modal_.Close();
        }
        how_modal_.End();
    }
}

}
