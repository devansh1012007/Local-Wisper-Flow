#include "ui/MainWindow.h"

#include "audio/MiniaudioCapture.h"
#include "core/AppVersion.h"
#include "core/Languages.h"
#include "core/Onboarding.h"
#include "core/SettingsStore.h"
#include "core/SettingsUiModel.h"
#include "core/UiBus.h"
#include "os/DataDir.h"
#include "ui/theme/ThemePalette.h"
#include "ui/theme/ThemeStyle.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <GL/gl.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

namespace metrics = lwf::ui::metrics;
using lwf::ui::Color;
using lwf::ui::kDarkPalette;
using lwf::ui::Pack;

constexpr float kBaseFontPx = 13.0f;  // built-in font until real fonts are approved

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
    const bool toggled = ImGui::InvisibleButton(id, ImVec2(w, h));
    ImDrawList& draw = *ImGui::GetWindowDrawList();
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
    : fsm_(fsm), audio_(audio), settings_(settings), bus_(bus) {}

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

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window_ = glfwCreateWindow(static_cast<int>(metrics::full_default_w * dpi_scale_),
                               static_cast<int>(metrics::full_default_h * dpi_scale_),
                               "Local Wisper Flow", nullptr, nullptr);
    if (window_ == nullptr) {
        return false;
    }
    glfwSetWindowSizeLimits(window_, static_cast<int>(metrics::full_min_w * dpi_scale_),
                            static_cast<int>(metrics::full_min_h * dpi_scale_), GLFW_DONT_CARE,
                            GLFW_DONT_CARE);

    // Center on the primary work area so the first open feels deliberate.
    int area_x = 0;
    int area_y = 0;
    int area_w = 1280;
    int area_h = 720;
    if (primary != nullptr) {
        glfwGetMonitorWorkarea(primary, &area_x, &area_y, &area_w, &area_h);
    }
    int win_w = 0;
    int win_h = 0;
    glfwGetWindowSize(window_, &win_w, &win_h);
    glfwSetWindowPos(window_, area_x + (area_w - win_w) / 2, area_y + (area_h - win_h) / 2);

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    ImGui::CreateContext();
    imgui_context_ = ImGui::GetCurrentContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

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
}

void MainWindow::SetDetectedHotkey(const std::string& chord) {
    detected_hotkey_ = chord;
}

bool MainWindow::FirstRunNeeded() const {
    return settings_.GetBool("first_run") || settings_.GetString("whisper_model_path").empty();
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

    int fb_width = 0;
    int fb_height = 0;
    glfwGetFramebufferSize(window_, &fb_width, &fb_height);
    glViewport(0, 0, fb_width, fb_height);
    const ImVec4 clear = Color(kDarkPalette.bg);
    glClearColor(clear.x, clear.y, clear.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const float scale = static_cast<float>(dpi_scale_);

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

    const float rail_w = metrics::rail_width * scale;
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
        case Page::Speech:
            DrawSpeechPage(scale);
            break;
        case Page::Cloud:
            DrawCloudPage(scale);
            break;
        case Page::Cleanup:
            DrawCleanupPage(scale);
            break;
        case Page::Privacy:
            DrawPrivacyPage(scale);
            break;
        case Page::About:
            DrawAboutPage(scale);
            break;
    }
}

void MainWindow::DrawRail(float scale) {
    const ImVec2 size(metrics::rail_width * scale, ImGui::GetIO().DisplaySize.y);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(kDarkPalette.surface));
    ImGui::BeginChild("##rail", size, ImGuiChildFlags_None);
    ImGui::PopStyleColor();
    {
        const ImVec2 rail_pos = ImGui::GetWindowPos();
        const float rail_right = rail_pos.x + size.x;
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        draw.AddLine(ImVec2(rail_right - 1.0f, rail_pos.y),
                     ImVec2(rail_right - 1.0f, rail_pos.y + size.y), Pack(kDarkPalette.border));

        // Wordmark at display size until real fonts land.
        ImGui::SetWindowFontScale(18.0f / kBaseFontPx);
        ImGui::TextUnformatted("Local Wisper Flow");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

        struct NavItem {
            const char* id;
            const char* label;
            Page page;
        };
        const NavItem items[] = {
            {"##nav_home", "Home", Page::Home},
            {"##nav_speech", "Speech", Page::Speech},
            {"##nav_cloud", "Cloud", Page::Cloud},
            {"##nav_cleanup", "Cleanup", Page::Cleanup},
            {"##nav_privacy", "Privacy", Page::Privacy},
            {"##nav_about", "About", Page::About},
        };

        const float item_h = 36.0f * scale;
        const float font_h = ImGui::GetFontSize();
        for (const NavItem& item : items) {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x;
            ImGui::InvisibleButton(item.id, ImVec2(w, item_h));
            if (ImGui::IsItemClicked()) {
                page_ = item.page;
            }
            const bool active = page_ == item.page;
            const bool hovered = ImGui::IsItemHovered();
            const std::uint32_t color =
                active ? kDarkPalette.accent : (hovered ? kDarkPalette.text : kDarkPalette.muted);
            if (active) {
                draw.AddRectFilled(
                    ImVec2(p.x, p.y + (item_h - 20.0f * scale) * 0.5f),
                    ImVec2(p.x + 3.0f * scale, p.y + (item_h + 20.0f * scale) * 0.5f),
                    Pack(kDarkPalette.accent), 2.0f * scale);
            }
            draw.AddText(ImVec2(p.x + 19.0f * scale, p.y + (item_h - font_h) * 0.5f),
                         Pack(color), item.label);
        }

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
    ImGui::SetWindowFontScale(28.0f / kBaseFontPx);
    ImGui::TextUnformatted(title);
    ImGui::SetWindowFontScale(1.0f);
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
    ImGui::PopStyleVar();
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

    ImGui::SetCursorPos(ImVec2(start_x, start_y + row_h + metrics::space_xs * scale));
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

void MainWindow::DrawHomePage(float scale) {
    DrawPageChrome("Home", "Dictation runs on this computer and starts with one hotkey.", scale);

    const float width = std::min(ImGui::GetContentRegionAvail().x, metrics::content_max * scale);
    const State fsm_state = fsm_.CurrentState();
    const bool enabled = settings_.GetBool("dictation_enabled");

    BeginCard("##card_now", width, scale);
    {
        ImGui::SetWindowFontScale(15.0f / kBaseFontPx);
        ImGui::TextUnformatted("Right now");
        ImGui::SetWindowFontScale(1.0f);
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
        ImGui::SetWindowFontScale(15.0f / kBaseFontPx);
        ImGui::TextUnformatted("Hotkey");
        ImGui::SetWindowFontScale(1.0f);
        DrawHotkeyChips(settings_.GetString("hotkey_ptt"), scale);
        ImGui::TextDisabled("Hold to speak, release to type.");
        if (ImGui::Button("Learn a new chord...")) {
            bus_.PostCommand(UiCommandType::StartLearnHotkey);
        }
    }
    EndCard(scale);

    BeginCard("##card_trust", width, scale);
    {
        ImGui::SetWindowFontScale(15.0f / kBaseFontPx);
        ImGui::TextUnformatted("Trust");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextUnformatted("Everything runs on this computer.");
        ImGui::TextDisabled("Cloud speech is off.");
        // When cloud speech lands, this card switches to the mockup's cloud
        // variant ("Speech is sent to <provider> for each sentence...").
    }
    EndCard(scale);

    BeginCard("##card_quick", width, scale);
    {
        ImGui::SetWindowFontScale(15.0f / kBaseFontPx);
        ImGui::TextUnformatted("Quick settings");
        ImGui::SetWindowFontScale(1.0f);
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
        ImGui::SetWindowFontScale(15.0f / kBaseFontPx);
        ImGui::TextUnformatted("Last dictation");
        ImGui::SetWindowFontScale(1.0f);
        if (last_transcript_.empty()) {
            ImGui::TextDisabled("Nothing yet.");
        } else {
            ImGui::TextWrapped("%s", last_transcript_.c_str());
            if (ImGui::Button("Copy")) {
                ImGui::SetClipboardText(last_transcript_.c_str());
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
        ImGui::SetWindowFontScale(15.0f / kBaseFontPx);
        ImGui::TextUnformatted("Model");
        ImGui::SetWindowFontScale(1.0f);

        if (model_path_buffer_[0] == '\0') {
            const std::string current = settings_.GetString("whisper_model_path");
            std::snprintf(model_path_buffer_, sizeof(model_path_buffer_), "%s", current.c_str());
        }
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##model_path", model_path_buffer_, sizeof(model_path_buffer_));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            validation_error_ = SettingsUiModel::ValidateModelPath(model_path_buffer_);
            if (validation_error_.empty()) {
                settings_.SetString("whisper_model_path", model_path_buffer_);
                restart_needed_ = true;
                ApplyAndSave();
            }
        }
        if (!validation_error_.empty()) {
            ImGui::TextColored(Color(kDarkPalette.err), "%s", validation_error_.c_str());
        }
        ImGui::TextDisabled("Restart is needed after changing the model path.");
    }
    EndCard(scale);

    BeginCard("##card_lang", width, scale);
    {
        ImGui::SetWindowFontScale(15.0f / kBaseFontPx);
        ImGui::TextUnformatted("Language");
        ImGui::SetWindowFontScale(1.0f);
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
        ImGui::SetWindowFontScale(15.0f / kBaseFontPx);
        ImGui::TextUnformatted("Listening");
        ImGui::SetWindowFontScale(1.0f);
        bool boost = settings_.GetBool("quiet_speech_boost");
        if (ToggleRow("Boost quiet speech", boost, "Lifts a soft voice without clipping", scale)) {
            settings_.SetBool("quiet_speech_boost", !boost);
            ApplyAndSave();
        }
    }
    EndCard(scale);
}

void MainWindow::DrawCloudPage(float scale) {
    DrawPageChrome("Cloud", "Where your speech is transcribed.", scale);
    DrawRestartBanner(scale);

    const float width = std::min(ImGui::GetContentRegionAvail().x, metrics::content_max * scale);

    BeginCard("##card_cloud_provider", width, scale);
    {
        ImGui::SetWindowFontScale(15.0f / kBaseFontPx);
        ImGui::TextUnformatted("Provider");
        ImGui::SetWindowFontScale(1.0f);

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
    }
    EndCard(scale);

    BeginCard("##card_cloud_consent", width, scale);
    {
        ImGui::SetWindowFontScale(15.0f / kBaseFontPx);
        ImGui::TextUnformatted("Consent");
        ImGui::SetWindowFontScale(1.0f);

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
        ImGui::SetWindowFontScale(15.0f / kBaseFontPx);
        ImGui::TextUnformatted("Cleanup");
        ImGui::SetWindowFontScale(1.0f);

        bool enabled = settings_.GetBool("cleanup_enabled");
        if (ToggleRow("Enable cleanup", enabled, nullptr, scale)) {
            settings_.SetBool("cleanup_enabled", !enabled);
            ApplyAndSave();
        }
        ImGui::Separator();

        int min_words = settings_.GetInt("cleanup_min_words");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderInt("##min_words", &min_words, 1, 100, "skip short: %d words")) {
            validation_error_ =
                SettingsUiModel::ValidateInt("cleanup_min_words", min_words, 1, 100);
            if (validation_error_.empty()) {
                settings_.SetInt("cleanup_min_words", min_words);
                ApplyAndSave();
            }
        }
        int timeout = settings_.GetInt("cleanup_timeout_ms");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderInt("##timeout", &timeout, 200, 30000, "give up after %d ms")) {
            settings_.SetInt("cleanup_timeout_ms", timeout);
            ApplyAndSave();
        }
        bool aggressive = settings_.GetBool("spoken_commands_aggressive");
        if (ToggleRow("Aggressive spoken commands", aggressive,
                      "Rewrite commands even inside longer sentences", scale)) {
            settings_.SetBool("spoken_commands_aggressive", !aggressive);
            ApplyAndSave();
        }
        if (!validation_error_.empty()) {
            ImGui::TextColored(Color(kDarkPalette.err), "%s", validation_error_.c_str());
        }
    }
    EndCard(scale);
}

void MainWindow::DrawPrivacyPage(float scale) {
    DrawPageChrome("Privacy", "What stays on this computer, and what gets written down.", scale);

    const float width = std::min(ImGui::GetContentRegionAvail().x, metrics::content_max * scale);

    BeginCard("##card_privacy", width, scale);
    {
        ImGui::SetWindowFontScale(15.0f / kBaseFontPx);
        ImGui::TextUnformatted("Privacy");
        ImGui::SetWindowFontScale(1.0f);

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
        ImGui::TextWrapped("%s", "No telemetry. No network calls except explicit model downloads.");
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
    }
    EndCard(scale);
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
                    settings_.Save();
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
            if (ImGui::Button("Skip (I have models)")) {
                welcome_step_ = 4;
            }
        } else {
            ImGui::TextWrapped("%s", "Step 4 of 4 - try it.");
            ImGui::TextWrapped("%s", "Hold the hotkey and read a sentence aloud.");
            if (ImGui::Button("Finish")) {
                settings_.SetBool("first_run", false);
                settings_.Save();
                welcome_step_ = 0;
                page_ = Page::Home;
            }
        }
    }
    EndCard(scale);
}
