#include "ui/MainWindow.h"

#include "audio/MiniaudioCapture.h"
#include "core/AppVersion.h"
#include "core/ApiKeyStore.h"
#include "core/ConsentGate.h"
#include "core/DictionaryStore.h"
#include "core/HistoryStore.h"
#include "core/HotkeyBinding.h"
#include "core/InjectionException.h"
#include "core/Languages.h"
#include "core/NotesStore.h"
#include "core/Onboarding.h"
#include "core/PersonalDictionary.h"
#include "core/SettingsStore.h"
#include "core/SettingsUiModel.h"
#include "core/Sha256.h"
#include "core/SnippetStore.h"
#include "core/StatsStore.h"
#include "core/TransformStore.h"
#include "core/UiBus.h"
#include "os/DataDir.h"
#include "os/FileDialogs.h"
#include "os/WindowsDownloader.h"
#include "ui/pages/DictionaryPage.h"
#include "ui/pages/InsightsPage.h"
#include "ui/pages/ScratchpadPage.h"
#include "ui/pages/SnippetsPage.h"
#include "ui/pages/HistoryPage.h"
#include "ui/pages/ProfilesPage.h"
#include "ui/pages/TransformsPage.h"
#include "ui/theme/ThemePalette.h"
#include "ui/theme/ThemeStyle.h"
#include "ui/widgets/Widgets.h"

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
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

// Full window shell (T2b): the ImGui style is selected via PaletteForTheme
// (Graphite default, Paper via the `theme` key). T3+ re-skinned the custom
// draws to the live palette; the remaining kLightPalette uses in this file
// are the dead chrome helpers (BeginCard/ToggleRow/BoolRow and the
// file-local ToggleSwitch — zero callers; pre-PR review follow-up) and
// comments. The Mini badge in Overlay.cpp follows the same selection (T15
// re-skins its badge chrome).
namespace metrics = lwf::ui::metrics;
using lwf::ui::Color;
using lwf::ui::kLightPalette;
using lwf::ui::Pack;

// Rail status-dot indent (Wave 2 T-003 file-local hoist): shared by the rail
// and Home status dots ahead of the status word. No 14px spacing token.
constexpr float kStatusDotIndent = 14.0f;

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
    const float h = metrics::control_min_height * scale;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool toggled = ImGui::InvisibleButton(id, ImVec2(w, h), ImGuiButtonFlags_EnableNav);
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    if (ImGui::IsItemFocused()) {
        // 2px focus ring (kLightPalette.focus) so keyboard focus is visible.
        draw.AddRect(ImVec2(p.x - 2.0f * scale, p.y - 2.0f * scale),
                     ImVec2(p.x + w + 2.0f * scale, p.y + h + 2.0f * scale),
                     Pack(kLightPalette.focus), h * 0.5f + 2.0f * scale, 0, 2.0f * scale);
    }
    draw.AddRectFilled(p, ImVec2(p.x + w, p.y + h),
                       value ? Pack(kLightPalette.accent) : Pack(kLightPalette.text, 0.15f),
                       h * 0.5f);
    const float knob = 24.0f * scale;
    const float pad = metrics::space_xs * scale;
    const float x = value ? p.x + w - pad - knob : p.x + pad;
    draw.AddCircleFilled(ImVec2(x + knob * 0.5f, p.y + h * 0.5f), knob * 0.5f,
                         Pack(kLightPalette.surface));
    return toggled;
}

// Button for the masked key rows: at least 32px tall (R10), accent fill
// for the primary action, and a 2px focus ring when keyboard focused
// (R11). The label carries the meaning; color never carries state alone.
// T6: the palette is passed explicitly (the live *current_palette_ from
// DrawApiKeyRow) so the chrome follows the theme switcher, never the
// Paper default. KeyButton has exactly one caller (DrawApiKeyRow, 4
// call sites); grep-verified before this signature change.
bool KeyButton(const char* id, const char* label, bool accent, float scale,
               const lwf::ui::Palette& palette) {
    const float height = metrics::control_min_height * scale;
    const std::string button_label =
        std::string(label == nullptr ? "" : label) + "##" + (id == nullptr ? "" : id);
    bool pressed = false;
    if (accent) {
        ImGui::PushStyleColor(ImGuiCol_Button, Color(palette.accent));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Color(palette.accent));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Color(palette.accent));
        ImGui::PushStyleColor(ImGuiCol_Text, Color(palette.on_accent));
        pressed = ImGui::Button(button_label.c_str(), ImVec2(0.0f, height));
        ImGui::PopStyleColor(4);
    } else {
        pressed = ImGui::Button(button_label.c_str(), ImVec2(0.0f, height));
    }
    if (ImGui::IsItemFocused()) {
        const ImVec2 item_min = ImGui::GetItemRectMin();
        const ImVec2 item_max = ImGui::GetItemRectMax();
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        draw.AddRect(ImVec2(item_min.x - 2.0f * scale, item_min.y - 2.0f * scale),
                     ImVec2(item_max.x + 2.0f * scale, item_max.y + 2.0f * scale),
                     Pack(palette.focus), metrics::radius_control * scale, 0,
                     2.0f * scale);
    }
    return pressed;
}

}  // namespace

MainWindow::MainWindow(const DictationFSM& fsm, const MiniaudioCapture& audio,
                       SettingsStore& settings, UiBus& bus)
    : fsm_(fsm), audio_(audio), settings_(settings), bus_(bus) {
    page_ = PageForRoute(settings_.GetString("last_page"));
}

MainWindow::~MainWindow() {
    // DECISION: join the models fetch worker here so it can never outlive
    // MainWindow; the worker writes only the latch members, which stay valid
    // until the destructor returns. A fetch in flight caps the exit wait at
    // the 8s transport timeout, and the app never freezes mid-session because
    // the UI thread only consumes the latch.
    if (openrouter_models_worker_.joinable()) {
        openrouter_models_worker_.join();
    }
    // DECISION: the speech-model download worker joins here, after the
    // cancel flag is set so its progress callback starts returning false.
    // The flag is the only bound available: the transfer layer sets no
    // network timeouts (see docs/PROGRESS.md T13a), so a stalled connection
    // can outlive the flag and hold the join. That residual hang risk is
    // a downloader follow-up, not something this window can fix.
    download_cancelled_.store(true);
    if (download_worker_.joinable()) {
        download_worker_.join();
    }
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
                               "Echo flow", nullptr, nullptr);
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
    // T2b theme selection (T5 owns the switcher row; this is the init-time
    // read so a stored "paper" starts in Paper and the default starts in
    // Graphite). current_palette_ is the last-applied palette. Any pending
    // ApplyTheme queued before Init is stale — Init re-reads the store.
    // Load-time normalization (T5): an unknown stored value falls back to
    // Graphite and is persisted, so PaletteForTheme can stay exact-match.
    {
        const std::string stored_theme = settings_.GetString("theme");
        if (stored_theme != lwf::ui::kThemeGraphite && stored_theme != lwf::ui::kThemePaper) {
            settings_.SetString("theme", lwf::ui::kThemeGraphite);
            settings_.Save();
        }
    }
    current_palette_ = &lwf::ui::PaletteForTheme(settings_.GetString("theme"));
    pending_theme_palette_ = nullptr;
    theme_pending_ = false;
    lwf::ui::ApplyImGuiStyle(*current_palette_, static_cast<float>(dpi_scale_));

    if (FirstRunNeeded()) {
        page_ = Page::Welcome;
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

void MainWindow::ApplyTheme(const lwf::ui::Palette& palette) {
    // CRITICAL: never call ApplyImGuiStyle here. The ApplySettings drain in
    // main.cpp may run while another window's ImGui context is current, and
    // ApplyImGuiStyle colors the current context. Queue the palette; RunOnce
    // re-applies it where this window's context is current (same deferred
    // pattern as pending_dpi_scale_). No-op when the selection is unchanged.
    if (&palette == current_palette_ && !theme_pending_) {
        return;
    }
    if (theme_pending_ && &palette == pending_theme_palette_) {
        return;
    }
    pending_theme_palette_ = &palette;
    theme_pending_ = true;
}

void MainWindow::SetLastTranscript(const std::string& text) {
    last_transcript_ = text;
    copied_until_ = -1.0f;  // a new transcript is not yet copied
}

void MainWindow::SetLearnedHotkey(const std::string& target_key, const std::string& chord) {
    if (learn_target_ == target_key) {
        learn_target_.clear();
    }
    if (target_key == "hotkey_handsfree") {
        hotkey_handsfree_field_.seeded = false;
    } else if (target_key == "hotkey_cancel") {
        hotkey_cancel_field_.seeded = false;
    } else if (target_key == "hotkey_command") {
        hotkey_command_field_.seeded = false;
    } else if (target_key == "hotkey_paste_last") {
        hotkey_paste_last_field_.seeded = false;
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

void MainWindow::SetDictionaryPage(std::unique_ptr<lwf::ui::pages::DictionaryPage> page) {
    dictionary_page_ = std::move(page);
}

void MainWindow::SetHistoryPage(std::unique_ptr<lwf::ui::pages::HistoryPage> page) {
    history_page_ = std::move(page);
}

void MainWindow::SetProfilesPage(std::unique_ptr<lwf::ui::pages::ProfilesPage> page) {
    profiles_page_ = std::move(page);
}

void MainWindow::SetInsightsPage(std::unique_ptr<lwf::ui::insights::InsightsPage> page) {
    insights_page_ = std::move(page);
}

void MainWindow::SetTransformsPage(std::unique_ptr<lwf::ui::TransformsPage> page) {
    transforms_page_ = std::move(page);
}

void MainWindow::SetScratchpadPage(std::unique_ptr<ScratchpadPage> page) {
    scratchpad_page_ = std::move(page);
}

void MainWindow::SetSnippetsPage(std::unique_ptr<lwf::ui::pages::SnippetsPage> page) {
    snippets_page_ = std::move(page);
}

void MainWindow::SetDataResetStores(DataResetStores stores) {
    reset_stores_ = stores;
}

void MainWindow::SetModelsTransport(IHttpTransport* transport) {
    models_transport_ = transport;
}

void MainWindow::SetModelsFetcher(ModelListFetcher fetcher) {
    models_fetcher_ = std::move(fetcher);
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

    // T13a: the speech-model download latch is consumed here, not on the
    // Speech page, so completion (including the config write after a
    // successful download) survives the window being hidden or the page
    // switched. The worker writes only the progress atomic and the latch,
    // and only the UI thread touches download_busy_.
    if (download_busy_.load()) {
        download_model_.OnProgress(download_downloaded_.load());
    }
    {
        std::lock_guard<std::mutex> latch_lock(download_latch_mutex_);
        if (download_latch_ready_) {
            const Onboarding::DownloadSnapshot result = download_latch_;
            download_latch_ready_ = false;
            switch (result.state) {
                case Onboarding::DownloadState::Success:
                    download_model_.OnSuccess();
                    settings_.SetString("whisper_model_path", download_stored_path_);
                    validation_error_.clear();
                    restart_needed_ = true;
                    ApplyAndSave();
                    std::snprintf(model_path_buffer_, sizeof(model_path_buffer_), "%s",
                                  download_stored_path_.c_str());
                    break;
                case Onboarding::DownloadState::Cancelled:
                    download_model_.OnCancelled();
                    break;
                case Onboarding::DownloadState::Failed:
                    download_model_.OnFailed(result.error);
                    break;
                default:
                    break;
            }
            download_busy_.store(false);
        }
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

    // A monitor scale change and a live theme swap land here, where this
    // window's ImGui context is current. The style (and, through
    // FontScaleMain, the dynamic font) re-apply so text and controls keep
    // filling the window instead of rendering at the old scale; a queued
    // theme promotes only on an actual palette change (pointer identity).
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

    // Host window fills the client area; strip and page are children.
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##full", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    DrawTopStrip(scale);

    const float strip_h = metrics::topstrip_height * scale;
    const ImVec2 content_pos(0.0f, strip_h);
    const ImVec2 content_size(ImGui::GetIO().DisplaySize.x,
                              ImGui::GetIO().DisplaySize.y - strip_h);
    ImGui::SetCursorScreenPos(content_pos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    // Followups D8: generous left/right page gutter (page_gutter, 48 px)
    // with the vertical padding unchanged (space_xl).
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(metrics::page_gutter * scale, metrics::space_xl * scale));
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
                     Pack(current_palette_->muted), "Saved");
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
        case Page::History:
            DrawHistoryPage(scale);
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
        case Page::Profiles:
            DrawProfilesPage(scale);
            break;
        case Page::Settings:
            DrawSettingsPage(scale);
            break;
    }
}

void MainWindow::DrawTopStrip(float scale) {
    // Hairline top strip (DESIGN.md section 7): wordmark at left, the seven
    // destinations in rail order, the status line (square + word + locality
    // phrase, T4b) at the right-end reserve, and the Hide ghost at the strip
    // end. Ctrl+digits activate the same destinations positionally (see the
    // shortcut block inside ##strip_items).
    // T11/T12 landed: History and Profiles sit mid-array at their DESIGN
    // section 7 positions, so the strip carries nine destinations and the
    // positional Ctrl mapping is Ctrl+1 (Home) .. Ctrl+9 (Settings) —
    // Snippets moved from Ctrl+4 to Ctrl+5 in that mapping. The strip must
    // keep working positionally if destinations are added or removed.
    const float strip_h = metrics::topstrip_height * scale;
    const float strip_w = ImGui::GetIO().DisplaySize.x;
    // No filled panel: window background with a 1 px hairline at the bottom.
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    // Followups D8: the strip's side inset matches the page gutter so the
    // wordmark aligns with the page content below it. The avail budget
    // below subtracts BOTH side paddings (the old budget ignored the
    // default 16 px WindowPadding and could run into the right padding).
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(metrics::page_gutter * scale, 0.0f));
    ImGui::BeginChild("##strip", ImVec2(strip_w, strip_h), ImGuiChildFlags_None);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Wordmark at the design size until real fonts land, vertically centred
    // in the strip. It reserves its slot so the items region can size off it.
    ImGui::PushFont(nullptr, metrics::font_wordmark_px);
    const ImVec2 wordmark_size = ImGui::CalcTextSize("Echo flow");
    ImGui::PopFont();
    const ImVec2 strip_origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(wordmark_size.x, strip_h));
    {
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        ImGui::PushFont(nullptr, metrics::font_wordmark_px);
        draw.AddText(ImVec2(strip_origin.x, strip_origin.y + (strip_h - wordmark_size.y) * 0.5f),
                     Pack(current_palette_->text), "Echo flow");
        ImGui::PopFont();
    }

    struct NavItem {
        const char* id;
        const char* label;
        Page page;
    };
    // Same order as the retired rail. Welcome is not a strip destination.
    const NavItem items[] = {
        {"##strip_home", "Home", Page::Home},
        {"##strip_history", "History", Page::History},
        {"##strip_insights", "Insights", Page::Insights},
        {"##strip_dictionary", "Dictionary", Page::Dictionary},
        {"##strip_snippets", "Snippets", Page::Snippets},
        {"##strip_transforms", "Transforms", Page::Transforms},
        {"##strip_scratchpad", "Scratchpad", Page::Scratchpad},
        {"##strip_profiles", "Profiles", Page::Profiles},
        {"##strip_settings", "Settings", Page::Settings},
    };

    // Right-end reserve: the T4b status line (square + word + phrase) plus
    // the Hide ghost (LightButton padding is metrics::space_m per side).
    // The reserve is 128 design px so square + word + short phrase fit
    // without spilling into Hide; growing it reflows truncation/scroll
    // below automatically (status_reserve feeds the avail budget).
    const float gap = metrics::space_s * scale;
    const float status_reserve = metrics::space_xl * 4.0f * scale;
    float hide_w = 0.0f;
    {
        ImGui::PushFont(nullptr, metrics::font_body_px);
        hide_w = ImGui::CalcTextSize("Hide").x + metrics::space_xl * scale + 2.0f * scale;
        ImGui::PopFont();
    }
    float avail = strip_w - 2.0f * metrics::page_gutter * scale - wordmark_size.x -
                  status_reserve - hide_w - 3.0f * gap;
    if (avail < 0.0f) {
        avail = 0.0f;
    }

    // Overflow (DESIGN.md section 7): labels never wrap. Truncate with an
    // ellipsis from the end first (the first item that does not fit shrinks
    // to the remaining width), then the items region scrolls horizontally
    // as the fallback. No second row, no menu. The edge fade is not
    // implemented (noted deviation): the scrollbar and the truncation together
    // keep every destination reachable without a fake fade.
    // INVARIANT: measurement is at the body font, matching TopStripItem.
    const float item_pad = (metrics::space_s + metrics::space_xs) * scale;
    std::vector<std::string> labels;
    labels.reserve(sizeof(items) / sizeof(items[0]));
    {
        ImGui::PushFont(nullptr, metrics::font_body_px);
        float running = 0.0f;
        bool truncated = false;
        // The render loop below puts ImGui::SameLine() between items; each
        // SameLine() advances by style.ItemSpacing.x, so the budget carries
        // one ItemSpacing.x per item after the first (kept in `natural` via
        // the labels.empty() first-item test). Rendering measures with the
        // same style that spaces items, so the budget cannot drift from the
        // drawn layout (ItemSpacing.x == metrics::space_s * scale).
        const float item_gap = ImGui::GetStyle().ItemSpacing.x;
        for (const NavItem& item : items) {
            const std::string label(item.label);
            const float natural = ImGui::CalcTextSize(label.c_str()).x + item_pad +
                                  (labels.empty() ? 0.0f : item_gap);
            if (!truncated && running + natural <= avail) {
                running += natural;
                labels.push_back(label);
                continue;
            }
            if (!truncated) {
                const float remaining = avail - running;
                std::string candidate = label;
                while (!candidate.empty() &&
                       ImGui::CalcTextSize((candidate + "...").c_str()).x + item_pad >
                           remaining) {
                    candidate.pop_back();
                }
                if (!candidate.empty()) {
                    labels.push_back(candidate + "...");
                    running = avail;
                    truncated = true;
                    continue;
                }
                // Too little room even for an ellipsis: keep the full label
                // and let the horizontal scroll carry it.
                labels.push_back(label);
                running += natural;
                truncated = true;
                continue;
            }
            labels.push_back(label);
            running += natural;
        }
        ImGui::PopFont();
    }

    // Items region: horizontal scroll is the overflow fallback. The thin
    // scrollbar keeps the 48 px strip height inside the token.
    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, metrics::space_s * scale);
    ImGui::BeginChild("##strip_items", ImVec2(avail, strip_h), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleVar();
    // Ctrl+digits (T4b): positional shortcuts mapped to the CURRENT items
    // array (nine destinations since T11/T12: Home is Ctrl+1 through
    // Settings is Ctrl+9; Snippets is Ctrl+5). Mapping follows the array
    // positionally, so adding or removing destinations shifts later
    // shortcuts accordingly. Guarded so typing digits inside
    // one of the InputText fields is never hijacked. The Full window is
    // the only surface here (RunOnce early-outs while hidden, so a hidden
    // window cannot receive these), and Welcome stays shortcut-active:
    // navigating away is a user choice; first_run stays true so Welcome
    // returns next launch (recorded in the T4b PROGRESS row).
    if (!ImGui::GetIO().WantTextInput &&
        (ImGui::GetIO().KeyMods & ImGuiMod_Ctrl) != 0) {
        static constexpr ImGuiKey kShortcutKeys[] = {
            ImGuiKey_1, ImGuiKey_2, ImGuiKey_3, ImGuiKey_4, ImGuiKey_5,
            ImGuiKey_6, ImGuiKey_7, ImGuiKey_8, ImGuiKey_9,
        };
        constexpr std::size_t item_count = sizeof(items) / sizeof(items[0]);
        constexpr std::size_t key_count = sizeof(kShortcutKeys) / sizeof(kShortcutKeys[0]);
        const std::size_t bound = item_count < key_count ? item_count : key_count;
        for (std::size_t shortcut = 0; shortcut < bound; ++shortcut) {
            // Shortcuts fire once per physical press: without the false
            // repeat flag a held Ctrl+digit re-fires every repeat tick and
            // would re-PersistPage/ClearApiKeyInputs while held.
            if (ImGui::IsKeyPressed(kShortcutKeys[shortcut], false)) {
                ActivateStripPage(items[shortcut].page);
                break;
            }
        }
    }
    // Restore keyboard focus after a page switch or re-show, only when the
    // user is actually driving with the keyboard (NavActive) so a mouse
    // click or tray pop never steals focus. Runs immediately before the
    // first strip item so that item receives it. Any mouse use cancels a
    // stale arm: a mouse-first user who clicks anywhere (strip or content)
    // must not have the next Tab yank focus back to the strip (R-W2-003).
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        page_focus_pending_ = false;
    }
    if (page_focus_pending_ && ImGui::GetIO().NavActive) {
        ImGui::SetKeyboardFocusHere();
        page_focus_pending_ = false;
    }
    std::size_t index = 0;
    for (const NavItem& item : items) {
        // DECISION: switch on the TopStripItem activation return, not
        // IsItemClicked. The widget's InvisibleButton activation returns
        // pressed=true for both mouse and keyboard (nav) activation, so one
        // check covers Tab+Enter/Space alike (R-W2-001).
        const bool pressed = lwf::ui::widgets::TopStripItem(item.id, labels[index].c_str(),
                                                            page_ == item.page, scale,
                                                            *current_palette_);
        if (pressed) {
            // Single shared press path: Ctrl+digit shortcuts land in the
            // same ActivateStripPage so persistence and focus match the
            // strip exactly (one function, not copy-pasted logic).
            ActivateStripPage(item.page);
        }
        ++index;
        if (index < sizeof(items) / sizeof(items[0])) {
            ImGui::SameLine();
        }
    }
    ImGui::EndChild();

    // Status line (T4b, DESIGN.md section 6 amendment (f)): 8 px square +
    // word + muted locality phrase, vertically centred in the 48 px strip
    // inside the reserve above. StatusKind is a palette-role enum, not the
    // FSM names, so the table below is the as-built mapping. Precedence is
    // top to bottom: validation error, paused, capturing, thinking, ready.
    // The phrase follows the speech fact first: cloud wins over cleanup
    // because the destination of the spoken audio is the bigger privacy
    // fact; OpenRouter only shows when cleanup is actually on and cloud is
    // off; otherwise the phrase is Local. Reads follow the Home pattern
    // (direct fsm_ + settings_ reads, no snapshot) by design.
    ImGui::SameLine();
    {
        lwf::ui::widgets::StatusKind status_kind = lwf::ui::widgets::StatusKind::Ok;
        const char* status_word = "Ready";
        if (!validation_error_.empty()) {
            status_kind = lwf::ui::widgets::StatusKind::Err;
            status_word = "Attention";
        } else if (!settings_.GetBool("dictation_enabled")) {
            status_kind = lwf::ui::widgets::StatusKind::Muted;
            status_word = "Paused";
        } else if (fsm_.CurrentState() == State::CAPTURING) {
            status_kind = lwf::ui::widgets::StatusKind::Accent;
            status_word = "Listening";
        } else if (fsm_.CurrentState() == State::TRANSCRIBING ||
                   fsm_.CurrentState() == State::POSTPROCESSING ||
                   fsm_.CurrentState() == State::EMITTING) {
            status_kind = lwf::ui::widgets::StatusKind::Accent;
            status_word = "Thinking";
        } else {
            status_kind = lwf::ui::widgets::StatusKind::Ok;
            status_word = "Ready";
        }

        const std::string provider = settings_.GetString("stt_provider");
        const bool cloud_active =
            provider != "local" && settings_.GetBool("stt_consent_accepted");
        std::string phrase;
        if (cloud_active) {
            if (provider == "gnani") {
                phrase = "Cloud: Gnani";
            } else if (provider == "elevenlabs") {
                phrase = "Cloud: ElevenLabs";
            } else if (provider == "muse") {
                phrase = "Cloud: Muse";
            } else {
                phrase = "Cloud";
            }
        } else if (settings_.GetBool("cleanup_enabled") &&
                   settings_.GetString("llm_provider") == "openrouter") {
            phrase = "OpenRouter";
        } else {
            phrase = "Local";
        }

        // Fit the phrase to the reserve at the same font StatusSquare uses
        // (body 13). The square + word are never truncated; if the phrase
        // would spill into Hide it is ellipsis-truncated to what fits, or
        // dropped entirely when even an ellipsis does not fit. INVARIANT:
        // status_reserve is measured in the same scaled px as the text
        // (FontScaleMain applies DPI once), so the fit holds at 100%,
        // 150% and 200% alike with no DPI-specific branch.
        ImGui::PushFont(nullptr, metrics::font_body_px);
        const ImVec2 word_size = ImGui::CalcTextSize(status_word);
        const float square_size = metrics::status_square * scale;
        const float inline_gap = lwf::ui::studio_metrics::gap_inline * scale;
        const float status_w = square_size + inline_gap + word_size.x;
        const float phrase_gap = metrics::space_s * scale;
        std::string fitted_phrase = phrase;
        if (!fitted_phrase.empty() &&
            status_w + phrase_gap + ImGui::CalcTextSize(fitted_phrase.c_str()).x >
                status_reserve) {
            while (!fitted_phrase.empty() &&
                   status_w + phrase_gap +
                           ImGui::CalcTextSize((fitted_phrase + "...").c_str()).x >
                       status_reserve) {
                fitted_phrase.pop_back();
            }
            if (fitted_phrase.empty()) {
                fitted_phrase.clear();
            } else {
                fitted_phrase += "...";
            }
        }
        const float phrase_w =
            fitted_phrase.empty() ? 0.0f : ImGui::CalcTextSize(fitted_phrase.c_str()).x;
        const float line_h = std::max(square_size, word_size.y);
        ImGui::PopFont();

        ImGui::SetCursorPosY((strip_h - line_h) * 0.5f);
        // DECISION: the palette is passed explicitly (*current_palette_).
        // The widget default is kLightPalette (Paper), which would render
        // the square in the wrong theme under Graphite.
        // NOTE: the phrase is placed with SetCursorScreenPos, not SameLine.
        // After the preceding SameLine the line baseline still belongs to
        // the strip top, so SameLine would put the phrase on the line
        // above the square (observed in capture) instead of beside it.
        const ImVec2 status_origin = ImGui::GetCursorScreenPos();
        lwf::ui::widgets::StatusSquare("##strip_status", status_kind, status_word, scale,
                                       *current_palette_);
        if (!fitted_phrase.empty()) {
            ImGui::SetCursorScreenPos(
                ImVec2(status_origin.x + status_w + phrase_gap, status_origin.y));
            ImGui::TextDisabled("%s", fitted_phrase.c_str());
        }
        // Pad the consumed width back out to the full reserve so the Hide
        // ghost keeps its fixed right-end position whatever the phrase fit.
        // INVARIANT: the padding SameLine carries 0 spacing so the strip
        // keeps its exact 3-gap width budget (wordmark->items, items->
        // status, padding->Hide); a default gap here would push Hide 8 px
        // past the right edge.
        const float drawn_w = status_w + (fitted_phrase.empty() ? 0.0f : phrase_gap + phrase_w);
        const float remaining_w = status_reserve - drawn_w;
        if (remaining_w > 0.0f) {
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::Dummy(ImVec2(remaining_w, 0.0f));
        }
    }
    // Hide affordance, relocated from the rail bottom: a ghost item at the
    // strip's right end. DECISION: reuse widgets::LightButton (the Hairline
    // ghost) instead of a hand-drawn InvisibleButton, so the strip end
    // keeps a focus ring and keyboard activation with no new chrome.
    ImGui::SameLine();
    ImGui::SetCursorPosY((strip_h - (metrics::space_xl + metrics::space_s) * scale) * 0.5f);
    if (lwf::ui::widgets::LightButton("##strip_hide", "Hide", scale, *current_palette_)) {
        SetVisible(false);
    }

    // Bottom hairline of the strip.
    {
        const ImVec2 strip_pos = ImGui::GetWindowPos();
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        draw.AddLine(ImVec2(strip_pos.x, strip_pos.y + strip_h - 1.0f * scale),
                     ImVec2(strip_pos.x + strip_w, strip_pos.y + strip_h - 1.0f * scale),
                     Pack(current_palette_->border), 1.0f * scale);
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
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(kLightPalette.surface));
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
    const float switch_h = metrics::control_min_height * scale;
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
    // Followups-2 D1: commit while dragging, not only on release. The store
    // always holds the dragged value, so a missed deactivate event (focus
    // loss mid-release, alt-tab) can never roll the knob back, and any
    // later ApplySettings drain picks the value up even without one.
    if (ImGui::IsItemEdited()) {
        settings_.SetDouble(key, static_cast<double>(value));
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        settings_.SetDouble(key, static_cast<double>(value));
        ApplyAndSave();
    }
}

void MainWindow::VolumeRow() {
    // Followups-2 D1: the volume knob commits live (DoubleRow semantics)
    // and additionally asks for a short sample tone at the dragged
    // amplitude, throttled so a continuous drag does not queue a tone per
    // frame. The final sample on release doubles as confirmation that the
    // knob took effect without waiting for the next dictation.
    float value = static_cast<float>(settings_.GetDouble("sound_volume"));
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##sound_volume", &value, 0.0f, 1.0f, "volume %.2f");
    if (ImGui::IsItemEdited()) {
        settings_.SetDouble("sound_volume", static_cast<double>(value));
        const float now = static_cast<float>(ImGui::GetTime());
        if (now >= tone_preview_until_) {
            tone_preview_until_ = now + 0.25f;
            bus_.PostCommand(UiCommandType::PreviewTone, std::to_string(value));
        }
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        settings_.SetDouble("sound_volume", static_cast<double>(value));
        tone_preview_until_ = static_cast<float>(ImGui::GetTime()) + 0.25f;
        bus_.PostCommand(UiCommandType::PreviewTone, std::to_string(value));
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
        const auto role = HotkeyBinding::RoleForConfigKey(key);
        if (role.has_value()) {
            const std::string candidate = field.buffer;
            const std::size_t first = candidate.find_first_not_of(" \t");
            if (first == std::string::npos) {
                field.buffer[0] = '\0';
                settings_.SetString(key, "");
                validation_error_.clear();
                if (restart) {
                    restart_needed_ = true;
                }
                ApplyAndSave();
                return;
            }
            const std::vector<Binding> registry = LoadBindings(settings_);
            const std::string error =
                HotkeyBinding::ValidateRecorderChord(candidate, registry, *role);
            if (!error.empty()) {
                validation_error_ = error;
                std::snprintf(field.buffer, sizeof(field.buffer), "%s",
                              settings_.GetString(key).c_str());
                return;
            }
            validation_error_.clear();
        }
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
    std::filesystem::remove(base / "history.jsonl", ec);
    std::filesystem::remove(base / "stats.json.bak", ec);
    std::filesystem::remove(base / "dictionary.json.bak", ec);
    std::filesystem::remove(base / "snippets.json.bak", ec);
    std::filesystem::remove(base / "transforms.json.bak", ec);
    std::filesystem::remove(base / "api_keys.json.bak", ec);
    std::filesystem::remove(base / "stats.json.tmp", ec);
    std::filesystem::remove(base / "dictionary.json.tmp", ec);
    std::filesystem::remove(base / "snippets.json.tmp", ec);
    std::filesystem::remove(base / "transforms.json.tmp", ec);
    std::filesystem::remove(base / "api_keys.json.tmp", ec);
    std::filesystem::remove(base / "history.jsonl.tmp", ec);
    if (reset_stores_.dictionary != nullptr) {
        reset_stores_.dictionary->Clear();
    }
    if (reset_stores_.dictionary_legacy != nullptr) {
        reset_stores_.dictionary_legacy->SetWords({});
        reset_stores_.dictionary_legacy->SetReplacements({});
    }
    if (reset_stores_.snippets != nullptr) {
        reset_stores_.snippets->SetEntries({});
    }
    if (reset_stores_.stats != nullptr) {
        reset_stores_.stats->Clear();
    }
    if (reset_stores_.transforms != nullptr) {
        (void)reset_stores_.transforms->Reload();
    }
    if (reset_stores_.notes != nullptr) {
        (void)reset_stores_.notes->Reload();
    }
    if (reset_stores_.api_keys != nullptr) {
        reset_stores_.api_keys->Reload();
    }
    if (reset_stores_.history != nullptr) {
        reset_stores_.history->Clear();
    }
    delete_data_armed_ = false;
    ClearApiKeyInputs();
}

void MainWindow::ClearApiKeyInputs() {
    std::memset(gnani_key_buffer_, 0, sizeof(gnani_key_buffer_));
    std::memset(elevenlabs_key_buffer_, 0, sizeof(elevenlabs_key_buffer_));
    std::memset(muse_key_buffer_, 0, sizeof(muse_key_buffer_));
    std::memset(openrouter_key_buffer_, 0, sizeof(openrouter_key_buffer_));
    gnani_remove_armed_ = false;
    elevenlabs_remove_armed_ = false;
    muse_remove_armed_ = false;
    openrouter_remove_armed_ = false;
    gnani_keep_focus_pending_ = false;
    elevenlabs_keep_focus_pending_ = false;
    muse_keep_focus_pending_ = false;
    openrouter_keep_focus_pending_ = false;
    gnani_key_error_.clear();
    elevenlabs_key_error_.clear();
    muse_key_error_.clear();
    openrouter_key_error_.clear();
}

std::string MainWindow::ResolveOpenRouterKey() const {
    std::string resolved_key;
    if (reset_stores_.api_keys != nullptr) {
        resolved_key = reset_stores_.api_keys->Get("openrouter");
    }
    if (resolved_key.empty()) {
        const char* env_key = std::getenv("OPENROUTER_API_KEY");
        if (env_key != nullptr) {
            resolved_key = env_key;
        }
    }
    return resolved_key;
}

void MainWindow::DrawApiKeyRow(const char* store_id, const char* display_name,
                               const char* env_name, const char* confirm_question,
                               const char* confirm_button, char* buffer,
                               std::size_t buffer_size, bool& remove_armed,
                               bool& focus_keep_pending, std::string& error, float scale) {
    ApiKeyStore* store = reset_stores_.api_keys;
    if (store == nullptr || store_id == nullptr || display_name == nullptr || buffer == nullptr ||
        confirm_question == nullptr || confirm_button == nullptr || buffer_size == 0) {
        ImGui::TextUnformatted(display_name != nullptr ? display_name : "Key");
        ImGui::TextDisabled("Keys are not available right now.");
        return;
    }
    const bool has_saved = store->Has(store_id);
    bool has_env = false;
    if (env_name != nullptr) {
        const char* env_value = std::getenv(env_name);
        has_env = env_value != nullptr && env_value[0] != '\0';
    }
    const SettingsUiModel::ApiKeySource source =
        SettingsUiModel::ApiKeySourceFor(has_saved, has_env);
    const char* status_word = SettingsUiModel::ApiKeyStatusWord(source);
    const char* source_label = SettingsUiModel::ApiKeySourceLabel(source);
    // Status: dot shape + word + source word (never color alone, R8).
    const std::uint32_t dot_color = source == SettingsUiModel::ApiKeySource::NotSet
                                        ? current_palette_->muted
                                        : current_palette_->ok;
    ImGui::TextUnformatted(display_name);
    ImGui::SameLine();
    const ImVec2 dot_pos = ImGui::GetCursorScreenPos();
    const float line = ImGui::GetTextLineHeight();
    ImDrawList& status_draw = *ImGui::GetWindowDrawList();
    status_draw.AddCircleFilled(
        ImVec2(dot_pos.x + 4.0f * scale + 2.0f * scale, dot_pos.y + line * 0.5f),
        4.0f * scale, Pack(dot_color));
    ImGui::Dummy(ImVec2(kStatusDotIndent * scale, 0.0f));
    ImGui::SameLine();
    ImGui::TextUnformatted(status_word);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", source_label);
    if (remove_armed) {
        // Esc cancels the named confirmation without touching the saved key.
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            remove_armed = false;
            focus_keep_pending = false;
            std::memset(buffer, 0, buffer_size);
            error.clear();
            return;
        }
        // SECURITY: the question names the service so the destructive
        // action is explicit (R13); the key value never appears here.
        ImGui::TextColored(Color(current_palette_->err), "%s", confirm_question);
        const std::string remove_id = std::string("##remove_") + store_id;
        bool do_remove = false;
        bool do_keep = false;
        do_remove =
            KeyButton(remove_id.c_str(), confirm_button, false, scale, *current_palette_);
        ImGui::SameLine();
        if (focus_keep_pending) {
            ImGui::SetKeyboardFocusHere();
            focus_keep_pending = false;
        }
        const std::string keep_id = std::string("##keep_") + store_id;
        do_keep = KeyButton(keep_id.c_str(), "Keep key", false, scale, *current_palette_);
        if (do_remove) {
            if (store->Remove(store_id)) {
                std::memset(buffer, 0, buffer_size);
                error.clear();
                remove_armed = false;
                focus_keep_pending = false;
                restart_needed_ = true;
                ShowSaved();
            } else {
                error = "Could not remove the key.";
            }
        } else if (do_keep) {
            std::memset(buffer, 0, buffer_size);
            error.clear();
            remove_armed = false;
            focus_keep_pending = false;
        }
        if (!error.empty()) {
            ImGui::TextColored(Color(current_palette_->err), "%s", error.c_str());
        }
        return;
    }
    const std::string input_id = std::string("##key_") + store_id;
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText(input_id.c_str(), buffer, buffer_size, ImGuiInputTextFlags_Password);
    if (ImGui::IsItemFocused()) {
        const ImVec2 input_min = ImGui::GetItemRectMin();
        const ImVec2 input_max = ImGui::GetItemRectMax();
        ImDrawList& input_draw = *ImGui::GetWindowDrawList();
        input_draw.AddRect(ImVec2(input_min.x - 2.0f * scale, input_min.y - 2.0f * scale),
                           ImVec2(input_max.x + 2.0f * scale, input_max.y + 2.0f * scale),
                           Pack(current_palette_->focus), metrics::radius_control * scale, 0,
                           2.0f * scale);
    }
    ImGui::TextDisabled("Paste a key to save");
    if (!error.empty()) {
        ImGui::TextColored(Color(current_palette_->err), "%s", error.c_str());
    }
    const std::string save_id = std::string("##save_") + store_id;
    if (KeyButton(save_id.c_str(), "Save key", true, scale, *current_palette_)) {
        const std::string trimmed = SettingsUiModel::TrimApiKeyInput(buffer);
        const std::string save_error = SettingsUiModel::ApiKeySaveError(trimmed);
        if (!save_error.empty()) {
            error = save_error;
        } else if (store->Set(store_id, trimmed)) {
            std::memset(buffer, 0, buffer_size);
            error.clear();
            restart_needed_ = true;
            ShowSaved();
        } else {
            error = "Could not save the key.";
        }
    }
    if (has_saved) {
        ImGui::SameLine();
        const std::string remove_id = std::string("##remove_saved_") + store_id;
        if (KeyButton(remove_id.c_str(), "Remove saved key", false, scale, *current_palette_)) {
            std::memset(buffer, 0, buffer_size);
            error.clear();
            remove_armed = true;
            focus_keep_pending = true;
        }
    }
}

void MainWindow::PersistPage() {
    if (page_ == Page::Welcome) {
        return;
    }
    settings_.SetString("last_page", RouteName(page_));
    settings_.Save();
}

void MainWindow::ActivateStripPage(Page page) {
    page_ = page;
    PersistPage();
    validation_error_.clear();  // errors belong to the page that set them
    ClearApiKeyInputs();
    // Focus is already on the pressed item (strip press) or must not be
    // yanked to the first strip item on a later Tab (Ctrl+digit); never
    // arm the restore here (R-W2-003). Bindings live only in SetVisible
    // and Welcome-Finish.
    page_focus_pending_ = false;
}

void MainWindow::OpenHistoryPage() {
    ActivateStripPage(Page::History);
}

void MainWindow::OpenProfilesPage() {
    ActivateStripPage(Page::Profiles);
}

void MainWindow::OpenSettingsPrivacy() {
    settings_tab_ = SettingsTab::Privacy;
    ActivateStripPage(Page::Settings);
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
    ImGui::TextColored(Color(current_palette_->warn), "Applies after restart.");
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
                           Pack(current_palette_->text, 0.05f), metrics::radius_control * scale);
        draw.AddRect(ImVec2(x, y), ImVec2(x + chip_w, y + chip_h),
                     Pack(current_palette_->border), metrics::radius_control * scale, 0, 1.0f * scale);
        draw.AddText(ImVec2(x + pad_x, y + (chip_h - text_size.y) * 0.5f),
                     Pack(current_palette_->text), parts[i].c_str());
        x += chip_w;
        if (i + 1 < parts.size()) {
            draw.AddText(ImVec2(x + 5.0f * scale, y + (chip_h - text_size.y) * 0.5f),
                         Pack(current_palette_->muted), "+");
            x += 18.0f * scale;
        }
    }
    ImGui::Dummy(ImVec2(x - origin.x, chip_h));
}

void MainWindow::DrawGeneralHotkeyRow(const std::string& config_key, const char* label,
                                      const char* helper, float scale) {
    const auto role = HotkeyBinding::RoleForConfigKey(config_key);
    const std::string current = settings_.GetString(config_key);
    // T5 ruled row (DESIGN §6): Row draws label + helper; the control slot
    // holds the chips and the Record/Clear buttons on one line. Conflict and
    // learn error strings are drawn after RowEnd so they never overflow the
    // 52 px row. The first row keeps the former BeginCard id so the
    // page-structure pin survives the card-to-ruled migration.
    const std::string row_id = config_key == "hotkey_ptt"
                                   ? "##card_general_hotkey"
                                   : ("##row_hotkey_" + config_key);
    lwf::ui::widgets::Row(row_id.c_str(), label, helper, 360.0f * scale, scale, *current_palette_);
    DrawHotkeyChips(current, scale);
    // DrawHotkeyChips reserves via Dummy (cursor drops to a new line); keep
    // the Record/Clear buttons on that same line inside the 52 px slot.
    ImGui::SameLine();
    bool has_conflict = false;
    if (!current.empty() && role.has_value()) {
        const std::vector<Binding> registry = LoadBindings(settings_);
        if (HotkeyBinding::FindConflict(current, registry, role).has_value()) {
            has_conflict = true;
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
        ImGui::SameLine(0.0f, metrics::space_s * scale);
        const std::string clear_id = "Clear##" + config_key;
        if (ImGui::Button(clear_id.c_str())) {
            settings_.SetString(config_key, "");
            if (config_key == "hotkey_handsfree") {
                hotkey_handsfree_field_.seeded = false;
            } else if (config_key == "hotkey_cancel") {
                hotkey_cancel_field_.seeded = false;
            } else if (config_key == "hotkey_command") {
                hotkey_command_field_.seeded = false;
            } else if (config_key == "hotkey_paste_last") {
                hotkey_paste_last_field_.seeded = false;
            }
            ApplyAndSave();
        }
        if (buttons_disabled) {
            ImGui::EndDisabled();
        }
    }
    lwf::ui::widgets::RowEnd(scale);
    if (has_conflict) {
        ImGui::TextColored(Color(current_palette_->err), "That shortcut is already in use");
    }
    if (learn_error_target_ == config_key && !learn_error_.empty()) {
        ImGui::TextColored(Color(current_palette_->err), "%s", learn_error_.c_str());
    }
}

void MainWindow::DrawHomePage(float scale) {
    DrawPageChrome("Home", "Dictation runs on this computer and starts with one hotkey.", scale);
    // T8 ruled rows (DESIGN §6): same migration note as DrawGeneralPage;
    // first row reuses the former BeginCard id. Trust and Last dictation
    // are taller than the 52 px ruled row (stacked sentences / wrapped
    // transcript + Copy button), so they stay uncarded blocks (block
    // exceptions, as in T6/T7); their former BeginCard ids are carried by
    // PushID so the page-structure pins keep passing. All keys here are
    // hot-apply: no restart flag is set on Home (no DrawRestartBanner).
    const State fsm_state = fsm_.CurrentState();
    const bool enabled = settings_.GetBool("dictation_enabled");

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Right now");
    ImGui::PopFont();
    {
        const std::string hotkey = settings_.GetString("hotkey_ptt");
        const char* rest = nullptr;
        std::uint32_t status_color = current_palette_->ok;
        const char* status_word = "Ready";
        if (!enabled) {
            status_word = "Paused";
            status_color = current_palette_->muted;
            rest = "Dictation is off.";
        } else if (fsm_state == State::CAPTURING) {
            status_word = "Listening";
            status_color = current_palette_->accent;
            rest = "Speak, then release.";
        } else if (fsm_state == State::TRANSCRIBING || fsm_state == State::POSTPROCESSING ||
                   fsm_state == State::EMITTING) {
            status_word = "Thinking";
            status_color = current_palette_->text;
            rest = "Turning speech into text.";
        } else if (hotkey.empty()) {
            rest = "Set a hotkey to start.";
        } else {
            rest = "Hold the hotkey to dictate.";
        }
        // Read-only readout row (Privacy OpenRouter-model precedent): the
        // status word + dot live in the control slot, the derived rest copy
        // is the row helper.
        lwf::ui::widgets::Row("##card_now", "Status", rest, 248.0f * scale, scale,
                              *current_palette_);
        const ImVec2 word_pos = ImGui::GetCursorScreenPos();
        const float line = ImGui::GetTextLineHeight();
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        draw.AddCircleFilled(ImVec2(word_pos.x + 4.0f * scale, word_pos.y + line * 0.5f),
                             4.0f * scale, Pack(status_color));
        ImGui::Dummy(ImVec2(kStatusDotIndent * scale, 0.0f));
        ImGui::SameLine();
        ImGui::TextUnformatted(status_word);
        lwf::ui::widgets::RowEnd(scale);
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Hotkey");
    ImGui::PopFont();
    {
        // 360 px compound slot (General hotkey-row precedent): chips + the
        // Learn button on the slot line.
        lwf::ui::widgets::Row("##card_hotkey", "Push-to-talk",
                              "Hold to speak, release to type.", 360.0f * scale, scale,
                              *current_palette_);
        DrawHotkeyChips(settings_.GetString("hotkey_ptt"), scale);
        // DrawHotkeyChips reserves via Dummy (cursor drops to a new line);
        // keep the Learn button on that same line inside the 52 px slot.
        ImGui::SameLine();
        if (ImGui::Button("Learn a new chord...")) {
            bus_.PostCommand(UiCommandType::StartLearnHotkey);
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Trust");
    ImGui::PopFont();
    // BLOCK EXCEPTION (DESIGN §6, as-built T8 — same note as DrawCloudPage):
    // the cloud and OpenRouter lines are independent and can stack (two
    // sentences + a warn line), taller than the 52 px ruled row, so Trust
    // stays an uncarded block on the page background. DECISION: Home does
    // not gate on cleanup_enabled (the strip does) — preserved as built.
    ImGui::PushID("##card_trust");
    {
        const std::string provider = settings_.GetString("stt_provider");
        const bool cloud_active = provider != "local" && settings_.GetBool("stt_consent_accepted");
        const bool openrouter_cleanup = settings_.GetString("llm_provider") == "openrouter";
        if (cloud_active) {
            const char* display = provider == "gnani" ? "Gnani"
                                  : provider == "elevenlabs" ? "ElevenLabs"
                                  : provider == "muse" ? "Muse" : provider.c_str();
            ImGui::Text("Speech is sent to %s for each sentence.", display);
            ImGui::TextDisabled("If a cloud request fails, that sentence is still dictated on this "
                                "computer.");
        }
        if (openrouter_cleanup) {
            ImGui::TextColored(Color(current_palette_->warn),
                               "Cleanup uses OpenRouter: text is sent to OpenRouter when cleanup is on.");
        }
        if (!cloud_active && !openrouter_cleanup) {
            ImGui::TextUnformatted("Everything runs on this computer.");
            ImGui::TextDisabled("Cloud speech and OpenRouter cleanup are off.");
        }
    }
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Quick settings");
    ImGui::PopFont();
    {
        bool dictation = settings_.GetBool("dictation_enabled");
        lwf::ui::widgets::Row("##card_quick", "Dictation", nullptr, 52.0f * scale, scale,
                              *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##dictation_enabled", dictation, scale,
                                           *current_palette_)) {
            settings_.SetBool("dictation_enabled", !dictation);
            bus_.PostCommand(UiCommandType::SetEnabled, !dictation ? "1" : "0");
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
        bool cleanup = settings_.GetBool("cleanup_enabled");
        lwf::ui::widgets::Row("##row_cleanup_enabled", "Clean up text",
                              "Polish dictation with the local model", 52.0f * scale, scale,
                              *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##cleanup_enabled", cleanup, scale,
                                           *current_palette_)) {
            settings_.SetBool("cleanup_enabled", !cleanup);
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
        bool hands_free = settings_.GetBool("double_tap_handsfree");
        lwf::ui::widgets::Row("##row_double_tap_handsfree", "Hands-free",
                              "Double-tap to talk without holding", 52.0f * scale, scale,
                              *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##double_tap_handsfree", hands_free, scale,
                                           *current_palette_)) {
            settings_.SetBool("double_tap_handsfree", !hands_free);
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Last dictation");
    ImGui::PopFont();
    // BLOCK EXCEPTION (DESIGN §6, as-built T8 — same note as DrawCloudPage):
    // the wrapped transcript plus the Copy button are taller than the
    // 52 px ruled row, so Last dictation stays an uncarded block.
    ImGui::PushID("##card_last");
    {
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
    ImGui::PopID();
}

void MainWindow::DrawSpeechPage(float scale) {
    DrawPageChrome("Speech", "The speech model and how it listens.", scale);
    DrawRestartBanner(scale);
    // T5 ruled rows (DESIGN §6): same migration note as DrawGeneralPage;
    // first rows reuse the former BeginCard ids (mic order is pinned).
    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Model");
    ImGui::PopFont();
    {
        if (model_path_buffer_[0] == '\0') {
            const std::string current = settings_.GetString("whisper_model_path");
            std::snprintf(model_path_buffer_, sizeof(model_path_buffer_), "%s", current.c_str());
        }
        // The Download button is catalog-driven (T13a): the exe-dir
        // models.json the Welcome step reads, the first whisper entry.
        // While its url is empty the button stays inert (Q2); Browse below
        // is never blocked by the catalog.
        const Onboarding::ModelCatalog download_catalog =
            Onboarding::ParseModelsJson(GetExeDir() + "\\models.json");
        const Onboarding::ModelDownload* speech_entry = nullptr;
        if (!download_catalog.whisper.empty()) {
            speech_entry = &download_catalog.whisper.front();
        }
        const bool download_available = speech_entry != nullptr && !speech_entry->url.empty() &&
                                        Onboarding::IsSafeFileName(speech_entry->name);
        const float model_control_w = 380.0f * scale;
        const float browse_width = ImGui::CalcTextSize("Browse...").x +
                                   ImGui::GetStyle().FramePadding.x * 2.0f;
        const float download_width = ImGui::CalcTextSize("Download").x +
                                     ImGui::GetStyle().FramePadding.x * 2.0f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        lwf::ui::widgets::Row("##card_model", "Speech model",
                              "Restart is needed after changing the model path.", model_control_w,
                              scale, *current_palette_);
        ImGui::SetNextItemWidth(model_control_w - browse_width - spacing - download_width -
                                (download_busy_.load() ? spacing + ImGui::CalcTextSize("Cancel").x +
                                                           ImGui::GetStyle().FramePadding.x * 2.0f
                                                       : 0.0f));
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
        ImGui::SameLine();
        ImGui::BeginDisabled(download_busy_.load() || !download_available);
        const bool download_pressed = ImGui::Button("Download##download_speech");
        ImGui::EndDisabled();
        if (download_pressed && download_available && !download_busy_.load()) {
            // The transfer is synchronous and slow, so it runs on a worker
            // thread with copies of everything it needs; the cancel flag
            // stops it on the next chunk. The UI thread keeps ownership of
            // the state model, and the latch is consumed in RunOnce.
            const std::string download_url = speech_entry->url;
            const std::string download_sha = speech_entry->sha256;
            const std::int64_t download_size = static_cast<std::int64_t>(speech_entry->size_bytes);
            const std::filesystem::path download_dir =
                std::filesystem::path(GetDataDir()) / "models";
            const std::string destination = (download_dir / speech_entry->name).string();
            const std::string stored_path = "models/" + speech_entry->name;
            std::error_code mkdir_ec;
            std::filesystem::create_directories(download_dir, mkdir_ec);
            // A destination the resume planner already finds complete skips
            // the transfer (and the thread) entirely.
            std::uint64_t existing_bytes = 0;
            std::string actual_hash;
            {
                std::error_code fs_ec;
                if (std::filesystem::exists(destination, fs_ec)) {
                    existing_bytes = static_cast<std::uint64_t>(
                        std::filesystem::file_size(destination, fs_ec));
                    bool hash_ok = false;
                    actual_hash = lwf_sha256::HexDigestOfFile(destination, hash_ok);
                    if (!hash_ok) {
                        actual_hash.clear();
                    }
                }
            }
            const Onboarding::DownloadStartPlan start_plan = Onboarding::PlanDownloadStart(
                existing_bytes, speech_entry->size_bytes, speech_entry->sha256, actual_hash);
            download_model_.Start(destination, speech_entry->size_bytes);
            if (start_plan.skip) {
                download_model_.OnSuccess();
                settings_.SetString("whisper_model_path", stored_path);
                validation_error_.clear();
                restart_needed_ = true;
                ApplyAndSave();
                std::snprintf(model_path_buffer_, sizeof(model_path_buffer_), "%s",
                              stored_path.c_str());
            } else {
                download_stored_path_ = stored_path;
                download_downloaded_.store(0);
                download_cancelled_.store(false);
                download_busy_.store(true);
                if (download_worker_.joinable()) {
                    download_worker_.join();
                }
                download_worker_ = std::thread(
                    [this, download_url, destination, download_sha, download_size]() {
                        Onboarding::WindowsDownloader downloader;
                        Onboarding::DownloadSnapshot result;
                        result.destination = destination;
                        try {
                            const bool ok = downloader.Download(
                                download_url, destination, download_sha, download_size,
                                [this](const Onboarding::DownloadProgress& progress) {
                                    download_downloaded_.store(progress.downloaded);
                                    return !download_cancelled_.load();
                                });
                            if (ok) {
                                result.state = Onboarding::DownloadState::Success;
                            } else {
                                result.state = Onboarding::DownloadState::Failed;
                                result.error =
                                    Onboarding::MapDownloadError(ErrorCode::DownloadFailed,
                                                                "download did not complete");
                            }
                        } catch (const InjectionException& e) {
                            if (download_cancelled_.load() ||
                                Onboarding::IsCancellationDetail(e.what())) {
                                result.state = Onboarding::DownloadState::Cancelled;
                            } else {
                                result.state = Onboarding::DownloadState::Failed;
                                result.error =
                                    Onboarding::MapDownloadError(e.Code(), e.what());
                            }
                        } catch (const std::exception& e) {
                            result.state = Onboarding::DownloadState::Failed;
                            result.error =
                                Onboarding::MapDownloadError(ErrorCode::Unknown, e.what());
                        } catch (...) {
                            result.state = Onboarding::DownloadState::Failed;
                            result.error =
                                Onboarding::MapDownloadError(ErrorCode::Unknown, "");
                        }
                        std::lock_guard<std::mutex> latch_lock(download_latch_mutex_);
                        download_latch_ = result;
                        download_latch_ready_ = true;
                    });
            }
        }
        if (download_busy_.load()) {
            ImGui::SameLine();
            if (ImGui::Button("Cancel##cancel_download")) {
                download_cancelled_.store(true);
            }
        }
        lwf::ui::widgets::RowEnd(scale);
        if (!validation_error_.empty()) {
            ImGui::TextColored(Color(current_palette_->err), "%s", validation_error_.c_str());
        }
        // Download state row (total=0 is indeterminate: breathing bar,
        // never a stuck 0%). Written in the page's err idiom so the word
        // always carries the state next to any color.
        {
            const Onboarding::DownloadSnapshot download_state = download_model_.Snapshot();
            if (download_state.state == Onboarding::DownloadState::Downloading) {
                const double fraction = download_model_.ProgressFraction();
                if (fraction >= 0.0) {
                    char overlay[32];
                    std::snprintf(overlay, sizeof(overlay), "%.0f%%", fraction * 100.0);
                    ImGui::ProgressBar(static_cast<float>(fraction), ImVec2(-FLT_MIN, 0.0f),
                                       overlay);
                } else {
                    ImGui::ProgressBar(0.5f + 0.5f * std::sin(ImGui::GetTime() * 3.0f),
                                       ImVec2(-FLT_MIN, 0.0f), "");
                }
            } else if (download_state.state == Onboarding::DownloadState::Success) {
                ImGui::TextDisabled("Download complete. Restart the app to use the new model.");
            } else if (download_state.state == Onboarding::DownloadState::Cancelled) {
                ImGui::TextDisabled("Download stopped.");
            } else if (download_state.state == Onboarding::DownloadState::Failed) {
                if (download_state.error.code == ErrorCode::ChecksumMismatch) {
                    ImGui::TextColored(
                        Color(current_palette_->err),
                        "The saved model file did not pass its check. The saved bytes differ "
                        "from the listed check value, so the file may be damaged. Choose "
                        "Download again to save a fresh copy.");
                } else {
                    ImGui::TextColored(
                        Color(current_palette_->err),
                        "The model download did not finish. The connection stopped before all "
                        "the file arrived. Check the connection and choose Download to try "
                        "again.");
                }
                if (!download_state.error.detail.empty()) {
                    // The detail is transfer-layer machine context only:
                    // it never carries a transcript or a key by design.
                    ImGui::TextDisabled("More detail: %s", download_state.error.detail.c_str());
                }
            } else if (!download_available) {
                ImGui::TextDisabled(
                    "No download address is listed for this file yet. You can still browse "
                    "to a file on this computer.");
            }
        }
    }
    {
        if (llm_model_path_buffer_[0] == '\0') {
            const std::string current = settings_.GetString("llm_model_path");
            std::snprintf(llm_model_path_buffer_, sizeof(llm_model_path_buffer_), "%s",
                          current.c_str());
        }
        const float model_control_w = 380.0f * scale;
        const float browse_width = ImGui::CalcTextSize("Browse...").x +
                                   ImGui::GetStyle().FramePadding.x * 2.0f;
        lwf::ui::widgets::Row("##row_model_cleanup", "Cleanup model",
                              "Leave empty to turn off cleanup.", model_control_w, scale,
                              *current_palette_);
        ImGui::SetNextItemWidth(model_control_w - browse_width - ImGui::GetStyle().ItemSpacing.x);
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
        lwf::ui::widgets::RowEnd(scale);
        if (!llm_model_error_.empty()) {
            ImGui::TextColored(Color(current_palette_->err), "%s", llm_model_error_.c_str());
        }
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Microphone");
    ImGui::PopFont();
    {
        if (!mic_devices_loaded_) {
            // DECISION: enumeration inits a ma_context, so it runs once per
            // app run on this first pass and is cached; never per frame.
            mic_devices_ = MiniaudioCapture::ListCaptureDevices();
            mic_devices_loaded_ = true;
        }
        // Entry 0 is always "System default" (stored value ""), the cached
        // device names follow in enumeration order.
        const int selected_index =
            SettingsUiModel::MatchMicDeviceIndex(mic_devices_, settings_.GetString("mic_device"));
        const char* preview = selected_index == 0 ? "System default"
                                                  : mic_devices_[selected_index - 1].c_str();
        lwf::ui::widgets::Row("##card_mic_device", "Microphone",
                              "Changes take effect after you restart the app.", 248.0f * scale,
                              scale, *current_palette_);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##mic_pick", preview)) {
            if (ImGui::Selectable("System default##mic_0", selected_index == 0)) {
                settings_.SetString("mic_device", "");
                restart_needed_ = true;
                ApplyAndSave();
            }
            for (std::size_t i = 0; i < mic_devices_.size(); ++i) {
                std::string item = mic_devices_[i] + "##mic_" + std::to_string(i + 1);
                if (ImGui::Selectable(item.c_str(),
                                      selected_index == static_cast<int>(i) + 1)) {
                    settings_.SetString("mic_device", mic_devices_[i]);
                    restart_needed_ = true;
                    ApplyAndSave();
                }
            }
            ImGui::EndCombo();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Microphone level");
    ImGui::PopFont();
    {
        // Read-only meter in a ruled row: no schema key, meter draw kept.
        lwf::ui::widgets::Row(
            "##card_mic_level", "Microphone level",
            "Speak while holding the hotkey. The bar moves only while recording.",
            320.0f * scale, scale, *current_palette_);
        const float level = audio_.AudioLevel();
        const float clamped = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float meter_w = ImGui::GetContentRegionAvail().x;
        const float meter_h = metrics::control_min_height * scale;
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        draw.AddRectFilled(origin, ImVec2(origin.x + meter_w, origin.y + meter_h),
                           Pack(current_palette_->surface), metrics::radius_control * scale);
        if (clamped > 0.0f) {
            draw.AddRectFilled(origin,
                               ImVec2(origin.x + meter_w * clamped, origin.y + meter_h),
                               Pack(current_palette_->text), metrics::radius_control * scale);
        }
        draw.AddRect(origin, ImVec2(origin.x + meter_w, origin.y + meter_h),
                     Pack(current_palette_->border), metrics::radius_control * scale);
        ImGui::Dummy(ImVec2(meter_w, meter_h));
        lwf::ui::widgets::RowEnd(scale);
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Language");
    ImGui::PopFont();
    {
        const std::string current = settings_.GetString("whisper_language");
        lwf::ui::widgets::Row("##card_lang", "Language", nullptr, 248.0f * scale, scale,
                              *current_palette_);
        ImGui::SetNextItemWidth(-1.0f);
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
        lwf::ui::widgets::RowEnd(scale);
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Listening");
    ImGui::PopFont();
    {
        const bool boost = settings_.GetBool("quiet_speech_boost");
        lwf::ui::widgets::Row("##card_listen", "Boost quiet speech",
                              "Lifts a soft voice without clipping", 52.0f * scale, scale,
                              *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##quiet_speech_boost", boost, scale,
                                           *current_palette_)) {
            settings_.SetBool("quiet_speech_boost", !boost);
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_vad_threshold", "Sensitivity", nullptr, 248.0f * scale, scale,
                              *current_palette_);
        DoubleRow("vad_threshold", "##vad_threshold", 0.0f, 1.0f, "sensitivity %.2f");
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_min_hold", "Minimum hold", nullptr, 248.0f * scale, scale,
                              *current_palette_);
        IntRow("min_hold_ms", "##min_hold_ms", 50, 5000, "hold at least %d ms");
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_max_recording", "Maximum recording", nullptr, 248.0f * scale,
                              scale, *current_palette_);
        IntRow("max_recording_ms", "##max_recording_ms", 1000, 120000, "record at most %d ms");
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_hands_free_silence", "Hands-free silence", nullptr,
                              248.0f * scale, scale, *current_palette_);
        IntRow("hands_free_silence_ms", "##hands_free_silence_ms", 0, 10000,
               "stop after %d ms silence");
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_double_tap_window", "Double-tap window", nullptr,
                              248.0f * scale, scale, *current_palette_);
        IntRow("double_tap_window_ms", "##double_tap_window_ms", 100, 2000, "double-tap %d ms");
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        const bool avoid_altgr = settings_.GetBool("avoid_altgr_conflict");
        lwf::ui::widgets::Row("##row_avoid_altgr", "Avoid AltGr conflict",
                              "Skip AltGr chords that conflict with Windows", 52.0f * scale, scale,
                              *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##avoid_altgr_conflict", avoid_altgr, scale,
                                           *current_palette_)) {
            settings_.SetBool("avoid_altgr_conflict", !avoid_altgr);
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Streaming");
    ImGui::PopFont();
    {
        const bool streaming = settings_.GetBool("streaming_stt");
        lwf::ui::widgets::Row("##card_streaming", "Stream speech", "Transcribe while you talk",
                              52.0f * scale, scale, *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##streaming_stt", streaming, scale,
                                           *current_palette_)) {
            settings_.SetBool("streaming_stt", !streaming);
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_streaming_min_segment", "Segment size", nullptr,
                              248.0f * scale, scale, *current_palette_);
        IntRow("streaming_min_segment_ms", "##streaming_min_segment_ms", 1000, 30000,
               "segment %d ms");
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_streaming_pause", "Pause", nullptr, 248.0f * scale, scale,
                              *current_palette_);
        IntRow("streaming_pause_ms", "##streaming_pause_ms", 300, 5000, "pause %d ms");
        lwf::ui::widgets::RowEnd(scale);
    }
}

void MainWindow::DrawCloudPage(float scale) {
    DrawPageChrome("Cloud", "Where your speech is transcribed.", scale);
    DrawRestartBanner(scale);
    // T6 ruled rows (DESIGN §6): same migration note as DrawGeneralPage;
    // first rows reuse the former BeginCard ids. INVARIANT (from the T5
    // review): slider/combo slots are 248 design px wide, not 240 — a
    // source-scan pin asserts the 248 value; keep it.
    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Service");
    ImGui::PopFont();
    {
        static const char* kProviderItems[] = {"local", "gnani", "elevenlabs", "muse"};
        const std::string provider = settings_.GetString("stt_provider");
        int provider_index = provider == "gnani" ? 1
                             : provider == "elevenlabs" ? 2
                             : provider == "muse" ? 3 : 0;
        lwf::ui::widgets::Row("##card_cloud_provider", "Service",
                              "Dictation runs on this machine by default. A cloud service sends "
                              "recordings to its own servers for transcription.",
                              248.0f * scale, scale, *current_palette_);
        ImGui::SetNextItemWidth(-1.0f);
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
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_stt_language", "Cloud language",
                              "ISO language, or a comma list (e.g. hi,en).", 248.0f * scale, scale,
                              *current_palette_);
        StringFieldRow("stt_language", stt_language_field_, "##stt_language", false);
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_stt_timeout", "Wait time", nullptr, 248.0f * scale, scale,
                              *current_palette_);
        IntRow("stt_timeout_ms", "##stt_timeout_ms", 1000, 60000, "wait at most %d ms");
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_stt_max_clips", "Maximum clips", nullptr, 248.0f * scale,
                              scale, *current_palette_);
        IntRow("stt_max_clips", "##stt_max_clips", 1, 200, "at most %d clips");
        lwf::ui::widgets::RowEnd(scale);
    }
    // IntRow writes validation_error_ for these two rows; Cloud drew none
    // before T6. Drawn after the row block in the live palette (T5 pattern).
    if (!validation_error_.empty()) {
        ImGui::TextColored(Color(current_palette_->err), "%s", validation_error_.c_str());
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Saved keys");
    ImGui::PopFont();
    // BLOCK EXCEPTION (DESIGN §6, as-built T6): a ruled Row reserves the
    // 52 px row height, but a key row (status line, masked input, save /
    // remove buttons) is taller than 52 px and holds several controls; it
    // cannot sit inside a Row slot. The section therefore stays an
    // uncarded block on the page background. The former BeginCard id is
    // carried by the PushID below (not a comment) so the page-structure
    // pin keeps passing.
    ImGui::PushID("##card_cloud_keys");
    ImGui::TextWrapped("Saved keys take precedence over environment variables. "
                       "Keys are kept apart from settings and are never shown again "
                       "after saving.");
    DrawApiKeyRow("gnani", "Gnani", ApiKeyEnvFor("gnani"), "Remove saved Gnani key?",
                  "Remove Gnani key", gnani_key_buffer_, sizeof(gnani_key_buffer_),
                  gnani_remove_armed_, gnani_keep_focus_pending_, gnani_key_error_, scale);
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));
    DrawApiKeyRow("elevenlabs", "ElevenLabs", ApiKeyEnvFor("elevenlabs"),
                  "Remove saved ElevenLabs key?", "Remove ElevenLabs key",
                  elevenlabs_key_buffer_, sizeof(elevenlabs_key_buffer_),
                  elevenlabs_remove_armed_, elevenlabs_keep_focus_pending_,
                  elevenlabs_key_error_, scale);
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));
    DrawApiKeyRow("muse", "Muse", ApiKeyEnvFor("muse"), "Remove saved Muse key?",
                  "Remove Muse key", muse_key_buffer_, sizeof(muse_key_buffer_),
                  muse_remove_armed_, muse_keep_focus_pending_, muse_key_error_, scale);
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Remote engine");
    ImGui::PopFont();
    {
        const bool remote = settings_.GetBool("use_remote");
        lwf::ui::widgets::Row("##card_remote_engine", "Use remote engine",
                              "Send audio to a local gRPC engine", 52.0f * scale, scale,
                              *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##use_remote", remote, scale, *current_palette_)) {
            settings_.SetBool("use_remote", !remote);
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_remote_endpoint", "Remote address",
                              "Host and port of the remote engine.", 248.0f * scale, scale,
                              *current_palette_);
        StringFieldRow("remote_endpoint", remote_endpoint_field_, "##remote_endpoint", false);
        lwf::ui::widgets::RowEnd(scale);
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Consent");
    ImGui::PopFont();
    {
        const bool consent = settings_.GetBool("stt_consent_accepted");
        lwf::ui::widgets::Row("##card_cloud_consent", "I understand; enable cloud transcription",
                              nullptr, 52.0f * scale, scale, *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##stt_consent_accepted", consent, scale,
                                           *current_palette_)) {
            settings_.SetBool("stt_consent_accepted", !consent);
            restart_needed_ = true;
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
        if (consent && settings_.GetString("stt_provider") != "local") {
            ImGui::TextColored(Color(current_palette_->warn),
                               "Cloud is active only while a key is present.");
            ImGui::TextWrapped("Saved keys take precedence over environment variables. "
                               "The key is never written to config.json, logs, or history. "
                               "If a cloud request fails, that sentence is still dictated "
                               "on this computer.");
        }
    }
}

void MainWindow::DrawCleanupPage(float scale) {
    DrawPageChrome("Cleanup", "How dictation text is polished before it is typed.", scale);
    DrawRestartBanner(scale);
    // T6 ruled rows (DESIGN §6): same migration note as DrawGeneralPage;
    // first rows reuse the former BeginCard ids. Slider slots are 248
    // design px wide, not 240 (pinned; see DrawCloudPage INVARIANT note).
    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Cleanup");
    ImGui::PopFont();
    {
        const bool enabled = settings_.GetBool("cleanup_enabled");
        lwf::ui::widgets::Row("##card_cleanup", "Enable cleanup", nullptr, 52.0f * scale, scale,
                              *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##cleanup_enabled", enabled, scale,
                                           *current_palette_)) {
            settings_.SetBool("cleanup_enabled", !enabled);
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_cleanup_min_words", "Minimum words", nullptr,
                              248.0f * scale, scale, *current_palette_);
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
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_cleanup_timeout", "Wait time", nullptr, 248.0f * scale, scale,
                              *current_palette_);
        int timeout = settings_.GetInt("cleanup_timeout_ms");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderInt("##timeout", &timeout, 200, 30000, "give up after %d ms");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            settings_.SetInt("cleanup_timeout_ms", timeout);
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        const bool aggressive = settings_.GetBool("spoken_commands_aggressive");
        lwf::ui::widgets::Row("##row_spoken_commands", "Aggressive spoken commands",
                              "Rewrite commands even inside longer sentences", 52.0f * scale,
                              scale, *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##spoken_commands_aggressive", aggressive, scale,
                                           *current_palette_)) {
            settings_.SetBool("spoken_commands_aggressive", !aggressive);
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        const bool fuzzy = settings_.GetBool("dictionary_fuzzy");
        lwf::ui::widgets::Row("##row_dictionary_fuzzy", "Fuzzy dictionary matching",
                              "Correct near-misses", 52.0f * scale, scale, *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##dictionary_fuzzy", fuzzy, scale,
                                           *current_palette_)) {
            settings_.SetBool("dictionary_fuzzy", !fuzzy);
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_command_timeout", "Command wait", nullptr, 248.0f * scale,
                              scale, *current_palette_);
        IntRow("command_timeout_ms", "##command_timeout_ms", 500, 60000, "command %d ms");
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_inject_delay", "Wait before typing", nullptr,
                              248.0f * scale, scale, *current_palette_);
        IntRow("inject_delay_ms", "##inject_delay_ms", 0, 500, "wait %d ms before typing");
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_cleanup_language_policy", "Language policy",
                              "Language policy for cleanup (e.g. keep).", 248.0f * scale, scale,
                              *current_palette_);
        StringFieldRow("cleanup_language_policy", cleanup_language_policy_field_,
                       "##cleanup_language_policy", false);
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_snippet_mode", "Snippet expansion", "When snippets expand.",
                              248.0f * scale, scale, *current_palette_);
        static const char* kSnippetItems[] = {"whole", "contains"};
        const std::string mode = settings_.GetString("snippet_mode");
        int mode_index = mode == "contains" ? 1 : 0;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##snippet_mode", kSnippetItems[mode_index])) {
            for (int item = 0; item < 2; ++item) {
                if (ImGui::Selectable(kSnippetItems[item], item == mode_index)) {
                    settings_.SetString("snippet_mode", kSnippetItems[item]);
                    ApplyAndSave();
                }
            }
            ImGui::EndCombo();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    if (!validation_error_.empty()) {
        ImGui::TextColored(Color(current_palette_->err), "%s", validation_error_.c_str());
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Cleanup backend");
    ImGui::PopFont();

    {
        static const char* kLlmItems[] = {"local", "openrouter"};
        const std::string backend = settings_.GetString("llm_provider");
        int backend_index = backend == "openrouter" ? 1 : 0;
        lwf::ui::widgets::Row("##card_cleanup_backend", "Cleanup backend", nullptr,
                              248.0f * scale, scale, *current_palette_);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##llm_provider", kLlmItems[backend_index])) {
            for (int item = 0; item < 2; ++item) {
                if (ImGui::Selectable(kLlmItems[item], item == backend_index)) {
                    settings_.SetString("llm_provider", kLlmItems[item]);
                    restart_needed_ = true;
                    ApplyAndSave();
                }
            }
            ImGui::EndCombo();
        }
        lwf::ui::widgets::RowEnd(scale);
        if (settings_.GetString("llm_provider") == "openrouter") {
            ImGui::TextColored(Color(current_palette_->warn),
                               "Cleanup uses OpenRouter: text is sent to OpenRouter when cleanup is on.");
        } else {
            ImGui::TextDisabled("Local cleanup runs on this machine.");
        }
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));
    // BLOCK EXCEPTION (DESIGN §6, as-built T6 — see DrawCloudPage note):
    // the key row and the model picker are each taller than the 52 px
    // ruled row and hold several controls, so both stay uncarded blocks
    // on the page background. Picker invariants below are unchanged:
    // lazy entry build only while the popup is open (##openrouter_model),
    // SetString -> restart -> ApplyAndSave per select, Refresh as the only
    // network trigger, manual fallback seed-once, and the literal
    // "OPENROUTER_API_KEY" (generic ResolveApiKey drops env support).
    ImGui::TextWrapped("Saved keys take precedence over environment variables. "
                       "Keys are kept apart from settings and are never shown again "
                       "after saving.");
    DrawApiKeyRow("openrouter", "OpenRouter", "OPENROUTER_API_KEY",
                  "Remove saved OpenRouter key?", "Remove OpenRouter key",
                  openrouter_key_buffer_, sizeof(openrouter_key_buffer_),
                  openrouter_remove_armed_, openrouter_keep_focus_pending_,
                  openrouter_key_error_, scale);
        // DECISION: the models fetch runs on a worker std::thread that writes
        // a mutex-guarded result latch, consumed each Cleanup-card frame here.
        // The 8s GET therefore never blocks either window; Refresh is the only
        // network trigger (no fetch on startup or page open, offline-first).
        // DECISION: the single injected transport is reused for every Refresh;
        // no client is created here.
        // DECISION: selecting the "No model set" placeholder clears the model
        // (sets openrouter_model to ""). That placeholder is the only entry
        // when nothing is set and no list is loaded.
        ImGui::TextUnformatted("Cleanup model");
        {
            std::lock_guard<std::mutex> latch_lock(openrouter_models_latch_mutex_);
            if (openrouter_models_latch_ready_) {
                // SECURITY: the fetch error text is already redacted + capped
                // by the fetch layer; it carries no key and no transcript,
                // and the "%s" format below never re-interprets it.
                if (openrouter_models_latch_.ok && !openrouter_models_latch_.empty) {
                    openrouter_models_ = openrouter_models_latch_.ids;
                    openrouter_models_error_.clear();
                } else if (openrouter_models_latch_.ok) {
                    openrouter_models_.clear();
                    openrouter_models_error_ = "No models returned.";
                } else {
                    openrouter_models_error_ = "Could not load the model list.";
                    if (!openrouter_models_latch_.error.empty()) {
                        openrouter_models_error_ += " ";
                        openrouter_models_error_ += openrouter_models_latch_.error;
                    }
                }
                openrouter_models_latch_ready_ = false;
                openrouter_models_busy_.store(false);
            }
        }
        if (openrouter_models_busy_.load()) {
            ImGui::TextDisabled("Loading models...");
        } else if (!openrouter_models_error_.empty()) {
            ImGui::TextDisabled("%s", openrouter_models_error_.c_str());
        } else if (!openrouter_models_.empty()) {
            ImGui::TextDisabled("%d models available.",
                                static_cast<int>(openrouter_models_.size()));
        } else {
            ImGui::TextDisabled("Model list not loaded.");
        }
        {
            const std::string current_model = settings_.GetString("openrouter_model");
            const int model_match =
                SettingsUiModel::MatchModelIndex(openrouter_models_, current_model);
            // The synthetic entry (index 0, added only when the stored value
            // is non-empty and not in the fetched list) keeps the active
            // model visible before Task A's sorted list.
            const bool synthetic_present = !current_model.empty() && model_match == -1;
            const char* model_preview =
                current_model.empty() ? "No model set" : current_model.c_str();
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##openrouter_model", model_preview)) {
                // DECISION: build the entry list lazily, only when the popup
                // is open, so the closed-combo frame does not copy every
                // fetched id per frame. INVARIANT: entry set/order and ids
                // unchanged — synthetic current-value entry first when
                // present, "No model set" placeholder when no fetched list
                // and no current value, then every fetched id in order with
                // per-item "##or_<index>" ids; selection follows the same
                // synthetic/model_match/empty rules as before.
                struct ModelEntry {
                    std::string value;
                    std::string label;
                };
                std::vector<ModelEntry> model_entries;
                if (synthetic_present) {
                    model_entries.push_back({current_model, current_model});
                } else if (openrouter_models_.empty() && current_model.empty()) {
                    model_entries.push_back({"", "No model set"});
                }
                for (const std::string& model_id : openrouter_models_) {
                    model_entries.push_back({model_id, model_id});
                }
                int selected_entry = -1;
                if (synthetic_present) {
                    selected_entry = 0;
                } else if (model_match >= 0) {
                    selected_entry = model_match;
                } else if (openrouter_models_.empty()) {
                    selected_entry = 0;
                }
                for (std::size_t entry_index = 0; entry_index < model_entries.size();
                     ++entry_index) {
                    const std::string item =
                        model_entries[entry_index].label + "##or_" +
                        std::to_string(entry_index);
                    if (ImGui::Selectable(item.c_str(),
                                          static_cast<int>(entry_index) == selected_entry)) {
                        settings_.SetString("openrouter_model",
                                             model_entries[entry_index].value);
                        restart_needed_ = true;
                        ApplyAndSave();
                        std::snprintf(openrouter_model_field_.buffer,
                                      sizeof(openrouter_model_field_.buffer), "%s",
                                      model_entries[entry_index].value.c_str());
                    }
                }
                ImGui::EndCombo();
            }
        }
        {
            const bool models_busy = openrouter_models_busy_.load();
            ImGui::BeginDisabled(models_busy);
            const bool refresh_pressed = ImGui::Button(models_busy ? "Loading models..."
                                                                    : "Refresh model list");
            ImGui::EndDisabled();
            if (refresh_pressed) {
                if (openrouter_models_busy_.load()) {
                    // Busy guard: re-Refresh while busy is a no-op.
                } else if (models_transport_ == nullptr || !models_fetcher_) {
                    openrouter_models_error_ = "Model list is not available.";
                } else {
                    // SECURITY: the key is copied for the worker, never shown.
                    const std::string resolved_key = ResolveOpenRouterKey();
                    openrouter_models_error_.clear();
                    openrouter_models_busy_.store(true);
                    IHttpTransport* transport = models_transport_;
                    ModelListFetcher fetcher = models_fetcher_;
                    if (openrouter_models_worker_.joinable()) {
                        openrouter_models_worker_.join();
                    }
                    openrouter_models_worker_ = std::thread([this, transport, fetcher, resolved_key]() {
                        ModelListResult result = fetcher(*transport, resolved_key);
                        std::lock_guard<std::mutex> latch_lock(openrouter_models_latch_mutex_);
                        openrouter_models_latch_ = std::move(result);
                        openrouter_models_latch_ready_ = true;
                    });
                }
            }
        }
        // Manual fallback (escape hatch): the buffer is seeded once from the
        // stored value and synced on each commit; it is never cleared on
        // commit or tab switch (declared, non-secret value; the model-path
        // field lifecycle, not the key-row lifecycle). TrimApiKeyInput is the
        // shared whitespace trim.
        openrouter_model_field_.Seed(settings_.GetString("openrouter_model"));
        ImGui::TextUnformatted("Type a model id");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##or_custom", openrouter_model_field_.buffer,
                         sizeof(openrouter_model_field_.buffer));
        if (ImGui::Button("Use this model")) {
            const std::string trimmed =
                SettingsUiModel::TrimApiKeyInput(openrouter_model_field_.buffer);
            settings_.SetString("openrouter_model", trimmed);
            restart_needed_ = true;
            ApplyAndSave();
            std::snprintf(openrouter_model_field_.buffer, sizeof(openrouter_model_field_.buffer),
                          "%s", trimmed.c_str());
        }
}

void MainWindow::DrawPrivacyPage(float scale) {
    DrawPageChrome("Privacy", "What stays on this computer, and what gets written down.", scale);
    DrawRestartBanner(scale);
    // T7 ruled rows (DESIGN §6): same migration note as DrawGeneralPage;
    // first row reuses the former BeginCard id. The delete block below is
    // taller than the 52 px ruled row (question + two buttons + helper),
    // so it stays an uncarded block (block exception, as in T6); the
    // former BeginCard id is carried by the PushID below so the
    // page-structure pin keeps passing. INVARIANT: DeleteLocalData removes
    // data files first, then clears the stores, and never touches
    // config.json (do not reorder).
    // File-scope note: focus_cancel_pending_ is a function-static rather
    // than a member because T7's file scope is MainWindow.cpp only;
    // set-true-on-arm + consumed-next-frame makes tab-switch staleness
    // unreachable (the flag is only read while armed, and arming re-sets it).
    static bool focus_cancel_pending_ = false;
    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Privacy");
    ImGui::PopFont();
    {
        bool debug_transcripts = settings_.GetBool("debug_log_transcripts");
        lwf::ui::widgets::Row("##card_privacy", "Log transcript text",
                              "Writes dictated text to the local log file", 52.0f * scale, scale,
                              *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##debug_log_transcripts", debug_transcripts, scale,
                                           *current_palette_)) {
            settings_.SetBool("debug_log_transcripts", !debug_transcripts);
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
        if (settings_.GetBool("debug_log_transcripts")) {
            ImGui::TextColored(Color(current_palette_->err),
                               "WARNING: dictated text will be written to the local log.");
        }
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("History");
    ImGui::PopFont();
    {
        const bool enabled = settings_.GetBool("history_enabled");
        lwf::ui::widgets::Row("##row_history_enabled", "Keep history",
                              "Save a local history of dictations", 52.0f * scale, scale,
                              *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##history_enabled", enabled, scale,
                                           *current_palette_)) {
            settings_.SetBool("history_enabled", !enabled);
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        const bool keep_raw = settings_.GetBool("keep_raw_in_history");
        lwf::ui::widgets::Row("##row_keep_raw_in_history", "Keep raw text in history",
                              "Store the text before cleanup", 52.0f * scale, scale,
                              *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##keep_raw_in_history", keep_raw, scale,
                                           *current_palette_)) {
            settings_.SetBool("keep_raw_in_history", !keep_raw);
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_history_max_entries", "History entries", nullptr,
                              248.0f * scale, scale, *current_palette_);
        IntRow("history_max_entries", "##history_max_entries", 10, 100000, "keep %d entries");
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_history_max_age_days", "History age", nullptr,
                              248.0f * scale, scale, *current_palette_);
        IntRow("history_max_age_days", "##history_max_age_days", 1, 3650, "delete after %d days");
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        const bool usage = settings_.GetBool("keep_usage_stats");
        lwf::ui::widgets::Row("##row_keep_usage_stats", "Keep usage statistics", "Used by Insights",
                              52.0f * scale, scale, *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##keep_usage_stats", usage, scale,
                                           *current_palette_)) {
            settings_.SetBool("keep_usage_stats", !usage);
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    // IntRow writes validation_error_ for the two history rows above;
    // Privacy drew none before T7. Drawn after the row block in the live
    // palette (T6-Cloud pattern).
    if (!validation_error_.empty()) {
        ImGui::TextColored(Color(current_palette_->err), "%s", validation_error_.c_str());
    }
    if (settings_.GetString("llm_provider") == "openrouter") {
        ImGui::TextColored(
            Color(current_palette_->warn),
            "Cleanup uses OpenRouter: text is sent to OpenRouter when cleanup is on.");
        const std::string openrouter_model = settings_.GetString("openrouter_model");
        lwf::ui::widgets::Row("##row_openrouter_model", "OpenRouter model", nullptr,
                              248.0f * scale, scale, *current_palette_);
        ImGui::Text("%s", openrouter_model.empty() ? "not set" : openrouter_model.c_str());
        lwf::ui::widgets::RowEnd(scale);
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));
    ImGui::TextWrapped("%s", "No telemetry. By default no network calls except explicit model "
                             "downloads. Cloud speech transcription and OpenRouter cleanup "
                             "are the only opt-in exceptions, both off by default. Each sends "
                             "only what its feature needs, and only while you have it enabled.");
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Local data");
    ImGui::PopFont();
    // BLOCK EXCEPTION (DESIGN §6, as-built T7 — see DrawCloudPage note):
    // the named question, both buttons and the helper are taller than the
    // 52 px ruled row, so the delete block stays uncarded on the page
    // background. Esc + focus-on-Cancel (DrawApiKeyRow precedent).
    ImGui::PushID("##card_delete_data");
    if (!delete_data_armed_) {
        if (ImGui::Button("Delete all local data")) {
            delete_data_armed_ = true;
            focus_cancel_pending_ = true;
        }
        ImGui::TextDisabled("Removes statistics, dictionary, snippets, transforms, notes, history "
                            "and saved API keys. Settings are kept.");
    } else {
        // Esc cancels the named confirmation without touching any data;
        // focus lands on Cancel the first armed frame (api-key precedent).
        // No early return: the PushID above must stay balanced this frame.
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            delete_data_armed_ = false;
            focus_cancel_pending_ = false;
        } else {
            ImGui::TextColored(Color(current_palette_->err),
                               "This cannot be undone. The app keeps running.");
            if (ImGui::Button("Confirm delete")) {
                DeleteLocalData();
            }
            ImGui::SameLine();
            if (focus_cancel_pending_) {
                ImGui::SetKeyboardFocusHere();
                focus_cancel_pending_ = false;
            }
            if (ImGui::Button("Cancel")) {
                delete_data_armed_ = false;
                focus_cancel_pending_ = false;
            }
        }
    }
    ImGui::PopID();
}

void MainWindow::DrawAboutPage(float scale) {
    DrawPageChrome("About", "The short version.", scale);
    DrawRestartBanner(scale);
    // T7 ruled rows (DESIGN §6): same migration note as DrawGeneralPage;
    // first row reuses the former BeginCard id. The whole tab is read-only
    // (version, config version and the stored chords below) — no control
    // here mutates settings. Shortcuts are a NEW read-only table (the plan
    // assumed one existed; none did): stored values via GetString, empty
    // shows "Not set" (the DrawHotkeyChips empty-read precedence). The
    // cancel chord's schema default is "Escape", shown as stored.
    {
        lwf::ui::widgets::Row("##card_about", "Echo flow", nullptr, 248.0f * scale, scale,
                              *current_palette_);
        ImGui::Text("%s", lwf::AppVersion());
        lwf::ui::widgets::RowEnd(scale);
    }
    ImGui::TextWrapped("%s",
                       "Local-first voice dictation. On-device speech recognition and optional "
                       "text cleanup, clipboard injection. Processing stays on this machine "
                       "unless you opt into cloud speech transcription or OpenRouter cleanup.");
    if (settings_.GetString("llm_provider") == "openrouter") {
        ImGui::TextColored(
            Color(current_palette_->warn),
            "Cleanup uses OpenRouter: text is sent to OpenRouter when cleanup is on.");
    }
    {
        lwf::ui::widgets::Row("##row_config_version", "Config version", nullptr,
                              248.0f * scale, scale, *current_palette_);
        ImGui::Text("%d", settings_.GetInt("schema_version"));
        lwf::ui::widgets::RowEnd(scale);
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Shortcuts");
    ImGui::PopFont();
    {
        struct ShortcutRow {
            const char* key;
            const char* label;
        };
        static const ShortcutRow kShortcuts[] = {
            {"hotkey_ptt", "Push-to-talk"},
            {"hotkey_handsfree", "Hands-free"},
            {"hotkey_cancel", "Cancel"},
            {"hotkey_command", "Command mode"},
            {"hotkey_paste_last", "Paste last"},
        };
        for (const ShortcutRow& shortcut : kShortcuts) {
            const std::string value = settings_.GetString(shortcut.key);
            const std::string row_id = std::string("##row_hotkey_") + shortcut.key;
            lwf::ui::widgets::Row(row_id.c_str(), shortcut.label, nullptr, 248.0f * scale, scale,
                                  *current_palette_);
            ImGui::Text("%s", value.empty() ? "Not set" : value.c_str());
            lwf::ui::widgets::RowEnd(scale);
        }
    }
}

void MainWindow::DrawInsightsPage(float scale) {
    if (insights_page_) {
        insights_page_->Draw(scale, *current_palette_);
        return;
    }
    DrawPageChrome("Insights", "Your dictation at a glance.", scale);
    ImGui::TextDisabled("Not available yet.");
}

void MainWindow::DrawDictionaryPage(float scale) {
    if (dictionary_page_) {
        dictionary_page_->Draw(scale, *current_palette_);
        return;
    }
    DrawPageChrome("Dictionary", "Words the app corrects.", scale);
    ImGui::TextDisabled("Not available yet.");
}

void MainWindow::DrawHistoryPage(float scale) {
    if (history_page_) {
        history_page_->Draw(scale, *current_palette_);
        return;
    }
    DrawPageChrome("History", "What you dictated, kept on this computer.", scale);
    ImGui::TextDisabled("Not available yet.");
}

void MainWindow::DrawProfilesPage(float scale) {
    if (profiles_page_) {
        profiles_page_->Draw(scale, *current_palette_);
        return;
    }
    DrawPageChrome("Profiles", "How dictation adapts to each app.", scale);
    ImGui::TextDisabled("Not available yet.");
}

void MainWindow::DrawSnippetsPage(float scale) {
    if (snippets_page_) {
        snippets_page_->Draw(scale, *current_palette_);
        return;
    }
    DrawPageChrome("Snippets", "Reusable phrases.", scale);
    ImGui::TextDisabled("Not available yet.");
}

void MainWindow::DrawTransformsPage(float scale) {
    if (transforms_page_) {
        transforms_page_->Draw(scale, *current_palette_);
        return;
    }
    DrawPageChrome("Transforms", "Rewrite selected text.", scale);
    ImGui::TextDisabled("Not available yet.");
}

void MainWindow::DrawScratchpadPage(float scale) {
    if (scratchpad_page_) {
        scratchpad_page_->Draw(scale, *current_palette_);
        return;
    }
    DrawPageChrome("Scratchpad", "A scratch space for notes.", scale);
    ImGui::TextDisabled("Not available yet.");
}

void MainWindow::DrawGeneralPage(float scale) {
    DrawPageChrome("General", "Everyday controls in one place.", scale);
    DrawRestartBanner(scale);
    // T5 ruled rows (DESIGN §6): each section keeps its heading, rows are
    // lwf::ui::widgets::Row with *current_palette_ (never the Paper
    // default). Section ids below reuse the former BeginCard ids on the
    // section's first row so structural pins survive the migration.
    // Headings indent to the row-label inset so wide glyphs never clip.
    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Hotkey");
    ImGui::PopFont();
    DrawGeneralHotkeyRow("hotkey_ptt", "Push-to-talk", "Hold to speak, release to type.",
                         scale);
    DrawGeneralHotkeyRow("hotkey_handsfree", "Hands-free",
                         "Double-tap to talk without holding.", scale);
    DrawGeneralHotkeyRow("hotkey_cancel", "Cancel", "Stop dictation immediately.", scale);
    DrawGeneralHotkeyRow("hotkey_command", "Command mode", "Hold to dictate a command.",
                         scale);
    DrawGeneralHotkeyRow("hotkey_paste_last", "Paste last",
                         "Paste your last dictation.", scale);
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Sound");
    ImGui::PopFont();
    {
        bool sound = settings_.GetBool("sound_enabled");
        lwf::ui::widgets::Row("##card_general_sound", "Sound",
                              "Play short sounds while dictating", 52.0f * scale, scale,
                              *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##sound_enabled", sound, scale, *current_palette_)) {
            settings_.SetBool("sound_enabled", !sound);
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_sound_volume", "Volume", nullptr, 248.0f * scale, scale,
                              *current_palette_);
        VolumeRow();
        lwf::ui::widgets::RowEnd(scale);
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Motion");
    ImGui::PopFont();
    {
        bool reduced = settings_.GetBool("reduce_motion");
        lwf::ui::widgets::Row("##card_general_motion", "Reduce motion", "Stop animations",
                              52.0f * scale, scale, *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##reduce_motion", reduced, scale, *current_palette_)) {
            settings_.SetBool("reduce_motion", !reduced);
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Window");
    ImGui::PopFont();
    {
        // Theme switcher (OQ-A1): display is Graphite/Paper but the write
        // must be exactly kThemeGraphite/kThemePaper (PaletteForTheme is
        // exact-match). hot_apply=true, so no restart flag; ApplyAndSave
        // posts ApplySettings and main.cpp swaps all three window styles.
        const std::string theme = settings_.GetString("theme");
        const bool paper = theme == lwf::ui::kThemePaper;
        lwf::ui::widgets::Row("##card_general_window", "Theme", "Applies immediately.",
                              248.0f * scale, scale, *current_palette_);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##theme", paper ? "Paper" : "Graphite")) {
            if (ImGui::Selectable("Graphite", !paper)) {
                settings_.SetString("theme", lwf::ui::kThemeGraphite);
                ApplyAndSave();
            }
            if (ImGui::Selectable("Paper", paper)) {
                settings_.SetString("theme", lwf::ui::kThemePaper);
                ApplyAndSave();
            }
            ImGui::EndCombo();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        const bool flow_bar = settings_.GetBool("scratchpad_flow_bar");
        lwf::ui::widgets::Row("##row_scratchpad_flow_bar", "Scratchpad bar",
                              "Show the scratchpad bar on the mini window", 52.0f * scale, scale,
                              *current_palette_);
        if (lwf::ui::widgets::ToggleSwitch("##scratchpad_flow_bar", flow_bar, scale,
                                           *current_palette_)) {
            settings_.SetBool("scratchpad_flow_bar", !flow_bar);
            ApplyAndSave();
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        const int mini_x = settings_.GetInt("mini_x");
        const int mini_y = settings_.GetInt("mini_y");
        lwf::ui::widgets::Row("##row_mini_position", "Mini window position",
                              "Set by dragging the mini window.", 300.0f * scale, scale,
                              *current_palette_);
        if (mini_x < 0 && mini_y < 0) {
            ImGui::TextDisabled("Default (bottom center)");
        } else {
            ImGui::Text("%d, %d", mini_x, mini_y);
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Other hotkeys");
    ImGui::PopFont();
    {
        lwf::ui::widgets::Row("##card_general_other_hotkeys", "Hands-free", nullptr,
                              248.0f * scale, scale, *current_palette_);
        StringFieldRow("hotkey_handsfree", hotkey_handsfree_field_, "##hotkey_handsfree", false);
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_other_hotkey_cancel", "Cancel", nullptr, 248.0f * scale,
                              scale, *current_palette_);
        StringFieldRow("hotkey_cancel", hotkey_cancel_field_, "##hotkey_cancel", false);
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_other_hotkey_command", "Command mode", nullptr,
                              248.0f * scale, scale, *current_palette_);
        StringFieldRow("hotkey_command", hotkey_command_field_, "##hotkey_command", false);
        lwf::ui::widgets::RowEnd(scale);
    }
    {
        lwf::ui::widgets::Row("##row_other_hotkey_paste_last", "Paste last", nullptr,
                              248.0f * scale, scale, *current_palette_);
        StringFieldRow("hotkey_paste_last", hotkey_paste_last_field_, "##hotkey_paste_last",
                       false);
        lwf::ui::widgets::RowEnd(scale);
    }
    ImGui::TextDisabled("Chords as text, e.g. Ctrl+Shift+T.");
    if (!validation_error_.empty()) {
        ImGui::TextColored(Color(current_palette_->err), "%s", validation_error_.c_str());
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));


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
    const float tab_pad = metrics::space_m * scale;
    for (std::size_t i = 0; i < 6; ++i) {
        const TabItem& item = tabs[i];
        if (i > 0) {
            ImGui::SameLine(0.0f, metrics::space_s * scale);
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
            ClearApiKeyInputs();
            delete_data_armed_ = false;
        }
        const bool active = settings_tab_ == item.tab;
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemFocused()) {
            draw.AddRect(ImVec2(p.x - 2.0f * scale, p.y - 2.0f * scale),
                         ImVec2(p.x + w + 2.0f * scale, p.y + tab_h + 2.0f * scale),
                         Pack(current_palette_->focus), 8.0f * scale, 0, 2.0f * scale);
        }
        if (active) {
            draw.AddRectFilled(p, ImVec2(p.x + w, p.y + tab_h),
                               Pack(current_palette_->text, 0.10f), 8.0f * scale);
        }
        // Pre-PR review fix: the Settings tab strip reads the live palette —
        // the kLightPalette reads left the active tab at 1.08:1 on Graphite
        // (the runtime default theme).
        const std::uint32_t color =
            (active || hovered) ? current_palette_->text : current_palette_->muted;
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
    // Followups D7: the four-step test wizard is retired. The page is a
    // short welcome (what the app does), the current push-to-talk chord,
    // the speech-model row (this page stays the destination while the
    // model path is empty), and one Get started action. T9 ruled rows
    // (DESIGN §6): the intro, the catalog listing and the download state
    // are taller than the 52 px ruled row, so those sections stay uncarded
    // blocks; the hotkey, model-path and get-started rows are ruled rows
    // with their controls in the slots. The download worker is the one
    // shared with the Speech tab (same latch, consumed in RunOnce), so a
    // transfer started here completes even if the page changes; this page
    // carries its own ids and never reuses them.
    DrawPageChrome("Welcome", "Dictation from this computer, in one hotkey.", scale);
    DrawRestartBanner(scale);

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("How it works");
    ImGui::PopFont();
    // BLOCK EXCEPTION (DESIGN §6, as-built T9): the intro paragraph is
    // taller than the 52 px ruled row, so it stays an uncarded block.
    ImGui::PushID("##card_welcome");
    {
        ImGui::TextWrapped("%s",
                           "Hold the push-to-talk hotkey, speak, and release. Your words are "
                           "typed where your cursor is, and everything runs on this computer.");
    }
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0.0f, metrics::space_s * scale));

    {
        const std::string chord = settings_.GetString("hotkey_ptt");
        const std::string helper =
            chord.empty()
                ? "No chord set yet. Learn one from the Home page."
                : "Current chord: " + chord + ". Change it any time from the Home page.";
        lwf::ui::widgets::Row("##card_welcome_hotkey", "Push-to-talk", helper.c_str(),
                              360.0f * scale, scale, *current_palette_);
        lwf::ui::widgets::RowEnd(scale);
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_s * scale));

    ImGui::SetCursorPosX(16.0f * scale);
    ImGui::PushFont(nullptr, metrics::font_card_title_px);
    ImGui::TextUnformatted("Speech model");
    ImGui::PopFont();

    // The Download control is catalog-driven (T13a): the exe-dir
    // models.json, the first whisper entry (same rule as the Speech
    // tab). While its url is empty the button stays inert (Q2); Browse
    // is never blocked by the catalog.
    const std::string path = GetExeDir() + "\\models.json";
    const Onboarding::ModelCatalog catalog = Onboarding::ParseModelsJson(path);
    const Onboarding::ModelDownload* welcome_entry = nullptr;
    if (!catalog.whisper.empty()) {
        welcome_entry = &catalog.whisper.front();
    }
    const bool download_available = welcome_entry != nullptr && !welcome_entry->url.empty() &&
                                    Onboarding::IsSafeFileName(welcome_entry->name);
    // BLOCK EXCEPTION (DESIGN §6, as-built T9 — same note as DrawCloudPage):
    // the catalog listing (warnings, both model lists) is taller than the
    // 52 px ruled row, so it stays an uncarded block; the path row below
    // is ruled exactly like the Speech model row.
    ImGui::PushID("##card_welcome_models");
    {
        for (const std::string& warning : catalog.warnings) {
            ImGui::TextDisabled("%s", warning.c_str());
        }
        for (const auto& model : catalog.whisper) {
            ImGui::BulletText("%s (%.0f MB)", model.name.c_str(),
                              static_cast<double>(model.size_bytes) / 1048576.0);
            if (model.sha256.empty()) {
                ImGui::SameLine();
                ImGui::TextColored(Color(current_palette_->warn), "unverified file");
            }
        }
        for (const auto& model : catalog.llm) {
            ImGui::BulletText("%s (%.0f MB)", model.name.c_str(),
                              static_cast<double>(model.size_bytes) / 1048576.0);
            if (model.sha256.empty()) {
                ImGui::SameLine();
                ImGui::TextColored(Color(current_palette_->warn), "unverified file");
            }
        }
        ImGui::TextWrapped("%s",
                           "Files are verified against the listed SHA-256 checksums.");
    }
    ImGui::PopID();
    {
        if (model_path_buffer_[0] == '\0') {
            const std::string current = settings_.GetString("whisper_model_path");
            std::snprintf(model_path_buffer_, sizeof(model_path_buffer_), "%s",
                          current.c_str());
        }
        const float model_control_w = 380.0f * scale;
        const float browse_width = ImGui::CalcTextSize("Browse...").x +
                                   ImGui::GetStyle().FramePadding.x * 2.0f;
        const float download_width = ImGui::CalcTextSize("Download").x +
                                     ImGui::GetStyle().FramePadding.x * 2.0f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        lwf::ui::widgets::Row("##card_welcome_model_path", "Model path",
                              "Restart is needed after changing the model path.",
                              model_control_w, scale, *current_palette_);
        ImGui::SetNextItemWidth(model_control_w - browse_width - spacing - download_width -
                                (download_busy_.load() ? spacing + ImGui::CalcTextSize("Cancel").x +
                                                           ImGui::GetStyle().FramePadding.x * 2.0f
                                                       : 0.0f));
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
        ImGui::SameLine();
        ImGui::BeginDisabled(download_busy_.load() || !download_available);
        const bool download_pressed = ImGui::Button("Download##download_welcome");
        ImGui::EndDisabled();
        if (download_pressed && download_available && !download_busy_.load()) {
            // The transfer is synchronous and slow, so it runs on a worker
            // thread with copies of everything it needs; the cancel flag
            // stops it on the next chunk. The UI thread keeps ownership of
            // the state model, and the latch is consumed in RunOnce.
            const std::string download_url = welcome_entry->url;
            const std::string download_sha = welcome_entry->sha256;
            const std::int64_t download_size =
                static_cast<std::int64_t>(welcome_entry->size_bytes);
            const std::filesystem::path download_dir =
                std::filesystem::path(GetDataDir()) / "models";
            const std::string destination = (download_dir / welcome_entry->name).string();
            const std::string stored_path = "models/" + welcome_entry->name;
            std::error_code mkdir_ec;
            std::filesystem::create_directories(download_dir, mkdir_ec);
            // A destination the resume planner already finds complete skips
            // the transfer (and the thread) entirely.
            std::uint64_t existing_bytes = 0;
            std::string actual_hash;
            {
                std::error_code fs_ec;
                if (std::filesystem::exists(destination, fs_ec)) {
                    existing_bytes = static_cast<std::uint64_t>(
                        std::filesystem::file_size(destination, fs_ec));
                    bool hash_ok = false;
                    actual_hash = lwf_sha256::HexDigestOfFile(destination, hash_ok);
                    if (!hash_ok) {
                        actual_hash.clear();
                    }
                }
            }
            const Onboarding::DownloadStartPlan start_plan = Onboarding::PlanDownloadStart(
                existing_bytes, welcome_entry->size_bytes, welcome_entry->sha256, actual_hash);
            download_model_.Start(destination, welcome_entry->size_bytes);
            if (start_plan.skip) {
                download_model_.OnSuccess();
                settings_.SetString("whisper_model_path", stored_path);
                validation_error_.clear();
                restart_needed_ = true;
                ApplyAndSave();
                std::snprintf(model_path_buffer_, sizeof(model_path_buffer_), "%s",
                              stored_path.c_str());
            } else {
                download_stored_path_ = stored_path;
                download_downloaded_.store(0);
                download_cancelled_.store(false);
                download_busy_.store(true);
                if (download_worker_.joinable()) {
                    download_worker_.join();
                }
                download_worker_ = std::thread(
                    [this, download_url, destination, download_sha, download_size]() {
                        Onboarding::WindowsDownloader downloader;
                        Onboarding::DownloadSnapshot result;
                        result.destination = destination;
                        try {
                            const bool ok = downloader.Download(
                                download_url, destination, download_sha, download_size,
                                [this](const Onboarding::DownloadProgress& progress) {
                                    download_downloaded_.store(progress.downloaded);
                                    return !download_cancelled_.load();
                                });
                            if (ok) {
                                result.state = Onboarding::DownloadState::Success;
                            } else {
                                result.state = Onboarding::DownloadState::Failed;
                                result.error =
                                    Onboarding::MapDownloadError(ErrorCode::DownloadFailed,
                                                                "download did not complete");
                            }
                        } catch (const InjectionException& e) {
                            if (download_cancelled_.load() ||
                                Onboarding::IsCancellationDetail(e.what())) {
                                result.state = Onboarding::DownloadState::Cancelled;
                            } else {
                                result.state = Onboarding::DownloadState::Failed;
                                result.error =
                                    Onboarding::MapDownloadError(e.Code(), e.what());
                            }
                        } catch (const std::exception& e) {
                            result.state = Onboarding::DownloadState::Failed;
                            result.error =
                                Onboarding::MapDownloadError(ErrorCode::Unknown, e.what());
                        } catch (...) {
                            result.state = Onboarding::DownloadState::Failed;
                            result.error =
                                Onboarding::MapDownloadError(ErrorCode::Unknown, "");
                        }
                        std::lock_guard<std::mutex> latch_lock(download_latch_mutex_);
                        download_latch_ = result;
                        download_latch_ready_ = true;
                    });
            }
        }
        if (download_busy_.load()) {
            ImGui::SameLine();
            if (ImGui::Button("Cancel##cancel_download_welcome")) {
                download_cancelled_.store(true);
            }
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    if (!validation_error_.empty()) {
        ImGui::TextColored(Color(current_palette_->err), "%s", validation_error_.c_str());
    }
    // Download state row (total=0 is indeterminate: breathing bar,
    // never a stuck 0%). Written in the page's err idiom so the word
    // always carries the state next to any color.
    {
        const Onboarding::DownloadSnapshot download_state = download_model_.Snapshot();
        if (download_state.state == Onboarding::DownloadState::Downloading) {
            const double fraction = download_model_.ProgressFraction();
            if (fraction >= 0.0) {
                char overlay[32];
                std::snprintf(overlay, sizeof(overlay), "%.0f%%", fraction * 100.0);
                ImGui::ProgressBar(static_cast<float>(fraction), ImVec2(-FLT_MIN, 0.0f),
                                   overlay);
            } else {
                ImGui::ProgressBar(0.5f + 0.5f * std::sin(ImGui::GetTime() * 3.0f),
                                   ImVec2(-FLT_MIN, 0.0f), "");
            }
        } else if (download_state.state == Onboarding::DownloadState::Success) {
            ImGui::TextDisabled("Download complete. Restart the app to use the new model.");
        } else if (download_state.state == Onboarding::DownloadState::Cancelled) {
            ImGui::TextDisabled("Download stopped.");
        } else if (download_state.state == Onboarding::DownloadState::Failed) {
            if (download_state.error.code == ErrorCode::ChecksumMismatch) {
                ImGui::TextColored(
                    Color(current_palette_->err),
                    "The saved model file did not pass its check. The saved bytes differ "
                    "from the listed check value, so the file may be damaged. Choose "
                    "Download again to save a fresh copy.");
            } else {
                ImGui::TextColored(
                    Color(current_palette_->err),
                    "The model download did not finish. The connection stopped before all "
                    "the file arrived. Check the connection and choose Download to try "
                    "again.");
            }
            if (!download_state.error.detail.empty()) {
                // The detail is transfer-layer machine context only:
                // it never carries a transcript or a key by design.
                ImGui::TextDisabled("More detail: %s", download_state.error.detail.c_str());
            }
        } else if (!download_available) {
            ImGui::TextDisabled(
                "No download address is listed for this file yet. You can still browse "
                "to a file on this computer.");
        }
    }

    ImGui::Dummy(ImVec2(0.0f, metrics::space_s * scale));
    {
        lwf::ui::widgets::Row("##card_welcome_finish", "Get started",
                              "You can change everything later in Settings.", 248.0f * scale,
                              scale, *current_palette_);
        if (lwf::ui::widgets::PrimaryButton("welcome_get_started", "Get started", scale,
                                            *current_palette_)) {
            settings_.SetBool("first_run", false);
            ApplyAndSave();
            validation_error_.clear();  // page switch: errors stay on their page
            page_ = Page::Home;
            PersistPage();
            page_focus_pending_ = true;
        }
        lwf::ui::widgets::RowEnd(scale);
    }
    ImGui::Dummy(ImVec2(0.0f, metrics::space_m * scale));
}
