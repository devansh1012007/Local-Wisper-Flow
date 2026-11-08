#include "ui/MainWindow.h"

#include "audio/MiniaudioCapture.h"
#include "core/AppVersion.h"
#include "core/ApiKeyStore.h"
#include "core/ConsentGate.h"
#include "core/DictionaryStore.h"
#include "core/HistoryStore.h"
#include "core/HotkeyBinding.h"
#include "core/Languages.h"
#include "core/NotesStore.h"
#include "core/Onboarding.h"
#include "core/PersonalDictionary.h"
#include "core/SettingsStore.h"
#include "core/SettingsUiModel.h"
#include "core/SnippetStore.h"
#include "core/StatsStore.h"
#include "core/TransformStore.h"
#include "core/UiBus.h"
#include "os/DataDir.h"
#include "os/FileDialogs.h"
#include "ui/pages/DictionaryPage.h"
#include "ui/pages/InsightsPage.h"
#include "ui/pages/ScratchpadPage.h"
#include "ui/pages/SnippetsPage.h"
#include "ui/pages/TransformsPage.h"
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
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

// Full window shell is Ivory primary (Phase 4, AC2 single accent): this file
// applies kLightPalette everywhere. The dark variant is test-only + the Mini
// badge in Overlay.cpp, which intentionally stays dark for desktop legibility
// (Ph6 scope; do not switch Overlay here).
namespace metrics = lwf::ui::metrics;
using lwf::ui::Color;
using lwf::ui::kLightPalette;
using lwf::ui::Pack;

// Rail width resolves from the canonical token instead of a second literal,
// kept as kRailWidth so the kRailWidth pin stays while the value flows from
// metrics::rail_width (220).
constexpr float kRailWidth = metrics::rail_width;

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
bool KeyButton(const char* id, const char* label, bool accent, float scale) {
    const float height = metrics::control_min_height * scale;
    const std::string button_label =
        std::string(label == nullptr ? "" : label) + "##" + (id == nullptr ? "" : id);
    bool pressed = false;
    if (accent) {
        ImGui::PushStyleColor(ImGuiCol_Button, Color(kLightPalette.accent));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Color(kLightPalette.accent));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Color(kLightPalette.accent));
        ImGui::PushStyleColor(ImGuiCol_Text, Color(kLightPalette.on_accent));
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
                     Pack(kLightPalette.focus), metrics::radius_control * scale, 0,
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
    lwf::ui::ApplyImGuiStyle(kLightPalette, static_cast<float>(dpi_scale_));

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
        lwf::ui::ApplyImGuiStyle(kLightPalette, static_cast<float>(dpi_scale_));
    }

    int fb_width = 0;
    int fb_height = 0;
    glfwGetFramebufferSize(window_, &fb_width, &fb_height);
    glViewport(0, 0, fb_width, fb_height);
    const ImVec4 clear = Color(kLightPalette.bg);
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
                     Pack(kLightPalette.muted), "Saved");
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
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(kLightPalette.surface));
    ImGui::BeginChild("##rail", size, ImGuiChildFlags_None);
    ImGui::PopStyleColor();
    {
        const ImVec2 rail_pos = ImGui::GetWindowPos();
        const float rail_right = rail_pos.x + size.x;
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        draw.AddLine(ImVec2(rail_right - 1.0f, rail_pos.y),
                     ImVec2(rail_right - 1.0f, rail_pos.y + size.y), Pack(kLightPalette.border));

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
                ClearApiKeyInputs();
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
                             Pack(kLightPalette.focus), 2.0f * scale, 0, 2.0f * scale);
            }
            const std::uint32_t color =
                active ? kLightPalette.text : (hovered ? kLightPalette.text : kLightPalette.muted);
            if (active) {
                draw.AddRectFilled(p, ImVec2(p.x + w, p.y + item_h),
                                   Pack(kLightPalette.text, 0.10f), 8.0f * scale);
            }
            draw.AddText(ImVec2(p.x + 19.0f * scale, p.y + (item_h - font_h) * 0.5f),
                         Pack(color), item.label);
        }
        ImGui::PopFont();

        // Rail bottom: live status and the way back to mini.
        const float line = ImGui::GetTextLineHeightWithSpacing();
        const float bottom_h =
            2.0f * metrics::space_m * scale + 3.0f * line + metrics::space_s * scale +
            metrics::control_min_height * scale;
        if (ImGui::GetContentRegionAvail().y > bottom_h) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
                                 ImGui::GetContentRegionAvail().y - bottom_h);
        }
        ImGui::Separator();
        const State fsm_state = fsm_.CurrentState();
        const bool enabled = settings_.GetBool("dictation_enabled");
        const char* status_word = "Ready";
        std::uint32_t status_color = kLightPalette.ok;
        if (!enabled) {
            status_word = "Paused";
            status_color = kLightPalette.muted;
        } else if (fsm_state == State::CAPTURING) {
            status_word = "Listening";
            status_color = kLightPalette.accent;
        } else if (fsm_state == State::TRANSCRIBING || fsm_state == State::POSTPROCESSING ||
                   fsm_state == State::EMITTING) {
            status_word = "Thinking";
            status_color = kLightPalette.text;
        }
        const ImVec2 status_pos = ImGui::GetCursorScreenPos();
        draw.AddCircleFilled(ImVec2(status_pos.x + 4.0f * scale, status_pos.y + line * 0.5f),
                             4.0f * scale, Pack(status_color));
        ImGui::Dummy(ImVec2(kStatusDotIndent * scale, 0.0f));
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
    const std::uint32_t dot_color =
        source == SettingsUiModel::ApiKeySource::NotSet ? kLightPalette.muted : kLightPalette.ok;
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
        ImGui::TextColored(Color(kLightPalette.err), "%s", confirm_question);
        const std::string remove_id = std::string("##remove_") + store_id;
        bool do_remove = false;
        bool do_keep = false;
        do_remove = KeyButton(remove_id.c_str(), confirm_button, false, scale);
        ImGui::SameLine();
        if (focus_keep_pending) {
            ImGui::SetKeyboardFocusHere();
            focus_keep_pending = false;
        }
        const std::string keep_id = std::string("##keep_") + store_id;
        do_keep = KeyButton(keep_id.c_str(), "Keep key", false, scale);
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
            ImGui::TextColored(Color(kLightPalette.err), "%s", error.c_str());
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
                           Pack(kLightPalette.focus), metrics::radius_control * scale, 0,
                           2.0f * scale);
    }
    ImGui::TextDisabled("Paste a key to save");
    if (!error.empty()) {
        ImGui::TextColored(Color(kLightPalette.err), "%s", error.c_str());
    }
    const std::string save_id = std::string("##save_") + store_id;
    if (KeyButton(save_id.c_str(), "Save key", true, scale)) {
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
        if (KeyButton(remove_id.c_str(), "Remove saved key", false, scale)) {
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
    ImGui::TextColored(Color(kLightPalette.warn), "Applies after restart.");
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
                           Pack(kLightPalette.text, 0.05f), metrics::radius_control * scale);
        draw.AddRect(ImVec2(x, y), ImVec2(x + chip_w, y + chip_h), Pack(kLightPalette.border),
                     metrics::radius_control * scale, 0, 1.0f * scale);
        draw.AddText(ImVec2(x + pad_x, y + (chip_h - text_size.y) * 0.5f),
                     Pack(kLightPalette.text), parts[i].c_str());
        x += chip_w;
        if (i + 1 < parts.size()) {
            draw.AddText(ImVec2(x + 5.0f * scale, y + (chip_h - text_size.y) * 0.5f),
                         Pack(kLightPalette.muted), "+");
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
            ImGui::TextColored(Color(kLightPalette.err), "That shortcut is already in use");
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
    if (learn_error_target_ == config_key && !learn_error_.empty()) {
        ImGui::TextColored(Color(kLightPalette.err), "%s", learn_error_.c_str());
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
        std::uint32_t status_color = kLightPalette.ok;
        const char* status_word = "Ready";
        if (!enabled) {
            status_word = "Paused";
            status_color = kLightPalette.muted;
            rest = "Dictation is off.";
        } else if (fsm_state == State::CAPTURING) {
            status_word = "Listening";
            status_color = kLightPalette.accent;
            rest = "Speak, then release.";
        } else if (fsm_state == State::TRANSCRIBING || fsm_state == State::POSTPROCESSING ||
                   fsm_state == State::EMITTING) {
            status_word = "Thinking";
            status_color = kLightPalette.text;
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
        ImGui::Dummy(ImVec2(kStatusDotIndent * scale, 0.0f));
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
            ImGui::TextColored(Color(kLightPalette.warn),
                               "Cleanup uses OpenRouter: text is sent to OpenRouter when cleanup is on.");
        }
        if (!cloud_active && !openrouter_cleanup) {
            ImGui::TextUnformatted("Everything runs on this computer.");
            ImGui::TextDisabled("Cloud speech and OpenRouter cleanup are off.");
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
            ImGui::TextColored(Color(kLightPalette.err), "%s", validation_error_.c_str());
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
            ImGui::TextColored(Color(kLightPalette.err), "%s", llm_model_error_.c_str());
        }
        ImGui::TextDisabled("Leave empty to turn off cleanup.");
    }
    EndCard(scale);

    BeginCard("##card_mic_device", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Microphone");
        ImGui::PopFont();
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
        ImGui::TextDisabled("Changes take effect after you restart the app.");
    }
    EndCard(scale);

    BeginCard("##card_mic_level", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Microphone level");
        ImGui::PopFont();
        ImGui::TextDisabled("Speak while holding the hotkey. The bar moves only while recording.");
        const float level = audio_.AudioLevel();
        const float clamped = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float meter_w = ImGui::GetContentRegionAvail().x;
        const float meter_h = metrics::control_min_height * scale;
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        draw.AddRectFilled(origin, ImVec2(origin.x + meter_w, origin.y + meter_h),
                           Pack(kLightPalette.surface), metrics::radius_control * scale);
        if (clamped > 0.0f) {
            draw.AddRectFilled(origin,
                               ImVec2(origin.x + meter_w * clamped, origin.y + meter_h),
                               Pack(kLightPalette.text), metrics::radius_control * scale);
        }
        draw.AddRect(origin, ImVec2(origin.x + meter_w, origin.y + meter_h),
                     Pack(kLightPalette.border), metrics::radius_control * scale);
        ImGui::Dummy(ImVec2(meter_w, meter_h));
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
        ImGui::TextUnformatted("Service");
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
        ImGui::TextWrapped("Dictation runs on this machine by default. A cloud service sends "
                           "recordings to its own servers for transcription.");
        ImGui::Separator();
        ImGui::TextUnformatted("Cloud language");
        StringFieldRow("stt_language", stt_language_field_, "##stt_language", false);
        ImGui::TextDisabled("ISO language, or a comma list (e.g. hi,en).");
        IntRow("stt_timeout_ms", "##stt_timeout_ms", 1000, 60000, "wait at most %d ms");
        IntRow("stt_max_clips", "##stt_max_clips", 1, 200, "at most %d clips");
    }
    EndCard(scale);

    BeginCard("##card_cloud_keys", width, scale);
    {
        ImGui::PushFont(nullptr, metrics::font_card_title_px);
        ImGui::TextUnformatted("Saved keys");
        ImGui::PopFont();
        ImGui::TextWrapped("Saved keys take precedence over environment variables. "
                           "Keys are kept apart from settings and are never shown again "
                           "after saving.");
        DrawApiKeyRow("gnani", "Gnani", ApiKeyEnvFor("gnani"), "Remove saved Gnani key?",
                      "Remove Gnani key", gnani_key_buffer_, sizeof(gnani_key_buffer_),
                      gnani_remove_armed_, gnani_keep_focus_pending_, gnani_key_error_, scale);
        ImGui::Separator();
        DrawApiKeyRow("elevenlabs", "ElevenLabs", ApiKeyEnvFor("elevenlabs"),
                      "Remove saved ElevenLabs key?", "Remove ElevenLabs key",
                      elevenlabs_key_buffer_, sizeof(elevenlabs_key_buffer_),
                      elevenlabs_remove_armed_, elevenlabs_keep_focus_pending_,
                      elevenlabs_key_error_, scale);
        ImGui::Separator();
        DrawApiKeyRow("muse", "Muse", ApiKeyEnvFor("muse"), "Remove saved Muse key?",
                      "Remove Muse key", muse_key_buffer_, sizeof(muse_key_buffer_),
                      muse_remove_armed_, muse_keep_focus_pending_, muse_key_error_, scale);
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
            ImGui::TextColored(Color(kLightPalette.warn),
                               "Cloud is active only while a key is present.");
            ImGui::TextWrapped("Saved keys take precedence over environment variables. "
                               "The key is never written to config.json, logs, or history. "
                               "If a cloud request fails, that sentence is still dictated "
                               "on this computer.");
        }
    }
    EndCard(scale);
}

void MainWindow::DrawCleanupPage(float scale) {
    DrawPageChrome("Cleanup", "How dictation text is polished before it is typed.", scale);
    DrawRestartBanner(scale);

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
            ImGui::TextColored(Color(kLightPalette.err), "%s", validation_error_.c_str());
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
                    restart_needed_ = true;
                    ApplyAndSave();
                }
            }
            ImGui::EndCombo();
        }
        if (settings_.GetString("llm_provider") == "openrouter") {
            ImGui::TextColored(Color(kLightPalette.warn),
                               "Cleanup uses OpenRouter: text is sent to OpenRouter when cleanup is on.");
        } else {
            ImGui::TextDisabled("Local cleanup runs on this machine.");
        }
        ImGui::Separator();
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
            const char* model_preview =
                current_model.empty() ? "No model set" : current_model.c_str();
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##openrouter_model", model_preview)) {
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
            ImGui::TextColored(Color(kLightPalette.err),
                               "WARNING: dictated text will be written to the local log.");
        }
        ImGui::Separator();
        BoolRow("history_enabled", "Keep history", "Save a local history of dictations", scale);
        BoolRow("keep_raw_in_history", "Keep raw text in history", "Store the text before cleanup", scale);
        IntRow("history_max_entries", "##history_max_entries", 10, 100000, "keep %d entries");
        IntRow("history_max_age_days", "##history_max_age_days", 1, 3650, "delete after %d days");
        BoolRow("keep_usage_stats", "Keep usage statistics", "Used by Insights", scale);
        if (settings_.GetString("llm_provider") == "openrouter") {
            ImGui::TextColored(Color(kLightPalette.warn),
                               "Cleanup uses OpenRouter: text is sent to OpenRouter when cleanup is on.");
            const std::string openrouter_model = settings_.GetString("openrouter_model");
            ImGui::Text("OpenRouter model: %s",
                        openrouter_model.empty() ? "not set" : openrouter_model.c_str());
        }
        ImGui::Separator();
        ImGui::TextWrapped("%s", "No telemetry. By default no network calls except explicit model "
                                 "downloads. Cloud speech transcription and OpenRouter cleanup "
                                 "are the only opt-in exceptions, both off by default. Each sends "
                                 "only what its feature needs, and only while you have it enabled.");
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
            ImGui::TextDisabled("Removes statistics, dictionary, snippets, transforms, notes, history and saved API keys. Settings are kept.");
        } else {
            ImGui::TextColored(Color(kLightPalette.err),
                               "This cannot be undone. The app keeps running.");
            if (ImGui::Button("Confirm delete")) {
                DeleteLocalData();
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
                           "Local-first voice dictation. On-device speech recognition and optional "
                           "text cleanup, clipboard injection. Processing stays on this machine "
                           "unless you opt into cloud speech transcription or OpenRouter cleanup.");
        if (settings_.GetString("llm_provider") == "openrouter") {
            ImGui::TextColored(Color(kLightPalette.warn),
                               "Cleanup uses OpenRouter: text is sent to OpenRouter when cleanup is on.");
        }
        ImGui::Text("Config version: %d", settings_.GetInt("schema_version"));
    }
    EndCard(scale);
}

void MainWindow::DrawInsightsPage(float scale) {
    if (insights_page_) {
        insights_page_->Draw(scale);
        return;
    }
    DrawPageChrome("Insights", "Your dictation at a glance.", scale);
    ImGui::TextDisabled("Not available yet.");
}

void MainWindow::DrawDictionaryPage(float scale) {
    if (dictionary_page_) {
        dictionary_page_->Draw(scale);
        return;
    }
    DrawPageChrome("Dictionary", "Words the app corrects.", scale);
    ImGui::TextDisabled("Not available yet.");
}

void MainWindow::DrawSnippetsPage(float scale) {
    if (snippets_page_) {
        snippets_page_->Draw(scale);
        return;
    }
    DrawPageChrome("Snippets", "Reusable phrases.", scale);
    ImGui::TextDisabled("Not available yet.");
}

void MainWindow::DrawTransformsPage(float scale) {
    if (transforms_page_) {
        transforms_page_->Draw(scale);
        return;
    }
    DrawPageChrome("Transforms", "Rewrite selected text.", scale);
    ImGui::TextDisabled("Not available yet.");
}

void MainWindow::DrawScratchpadPage(float scale) {
    if (scratchpad_page_) {
        scratchpad_page_->Draw(scale);
        return;
    }
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
        if (!validation_error_.empty()) {
            ImGui::TextColored(Color(kLightPalette.err), "%s", validation_error_.c_str());
        }
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
        }
        const bool active = settings_tab_ == item.tab;
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemFocused()) {
            draw.AddRect(ImVec2(p.x - 2.0f * scale, p.y - 2.0f * scale),
                         ImVec2(p.x + w + 2.0f * scale, p.y + tab_h + 2.0f * scale),
                         Pack(kLightPalette.focus), 8.0f * scale, 0, 2.0f * scale);
        }
        if (active) {
            draw.AddRectFilled(p, ImVec2(p.x + w, p.y + tab_h),
                               Pack(kLightPalette.text, 0.10f), 8.0f * scale);
        }
        const std::uint32_t color =
            active ? kLightPalette.text : (hovered ? kLightPalette.text : kLightPalette.muted);
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
                    ImGui::TextColored(Color(kLightPalette.warn), "unverified file");
                }
            }
            for (const auto& model : catalog.llm) {
                ImGui::BulletText("%s (%.0f MB)", model.name.c_str(),
                                  static_cast<double>(model.size_bytes) / 1048576.0);
                if (model.sha256.empty()) {
                    ImGui::SameLine();
                    ImGui::TextColored(Color(kLightPalette.warn), "unverified file");
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
                ImGui::TextColored(Color(kLightPalette.err), "%s", validation_error_.c_str());
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
