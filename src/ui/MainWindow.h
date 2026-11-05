#pragma once

#include "core/DictationFSM.h"

#include <string>

struct GLFWwindow;
struct ImGuiContext;

class MiniaudioCapture;
class SettingsStore;
class UiBus;

// The Full window: rail navigation plus pages (Home, Speech, Cloud, Cleanup,
// Privacy, About, and the first-run Welcome). Owns its GLFW window and its
// Dear ImGui context (one context per OS window, docs/UI_AUDIT.md section 1).
//
// Closing the window hides it - the tray owns Quit. Settings changes apply
// live: every edit is written, saved and followed by an ApplySettings command
// on the UiBus; pages that need a restart show a banner. Visual spec:
// docs/design/mockup.html sections 3 and 5.
class MainWindow {
public:
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

    bool FirstRunNeeded() const;

private:
    // GLFW trampoline: stores the new monitor scale; it is applied on the
    // next RunOnce with this window's ImGui context current.
    static void OnContentScale(GLFWwindow* window, float scale_x, float scale_y);

    void RenderPages(float scale);

    void DrawRail(float scale);
    void DrawPageChrome(const char* title, const char* subtitle, float scale);
    void DrawHomePage(float scale);
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

    enum class Page { Welcome, Home, Speech, Cloud, Cleanup, Privacy, About };

    const DictationFSM& fsm_;
    const MiniaudioCapture& audio_;
    SettingsStore& settings_;
    UiBus& bus_;

    GLFWwindow* window_ = nullptr;
    ImGuiContext* imgui_context_ = nullptr;
    bool shutdown_done_ = false;
    bool close_requested_ = false;

    Page page_ = Page::Home;
    int welcome_step_ = 1;
    bool restart_needed_ = false;
    float saved_until_ = -1.0f;
    double dpi_scale_ = 1.0;
    float pending_dpi_scale_ = 0.0f;  // deferred content-scale callback value

    std::string last_transcript_;
    std::string detected_hotkey_;
    std::string validation_error_;
    char model_path_buffer_[512] = {};
};
