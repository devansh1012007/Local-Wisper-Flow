#pragma once

#include "core/DictationFSM.h"

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct GLFWwindow;
struct ImGuiContext;

class MiniaudioCapture;
class SettingsStore;
class UiBus;
class ScratchpadPage;
class DictionaryStore;
class SnippetStore;
class StatsStore;
class TransformStore;
class NotesStore;
class ApiKeyStore;
class HistoryStore;
class PersonalDictionary;
class IHttpTransport;

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
// Phase 4 Ivory primary: the shell applies kLightPalette everywhere; the dark
// variant is test-only + the Mini badge in Overlay.cpp, which intentionally
// stays dark for desktop legibility (Ph6 scope; do not switch Overlay here).
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

    struct DataResetStores {
        DictionaryStore* dictionary = nullptr;
        SnippetStore* snippets = nullptr;
        StatsStore* stats = nullptr;
        TransformStore* transforms = nullptr;
        NotesStore* notes = nullptr;
        ApiKeyStore* api_keys = nullptr;
        HistoryStore* history = nullptr;
        PersonalDictionary* dictionary_legacy = nullptr;
    };

    void SetDataResetStores(DataResetStores stores);

    // Non-owning transport for the Cleanup model list Refresh. The caller
    // (main.cpp) keeps the instance alive longer than this window; nullptr
    // disables Refresh and never crashes the picker.
    void SetModelsTransport(IHttpTransport* transport);

    // One off-thread models-list fetch, primitives only, so this header
    // stays free of the backend model tree (the F9 copy lint scans every
    // literal in src/ui, including #include paths, for banned terms).
    // main.cpp maps the backend fetch result onto this shape; the worker
    // calls the fetcher off the UI thread and only the latch result is read
    // back on the UI thread.
    struct ModelListResult {
        bool ok = false;  // false when the fetch failed (error is set)
        bool empty = false;  // true when the fetch succeeded with zero ids
        std::vector<std::string> ids;
        std::string error;
    };
    using ModelListFetcher =
        std::function<ModelListResult(IHttpTransport&, const std::string&)>;
    void SetModelsFetcher(ModelListFetcher fetcher);

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
    // Masked key entry row shared by Cloud (Gnani, ElevenLabs, Muse) and
    // Cleanup (OpenRouter). Never seeds buffer with stored value.
    void DrawApiKeyRow(const char* store_id, const char* display_name, const char* env_name,
                       const char* confirm_question, const char* confirm_button, char* buffer,
                       std::size_t buffer_size, bool& remove_armed, bool& focus_keep_pending,
                       std::string& error, float scale);
    void ClearApiKeyInputs();
    // Store-first, then environment key resolution for the OpenRouter models
    // Refresh, mirroring main.cpp:376-382. SECURITY: callers keep the value
    // off-screen and never log it.
    std::string ResolveOpenRouterKey() const;

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
    DataResetStores reset_stores_;
    std::string learn_target_;
    std::string learn_error_target_;
    std::string learn_error_;
    std::string validation_error_;
    std::string llm_model_error_;
    bool delete_data_armed_ = false;
    char model_path_buffer_[512] = {};
    char llm_model_path_buffer_[512] = {};
    // API key entry state (Cloud + Cleanup). Buffers hold uncommitted
    // masked input only; they are cleared on save, remove, cancel and
    // page/tab switch. Armed flags drive the named two-step removal.
    char gnani_key_buffer_[512] = {};
    char elevenlabs_key_buffer_[512] = {};
    char muse_key_buffer_[512] = {};
    char openrouter_key_buffer_[512] = {};
    bool gnani_remove_armed_ = false;
    bool elevenlabs_remove_armed_ = false;
    bool muse_remove_armed_ = false;
    bool openrouter_remove_armed_ = false;
    bool gnani_keep_focus_pending_ = false;
    bool elevenlabs_keep_focus_pending_ = false;
    bool muse_keep_focus_pending_ = false;
    bool openrouter_keep_focus_pending_ = false;
    std::string gnani_key_error_;
    std::string elevenlabs_key_error_;
    std::string muse_key_error_;
    std::string openrouter_key_error_;
    TextField hotkey_handsfree_field_;
    TextField hotkey_cancel_field_;
    TextField hotkey_command_field_;
    TextField hotkey_paste_last_field_;
    TextField remote_endpoint_field_;
    TextField stt_language_field_;
    TextField cleanup_language_policy_field_;
    TextField openrouter_model_field_;
    // OpenRouter cleanup-model picker (Settings > Cleanup). The fetched id
    // list is a runtime cache only, never persisted. The fetch runs on a
    // worker std::thread (max 8s) that calls the injected ModelListFetcher
    // and writes its result into the mutex-guarded latch; the UI thread
    // consumes the latch each Cleanup-card frame, so the 8s GET never
    // freezes either window. INVARIANT: openrouter_models_busy_ is touched
    // only on the UI thread (set on spawn, cleared on consume); the worker
    // never reads or writes it.
    IHttpTransport* models_transport_ = nullptr;
    ModelListFetcher models_fetcher_;
    std::vector<std::string> openrouter_models_;
    std::atomic<bool> openrouter_models_busy_{false};
    std::string openrouter_models_error_;
    std::mutex openrouter_models_latch_mutex_;
    bool openrouter_models_latch_ready_ = false;  // guarded by latch mutex
    ModelListResult openrouter_models_latch_;  // guarded by latch mutex
    std::thread openrouter_models_worker_;  // joined in ~MainWindow
    // Microphone picker (Settings > Speech): capture-device display names
    // enumerated ONCE per app run on the first DrawSpeechPage pass.
    // INVARIANT: ListCaptureDevices() inits a ma_context, so it must never be
    // called per frame; mic_devices_loaded_ latches the one enumeration.
    std::vector<std::string> mic_devices_;
    bool mic_devices_loaded_ = false;
};
