#include "ui/MainWindow.h"

#include "audio/MiniaudioCapture.h"
#include "core/AppVersion.h"
#include "core/HotkeyBinding.h"
#include "core/Languages.h"
#include "core/Onboarding.h"
#include "core/SettingsStore.h"
#include "core/SettingsUiModel.h"
#include "core/UiBus.h"
#include "os/DataDir.h"
#include "os/FileDialogs.h"
#include "ui/theme/ThemePalette.h"
#include "ui/theme/ThemeStyle.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <GL/gl.h>

#ifdef _WIN32
extern "C" __declspec(dllimport) int __stdcall ShellExecuteA(void*, const char*, const char*, const char*, const char*, int);
extern "C" void* glfwGetWin32Window(GLFWwindow* window);
#endif

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

namespace metrics = lwf::ui::metrics;
using lwf::ui::Color;
using lwf::ui::kDarkPalette;
using lwf::ui::Pack;

constexpr float kRailWidth = 220.0f;

std::vector<std::string> SplitChord(const std::string& chord) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    for (;;) {
        const std::size_t plus = chord.find('+', start);
        if (plus == std::string::npos) {
            parts.push_back(chord.substr(start));
            break;
        }
        parts.push_back(chord.substr(start, plus - start));
        start = plus + 1;
    }
    std::vector<std::string> cleaned;
    for (std::string part : parts) {
        const std::size_t first = part.find_first_not_of(" \t");
        const std::size_t last = part.find_last_not_of(" \t");
        if (first != std::string::npos) {
            cleaned.push_back(part.substr(first, last - first + 1));
        }
    }
    return cleaned;
}

// A switch in the mockup's shape: pill track, round knob, accent when on.
bool ToggleSwitch(const char* id, bool value, float scale) {
    const float w = 52.0f * scale;
    const float h = 32.0f * scale;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool toggled = ImGui::InvisibleButton(id, ImVec2(w, h), ImGuiButtonFlags_EnableNav);
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    if (ImGui::IsItemFocused()) {
        // 2px focus ring (kDarkPalette.focus) so keyboard focus is visible.
        draw.AddRect(ImVec2(p.x - 2.0f * scale, p.y - 2.0f * scale),
                     ImVec2(p.x + w + 2.0f * scale, p.y + h + 2.0f * scale),
                     Pack(kDarkPalette.focus), h * 0.5f + 2.0f * scale, 0, 2.0f * scale);
    }
    draw.AddRectFilled(p, ImVec2(p.x + w, p.y + h),
                       value ? Pack(kDarkPalette.accent) : Pack(kDarkPalette.text, 0.15f),
                       h * 0.5f);
    const float knob = 24.0f * scale;
    const float pad = 4.0f * scale;
    const float x = value ? p.x + w - pad - knob : p.x + pad;
    draw.AddCircleFilled(ImVec2(x + knob * 0.5f, p.y + h * 0.5f), knob * 0.5f,
                         Pack(kDarkPalette.surface));
    return toggled;
}

}  // namespace

MainWindow::MainWindow(const DictationFSM& fsm, const MiniaudioCapture& audio,
                       SettingsStore& settings, UiBus& bus)
    : fsm_(fsm), audio_(audio), settings_(settings), bus_(bus) {
    page_ = PageForRoute(settings_.GetString("last_page"));
}

MainWindow::~MainWindow() {
    Shutdown();
}

bool MainWindow::Init() {
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

    // Work area first, so the created window never exceeds the screen: an
    // oversized window gets clamped by the OS and the layout shrinks with it.
    int area_x = 0;
    int area_y = 0;
    int area_w = 1280;
    int area_h = 720;
    if (primary != nullptr) {
        glfwGetMonitorWorkarea(primary, &area_x, &area_y, &area_w, &area_h);
    }
    const int want_w = static_cast<int>(metrics::full_default_w * dpi_scale_);
    const int want_h = static_cast<int>(metrics::full_default_h * dpi_scale_);

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window_ = glfwCreateWindow(std::min(want_w, area_w), std::min(want_h, area_h),
                               "Local Wisper Flow", nullptr, nullptr);
    if (window_ == nullptr) {
        return false;
    }
    glfwSetWindowSizeLimits(window_, static_cast<int>(metrics::full_min_w * dpi_scale_),
                            static_cast<int>(metrics::full_min_h * dpi_scale_), GLFW_DONT_CARE,
                            GLFW_DONT_CARE);

    // Center on the primary work area so the first open feels deliberate.
    int win_w = 0;
    int win_h = 0;
    glfwGetWindowSize(window_, &win_w, &win_h);
    glfwSetWindowPos(window_, area_x + (area_w - win_w) / 2, area_y + (area_h - win_h) / 2);

    // Follow monitor scale changes live (docking, display settings).
    glfwSetWindowUserPointer(window_, this);
    glfwSetWindowContentScaleCallback(window_, &MainWindow::OnContentScale);

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    // NOTE: ImGui::CreateContext() restores the previous context after
    // initializing the new one, so GetCurrentContext() would return another
    // window's context here. Use the returned context and make it current.
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
    lwf::ui::ApplyImGuiStyle(kDarkPalette, static_cast<float>(dpi_scale_));

    if (FirstRunNeeded()) {
        page_ = Page::Welcome;
        welcome_step_ = 1;
    }
    return true;
}

void MainWindow::Shutdown() noexcept {
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

void MainWindow::SetVisible(bool visible) {
    if (window_ == nullptr) {
        return;
    }
    if (visible) {
        glfwShowWindow(window_);
        page_focus_pending_ = true;  // refocus once keyboard nav is active
    } else if (glfwGetWindowAttrib(window_, GLFW_VISIBLE)) {
        glfwHideWindow(window_);
    }
}

bool MainWindow::IsVisible() const {
    return window_ != nullptr && glfwGetWindowAttrib(window_, GLFW_VISIBLE) != 0;
}

bool MainWindow::TakeCloseRequest() {
    const bool result = close_requested_;
    close_requested_ = false;
    return result;
}

void MainWindow::SetLastTranscript(const std::string& text) {
    last_transcript_ = text;
    copied_until_ = -1.0f;  // a new transcript is not yet copied
}

void MainWindow::SetDetectedHotkey(const std::string& chord) {
    detected_hotkey_ = chord;
    if (learn_target_ == "hotkey_ptt") {
        learn_target_.clear();
    }
}

void MainWindow::SetLearnedHotkey(const std::string& target_key, const std::string& chord) {
    if (learn_target_ == target_key) {
        learn_target_.clear();
    }
    if (target_key == "hotkey_ptt") {
        detected_hotkey_ = chord;
    }
    if (learn_error_target_ == target_key) {
        learn_error_.clear();
        learn_error_target_.clear();
    }
}

void MainWindow::SetRecorderError(const std::string& target_key, const std::string& message) {
    if (learn_target_ == target_key) {
        learn_target_.clear();
    }
    learn_error_target_ = target_key;
    learn_error_ = message;
    if (message.empty()) {
        learn_error_.clear();
        learn_error_target_.clear();
    }
}

bool MainWindow::FirstRunNeeded() const {
    return settings_.GetBool("first_run") || settings_.GetString("whisper_model_path").empty();
}

void MainWindow::OnContentScale(GLFWwindow* window, float scale_x, float) {
    if (MainWindow* self = static_cast<MainWindow*>(glfwGetWindowUserPointer(window))) {
        if (scale_x > 0.0f) {
            self->pending_dpi_scale_ = scale_x;
        }
    }
}

void MainWindow::RunOnce(float) {
    if (window_ == nullptr || imgui_context_ == nullptr) {
        return;
    }

    // Closing hides; the tray owns Quit.
    if (glfwWindowShouldClose(window_)) {
        glfwSetWindowShouldClose(window_, 0);
        glfwHideWindow(window_);
        close_requested_ = true;
    }
    if (!glfwGetWindowAttrib(window_, GLFW_VISIBLE)) {
        return;
    }

    ImGui::SetCurrentContext(imgui_context_);
    glfwMakeContextCurrent(window_);

    // A monitor scale change lands here, where this window's ImGui context
    // is current. The style (and, through FontScaleMain, the dynamic font)
    // re-apply so text and controls keep filling the window instead of
    // rendering at the old scale.
    if (pending_dpi_scale_ > 0.0f) {
        dpi_scale_ = pending_dpi_scale_;
        pending_dpi_scale_ = 0.0f;
        lwf::ui::ApplyImGuiStyle(kDarkPalette, static_cast<float>(dpi_scale_));
    }

    int fb_width = 0;
    int fb_height = 0;
    glfwGetFramebufferSize(window_, &fb_width, &fb_height);
    glViewport(0, 0, fb_width, fb_height);
    const ImVec4 clear = Color(kDarkPalette.bg);
    glClearColor(clear.x, clear.y, clear.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Re-assert the GL context before ImGui backend calls, since
    // Overlay::RenderFrame() may have left the overlay's context current.
    glfwMakeContextCurrent(window_);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const float scale = static_cast<float>(dpi_scale_);

    // Body text renders at the design-px token; display sizes below push
    // narrower scopes with their own design-px tokens. The DPI factor is
    // applied once via style.FontScaleMain in ApplyImGuiStyle.
    ImGui::PushFont(nullptr, metrics::font_body_px);

    // Host window fills the client area; rail and page are children.
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##full", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    DrawRail(scale);

    const float rail_w = kRailWidth * scale;
    const ImVec2 content_pos(rail_w, 0.0f);
    const ImVec2 content_size(ImGui::GetIO().DisplaySize.x - rail_w,
                              ImGui::GetIO().DisplaySize.y);
    ImGui::SetCursorScreenPos(content_pos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(metrics::space_xl * scale, metrics::space_xl * scale));
    ImGui::BeginChild("##content", content_size, ImGuiChildFlags_None);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    RenderPages(scale);

    ImGui::EndChild();

    // "Saved" flashes top-right of the content area after a live change.
    if (saved_until_ > 0.0f && ImGui::GetTime() < saved_until_) {
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        const ImVec2 text_size = ImGui::CalcTextSize("Saved");
        const float right = content_pos.x + content_size.x - metrics::space_m * scale;
        draw.AddText(ImVec2(right - text_size.x, content_pos.y + 10.0f * scale),
                     Pack(kDarkPalette.muted), "Saved");
    }

    ImGui::End();
    ImGui::PopFont();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
}

void MainWindow::RenderPages(float scale) {
    switch (page_) {
        case Page::Welcome:
            DrawWelcomePage(scale);
            break;
        case Page::Home:
            DrawHomePage(scale);
            break;
        case Page::Insights:
            DrawInsightsPage(scale);
            break;
        case Page::Dictionary:
            DrawDictionaryPage(scale);
            break;
        case Page::Snippets:
            DrawSnippetsPage(scale);
            break;
        case Page::Transforms:
            DrawTransformsPage(scale);
            break;
        case Page::Scratchpad:
            DrawScratchpadPage(scale);
            break;
        case Page::Settings:
            DrawSettingsPage(scale);
            break;
    }
}

void MainWindow::DrawRail(float scale) {
    const ImVec2 size(kRailWidth * scale, ImGui::GetIO().DisplaySize.y);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(kDarkPalette.surface));
    ImGui::BeginChild("##rail", size, ImGuiChildFlags_None);
    ImGui::PopStyleColor();
    {
        const ImVec2 rail_pos = ImGui::GetWindowPos();
        const float rail_right = rail_pos.x + size.x;
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        draw.AddLine(ImVec2(rail_right - 1.0f, rail_pos.y),
                     ImVec2(rail_right - 1.0f, rail_pos.y + size.y), Pack(kDarkPalette.border));

        // Wordmark at the design size until real fonts land.
        ImGui::PushFont(nullptr, metrics::font_wordmark_px);
        ImGui::TextUnformatted("Local Wisper Flow");
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

        struct NavItem {
            const char* id;
            const char* label;
            Page page;
        };
        const NavItem items[] = {
            {"##nav_home", "Home", Page::Home},
            {"##nav_insights", "Insights", Page::Insights},
            {"##nav_dictionary", "Dictionary", Page::Dictionary},
            {"##nav_snippets", "Snippets", Page::Snippets},
            {"##nav_transforms", "Transforms", Page::Transforms},
            {"##nav_scratchpad", "Scratchpad", Page::Scratchpad},
            {"##nav_settings", "Settings", Page::Settings},
        };

        const float item_h = 40.0f * scale;
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        const float font_h = ImGui::GetFontSize();
        // Restore keyboard focus after a page switch or re-show, only when the
        // user is actually driving with the keyboard (NavActive) so a mouse
        // click or tray pop never steals focus. The next focusable item (the
        // first rail button) receives it. Any mouse use cancels a stale arm:
        // a mouse-first user who clicks anywhere (rail or content) must not
        // have the next Tab yank focus back to the rail (R-W2-003).
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            page_focus_pending_ = false;
        }
        if (page_focus_pending_ && ImGui::GetIO().NavActive) {
            ImGui::SetKeyboardFocusHere();
            page_focus_pending_ = false;
        }
        for (const NavItem& item : items) {
            if (item.page == Page::Settings) {
                ImGui::Dummy(ImVec2(0.0f, metrics::space_s * scale));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, metrics::space_s * scale));
            }
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x;
            // DECISION: switch on the InvisibleButton activation return, not
            // IsItemClicked. Vendored IsItemClicked is IsMouseClicked &&
            // IsItemHovered, while keyboard (nav) activation returns
            // pressed=true without setting HoveredId, so discarding the
            // return made Enter/Space on a focused rail item a no-op
            // (R-W2-001). The return covers mouse and keyboard alike.
            const bool pressed =
                ImGui::InvisibleButton(item.id, ImVec2(w, item_h), ImGuiButtonFlags_EnableNav);
            if (pressed) {
                page_ = item.page;
                PersistPage();
                validation_error_.clear();  // errors belong to the page that set them
                // Focus is already on the pressed item; never arm the restore
                // here (mouse or keyboard). A stale arm would yank focus to
                // the first rail button on a later Tab (R-W2-003).
                page_focus_pending_ = false;
            }
            const bool active = page_ == item.page;
            const bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemFocused()) {
                draw.AddRect(ImVec2(p.x - 2.0f * scale, p.y - 2.0f * scale),
                             ImVec2(p.x + w + 2.0f * scale, p.y + item_h + 2.0f * scale),
                             Pack(kDarkPalette.focus), 2.0f * scale, 0, 2.0f * scale);
            }
            const std::uint32_t color =
                active ? kDarkPalette.text : (hovered ? kDarkPalette.text : kDarkPalette.muted);
            if (active) {
                draw.AddRectFilled(p, ImVec2(p.x + w, p.y + item_h),
                                   Pack(kDarkPalette.text, 0.10f), 8.0f * scale);
            }
            draw.AddText(ImVec2(p.x + 19.0f * scale, p.y + (item_h - font_h) * 0.5f),
                         Pack(color), item.label);
        }
        ImGui::PopFont();

        // Rail bottom: live status and the way back to mini.
        const float line = ImGui::GetTextLineHeightWithSpacing();
        const float bottom_h =
            2.0f * metrics::space_m * scale + 3.0f * line + 8.0f * scale + 32.0f * scale;
        if (ImGui::GetContentRegionAvail().y > bottom_h) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
                                 ImGui::GetContentRegionAvail().y - bottom_h);
        }
        ImGui::Separator();
        const State fsm_state = fsm_.CurrentState();
        const bool enabled = settings_.GetBool("dictation_enabled");
        const char* status_word = "Ready";
        std::uint32_t status_color = kDarkPalette.ok;
        if (!enabled) {
            status_word = "Paused";
            status_color = kDarkPalette.muted;
        } else if (fsm_state == State::CAPTURING) {
            status_word = "Listening";
            status_color = kDarkPalette.accent;
        } else if (fsm_state == State::TRANSCRIBING || fsm_state == State::POSTPROCESSING ||
                   fsm_state == State::EMITTING) {
            status_word = "Thinking";
            status_color = kDarkPalette.text;
        }
        const ImVec2 status_pos = ImGui::GetCursorScreenPos();
        draw.AddCircleFilled(ImVec2(status_pos.x + 4.0f * scale, status_pos.y + line * 0.5f),
                             4.0f * scale, Pack(status_color));
        ImGui::Dummy(ImVec2(14.0f * scale, 0.0f));
        ImGui::SameLine();
        ImGui::TextUnformatted(status_word);
        ImGui::TextDisabled("The mini dot shows this too.");
        if (ImGui::Button("Hide window")) {
            SetVisible(false);
        }
    }
    ImGui::EndChild();
}

void MainWindow::DrawPageChrome(const char* title, const char* subtitle, float scale) {
    ImGui::PushFont(nullptr, metrics::font_page_title_px);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    ImGui::TextDisabled("%s", subtitle);
    ImGui::Dummy(ImVec2(0.0f, metrics::space_s * scale));
}

void MainWindow::BeginCard(const char* id, float width, float scale) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(kDarkPalette.surface));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, metrics::radius_card * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(metrics::space_m * scale, metrics::space_m * scale));
    ImGui::BeginChild(id, ImVec2(width, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void MainWindow::EndCard(float scale) {
    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));
}

bool MainWindow::ToggleRow(const char* label, bool value, const char* helper, float scale) {
    const float start_x = ImGui::GetCursorPosX();
    const float start_y = ImGui::GetCursorPosY();
    const float line = ImGui::GetTextLineHeightWithSpacing();
    const float row_h = (helper != nullptr && helper[0] != '\0') ? 2.0f * line : line;
    const float switch_w = 52.0f * scale;
    const float switch_h = 32.0f * scale;
    const float avail = ImGui::GetContentRegionAvail().x;

    // Switch on the right, vertically centered in the row.
    ImGui::SetCursorPos(ImVec2(start_x + avail - switch_w, start_y + (row_h - switch_h) * 0.5f));
    const bool toggled = ToggleSwitch(label, value, scale);

    // Label (and helper) on the left.
    const float text_h = row_h - 2.0f * scale;
    ImGui::SetCursorPos(ImVec2(start_x, start_y + (row_h - text_h) * 0.5f));
    ImGui::TextUnformatted(label);
    if (helper != nullptr && helper[0] != '\0') {
        ImGui::TextDisabled("%s", helper);
    }

    // No trailing SetCursorPos to the next row: the final text item already
    // auto-advanced the cursor onto it. A trailing move past the item-driven
    // content boundary without a following item tripped ImGui's SetCursorPos
    // boundary check at End ("Code uses SetCursorPos() to extend window/
    // parent boundaries; submit an item e.g. Dummy() afterwards").
    return toggled;
}

void MainWindow::ApplyAndSave() {
    settings_.Save();
    bus_.PostCommand(UiCommandType::ApplySettings);
    ShowSaved();
}

void MainWindow::ShowSaved() {
    saved_until_ = static_cast<float>(ImGui::GetTime()) + metrics::saved_dwell;
}

void MainWindow::IntRow(const char* key, const char* id, int min_value, int max_value,
                        const char* format) {
    int value = settings_.GetInt(key);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt(id, &value, min_value, max_value, format);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        validation_error_ = SettingsUiModel::ValidateInt(key, value, min_value, max_value);
        if (validation_error_.empty()) {
            settings_.SetInt(key, value);
            ApplyAndSave();
        }
    }
}

void MainWindow::DoubleRow(const char* key, const char* id, float min_value, float max_value,
                           const char* format) {
    float value = static_cast<float>(settings_.GetDouble(key));
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat(id, &value, min_value, max_value, format);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        settings_.SetDouble(key, static_cast<double>(value));
        ApplyAndSave();
    }
}

void MainWindow::BoolRow(const char* key, const char* label, const char* helper, float scale) {
    const bool value = settings_.GetBool(key);
    if (ToggleRow(label, value, helper, scale)) {
        settings_.SetBool(key, !value);
        ApplyAndSave();
    }
}

void MainWindow::StringFieldRow(const char* key, TextField& field, const char* id, bool restart) {
    field.Seed(settings_.GetString(key));
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText(id, field.buffer, sizeof(field.buffer));
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        settings_.SetString(key, field.buffer);
        if (restart) {
            restart_needed_ = true;
        }
        ApplyAndSave();
    }
}

void MainWindow::DeleteLocalData() {
    const std::filesystem::path base(GetDataDir());
    std::error_code ec;
    std::filesystem::remove(base / "stats.json", ec);
    std::filesystem::remove(base / "dictionary.json", ec);
    std::filesystem::remove(base / "snippets.json", ec);
    std::filesystem::remove(base / "transforms.json", ec);
    std::filesystem::remove_all(base / "notes", ec);
    std::filesystem::remove(base / "api_keys.json", ec);
    delete_data_armed_ = false;
}

void MainWindow::PersistPage() {
    if (page_ == Page::Welcome) {
        return;
    }
    settings_.SetString("last_page", RouteName(page_));
    settings_.Save();
}

#ifdef _WIN32
namespace {

void* DialogOwnerHwnd(GLFWwindow* window) {
    return window != nullptr ? glfwGetWin32Window(window) : nullptr;
}

}  // namespace

#endif

bool MainWindow::PickAndApplyModelPath(char* buffer, std::size_t buffer_size,
                                       const char* setting_key, std::string& error) {
    const std::string picked =
#ifdef _WIN32
        FileDialogs::PickModelFile(buffer, DialogOwnerHwnd(window_));
#else
        FileDialogs::PickModelFile(buffer);
#endif
    if (picked.empty()) {
        return false;
    }
    const std::string normalized = FileDialogs::NormalizeModelPath(picked);
    std::snprintf(buffer, buffer_size, "%s", normalized.c_str());
    error = SettingsUiModel::ValidateModelPath(buffer);
    if (!error.empty()) {
        return false;
    }
    settings_.SetString(setting_key, buffer);
    restart_needed_ = true;
    ApplyAndSave();
    return true;
}

void MainWindow::DrawRestartBanner(float scale) {
    if (!restart_needed_) {
        return;
    }
    ImGui::TextColored(Color(kDarkPalette.warn), "Applies after restart.");
    ImGui::Dummy(ImVec2(0.0f, metrics::space_xs * scale));
}

void MainWindow::DrawHotkeyChips(const std::string& chord, float scale) {
    if (chord.empty()) {
        ImGui::TextDisabled("No hotkey set yet.");
        return;
    }
    const std::vector<std::string> parts = SplitChord(chord);
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    const float chip_h = 36.0f * scale;
    const float pad_x = 14.0f * scale;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    float x = origin.x;
    const float y = origin.y;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const ImVec2 text_size = ImGui::CalcTextSize(parts[i].c_str());
        const float chip_w = text_size.x + 2.0f * pad_x;
        draw.AddRectFilled(ImVec2(x, y), ImVec2(x + chip_w, y + chip_h),
                           Pack(kDarkPalette.text, 0.05f), metrics::radius_control * scale);
        draw.AddRect(ImVec2(x, y), ImVec2(x + chip_w, y + chip_h), Pack(kDarkPalette.border),
                     metrics::radius_control * scale, 0, 1.0f * scale);
        draw.AddText(ImVec2(x + pad_x, y + (chip_h - text_size.y) * 0.5f),
                     Pack(kDarkPalette.text), parts[i].c_str());
        x += chip_w;
        if (i + 1 < parts.size()) {
            draw.AddText(ImVec2(x + 5.0f * scale, y + (chip_h - text_size.y) * 0.5f),
                         Pack(kDarkPalette.muted), "+");
            x += 18.0f * scale;
        }
    }
    ImGui::Dummy(ImVec2(x - origin.x, chip_h));
}

void MainWindow::DrawGeneralHotkeyRow(const std::string& config_key, const char* label,
                                      const char* helper, float scale) {
    const auto role = HotkeyBinding::RoleForConfigKey(config_key);
    const std::string current = settings_.GetString(config_key);
    ImGui::TextUnformatted(label);
    ImGui::TextDisabled("%s", helper);
    DrawHotkeyChips(current, scale);
    if (!current.empty() && role.has_value()) {
        const std::vector<Binding> registry = LoadBindings(settings_);
        if (HotkeyBinding::FindConflict(current, registry, role).has_value()) {
            ImGui::TextColored(Color(kDarkPalette.err), "That shortcut is already in use");
        }
    }
    const bool armed = learn_target_ == config_key;
    const bool any_armed = !learn_target_.empty();
    if (armed) {
        ImGui::TextDisabled("Press a chord...");
    } else {
        const bool buttons_disabled = any_armed;
        if (buttons_disabled) {
            ImGui::BeginDisabled();
        }
        const std::string record_id = "Record##" + config_key;
        if (ImGui::Button(record_id.c_str())) {
            learn_target_ = config_key;
            if (learn_error_target_ == config_key) {
                learn_error_.clear();
                learn_error_target_.clear();
            }
            bus_.PostCommand(UiCommandType::StartLearnHotkey,
                             UiBus::StartLearnPayload(config_key));
        }
        ImGui::SameLine(0.0f, 8.0f * scale);
        const std::string clear_id = "Clear##" + config_key;
        if (ImGui::Button(clear_id.c_str())) {
            settings_.SetString(config_key, "");
            ApplyAndSave();
        }
        if (buttons_disabled) {
            ImGui::EndDisabled();
        }
    }
    if (learn_error_target_ == config_key && !learn_error_.empty()) {
        ImGui::TextColored(Color(kDarkPalette.err), "%s", learn_error_.c_str());
    }
}

void MainWindow::DrawHomePage(float scale) {
    DrawPageChrome("Home", "Dictation runs on this computer and starts with one hotkey.", scale);

    const float width = std::min(ImGui::GetContentRegionAvail().x, metrics::content_max * scale);
    const State fsm_state = fsm_.CurrentState();
    const bool enabled = settings_.GetBool("dictation_enabled");

    BeginCard("##card_now", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Right now");
        ImGui::PopFont();
        const std::string hotkey = settings_.GetString("hotkey_ptt");
        const char* rest = nullptr;
        std::uint32_t status_color = kDarkPalette.ok;
        const char* status_word = "Ready";
        if (!enabled) {
            status_word = "Paused";
            status_color = kDarkPalette.muted;
            rest = "Dictation is off.";
        } else if (fsm_state == State::CAPTURING) {
            status_word = "Listening";
            status_color = kDarkPalette.accent;
            rest = "Speak, then release.";
        } else if (fsm_state == State::TRANSCRIBING || fsm_state == State::POSTPROCESSING ||
                   fsm_state == State::EMITTING) {
            status_word = "Thinking";
            status_color = kDarkPalette.text;
            rest = "Turning speech into text.";
        } else if (hotkey.empty()) {
            rest = "Set a hotkey to start.";
        } else {
            rest = "Hold the hotkey to dictate.";
        }
        const ImVec2 word_pos = ImGui::GetCursorScreenPos();
        const float line = ImGui::GetTextLineHeight();
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        draw.AddCircleFilled(ImVec2(word_pos.x + 4.0f * scale, word_pos.y + line * 0.5f),
                             4.0f * scale, Pack(status_color));
        ImGui::Dummy(ImVec2(14.0f * scale, 0.0f));
        ImGui::SameLine();
        ImGui::TextUnformatted(status_word);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", rest);
    }
    EndCard(scale);

    BeginCard("##card_hotkey", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Hotkey");
        ImGui::PopFont();
        DrawHotkeyChips(settings_.GetString("hotkey_ptt"), scale);
        ImGui::TextDisabled("Hold to speak, release to type.");
        if (ImGui::Button("Learn a new chord...")) {
            bus_.PostCommand(UiCommandType::StartLearnHotkey);
        }
    }
    EndCard(scale);

    BeginCard("##card_trust", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Trust");
        ImGui::PopFont();
        const std::string provider = settings_.GetString("stt_provider");
        const bool cloud_active = provider != "local" && settings_.GetBool("stt_consent_accepted");
        if (cloud_active) {
            const char* display = provider == "gnani" ? "Gnani"
                                  : provider == "elevenlabs" ? "ElevenLabs"
                                  : provider == "muse" ? "Muse" : provider.c_str();
            ImGui::Text("Speech is sent to %s for each sentence.", display);
            ImGui::TextDisabled("If a cloud request fails, that utterance falls back to "
                                "local Whisper.");
        } else {
            ImGui::TextUnformatted("Everything runs on this computer.");
            ImGui::TextDisabled("Cloud speech is off.");
        }
    }
    EndCard(scale);

    BeginCard("##card_quick", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Quick settings");
        ImGui::PopFont();
        bool dictation = settings_.GetBool("dictation_enabled");
        if (ToggleRow("Dictation", dictation, nullptr, scale)) {
            settings_.SetBool("dictation_enabled", !dictation);
            bus_.PostCommand(UiCommandType::SetEnabled, !dictation ? "1" : "0");
            ApplyAndSave();
        }
        bool cleanup = settings_.GetBool("cleanup_enabled");
        if (ToggleRow("Clean up text", cleanup, "Polish dictation with the local model", scale)) {
            settings_.SetBool("cleanup_enabled", !cleanup);
            ApplyAndSave();
        }
        bool hands_free = settings_.GetBool("double_tap_handsfree");
        if (ToggleRow("Hands-free", hands_free, "Double-tap to talk without holding", scale)) {
            settings_.SetBool("double_tap_handsfree", !hands_free);
            ApplyAndSave();
        }
    }
    EndCard(scale);

    BeginCard("##card_last", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Last dictation");
        ImGui::PopFont();
        if (last_transcript_.empty()) {
            ImGui::TextDisabled("Nothing yet.");
        } else {
            ImGui::TextWrapped("%s", last_transcript_.c_str());
            const bool copied = copied_until_ > 0.0f && ImGui::GetTime() < copied_until_;
            if (ImGui::Button(copied ? "Copied" : "Copy")) {
                ImGui::SetClipboardText(last_transcript_.c_str());
                copied_until_ = static_cast<float>(ImGui::GetTime()) + metrics::saved_dwell;
            }
        }
    }
    EndCard(scale);
}

void MainWindow::DrawSpeechPage(float scale) {
    DrawPageChrome("Speech", "The speech model and how it listens.", scale);
    DrawRestartBanner(scale);

    const float width = std::min(ImGui::GetContentRegionAvail().x, metrics::content_max * scale);

    BeginCard("##card_model", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Model");
        ImGui::PopFont();

        if (model_path_buffer_[0] == '\0') {
            const std::string current = settings_.GetString("whisper_model_path");
            std::snprintf(model_path_buffer_, sizeof(model_path_buffer_), "%s", current.c_str());
        }
        const float browse_width = ImGui::CalcTextSize("Browse...").x +
                                   ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetNextItemWidth(-(browse_width + ImGui::GetStyle().ItemSpacing.x));
        ImGui::InputText("##model_path", model_path_buffer_, sizeof(model_path_buffer_));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            validation_error_ = SettingsUiModel::ValidateModelPath(model_path_buffer_);
            if (validation_error_.empty()) {
                settings_.SetString("whisper_model_path", model_path_buffer_);
                restart_needed_ = true;
                ApplyAndSave();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse...##browse_whisper")) {
            PickAndApplyModelPath(model_path_buffer_, sizeof(model_path_buffer_),
                                 "whisper_model_path", validation_error_);
        }
        if (!validation_error_.empty()) {
            ImGui::TextColored(Color(kDarkPalette.err), "%s", validation_error_.c_str());
        }
        ImGui::TextDisabled("Restart is needed after changing the model path.");
        ImGui::Separator();
        ImGui::TextUnformatted("Cleanup model");
        if (llm_model_path_buffer_[0] == '\0') {
            const std::string current = settings_.GetString("llm_model_path");
            std::snprintf(llm_model_path_buffer_, sizeof(llm_model_path_buffer_), "%s",
                          current.c_str());
        }
        ImGui::SetNextItemWidth(-(browse_width + ImGui::GetStyle().ItemSpacing.x));
        ImGui::InputText("##llm_model_path", llm_model_path_buffer_,
                         sizeof(llm_model_path_buffer_));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            llm_model_error_ = SettingsUiModel::ValidateModelPath(llm_model_path_buffer_);
            if (llm_model_error_.empty()) {
                settings_.SetString("llm_model_path", llm_model_path_buffer_);
                restart_needed_ = true;
                ApplyAndSave();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse...##browse_llm")) {
            PickAndApplyModelPath(llm_model_path_buffer_, sizeof(llm_model_path_buffer_),
                                 "llm_model_path", llm_model_error_);
        }
        if (!llm_model_error_.empty()) {
            ImGui::TextColored(Color(kDarkPalette.err), "%s", llm_model_error_.c_str());
        }
        ImGui::TextDisabled("Leave empty to turn off cleanup.");
    }
    EndCard(scale);

    BeginCard("##card_lang", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Language");
        ImGui::PopFont();
        const std::string current = settings_.GetString("whisper_language");
        if (ImGui::BeginCombo("##language", current.c_str())) {
            if (ImGui::Selectable("auto", current == "auto")) {
                settings_.SetString("whisper_language", "auto");
                ApplyAndSave();
            }
            for (const auto& language : Languages::WhisperLanguages()) {
                if (ImGui::Selectable(language.name, current == language.code)) {
                    settings_.SetString("whisper_language", language.code);
                    ApplyAndSave();
                }
            }
            ImGui::EndCombo();
        }
    }
    EndCard(scale);

    BeginCard("##card_listen", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Listening");
        ImGui::PopFont();
        bool boost = settings_.GetBool("quiet_speech_boost");
        if (ToggleRow("Boost quiet speech", boost, "Lifts a soft voice without clipping", scale)) {
            settings_.SetBool("quiet_speech_boost", !boost);
            ApplyAndSave();
        }
        ImGui::Separator();
        DoubleRow("vad_threshold", "##vad_threshold", 0.0f, 1.0f, "sensitivity %.2f");
        IntRow("min_hold_ms", "##min_hold_ms", 50, 5000, "hold at least %d ms");
        IntRow("max_recording_ms", "##max_recording_ms", 1000, 120000, "record at most %d ms");
        IntRow("hands_free_silence_ms", "##hands_free_silence_ms", 0, 10000, "stop after %d ms silence");
        IntRow("double_tap_window_ms", "##double_tap_window_ms", 100, 2000, "double-tap %d ms");
        BoolRow("avoid_altgr_conflict", "Avoid AltGr conflict", "Skip AltGr chords that conflict with Windows", scale);
    }
    EndCard(scale);

    BeginCard("##card_streaming", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Streaming");
        ImGui::PopFont();
        BoolRow("streaming_stt", "Stream speech", "Transcribe while you talk", scale);
        IntRow("streaming_min_segment_ms", "##streaming_min_segment_ms", 1000, 30000, "segment %d ms");
        IntRow("streaming_pause_ms", "##streaming_pause_ms", 300, 5000, "pause %d ms");
    }
    EndCard(scale);
}

void MainWindow::DrawCloudPage(float scale) {
    DrawPageChrome("Cloud", "Where your speech is transcribed.", scale);
    DrawRestartBanner(scale);

    const float width = std::min(ImGui::GetContentRegionAvail().x, metrics::content_max * scale);

    BeginCard("##card_cloud_provider", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Provider");
        ImGui::PopFont();

        static const char* kProviderItems[] = {"local", "gnani", "elevenlabs", "muse"};
        const std::string provider = settings_.GetString("stt_provider");
        int provider_index = provider == "gnani" ? 1
                             : provider == "elevenlabs" ? 2
                             : provider == "muse" ? 3 : 0;
        if (ImGui::BeginCombo("##stt_provider", kProviderItems[provider_index])) {
            for (int item = 0; item < 4; ++item) {
                if (ImGui::Selectable(kProviderItems[item], item == provider_index)) {
                    settings_.SetString("stt_provider", kProviderItems[item]);
                    restart_needed_ = true;
                    ApplyAndSave();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::TextWrapped("Local Whisper keeps every recording on this machine. A cloud "
                           "provider sends recordings to that provider's servers for "
                           "transcription.");
        ImGui::Separator();
        ImGui::TextUnformatted("Cloud language");
        StringFieldRow("stt_language", stt_language_field_, "##stt_language", false);
        ImGui::TextDisabled("ISO language, or a comma list (e.g. hi,en).");
        IntRow("stt_timeout_ms", "##stt_timeout_ms", 1000, 60000, "wait at most %d ms");
        IntRow("stt_max_clips", "##stt_max_clips", 1, 200, "at most %d clips");
    }
    EndCard(scale);

    BeginCard("##card_remote_engine", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Remote engine");
        ImGui::PopFont();
        BoolRow("use_remote", "Use remote engine", "Send audio to a local gRPC engine", scale);
        StringFieldRow("remote_endpoint", remote_endpoint_field_, "##remote_endpoint", false);
        ImGui::TextDisabled("Host and port of the remote engine.");
    }
    EndCard(scale);

    BeginCard("##card_cloud_consent", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Consent");
        ImGui::PopFont();

        bool consent = settings_.GetBool("stt_consent_accepted");
        if (ToggleRow("I understand; enable cloud transcription", consent, nullptr, scale)) {
            settings_.SetBool("stt_consent_accepted", !consent);
            restart_needed_ = true;
            ApplyAndSave();
        }
        if (consent && settings_.GetString("stt_provider") != "local") {
            ImGui::TextColored(Color(kDarkPalette.warn),
                               "Cloud is active only while the provider API key is present in "
                               "the environment.");
            ImGui::TextWrapped("The API key is read from the environment and is never written "
                               "to config.json, logs, or history. If a cloud request fails, "
                               "that utterance falls back to local Whisper.");
        }
    }
    EndCard(scale);
}

void MainWindow::DrawCleanupPage(float scale) {
    DrawPageChrome("Cleanup", "How dictation text is polished before it is typed.", scale);

    const float width = std::min(ImGui::GetContentRegionAvail().x, metrics::content_max * scale);

    BeginCard("##card_cleanup", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Cleanup");
        ImGui::PopFont();

        bool enabled = settings_.GetBool("cleanup_enabled");
        if (ToggleRow("Enable cleanup", enabled, nullptr, scale)) {
            settings_.SetBool("cleanup_enabled", !enabled);
            ApplyAndSave();
        }
        ImGui::Separator();

        int min_words = settings_.GetInt("cleanup_min_words");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderInt("##min_words", &min_words, 1, 100, "skip short: %d words");
        // Commit once, when the drag ends, not on every tick of the drag.
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            validation_error_ =
                SettingsUiModel::ValidateInt("cleanup_min_words", min_words, 1, 100);
            if (validation_error_.empty()) {
                settings_.SetInt("cleanup_min_words", min_words);
                ApplyAndSave();
            }
        }
        int timeout = settings_.GetInt("cleanup_timeout_ms");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderInt("##timeout", &timeout, 200, 30000, "give up after %d ms");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            settings_.SetInt("cleanup_timeout_ms", timeout);
            ApplyAndSave();
        }
        bool aggressive = settings_.GetBool("spoken_commands_aggressive");
        if (ToggleRow("Aggressive spoken commands", aggressive,
                      "Rewrite commands even inside longer sentences", scale)) {
            settings_.SetBool("spoken_commands_aggressive", !aggressive);
            ApplyAndSave();
        }
        BoolRow("dictionary_fuzzy", "Fuzzy dictionary matching", "Correct near-misses", scale);
        IntRow("command_timeout_ms", "##command_timeout_ms", 500, 60000, "command %d ms");
        IntRow("inject_delay_ms", "##inject_delay_ms", 0, 500, "wait %d ms before typing");
        StringFieldRow("cleanup_language_policy", cleanup_language_policy_field_,
                       "##cleanup_language_policy", false);
        ImGui::TextDisabled("Language policy for cleanup (e.g. keep).");
        {
            static const char* kSnippetItems[] = {"whole", "contains"};
            const std::string mode = settings_.GetString("snippet_mode");
            int mode_index = mode == "contains" ? 1 : 0;
            if (ImGui::BeginCombo("##snippet_mode", kSnippetItems[mode_index])) {
                for (int item = 0; item < 2; ++item) {
                    if (ImGui::Selectable(kSnippetItems[item], item == mode_index)) {
                        settings_.SetString("snippet_mode", kSnippetItems[item]);
                        ApplyAndSave();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::TextDisabled("When snippets expand.");
        }
        if (!validation_error_.empty()) {
            ImGui::TextColored(Color(kDarkPalette.err), "%s", validation_error_.c_str());
        }
    }
    EndCard(scale);

    BeginCard("##card_cleanup_backend", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Cleanup backend");
        ImGui::PopFont();
        static const char* kLlmItems[] = {"local", "openrouter"};
        const std::string backend = settings_.GetString("llm_provider");
        int backend_index = backend == "openrouter" ? 1 : 0;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##llm_provider", kLlmItems[backend_index])) {
            for (int item = 0; item < 2; ++item) {
                if (ImGui::Selectable(kLlmItems[item], item == backend_index)) {
                    settings_.SetString("llm_provider", kLlmItems[item]);
                    ApplyAndSave();
                }
            }
            ImGui::EndCombo();
        }
        if (settings_.GetString("llm_provider") == "openrouter") {
            ImGui::TextColored(Color(kDarkPalette.warn),
                               "Cleanup uses OpenRouter: text is sent to OpenRouter when cleanup is on.");
        } else {
            ImGui::TextDisabled("Local cleanup runs on this machine.");
        }
    }
    EndCard(scale);
}

void MainWindow::DrawPrivacyPage(float scale) {
    DrawPageChrome("Privacy", "What stays on this computer, and what gets written down.", scale);

    const float width = std::min(ImGui::GetContentRegionAvail().x, metrics::content_max * scale);

    BeginCard("##card_privacy", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Privacy");
        ImGui::PopFont();

        bool debug_transcripts = settings_.GetBool("debug_log_transcripts");
        if (ToggleRow("Log transcript text", debug_transcripts,
                      "Writes dictated text to the local log file", scale)) {
            settings_.SetBool("debug_log_transcripts", !debug_transcripts);
            ApplyAndSave();
        }
        if (settings_.GetBool("debug_log_transcripts")) {
            ImGui::TextColored(Color(kDarkPalette.err),
                               "WARNING: dictated text will be written to the local log.");
        }
        ImGui::Separator();
        BoolRow("history_enabled", "Keep history", "Save a local history of dictations", scale);
        BoolRow("keep_raw_in_history", "Keep raw text in history", "Store the text before cleanup", scale);
        IntRow("history_max_entries", "##history_max_entries", 10, 100000, "keep %d entries");
        IntRow("history_max_age_days", "##history_max_age_days", 1, 3650, "delete after %d days");
        BoolRow("keep_usage_stats", "Keep usage statistics", "Used by Insights", scale);
        if (settings_.GetString("llm_provider") == "openrouter") {
            ImGui::TextColored(Color(kDarkPalette.warn),
                               "Cleanup uses OpenRouter: text is sent to OpenRouter when cleanup is on.");
        }
        ImGui::Separator();
        ImGui::TextWrapped("%s", "No telemetry. No network calls except explicit model downloads.");
    }
    EndCard(scale);

    BeginCard("##card_delete_data", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Local data");
        ImGui::PopFont();
        if (!delete_data_armed_) {
            if (ImGui::Button("Delete all local data")) {
                delete_data_armed_ = true;
            }
            ImGui::TextDisabled("Removes statistics, dictionary, snippets, transforms, notes and saved API keys. Settings are kept.");
        } else {
            ImGui::TextColored(Color(kDarkPalette.err),
                               "This cannot be undone. The app keeps running; restart to reseed empty files.");
            if (ImGui::Button("Confirm delete")) {
                DeleteLocalData();
                ShowSaved();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                delete_data_armed_ = false;
            }
        }
    }
    EndCard(scale);
}

void MainWindow::DrawAboutPage(float scale) {
    DrawPageChrome("About", "The short version.", scale);

    const float width = std::min(ImGui::GetContentRegionAvail().x, metrics::content_max * scale);

    BeginCard("##card_about", width, scale);
    {
        ImGui::Text("Local Wisper Flow %s", lwf::AppVersion());
        ImGui::Separator();
        ImGui::TextWrapped("%s",
                           "Local-first voice dictation. Whisper STT, optional local LLM cleanup, "
                           "clipboard injection. All processing stays on this machine.");
        if (settings_.GetString("llm_provider") == "openrouter") {
            ImGui::TextColored(Color(kDarkPalette.warn),
                               "Cleanup uses OpenRouter: text is sent to OpenRouter when cleanup is on.");
        }
        ImGui::Text("Config version: %d", settings_.GetInt("schema_version"));
    }
    EndCard(scale);
}

void MainWindow::DrawInsightsPage(float scale) {
    DrawPageChrome("Insights", "Your dictation at a glance.", scale);
    ImGui::TextDisabled("Not available yet.");
}

void MainWindow::DrawDictionaryPage(float scale) {
    DrawPageChrome("Dictionary", "Words the app corrects.", scale);
    ImGui::TextDisabled("Not available yet.");
}

void MainWindow::DrawSnippetsPage(float scale) {
    DrawPageChrome("Snippets", "Reusable phrases.", scale);
    ImGui::TextDisabled("Not available yet.");
}

void MainWindow::DrawTransformsPage(float scale) {
    DrawPageChrome("Transforms", "Rewrite selected text.", scale);
    ImGui::TextDisabled("Not available yet.");
}

void MainWindow::DrawScratchpadPage(float scale) {
    DrawPageChrome("Scratchpad", "A scratch space for notes.", scale);
    ImGui::TextDisabled("Not available yet.");
}

void MainWindow::DrawGeneralPage(float scale) {
    DrawPageChrome("General", "Everyday controls in one place.", scale);

    const float width = std::min(ImGui::GetContentRegionAvail().x, metrics::content_max * scale);

    BeginCard("##card_general_hotkey", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Hotkey");
        ImGui::PopFont();
        DrawGeneralHotkeyRow("hotkey_ptt", "Push-to-talk", "Hold to speak, release to type.",
                             scale);
        ImGui::Separator();
        DrawGeneralHotkeyRow("hotkey_handsfree", "Hands-free",
                             "Double-tap to talk without holding.", scale);
        ImGui::Separator();
        DrawGeneralHotkeyRow("hotkey_cancel", "Cancel", "Stop dictation immediately.", scale);
        ImGui::Separator();
        DrawGeneralHotkeyRow("hotkey_command", "Command mode", "Hold to dictate a command.",
                             scale);
        ImGui::Separator();
        DrawGeneralHotkeyRow("hotkey_paste_last", "Paste last",
                             "Paste your last dictation.", scale);
    }
    EndCard(scale);

    BeginCard("##card_general_sound", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Sound");
        ImGui::PopFont();
        bool sound = settings_.GetBool("sound_enabled");
        if (ToggleRow("Sound", sound, "Play short sounds while dictating", scale)) {
            settings_.SetBool("sound_enabled", !sound);
            ApplyAndSave();
        }
        DoubleRow("sound_volume", "##sound_volume", 0.0f, 1.0f, "volume %.2f");
        ImGui::TextUnformatted("Microphone");
        StringFieldRow("mic_device", mic_device_field_, "##mic_device", true);
        ImGui::TextDisabled("Empty means the system default device. Applies after restart.");
    }
    EndCard(scale);

    BeginCard("##card_general_motion", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Motion");
        ImGui::PopFont();
        bool reduced = settings_.GetBool("reduce_motion");
        if (ToggleRow("Reduce motion", reduced, "Stop animations", scale)) {
            settings_.SetBool("reduce_motion", !reduced);
            ApplyAndSave();
        }
    }
    EndCard(scale);

    BeginCard("##card_general_window", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Window");
        ImGui::PopFont();
        const int mini_x = settings_.GetInt("mini_x");
        const int mini_y = settings_.GetInt("mini_y");
        if (mini_x < 0 && mini_y < 0) {
            ImGui::TextDisabled("Mini window: default position (bottom center).");
        } else {
            ImGui::Text("Mini window position: %d, %d", mini_x, mini_y);
        }
        ImGui::TextDisabled("Set by dragging the mini window.");
        BoolRow("scratchpad_flow_bar", "Scratchpad bar", "Show the scratchpad bar on the mini window", scale);
    }
    EndCard(scale);

    BeginCard("##card_general_other_hotkeys", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Other hotkeys");
        ImGui::PopFont();
        ImGui::TextUnformatted("Hands-free");
        StringFieldRow("hotkey_handsfree", hotkey_handsfree_field_, "##hotkey_handsfree", false);
        ImGui::TextUnformatted("Cancel");
        StringFieldRow("hotkey_cancel", hotkey_cancel_field_, "##hotkey_cancel", false);
        ImGui::TextUnformatted("Command mode");
        StringFieldRow("hotkey_command", hotkey_command_field_, "##hotkey_command", false);
        ImGui::TextUnformatted("Paste last");
        StringFieldRow("hotkey_paste_last", hotkey_paste_last_field_, "##hotkey_paste_last", false);
        ImGui::TextDisabled("Chords as text, e.g. Ctrl+Shift+T.");
    }
    EndCard(scale);

    BeginCard("##card_general_legacy", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Legacy");
        ImGui::PopFont();
        IntRow("hotkey_code", "##hotkey_code", 0, 255, "legacy key code %d");
    }
    EndCard(scale);
}

void MainWindow::DrawSettingsPage(float scale) {
    struct TabItem {
        const char* id;
        const char* label;
        SettingsTab tab;
    };
    const TabItem tabs[] = {
        {"##tab_general", "General", SettingsTab::General},
        {"##tab_speech", "Speech", SettingsTab::Speech},
        {"##tab_cloud", "Cloud", SettingsTab::Cloud},
        {"##tab_cleanup", "Cleanup", SettingsTab::Cleanup},
        {"##tab_privacy", "Privacy", SettingsTab::Privacy},
        {"##tab_about", "About", SettingsTab::About},
    };
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    const float tab_h = 36.0f * scale;
    const float tab_pad = 16.0f * scale;
    for (std::size_t i = 0; i < 6; ++i) {
        const TabItem& item = tabs[i];
        if (i > 0) {
            ImGui::SameLine(0.0f, 8.0f * scale);
        }
        const ImVec2 text_size = ImGui::CalcTextSize(item.label);
        const float w = text_size.x + 2.0f * tab_pad;
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const bool pressed =
            ImGui::InvisibleButton(item.id, ImVec2(w, tab_h), ImGuiButtonFlags_EnableNav);
        if (pressed) {
            settings_tab_ = item.tab;
            validation_error_.clear();
            llm_model_error_.clear();
        }
        const bool active = settings_tab_ == item.tab;
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemFocused()) {
            draw.AddRect(ImVec2(p.x - 2.0f * scale, p.y - 2.0f * scale),
                         ImVec2(p.x + w + 2.0f * scale, p.y + tab_h + 2.0f * scale),
                         Pack(kDarkPalette.focus), 8.0f * scale, 0, 2.0f * scale);
        }
        if (active) {
            draw.AddRectFilled(p, ImVec2(p.x + w, p.y + tab_h),
                               Pack(kDarkPalette.text, 0.10f), 8.0f * scale);
        }
        const std::uint32_t color =
            active ? kDarkPalette.text : (hovered ? kDarkPalette.text : kDarkPalette.muted);
        draw.AddText(ImVec2(p.x + tab_pad, p.y + (tab_h - text_size.y) * 0.5f), Pack(color),
                     item.label);
    }
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, metrics::space_s * scale));
    switch (settings_tab_) {
        case SettingsTab::General:
            DrawGeneralPage(scale);
            break;
        case SettingsTab::Speech:
            DrawSpeechPage(scale);
            break;
        case SettingsTab::Cloud:
            DrawCloudPage(scale);
            break;
        case SettingsTab::Cleanup:
            DrawCleanupPage(scale);
            break;
        case SettingsTab::Privacy:
            DrawPrivacyPage(scale);
            break;
        case SettingsTab::About:
            DrawAboutPage(scale);
            break;
    }
}

void MainWindow::DrawWelcomePage(float scale) {
    DrawPageChrome("Welcome", "Three small steps and you are dictating.", scale);

    const float width = std::min(ImGui::GetContentRegionAvail().x, metrics::content_max * scale);

    BeginCard("##card_welcome", width, scale);
    {
        if (welcome_step_ == 1) {
            ImGui::TextWrapped("%s", "Step 1 of 4 - microphone test.");
            ImGui::TextWrapped("%s", "Speak now; the bar should move.");
            const float level = audio_.AudioLevel();
            ImGui::ProgressBar(level, ImVec2(-FLT_MIN, 0.0f));
            if (level > 0.05f && ImGui::Button("Microphone works")) {
                welcome_step_ = 2;
            }
            ImGui::SameLine();
            if (ImGui::Button("Open microphone settings")) {
#ifdef _WIN32
                ShellExecuteA(nullptr, "open", "ms-settings:privacy-microphone", nullptr, nullptr,
                              1);
#endif
            }
        } else if (welcome_step_ == 2) {
            ImGui::TextWrapped("%s", "Step 2 of 4 - press your push-to-talk chord now.");
            if (detected_hotkey_.empty()) {
                ImGui::TextDisabled("Waiting for a chord...");
            } else {
                ImGui::Text("Detected: %s", detected_hotkey_.c_str());
                if (ImGui::Button("That is my hotkey")) {
                    settings_.SetString("hotkey_ptt", detected_hotkey_);
                    ApplyAndSave();  // also posts ApplySettings + shows Saved
                    welcome_step_ = 3;
                }
            }
            ImGui::TextDisabled("You can also learn a chord later from the Home page.");
        } else if (welcome_step_ == 3) {
            ImGui::TextWrapped("%s", "Step 3 of 4 - models.");
            const std::string path = GetExeDir() + "\\models.json";
            const Onboarding::ModelCatalog catalog = Onboarding::ParseModelsJson(path);
            for (const std::string& warning : catalog.warnings) {
                ImGui::TextDisabled("%s", warning.c_str());
            }
            for (const auto& model : catalog.whisper) {
                ImGui::BulletText("%s (%.0f MB)", model.name.c_str(),
                                  static_cast<double>(model.size_bytes) / 1048576.0);
                if (model.sha256.empty()) {
                    ImGui::SameLine();
                    ImGui::TextColored(Color(kDarkPalette.warn), "unverified file");
                }
            }
            for (const auto& model : catalog.llm) {
                ImGui::BulletText("%s (%.0f MB)", model.name.c_str(),
                                  static_cast<double>(model.size_bytes) / 1048576.0);
                if (model.sha256.empty()) {
                    ImGui::SameLine();
                    ImGui::TextColored(Color(kDarkPalette.warn), "unverified file");
                }
            }
            ImGui::TextWrapped("%s",
                               "Files are verified against the listed SHA-256 checksums.");
            if (model_path_buffer_[0] == '\0') {
                const std::string current = settings_.GetString("whisper_model_path");
                std::snprintf(model_path_buffer_, sizeof(model_path_buffer_), "%s",
                              current.c_str());
            }
            const float browse_width = ImGui::CalcTextSize("Browse...").x +
                                       ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::SetNextItemWidth(-(browse_width + ImGui::GetStyle().ItemSpacing.x));
            ImGui::InputText("##welcome_model_path", model_path_buffer_,
                             sizeof(model_path_buffer_));
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                validation_error_ = SettingsUiModel::ValidateModelPath(model_path_buffer_);
                if (validation_error_.empty()) {
                    settings_.SetString("whisper_model_path", model_path_buffer_);
                    restart_needed_ = true;
                    ApplyAndSave();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Browse...##browse_welcome")) {
                PickAndApplyModelPath(model_path_buffer_, sizeof(model_path_buffer_),
                                     "whisper_model_path", validation_error_);
            }
            if (!validation_error_.empty()) {
                ImGui::TextColored(Color(kDarkPalette.err), "%s", validation_error_.c_str());
            }
            if (ImGui::Button("Skip (I have models)")) {
                welcome_step_ = 4;
            }
        } else {
            ImGui::TextWrapped("%s", "Step 4 of 4 - try it.");
            ImGui::TextWrapped("%s", "Hold the hotkey and read a sentence aloud.");
            if (ImGui::Button("Finish")) {
                settings_.SetBool("first_run", false);
                ApplyAndSave();
                welcome_step_ = 0;
                validation_error_.clear();  // page switch: errors stay on their page
                page_ = Page::Home;
                PersistPage();
                page_focus_pending_ = true;
            }
        }
    }
    EndCard(scale);
}
