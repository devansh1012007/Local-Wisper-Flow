#pragma once

#include "core/DictationFSM.h"

#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>

struct GLFWwindow;
struct ImGuiContext;

class MiniaudioCapture;
class SettingsStore;
class UiBus;
class ScratchpadPage;

namespace lwf::ui::pages {
class DictionaryPage;
class SnippetsPage;
}
namespace lwf::ui::insights {
class InsightsPage;
}
namespace lwf::ui {
class TransformsPage;
}

// The Full window: rail navigation plus pages (Home, Insights, Dictionary,
// Snippets, Transforms, Scratchpad, Settings, and the first-run Welcome).
// Owns its GLFW window and its Dear ImGui context (one context per OS window, docs/UI_AUDIT.md section 1).
//
// Closing the window hides it - the tray owns Quit. Settings changes apply
// live: every edit is written, saved and followed by an ApplySettings command
// on the UiBus; pages that need a restart show a banner. Visual spec:
// docs/design/mockup.html sections 3 and 5.
class MainWindow {
public:
    enum class Page { Welcome, Home, Insights, Dictionary, Snippets, Transforms, Scratchpad, Settings };

    static const char* RouteName(Page page) {
        switch (page) {
            case Page::Home:
                return "Home";
            case Page::Insights:
                return "Insights";
            case Page::Dictionary:
                return "Dictionary";
            case Page::Snippets:
                return "Snippets";
            case Page::Transforms:
                return "Transforms";
            case Page::Scratchpad:
                return "Scratchpad";
            case Page::Settings:
                return "Settings";
            default:
                return "Welcome";
        }
    }

    static Page PageForRoute(const std::string& route) {
        if (route == "Insights") {
            return Page::Insights;
        }
        if (route == "Dictionary") {
            return Page::Dictionary;
        }
        if (route == "Snippets") {
            return Page::Snippets;
        }
        if (route == "Transforms") {
            return Page::Transforms;
        }
        if (route == "Scratchpad") {
            return Page::Scratchpad;
        }
        if (route == "Settings") {
            return Page::Settings;
        }
        return Page::Home;
    }

    MainWindow(const DictationFSM& fsm, const MiniaudioCapture& audio,
               SettingsStore& settings, UiBus& bus);
    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    bool Init();
    void RunOnce(float delta_seconds);
    // Explicit teardown; also runs from the destructor. Destroys this
    // window's ImGui context, so call it before glfwTerminate.
    void Shutdown() noexcept;

    void SetVisible(bool visible);
    bool IsVisible() const;
    // True exactly once after the user closed the window via the title bar.
    bool TakeCloseRequest();

    // Fed from the main loop.
    void SetLastTranscript(const std::string& text);
    void SetDetectedHotkey(const std::string& chord);
    void SetLearnedHotkey(const std::string& target_key, const std::string& chord);
    void SetRecorderError(const std::string& target_key, const std::string& message);
    void SetDictionaryPage(std::unique_ptr<lwf::ui::pages::DictionaryPage> page);
    void SetInsightsPage(std::unique_ptr<lwf::ui::insights::InsightsPage> page);
    void SetTransformsPage(std::unique_ptr<lwf::ui::TransformsPage> page);
    void SetScratchpadPage(std::unique_ptr<ScratchpadPage> page);
    void SetSnippetsPage(std::unique_ptr<lwf::ui::pages::SnippetsPage> page);

    bool FirstRunNeeded() const;

private:
    // GLFW trampoline: stores the new monitor scale; it is applied on the
    // next RunOnce with this window's ImGui context current.
    static void OnContentScale(GLFWwindow* window, float scale_x, float scale_y);

    void RenderPages(float scale);

    void DrawRail(float scale);
    void DrawPageChrome(const char* title, const char* subtitle, float scale);
    void DrawHomePage(float scale);
    void DrawInsightsPage(float scale);
    void DrawDictionaryPage(float scale);
    void DrawSnippetsPage(float scale);
    void DrawTransformsPage(float scale);
    void DrawScratchpadPage(float scale);
    void DrawSettingsPage(float scale);
    void DrawGeneralPage(float scale);
    void DrawSpeechPage(float scale);
    void DrawCloudPage(float scale);
    void DrawCleanupPage(float scale);
    void DrawPrivacyPage(float scale);
    void DrawAboutPage(float scale);
    void DrawWelcomePage(float scale);

    void BeginCard(const char* id, float width, float scale);
    void EndCard(float scale);
    bool ToggleRow(const char* label, bool value, const char* helper, float scale);
    void ApplyAndSave();
    void ShowSaved();
    void DrawRestartBanner(float scale);
    void DrawHotkeyChips(const std::string& chord, float scale);
    void DrawGeneralHotkeyRow(const std::string& config_key, const char* label,
                              const char* helper, float scale);
    void PersistPage();
    bool PickAndApplyModelPath(char* buffer, std::size_t buffer_size, const char* setting_key,
                               std::string& error);
    void DeleteLocalData();

    struct TextField {
        char buffer[512] = {};
        bool seeded = false;
        void Seed(const std::string& value) {
            if (!seeded) {
                std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
                seeded = true;
            }
        }
    };

    void IntRow(const char* key, const char* id, int min_value, int max_value,
                const char* format);
    void DoubleRow(const char* key, const char* id, float min_value, float max_value,
                   const char* format);
    void BoolRow(const char* key, const char* label, const char* helper, float scale);
    void StringFieldRow(const char* key, TextField& field, const char* id, bool restart);

    enum class SettingsTab { General, Speech, Cloud, Cleanup, Privacy, About };

    const DictationFSM& fsm_;
    const MiniaudioCapture& audio_;
    SettingsStore& settings_;
    UiBus& bus_;

    GLFWwindow* window_ = nullptr;
    ImGuiContext* imgui_context_ = nullptr;
    bool shutdown_done_ = false;
    bool close_requested_ = false;

    Page page_ = Page::Home;
    SettingsTab settings_tab_ = SettingsTab::General;
    int welcome_step_ = 1;
    bool restart_needed_ = false;
    float saved_until_ = -1.0f;
    float copied_until_ = -1.0f;  // "Copied" dwell after Copy; reset on new transcript
    bool page_focus_pending_ = false;  // restore keyboard focus after switch/re-show
    double dpi_scale_ = 1.0;
    float pending_dpi_scale_ = 0.0f;  // deferred content-scale callback value

    std::string last_transcript_;
    std::string detected_hotkey_;
    std::unique_ptr<lwf::ui::pages::DictionaryPage> dictionary_page_;
    std::unique_ptr<lwf::ui::pages::SnippetsPage> snippets_page_;
    std::unique_ptr<lwf::ui::insights::InsightsPage> insights_page_;
    std::unique_ptr<lwf::ui::TransformsPage> transforms_page_;
    std::unique_ptr<ScratchpadPage> scratchpad_page_;
    std::string learn_target_;
    std::string learn_error_target_;
    std::string learn_error_;
    std::string validation_error_;
    std::string llm_model_error_;
    bool delete_data_armed_ = false;
    char model_path_buffer_[512] = {};
    char llm_model_path_buffer_[512] = {};
    TextField hotkey_handsfree_field_;
    TextField hotkey_cancel_field_;
    TextField hotkey_command_field_;
    TextField hotkey_paste_last_field_;
    TextField remote_endpoint_field_;
    TextField stt_language_field_;
    TextField cleanup_language_policy_field_;
    TextField mic_device_field_;
};
