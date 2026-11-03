#include "ui/SettingsWindow.h"

#include "core/AppVersion.h"
#include "core/SettingsStore.h"
#include "core/SettingsUiModel.h"
#include "core/UiBus.h"
#include "os/WindowsHotkeyListener.h"

#include "imgui.h"

#include <GLFW/glfw3.h>

#include <algorithm>

SettingsWindow::SettingsWindow(SettingsStore& settings, UiBus& bus)
    : settings_(settings), bus_(bus) {}

bool SettingsWindow::Init() {
    if (!glfwInit()) {
        return false;
    }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window_ = glfwCreateWindow(720, 560, "Local Wisper Flow Settings", nullptr, nullptr);
    if (window_ == nullptr) {
        return false;
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(window_), false);
    ImGui_ImplOpenGL3_Init("#version 130");
    return true;
}

void SettingsWindow::Shutdown() {
    if (window_ == nullptr) {
        return;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    glfwDestroyWindow(static_cast<GLFWwindow*>(window_));
    window_ = nullptr;
}

bool SettingsWindow::IsOpen() const {
    return open_;
}

void SettingsWindow::Open() {
    open_ = true;
    if (window_ != nullptr) {
        glfwShowWindow(static_cast<GLFWwindow*>(window_));
    }
}

void SettingsWindow::Close() {
    open_ = false;
    if (window_ != nullptr) {
        glfwHideWindow(static_cast<GLFWwindow*>(window_));
    }
}

void SettingsWindow::RenderFrame() {
    if (!open_ || window_ == nullptr) {
        return;
    }
    GLFWwindow* glfw_window = static_cast<GLFWwindow*>(window_);
    glfwMakeContextCurrent(glfw_window);
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(glfw_window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Settings", &open_);
    if (ImGui::BeginTabBar("settings_tabs")) {
        if (ImGui::BeginTabItem("General")) {
            int min_hold = settings_.GetInt("min_hold_ms");
            if (ImGui::SliderInt("min hold ms", &min_hold, 50, 5000)) {
                validation_error_ = SettingsUiModel::ValidateInt("min_hold_ms", min_hold, 50, 5000);
                if (validation_error_.empty()) {
                    settings_.SetInt("min_hold_ms", min_hold);
                }
            }
            ImGui::Checkbox("hands-free double tap", nullptr);
            ImGui::Text("PTT hotkey: %s", settings_.GetString("hotkey_ptt").c_str());
            if (ImGui::Button("Learn new hotkey...")) {
                bus_.PostCommand(UiCommandType::StartLearnHotkey);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Speech")) {
            char model_path[512] = {};
            const std::string current = settings_.GetString("whisper_model_path");
            std::copy(current.begin(), current.end(), model_path);
            if (ImGui::InputText("whisper model", model_path, sizeof(model_path))) {
                validation_error_ = SettingsUiModel::ValidateModelPath(model_path);
                if (validation_error_.empty()) {
                    settings_.SetString("whisper_model_path", model_path);
                    restart_needed_ = true;
                }
            }
            const auto& languages = Languages::WhisperLanguages();
            const char* current_language = settings_.GetString("whisper_language").c_str();
            if (ImGui::BeginCombo("language", current_language)) {
                if (ImGui::Selectable("auto", settings_.GetString("whisper_language") == "auto")) {
                    settings_.SetString("whisper_language", "auto");
                }
                for (const auto& language : languages) {
                    if (ImGui::Selectable(language.name, settings_.GetString("whisper_language") == language.code)) {
                        settings_.SetString("whisper_language", language.code);
                    }
                }
                ImGui::EndCombo();
            }
            bool boost = settings_.GetBool("quiet_speech_boost");
            if (ImGui::Checkbox("boost quiet speech", &boost)) {
                settings_.SetBool("quiet_speech_boost", boost);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Cleanup")) {
            bool enabled = settings_.GetBool("cleanup_enabled");
            if (ImGui::Checkbox("enable cleanup", &enabled)) {
                settings_.SetBool("cleanup_enabled", enabled);
            }
            int timeout = settings_.GetInt("cleanup_timeout_ms");
            if (ImGui::SliderInt("timeout ms", &timeout, 200, 30000)) {
                settings_.SetInt("cleanup_timeout_ms", timeout);
            }
            bool aggressive = settings_.GetBool("spoken_commands_aggressive");
            if (ImGui::Checkbox("aggressive spoken commands", &aggressive)) {
                settings_.SetBool("spoken_commands_aggressive", aggressive);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("History")) {
            bool history_enabled = settings_.GetBool("history_enabled");
            if (ImGui::Checkbox("keep history", &history_enabled)) {
                settings_.SetBool("history_enabled", history_enabled);
            }
            bool keep_raw = settings_.GetBool("keep_raw_in_history");
            if (ImGui::Checkbox("store raw transcripts", &keep_raw)) {
                settings_.SetBool("keep_raw_in_history", keep_raw);
            }
            if (keep_raw) {
                ImGui::TextColored(ImVec4(0.95f, 0.8f, 0.2f, 1.0f),
                                   "raw transcripts stored locally only");
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Privacy")) {
            bool debug_transcripts = settings_.GetBool("debug_log_transcripts");
            if (ImGui::Checkbox("log transcript text", &debug_transcripts)) {
                settings_.SetBool("debug_log_transcripts", debug_transcripts);
            }
            if (debug_transcripts) {
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
                                   "WARNING: dictated text will be written to the local log");
            }
            ImGui::TextWrapped("No telemetry. No network calls except explicit model downloads.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("About")) {
            ImGui::Text("Local Wisper Flow %s", AppVersion());
            ImGui::TextWrapped("Local-first voice dictation. Whisper STT, optional local LLM "
                               "cleanup, clipboard injection. All processing stays on this "
                               "machine.");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    if (!validation_error_.empty()) {
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", validation_error_.c_str());
    }
    if (restart_needed_) {
        ImGui::TextColored(ImVec4(0.95f, 0.8f, 0.2f, 1.0f), "restart needed for some changes");
    }
    ImGui::Separator();
    if (ImGui::Button("Save")) {
        settings_.Save();
        bus_.PostCommand(UiCommandType::ApplySettings);
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        settings_.Reload();
        Close();
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(glfw_window);
}
