#include "ui/pages/TransformsPage.h"

#include "core/DictationEngine.h"
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
using lwf::ui::Pack;

constexpr float kCardPadding = 20.0f;
constexpr float kCardGap = 16.0f;
constexpr float kCardHeight = 172.0f;

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

void DrawKeycapRow(const std::vector<std::string>& tokens, float scale,
                   const lwf::ui::Palette& palette) {
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) {
            ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
        }
        widgets::Keycap(tokens[i].c_str(), scale, palette);
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

}

void TransformsPage::Draw(float scale, const Palette& palette) {
    DrawHeader(scale, palette);
    DrawMyTransforms(scale, palette);
    DrawGrid(scale, palette);
    DrawEditor(scale, palette);
    DrawResetConfirm(scale, palette);
    DrawDiff(scale, palette);
    DrawHowItWorks(scale, palette);
    toast_.Draw(scale, palette);
}

void TransformsPage::DrawHeader(float scale, const Palette& palette) {
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted("Transforms");
    ImGui::PopFont();
    const ImVec2 title_min = ImGui::GetItemRectMin();
    const ImVec2 title_max = ImGui::GetItemRectMax();
    ImGui::SameLine(0.0f, studio_metrics::gap_section * scale);
    widgets::BadgeBeta(scale, palette);

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
    if (widgets::IconButton("transforms_info", "i", scale, palette)) {
        how_modal_.Open();
    }
    ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
    if (widgets::ToggleSwitch("transforms_optin", OptIn(), scale, palette)) {
        SetOptIn(!OptIn());
    }
    ImGui::SameLine(0.0f, studio_metrics::gap_section * scale);
    DrawKeycapRow(tokens, scale, palette);
    ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::TextUnformatted("to view changes");
    ImGui::PopFont();
    if (HasDiff()) {
        ImGui::SameLine(0.0f, studio_metrics::gap_section * scale);
        if (widgets::LightButton("transforms_view_diff", "View changes", scale, palette)) {
            OpenDiff();
        }
    }
    ImGui::SetCursorScreenPos(ImVec2(start.x, title_max.y + 16.0f * scale));
}

void TransformsPage::DrawMyTransforms(float scale, const Palette& palette) {
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
    const float reset_w = ImGui::CalcTextSize("Reset all").x + 32.0f * scale;
    ImGui::PopFont();
    const float avail = ImGui::GetContentRegionAvail().x;
    float cursor_x = start.x + avail - reset_w -
                     (ShowCreateCard() ? (studio_metrics::gap_section * scale + create_w) : 0.0f);
    ImGui::SetCursorScreenPos(ImVec2(cursor_x, start.y));
    if (widgets::OutlinePill("transforms_reset", "Reset all", true, scale, palette)) {
        reset_modal_.Open();
    }
    ImGui::SameLine(0.0f, studio_metrics::gap_section * scale);
    if (ShowCreateCard()) {
        if (widgets::PrimaryButton("transforms_create", "Create New", scale, palette)) {
            const std::string free_slot =
                transforms::FirstEmptyCustomSlot(store_.Entries());
            if (!free_slot.empty()) {
                OpenEditor(std::stoi(free_slot));
            }
        }
    }
    ImGui::SetCursorScreenPos(ImVec2(start.x, title_max.y + 16.0f * scale));
}

void TransformsPage::DrawGrid(float scale, const Palette& palette) {
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
            DrawCard(cards[static_cast<std::size_t>(i)], w, h, scale, palette);
        } else {
            DrawCreateCard(w, h, scale, palette);
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
                              float scale, const Palette& palette) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const std::string child_id = "transforms_card_" + std::to_string(entry.slot);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(palette.surface));
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
    ImGui::TextColored(Color(palette.muted), "%s", subtitle.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_inline * scale));
    const std::vector<std::string> tokens = transforms::SplitKeycaps(entry.shortcut);
    DrawKeycapRow(tokens, scale, palette);
    if (store_.Conflicted(entry.slot)) {
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_inline * scale));
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::TextColored(Color(palette.muted), "%s", "Shortcut conflict");
        ImGui::PopFont();
    }
    ImGui::EndChild();
    ImGui::SetCursorScreenPos(pos);
    const std::string edit_id = "transforms_edit_" + std::to_string(entry.slot);
    if (ImGui::InvisibleButton(edit_id.c_str(), ImVec2(width, height),
                               ImGuiButtonFlags_EnableNav | ImGuiButtonFlags_AllowOverlap)) {
        OpenEditor(entry.slot);
    }
    if (ImGui::IsItemFocused()) {
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        draw.AddRect(ImVec2(pos.x - 2.0f * scale, pos.y - 2.0f * scale),
                     ImVec2(pos.x + width + 2.0f * scale, pos.y + height + 2.0f * scale),
                     Pack(palette.accent), studio_metrics::radius_card * scale, 0, 2.0f * scale);
    }
    // T10c per-slot Run: the overlay above covers the whole card. Vendored
    // ImGui hit-testing is first-submitted-wins (ItemHoverable), so the
    // overlay carries AllowOverlap: the later Run button then receives its
    // hover/click, and the overlay covers the rest of the card. Run is
    // anchored to the card's bottom edge.
    // Reserve exactly the LightButton height (40px tokens; not hit_size).
    // Run is disabled while opted out; opening the editor stays
    // available, as before.
    // opening the editor stays available, as before.
    const bool run_enabled = OptIn();
    if (!run_enabled) {
        ImGui::BeginDisabled();
    }
    ImGui::SetCursorScreenPos(ImVec2(
        pos.x + kCardPadding * scale,
        pos.y + height - 40.0f * scale - kCardPadding * scale));
    const std::string run_id = "transforms_run_" + std::to_string(entry.slot);
    if (widgets::LightButton(run_id.c_str(), "Run", scale, palette)) {
        RunSlot(entry.slot);
    }
    if (!run_enabled) {
        ImGui::EndDisabled();
    }
}

void TransformsPage::DrawCreateCard(float width, float height, float scale,
                                    const Palette& palette) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(palette.surface));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, studio_metrics::radius_card * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(kCardPadding * scale, kCardPadding * scale));
    ImGui::BeginChild("transforms_card_create", ImVec2(width, height), ImGuiChildFlags_Borders);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    // Followups D5: the create card's three lines are centered both ways so
    // the empty slot reads as one calm invitation instead of a left column.
    const float inner_w = ImGui::GetContentRegionAvail().x;
    const float inner_h = height - 2.0f * kCardPadding * scale;
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    const float glyph_h = ImGui::GetTextLineHeight();
    const float glyph_w = ImGui::CalcTextSize("+").x;
    ImGui::PopFont();
    ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
    const float title_h = ImGui::GetTextLineHeight();
    const float title_w = ImGui::CalcTextSize("Create your own").x;
    ImGui::PopFont();
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    const float sub_h = ImGui::GetTextLineHeight();
    const float sub_w = ImGui::CalcTextSize("Upload your own prompt").x;
    ImGui::PopFont();
    const float gap = studio_metrics::gap_compact * scale;
    const float block_h = glyph_h + gap + title_h + gap + sub_h;
    float y = kCardPadding * scale + std::max(0.0f, (inner_h - block_h) * 0.5f);
    ImGui::SetCursorPos(ImVec2(kCardPadding * scale + std::max(0.0f, (inner_w - glyph_w) * 0.5f), y));
    ImGui::PushFont(nullptr, studio_metrics::font_page_title_px);
    ImGui::TextUnformatted("+");
    ImGui::PopFont();
    y += glyph_h + gap;
    ImGui::SetCursorPos(ImVec2(kCardPadding * scale + std::max(0.0f, (inner_w - title_w) * 0.5f), y));
    ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
    ImGui::TextUnformatted("Create your own");
    ImGui::PopFont();
    y += title_h + gap;
    ImGui::SetCursorPos(ImVec2(kCardPadding * scale + std::max(0.0f, (inner_w - sub_w) * 0.5f), y));
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::TextColored(Color(palette.muted), "%s", "Upload your own prompt");
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

void TransformsPage::DrawEditor(float scale, const Palette& palette) {
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
        ImGui::TextColored(Color(palette.muted), "%s", counter.c_str());
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
            if (widgets::LightButton("transforms_record_cancel", "Cancel", scale, palette)) {
                CancelRecord();
            }
        } else {
            const std::vector<std::string> tokens =
                transforms::SplitKeycaps(edit_shortcut_);
            DrawKeycapRow(tokens, scale, palette);
            ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
            if (widgets::LightButton("transforms_record", "Record", scale, palette)) {
                BeginRecord();
            }
            ImGui::SameLine(0.0f, studio_metrics::gap_inline * scale);
            if (widgets::LightButton("transforms_record_clear", "Use default", scale, palette)) {
                ClearRecordedShortcut();
            }
        }
        if (recording_) {
            ImGui::BeginDisabled();
        }
        if (!edit_error_.empty()) {
            ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_inline * scale));
            ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
            ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 24.0f * scale);
            ImGui::TextColored(Color(palette.err), "%s", edit_error_.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopFont();
        }
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_inline * scale));
        if (widgets::PrimaryButton("transforms_save", "Save", scale, palette)) {
            if (SaveEdit()) {
                toast_.Show("Saved", "");
            }
        }
        ImGui::SameLine(0.0f, studio_metrics::gap_section * scale);
        if (widgets::LightButton("transforms_delete", "Delete", scale, palette)) {
            DeleteEdit();
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

void TransformsPage::DrawResetConfirm(float scale, const Palette& palette) {
    if (reset_modal_.Begin("Reset transforms?", scale)) {
        const ResetPreview plan = ResetPlan();
        ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
        ImGui::TextUnformatted("Reset all transforms?");
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
        ImGui::TextUnformatted("Every slot ends up empty.");
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_section * scale));
        if (widgets::LightButton("transforms_reset_cancel", "Cancel", scale, palette)) {
            reset_modal_.Close();
        }
        ImGui::SameLine(0.0f, studio_metrics::gap_section * scale);
        if (widgets::PrimaryButton("transforms_reset_confirm", "Reset", scale, palette)) {
            PerformReset();
            reset_modal_.Close();
            toast_.Show("Transforms reset", "");
        }
        reset_modal_.End();
    }
}

void TransformsPage::DrawDiff(float scale, const Palette& palette) {
    if (diff_modal_.Begin("Transform result", scale)) {
        ImGui::PushFont(nullptr, studio_metrics::font_card_title_px);
        ImGui::TextUnformatted("Transform result");
        ImGui::PopFont();
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        ImGui::TextColored(Color(palette.muted), "%s",
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
                        Pack(palette.border), 4.0f * scale);
                }
                const std::uint32_t text_color =
                    token.op == lwf::DiffOp::Insert ? palette.accent
                    : (token.op == lwf::DiffOp::Delete ? palette.muted
                                                        : palette.text);
                draw.AddText(ImVec2(cursor_x, cursor_y), Pack(text_color),
                             token.text.c_str());
                if (token.op == lwf::DiffOp::Delete) {
                    const float mid = cursor_y + token_size.y * 0.5f;
                    draw.AddLine(ImVec2(cursor_x, mid),
                                 ImVec2(cursor_x + token_size.x, mid),
                                 Pack(palette.muted), 1.0f * scale);
                }
                cursor_x += token_size.x;
                line_fresh = false;
            }
            ImGui::PopFont();
            ImGui::SetCursorScreenPos(ImVec2(left.x, cursor_y + line_h + 12.0f * scale));
            if (widgets::PrimaryButton("transforms_copy", "Copy result", scale, palette)) {
                ImGui::SetClipboardText(CopyResult().c_str());
                toast_.Show("Copied", "");
            }
        }
        if (widgets::LightButton("transforms_diff_close", "Close", scale, palette)) {
            diff_modal_.Close();
        }
        diff_modal_.End();
    }
}

void TransformsPage::DrawHowItWorks(float scale, const Palette& palette) {
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
        if (widgets::PrimaryButton("transforms_how_close", "Close", scale, palette)) {
            how_modal_.Close();
        }
        how_modal_.End();
    }
}

}
