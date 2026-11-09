#include "ui/ScratchpadWindow.h"

#include "ui/theme/ThemePalette.h"
#include "ui/theme/ThemeStyle.h"
#include "ui/widgets/Widgets.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <GL/gl.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstring>

namespace {

using lwf::ui::Color;
using lwf::ui::Pack;
// T2b: the ImGui style follows PaletteForTheme (Graphite default, Paper via
// the `theme` key). T10e: the tab chrome reads the live current_palette_
// member (Init promotes the store selection, ApplyTheme queues the live
// swap, RenderFrame applies it with this window's context current).
// Historical mapping note retained: studio style -> ApplyImGuiStyle mapping
// was 1:1 (card_bg_white/card_bg -> surface, teal_700 -> accent, ink -> text,
// page_bg -> bg); Padding/rounding/ChildBg drift noted in audit (32 -> 16,
// 8 -> 6, white -> transparent); studio_metrics fonts/radii kept as-is.
namespace studio_metrics = lwf::ui::studio_metrics;

int ScratchpadBodyCallback(ImGuiInputTextCallbackData* data) {
    if (data == nullptr || data->UserData == nullptr) {
        return 0;
    }
    ScratchpadWindow* window = static_cast<ScratchpadWindow*>(data->UserData);
    window->SetInputCaret(data->CursorPos, data->SelectionStart, data->SelectionEnd);
    return 0;
}

}

ScratchpadWindow::ScratchpadWindow(NotesStore& notes, SettingsStore& settings)
    : notes_(notes), settings_(settings) {}

ScratchpadWindow::~ScratchpadWindow() {
    Shutdown();
}

bool ScratchpadWindow::Init() {
    if (!glfwInit()) {
        return false;
    }
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    GLFWmonitor* const primary = glfwGetPrimaryMonitor();
    if (primary != nullptr) {
        glfwGetMonitorContentScale(primary, &scale_x, &scale_y);
    }
    dpi_scale_ = scale_x > 0.0f ? scale_x : 1.0;
    int area_x = 0;
    int area_y = 0;
    int area_w = 1280;
    int area_h = 720;
    if (primary != nullptr) {
        glfwGetMonitorWorkarea(primary, &area_x, &area_y, &area_w, &area_h);
    }
    const int want_w = static_cast<int>(760 * dpi_scale_);
    const int want_h = static_cast<int>(620 * dpi_scale_);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window_ = glfwCreateWindow(std::min(want_w, area_w), std::min(want_h, area_h), "Scratchpad", nullptr, nullptr);
    if (window_ == nullptr) {
        return false;
    }
    glfwSetWindowSizeLimits(window_, static_cast<int>(480 * dpi_scale_), static_cast<int>(360 * dpi_scale_), GLFW_DONT_CARE, GLFW_DONT_CARE);
    int win_w = 0;
    int win_h = 0;
    glfwGetWindowSize(window_, &win_w, &win_h);
    glfwSetWindowPos(window_, area_x + (area_w - win_w) / 2, area_y + (area_h - win_h) / 2);
    glfwSetWindowUserPointer(window_, this);
    glfwSetWindowContentScaleCallback(window_, &ScratchpadWindow::OnContentScale);
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    imgui_context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    if (!ImGui_ImplOpenGL3_Init("#version 130")) {
        Shutdown();
        return false;
    }
    // T2b theme selection: read the store so a stored "paper" starts in
    // Paper and the default starts in Graphite. current_palette_ is the
    // last-applied palette. Any pending ApplyTheme queued before Init is
    // stale — Init re-reads the store.
    current_palette_ = &lwf::ui::PaletteForTheme(settings_.GetString("theme"));
    pending_theme_palette_ = nullptr;
    theme_pending_ = false;
    lwf::ui::ApplyImGuiStyle(*current_palette_, static_cast<float>(dpi_scale_));
    return true;
}

void ScratchpadWindow::Shutdown() noexcept {
    if (shutdown_done_) {
        return;
    }
    shutdown_done_ = true;
    if (imgui_context_ != nullptr) {
        ImGui::SetCurrentContext(imgui_context_);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(imgui_context_);
        imgui_context_ = nullptr;
    }
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
}

void ScratchpadWindow::SetVisible(bool visible) {
    if (window_ == nullptr) {
        return;
    }
    if (visible) {
        glfwShowWindow(window_);
        glfwFocusWindow(window_);
        focus_editor_pending_ = true;
    } else if (glfwGetWindowAttrib(window_, GLFW_VISIBLE)) {
        glfwHideWindow(window_);
    }
    if (!visible) {
        dictation_active_.store(false);
    }
}

bool ScratchpadWindow::IsVisible() const {
    return window_ != nullptr && glfwGetWindowAttrib(window_, GLFW_VISIBLE) != 0;
}

bool ScratchpadWindow::TakeCloseRequest() {
    const bool result = close_requested_;
    close_requested_ = false;
    return result;
}

void ScratchpadWindow::ApplyTheme(const lwf::ui::Palette& palette) {
    // CRITICAL: never call ApplyImGuiStyle here. The ApplySettings drain in
    // main.cpp may run while another window's ImGui context is current, and
    // ApplyImGuiStyle colors the current context. Queue the palette; the
    // hidden-window early-out in RenderFrame (below) already defers the
    // application until the editor is visible, same as a pending DPI move.
    if (&palette == current_palette_ && !theme_pending_) {
        return;
    }
    if (theme_pending_ && &palette == pending_theme_palette_) {
        return;
    }
    pending_theme_palette_ = &palette;
    theme_pending_ = true;
}

void ScratchpadWindow::OpenNote(const std::string& id) {
    if (id.empty()) {
        OpenNewNote();
        return;
    }
    for (std::size_t i = 0; i < tabs_.size(); ++i) {
        if (tabs_[i].id == id) {
            active_index_ = static_cast<int>(i);
            inputs_synced_ = false;
            SetVisible(true);
            return;
        }
    }
    const auto note = notes_.Get(id);
    if (!note.has_value()) {
        return;
    }
    lwf::ui::scratchpad::ScratchpadTab tab;
    tab.id = note->id;
    tab.title = note->title;
    tab.body = note->body;
    tab.caret = tab.body.size();
    tab.selection_start = tab.caret;
    tab.selection_end = tab.caret;
    tab.dirty = false;
    tabs_.push_back(tab);
    active_index_ = static_cast<int>(tabs_.size() - 1);
    inputs_synced_ = false;
    SetVisible(true);
}

void ScratchpadWindow::OpenNewNote() {
    const auto created = notes_.Create("Untitled", "");
    if (!created.has_value()) {
        return;
    }
    lwf::ui::scratchpad::ScratchpadTab tab;
    tab.id = created->id;
    tab.title = created->title;
    tab.body = created->body;
    tab.caret = tab.body.size();
    tab.selection_start = tab.caret;
    tab.selection_end = tab.caret;
    tab.dirty = false;
    tabs_.push_back(tab);
    active_index_ = static_cast<int>(tabs_.size() - 1);
    inputs_synced_ = false;
    SetVisible(true);
}

void ScratchpadWindow::SetDictationActive(bool active) {
    dictation_active_.store(active);
}

bool ScratchpadWindow::IsDictationActive() const {
    return dictation_active_.load();
}

std::atomic<bool>& ScratchpadWindow::DictationActiveFlag() {
    return dictation_active_;
}

ScratchpadBuffer& ScratchpadWindow::DictationBuffer() {
    return dictation_buffer_;
}

void ScratchpadWindow::DrainDictation() {
    if (!dictation_active_.load()) {
        return;
    }
    if (tabs_.empty() || active_index_ < 0 || active_index_ >= static_cast<int>(tabs_.size())) {
        return;
    }
    const std::string snapshot = dictation_buffer_.Text();
    if (snapshot.size() <= drained_len_) {
        return;
    }
    const std::string delta = snapshot.substr(drained_len_);
    lwf::ui::scratchpad::ScratchpadTab& tab = tabs_[static_cast<std::size_t>(active_index_)];
    lwf::ui::scratchpad::InsertAtCaret(tab.body, tab.caret, delta);
    tab.selection_start = tab.caret;
    tab.selection_end = tab.caret;
    tab.dirty = true;
    inputs_synced_ = false;
    const std::string after = dictation_buffer_.Text();
    if (after.size() == snapshot.size()) {
        dictation_buffer_.Clear();
        drained_len_ = 0;
    } else {
        drained_len_ = snapshot.size();
    }
}

void ScratchpadWindow::InsertDictatedText(const std::string& text) {
    if (!dictation_active_.load()) {
        return;
    }
    if (tabs_.empty() || active_index_ < 0 || active_index_ >= static_cast<int>(tabs_.size())) {
        return;
    }
    lwf::ui::scratchpad::ScratchpadTab& tab = tabs_[static_cast<std::size_t>(active_index_)];
    lwf::ui::scratchpad::InsertAtCaret(tab.body, tab.caret, text);
    tab.selection_start = tab.caret;
    tab.selection_end = tab.caret;
    tab.dirty = true;
    inputs_synced_ = false;
}

void ScratchpadWindow::SetInputCaret(int cursor_pos, int selection_start, int selection_end) {
    if (tabs_.empty() || active_index_ < 0 || active_index_ >= static_cast<int>(tabs_.size())) {
        return;
    }
    lwf::ui::scratchpad::ScratchpadTab& tab = tabs_[static_cast<std::size_t>(active_index_)];
    const std::size_t size = tab.body.size();
    std::size_t caret = cursor_pos < 0 ? 0 : static_cast<std::size_t>(cursor_pos);
    std::size_t start = selection_start < 0 ? 0 : static_cast<std::size_t>(selection_start);
    std::size_t end = selection_end < 0 ? 0 : static_cast<std::size_t>(selection_end);
    if (caret > size) {
        caret = size;
    }
    if (start > size) {
        start = size;
    }
    if (end > size) {
        end = size;
    }
    tab.caret = caret;
    tab.selection_start = start;
    tab.selection_end = end;
}

void ScratchpadWindow::ApplyToolbarAction(lwf::ui::scratchpad::ToolbarAction action) {
    if (tabs_.empty() || active_index_ < 0 || active_index_ >= static_cast<int>(tabs_.size())) {
        return;
    }
    lwf::ui::scratchpad::ScratchpadTab& tab = tabs_[static_cast<std::size_t>(active_index_)];
    std::size_t start = tab.selection_start;
    std::size_t end = tab.selection_end;
    if (start == end) {
        start = tab.caret;
        end = tab.caret;
    }
    std::string next;
    switch (action) {
        case lwf::ui::scratchpad::ToolbarAction::Bold:
            next = lwf::ui::scratchpad::ApplyBold(tab.body, start, end);
            break;
        case lwf::ui::scratchpad::ToolbarAction::Italic:
            next = lwf::ui::scratchpad::ApplyItalic(tab.body, start, end);
            break;
        case lwf::ui::scratchpad::ToolbarAction::Bullet:
            next = lwf::ui::scratchpad::ApplyBullet(tab.body, start, end);
            break;
        case lwf::ui::scratchpad::ToolbarAction::Numbered:
            next = lwf::ui::scratchpad::ApplyNumbered(tab.body, start, end);
            break;
        case lwf::ui::scratchpad::ToolbarAction::Heading:
            next = lwf::ui::scratchpad::ApplyHeading(tab.body, start, end);
            break;
    }
    tab.body.swap(next);
    if (tab.caret > tab.body.size()) {
        tab.caret = tab.body.size();
    }
    tab.selection_start = tab.caret;
    tab.selection_end = tab.caret;
    tab.dirty = true;
    inputs_synced_ = false;
}

bool ScratchpadWindow::SaveActiveTab() {
    if (tabs_.empty() || active_index_ < 0 || active_index_ >= static_cast<int>(tabs_.size())) {
        return false;
    }
    const lwf::ui::scratchpad::ScratchpadTab& tab = tabs_[static_cast<std::size_t>(active_index_)];
    if (!tab.dirty) {
        return true;
    }
    if (!notes_.Update(tab.id, tab.title, tab.body)) {
        return false;
    }
    tabs_[static_cast<std::size_t>(active_index_)].dirty = false;
    return true;
}

std::size_t ScratchpadWindow::TabCount() const {
    return tabs_.size();
}

int ScratchpadWindow::ActiveIndex() const {
    return active_index_;
}

std::string ScratchpadWindow::ActiveBody() const {
    if (tabs_.empty() || active_index_ < 0 || active_index_ >= static_cast<int>(tabs_.size())) {
        return {};
    }
    return tabs_[static_cast<std::size_t>(active_index_)].body;
}

std::string ScratchpadWindow::ActiveTitle() const {
    if (tabs_.empty() || active_index_ < 0 || active_index_ >= static_cast<int>(tabs_.size())) {
        return {};
    }
    return lwf::ui::scratchpad::EditorDisplayTitle(tabs_[static_cast<std::size_t>(active_index_)].title);
}

std::size_t ScratchpadWindow::ActiveWordCount() const {
    return lwf::ui::scratchpad::CountWords(ActiveBody());
}

void ScratchpadWindow::OnContentScale(GLFWwindow* window, float scale_x, float) {
    if (ScratchpadWindow* self = static_cast<ScratchpadWindow*>(glfwGetWindowUserPointer(window))) {
        if (scale_x > 0.0f) {
            self->pending_dpi_scale_ = scale_x;
        }
    }
}

void ScratchpadWindow::SyncInputsFromActiveTab() {
    if (tabs_.empty() || active_index_ < 0 || active_index_ >= static_cast<int>(tabs_.size())) {
        return;
    }
    const std::string& id = tabs_[static_cast<std::size_t>(active_index_)].id;
    if (inputs_synced_ && synced_tab_id_ == id) {
        return;
    }
    const lwf::ui::scratchpad::ScratchpadTab& tab = tabs_[static_cast<std::size_t>(active_index_)];
    std::snprintf(title_input_, sizeof(title_input_), "%s", tab.title.c_str());
    body_input_.assign(tab.body.begin(), tab.body.end());
    body_input_.push_back('\0');
    synced_tab_id_ = id;
    inputs_synced_ = true;
}

void ScratchpadWindow::SyncActiveTabFromInputs() {
    if (tabs_.empty() || active_index_ < 0 || active_index_ >= static_cast<int>(tabs_.size())) {
        return;
    }
    if (body_input_.empty()) {
        body_input_.push_back('\0');
    }
    lwf::ui::scratchpad::ScratchpadTab& tab = tabs_[static_cast<std::size_t>(active_index_)];
    const std::string title_now(title_input_);
    const std::string body_now(body_input_.data(), body_input_.size() > 0 ? body_input_.size() - 1 : 0);
    bool changed = false;
    if (title_now != tab.title) {
        tab.title = title_now;
        changed = true;
    }
    if (body_now != tab.body) {
        tab.body = body_now;
        if (tab.caret > tab.body.size()) {
            tab.caret = tab.body.size();
        }
        if (tab.selection_start > tab.body.size()) {
            tab.selection_start = tab.body.size();
        }
        if (tab.selection_end > tab.body.size()) {
            tab.selection_end = tab.body.size();
        }
        changed = true;
    }
    if (changed) {
        tab.dirty = true;
        last_edit_at_ = static_cast<double>(ImGui::GetTime());
    }
}

void ScratchpadWindow::RefreshDictationActive() {
    if (window_ == nullptr) {
        dictation_active_.store(false);
        return;
    }
    if (!glfwGetWindowAttrib(window_, GLFW_VISIBLE)) {
        dictation_active_.store(false);
        return;
    }
    const bool focused = glfwGetWindowAttrib(window_, GLFW_FOCUSED) != 0;
    dictation_active_.store(focused);
}

void ScratchpadWindow::MarkDirtyAndSave() {
    const double now = static_cast<double>(ImGui::GetTime());
    if (last_edit_at_ < 0.0) {
        return;
    }
    if (now - last_edit_at_ < 0.5) {
        return;
    }
    if (SaveActiveTab()) {
        ShowSaved(now);
        last_edit_at_ = -1.0;
    }
}

void ScratchpadWindow::ShowSaved(double now) {
    saved_until_ = now + 1.5;
}

void ScratchpadWindow::DrawTabs(float scale) {
    int close_index = -1;
    for (std::size_t i = 0; i < tabs_.size(); ++i) {
        const lwf::ui::scratchpad::ScratchpadTab& tab = tabs_[i];
        const std::string display = lwf::ui::scratchpad::EditorDisplayTitle(tab.title);
        const std::string tab_id = "##tab_" + tab.id;
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        const ImVec2 text_size = ImGui::CalcTextSize(display.c_str());
        ImGui::PopFont();
        const float tab_w = text_size.x + 36.0f * scale;
        const float tab_h = 32.0f * scale;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const bool pressed = ImGui::InvisibleButton(tab_id.c_str(), ImVec2(tab_w, tab_h), ImGuiButtonFlags_EnableNav);
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        const bool active = static_cast<int>(i) == active_index_;
        if (active) {
            draw.AddRectFilled(pos, ImVec2(pos.x + tab_w, pos.y + tab_h), Pack(current_palette_->surface),
                               studio_metrics::radius_chip * scale);
        } else {
            draw.AddRectFilled(pos, ImVec2(pos.x + tab_w, pos.y + tab_h), Pack(current_palette_->surface),
                               studio_metrics::radius_chip * scale);
        }
        if (ImGui::IsItemFocused()) {
            draw.AddRect(ImVec2(pos.x - 2.0f * scale, pos.y - 2.0f * scale),
                         ImVec2(pos.x + tab_w + 2.0f * scale, pos.y + tab_h + 2.0f * scale),
                         Pack(current_palette_->accent), studio_metrics::radius_chip * scale, 0, 2.0f * scale);
        }
        ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
        draw.AddText(ImVec2(pos.x + 12.0f * scale, pos.y + (tab_h - text_size.y) * 0.5f), Pack(current_palette_->text),
                     display.c_str());
        ImGui::PopFont();
        if (pressed) {
            active_index_ = static_cast<int>(i);
            inputs_synced_ = false;
        }
        ImGui::SameLine(0.0f, 0.0f);
        const ImVec2 close_pos = ImGui::GetCursorScreenPos();
        const std::string close_id = "##tabclose_" + tab.id;
        const bool close_pressed =
            ImGui::InvisibleButton(close_id.c_str(), ImVec2(28.0f * scale, tab_h), ImGuiButtonFlags_EnableNav);
        draw.AddText(ImVec2(close_pos.x + 8.0f * scale, close_pos.y + (tab_h - 16.0f * scale) * 0.5f),
                     Pack(current_palette_->muted), "x");
        if (close_pressed) {
            close_index = static_cast<int>(i);
        }
        if (active_index_ < 0 || active_index_ >= static_cast<int>(tabs_.size())) {
            break;
        }
        ImGui::SameLine(0.0f, 4.0f * scale);
    }
    if (close_index >= 0 && close_index < static_cast<int>(tabs_.size())) {
        if (close_index == active_index_) {
            SaveActiveTab();
            tabs_.erase(tabs_.begin() + static_cast<std::ptrdiff_t>(close_index));
            if (tabs_.empty()) {
                active_index_ = -1;
            } else {
                active_index_ = 0;
            }
            inputs_synced_ = false;
        } else {
            SaveActiveTab();
            tabs_.erase(tabs_.begin() + static_cast<std::ptrdiff_t>(close_index));
            if (active_index_ > close_index) {
                active_index_ -= 1;
            }
            inputs_synced_ = false;
        }
    }
    ImGui::SameLine(0.0f, 4.0f * scale);
    if (ImGui::InvisibleButton("##tab_plus", ImVec2(32.0f * scale, 32.0f * scale), ImGuiButtonFlags_EnableNav)) {
        OpenNewNote();
    }
    ImGui::Separator();
}

void ScratchpadWindow::DrawToolbar(float scale) {
    const char* labels[] = {"Bold", "Italic", "Bullet", "Numbered", "Heading", "Mic"};
    const lwf::ui::scratchpad::ToolbarAction actions[] = {lwf::ui::scratchpad::ToolbarAction::Bold,
                                                          lwf::ui::scratchpad::ToolbarAction::Italic,
                                                          lwf::ui::scratchpad::ToolbarAction::Bullet,
                                                          lwf::ui::scratchpad::ToolbarAction::Numbered,
                                                          lwf::ui::scratchpad::ToolbarAction::Heading};
    for (int i = 0; i < 6; ++i) {
        if (i > 0) {
            ImGui::SameLine(0.0f, 6.0f * scale);
        }
        const std::string id = std::string("##toolbar_") + labels[i];
        if (ImGui::Button(id.c_str(), ImVec2(0.0f, 32.0f * scale))) {
            if (i == 5) {
                focus_editor_pending_ = true;
            } else {
                ApplyToolbarAction(actions[i]);
            }
        }
    }
    ImGui::Separator();
}

void ScratchpadWindow::DrawEditor(float scale) {
    if (tabs_.empty() || active_index_ < 0 || active_index_ >= static_cast<int>(tabs_.size())) {
        ImGui::TextDisabled("No note open.");
        return;
    }
    ImGui::SetNextItemWidth(-1.0f * scale);
    ImGui::InputText("##editor_title", title_input_, sizeof(title_input_));
    if (body_input_.size() < 2) {
        body_input_.assign(2, '\0');
    }
    const float editor_h = 320.0f * scale;
    if (focus_editor_pending_) {
        ImGui::SetKeyboardFocusHere();
        focus_editor_pending_ = false;
    }
    ImGui::InputTextMultiline("##editor_body", body_input_.data(), body_input_.size(),
                              ImVec2(-1.0f * scale, editor_h),
                              ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_EnterReturnsTrue,
                              &ScratchpadBodyCallback, this);
    SyncActiveTabFromInputs();
}

void ScratchpadWindow::DrawBottomBar(float scale) {
    (void)scale;
    const std::string words = lwf::ui::scratchpad::CountWordsLabel(ActiveBody());
    ImGui::TextDisabled("%s", words.c_str());
    const double now = static_cast<double>(ImGui::GetTime());
    if (saved_until_ > 0.0 && now < saved_until_) {
        ImGui::SameLine();
        ImGui::TextDisabled("Saved");
    }
}

void ScratchpadWindow::RenderFrame(float delta_seconds) {
    (void)delta_seconds;
    if (window_ == nullptr || imgui_context_ == nullptr) {
        return;
    }
    if (glfwWindowShouldClose(window_)) {
        glfwSetWindowShouldClose(window_, 0);
        glfwHideWindow(window_);
        close_requested_ = true;
    }
    if (!glfwGetWindowAttrib(window_, GLFW_VISIBLE)) {
        dictation_active_.store(false);
        return;
    }
    RefreshDictationActive();
    ImGui::SetCurrentContext(imgui_context_);
    glfwMakeContextCurrent(window_);
    if (pending_dpi_scale_ > 0.0f || theme_pending_) {
        if (pending_dpi_scale_ > 0.0f) {
            dpi_scale_ = pending_dpi_scale_;
            pending_dpi_scale_ = 0.0f;
        }
        if (theme_pending_) {
            current_palette_ = pending_theme_palette_;
            pending_theme_palette_ = nullptr;
            theme_pending_ = false;
        }
        lwf::ui::ApplyImGuiStyle(*current_palette_, static_cast<float>(dpi_scale_));
    }
    int fb_width = 0;
    int fb_height = 0;
    glfwGetFramebufferSize(window_, &fb_width, &fb_height);
    glViewport(0, 0, fb_width, fb_height);
    const ImVec4 clear = Color(current_palette_->bg);
    glClearColor(clear.x, clear.y, clear.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwMakeContextCurrent(window_);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    const float scale = static_cast<float>(dpi_scale_);
    ImGui::PushFont(nullptr, studio_metrics::font_helper_px);
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * scale, 16.0f * scale));
    ImGui::Begin("##scratchpad_editor", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    DrainDictation();
    SyncInputsFromActiveTab();
    DrawTabs(scale);
    DrawToolbar(scale);
    DrawEditor(scale);
    DrawBottomBar(scale);
    ImGui::End();
    ImGui::PopFont();
    MarkDirtyAndSave();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
}
