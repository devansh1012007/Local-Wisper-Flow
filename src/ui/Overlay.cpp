#define GLFW_EXPOSE_NATIVE_WIN32
#include "ui/Overlay.h"

#include "ui/theme/ThemePalette.h"
#include "ui/theme/ThemeStyle.h"

#include <GLFW/glfw3native.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/gl.h>

#include "os/WindowStyles.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

namespace metrics = lwf::ui::metrics;
// Mini badge (T2b): the ImGui style follows PaletteForTheme with the other
// windows; the badge's custom Pack(current_palette_->...) draws follow the
// selected theme after T15.

using lwf::ui::Pack;

constexpr float kPi = 3.14159265358979323846f;
constexpr int kDragClickThresholdPx = 4;  // below this a press stays a click

float Wave01(float t, float period) {
    return 0.5f + 0.5f * std::sin(2.0f * kPi * t / period);
}

OverlayModel::Rect PrimaryWorkArea() {
    OverlayModel::Rect area;
    WindowStyles::PrimaryWorkArea(area.x, area.y, area.width, area.height);
    if (area.width == 0) {
        area.width = 1920;
        area.height = 1040;
    }
    return area;
}

}  // namespace

Overlay::~Overlay() {
    Shutdown();
}

bool Overlay::Init() {
    if (!glfwInit()) {
        return false;
    }

    // Monitor DPI drives every pixel metric below.
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    GLFWmonitor* const primary = glfwGetPrimaryMonitor();
    if (primary != nullptr) {
        glfwGetMonitorContentScale(primary, &scale_x, &scale_y);
    }
    dpi_scale_ = scale_x > 0.0f ? static_cast<double>(scale_x) : 1.0;

    work_area_ = PrimaryWorkArea();

    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    // Followups D6: the window hugs the icons (dot + status strip). The
    // attention state re-applies the expanded extent through
    // ApplyWindowGeometry; everything outside the icons stays transparent.
    int design_w = 0;
    int design_h = 0;
    ComputeWindowExtent(&design_w, &design_h);
    const int window_w = static_cast<int>(static_cast<float>(design_w) * scale_x);
    const int window_h = static_cast<int>(static_cast<float>(design_h) * scale_y);
    window_ = glfwCreateWindow(window_w, window_h, "lwf-mini", nullptr, nullptr);
    if (window_ == nullptr) {
        return false;
    }

    // This window's own Dear ImGui context. Never share a context between
    // OS windows: the backends keep a single platform handle per context.
    // NOTE: ImGui::CreateContext() restores the previous context after
    // initializing the new one, so GetCurrentContext() would return another
    // window's context here. Use the returned context and make it current.
    imgui_context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context_);
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.IniFilename = nullptr;

    // Tiny status-label font for this window only. SizePixels is the design
    // size; style.FontScaleMain (set below) applies the DPI factor on top:
    // GetFontSize() == SizePixels * FontScaleMain.
    ImFontConfig label_font;
    label_font.SizePixels = metrics::font_mini_text_px;
    io.Fonts->AddFontDefaultBitmap(&label_font);

    // Second 13px bitmap font for the attention message (mockup type scale
    // helper text size: metrics::font_body_px). DrawAttentionMessage pushes
    // this font via PushFont(message_font_, font_body_px) so the message
    // renders at the body size instead of the 8.5px status-label size.
    ImFontConfig message_font_config;
    message_font_config.SizePixels = metrics::font_body_px;
    message_font_ = io.Fonts->AddFontDefaultBitmap(&message_font_config);
    if (message_font_ == nullptr) {
        // Fall back to the default (already added) font; the PushFont scope
        // in DrawAttentionMessage still applies the body design size.
        message_font_ = io.Fonts->Fonts[0];
    }

    // Track monitor scale changes so the badge follows DPI moves live.
    glfwSetWindowUserPointer(window_, this);
    glfwSetWindowContentScaleCallback(window_, &Overlay::OnContentScale);

    ApplyWindowGeometry();

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    // T2b theme selection. This window has no SettingsStore ref, so main.cpp
    // feeds the palette via a pre-Init SetTheme; promote any pending
    // selection (a pre-Init SetTheme queues it) or default to Graphite.
    if (theme_pending_ && pending_theme_palette_ != nullptr) {
        current_palette_ = pending_theme_palette_;
        pending_theme_palette_ = nullptr;
        theme_pending_ = false;
    }
    if (current_palette_ == nullptr) {
        // DECISION: no pre-Init SetTheme (defaults to Graphite). Route the
        // fallback through the canonical selector so the default follows the
        // schema combination kThemeGraphite -> kDarkPalette, never a bare
        // palette literal.
        current_palette_ = &lwf::ui::PaletteForTheme(std::string(lwf::ui::kThemeGraphite));
    }
    lwf::ui::ApplyImGuiStyle(*current_palette_, static_cast<float>(dpi_scale_));

    // Topmost tool window, never activates, click-through until hovered.
    WindowStyles::ApplyOverlayStyles(glfwGetWin32Window(window_));
    pass_through_ = true;
    return true;
}

void Overlay::Shutdown() {
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

void Overlay::SetState(OverlayModel::PillState state, const std::string& message) {
    if (state_ == state && state != OverlayModel::PillState::Attention) {
        return;  // Re-asserting the same state must not restart the dwell.
    }
    state_ = state;
    message_ = message;
    const OverlayModel::Visual visual = OverlayModel::VisualFor(state, message);
    dwell_remaining_ms_ = static_cast<float>(visual.dwell_after_ms);
    // SECURITY: none (no auth path) — but the badge must repaint, and the
    // idle early-out must not swallow a dwell/fade restart.
    dirty_ = true;
    // The window extent depends on the state (Attention expands for the
    // message; leaving Attention shrinks back), so re-apply the geometry.
    ApplyWindowGeometry();
}

void Overlay::PushLevel(float level) {
    waveform_.Push(level);
    dirty_ = true;
}

void Overlay::SetReduceMotion(bool reduce) {
    reduce_motion_ = reduce;
    dirty_ = true;
}

void Overlay::SetTheme(const lwf::ui::Palette& palette) {
    // CRITICAL: never call ApplyImGuiStyle here. The ApplySettings drain in
    // main.cpp may run while another window's ImGui context is current.
    // Queue the palette (same deferred pattern as pending_dpi_scale_); a
    // pre-Init call is promoted by Init, a live call by RenderFrame where
    // this window's context is current. Mirror SetReduceMotion by setting
    // dirty_ so the idle early-out cannot skip the re-style. No-op when the
    // selection is unchanged.
    if (&palette == current_palette_ && !theme_pending_) {
        return;
    }
    if (theme_pending_ && &palette == pending_theme_palette_) {
        return;
    }
    pending_theme_palette_ = &palette;
    theme_pending_ = true;
    dirty_ = true;
}

void Overlay::SetWorkArea(const OverlayModel::Rect& area, double dpi_scale) {
    work_area_ = area;
    dpi_scale_ = dpi_scale;
    pending_dpi_scale_ = 0.0f;
    cached_status_valid_ = false;
    dirty_ = true;
    if (window_ != nullptr) {
        ApplyWindowGeometry();
    }
}

void Overlay::SetSavedPosition(int x, int y) {
    if (x < 0 && y < 0) {
        return;  // sentinel: no saved position, keep the default anchor
    }
    has_saved_position_ = true;
    saved_x_ = x;
    saved_y_ = y;
    dirty_ = true;
}

void Overlay::SetPositionPersistHandler(std::function<void(int, int)> handler) {
    on_position_persist_ = std::move(handler);
}

void Overlay::OnContentScale(GLFWwindow* window, float scale_x, float) {
    if (Overlay* self = static_cast<Overlay*>(glfwGetWindowUserPointer(window))) {
        if (scale_x > 0.0f) {
            self->pending_dpi_scale_ = scale_x;
            self->dirty_ = true;
        }
    }
}

void Overlay::SetActivateHandler(std::function<void()> handler) {
    on_activate_ = std::move(handler);
}

bool Overlay::Visible() const {
    return state_ != OverlayModel::PillState::Hidden;
}

void Overlay::ComputeWindowExtent(int* width, int* height) const {
    // Design pixels; callers scale once by dpi_scale_.
    const bool expanded =
        state_ == OverlayModel::PillState::Attention && !message_.empty();
    if (expanded) {
        // Attention expands to the message reserve under the dot (the box
        // spans the window width; same reserve as the pre-D6 badge).
        *width = static_cast<int>(metrics::mini_message_w);
        *height = static_cast<int>(metrics::mini_ring_pad + metrics::mini_dot +
                                   metrics::mini_message_h);
        return;
    }
    float w = 2.0f * metrics::mini_ring_pad + metrics::mini_dot;
    *width = static_cast<int>(w);
    // One top pad only: the status strip carries the bottom edge, so the
    // hugged height stays tight (ring_pad + dot + status strip).
    *height = static_cast<int>(metrics::mini_ring_pad + metrics::mini_dot +
                               metrics::mini_status_h);
}

void Overlay::ApplyWindowGeometry() {
    if (window_ == nullptr) {
        return;
    }
    int design_w = 0;
    int design_h = 0;
    ComputeWindowExtent(&design_w, &design_h);
    const int window_w = static_cast<int>(static_cast<float>(design_w) * dpi_scale_);
    const int window_h = static_cast<int>(static_cast<float>(design_h) * dpi_scale_);
    OverlayModel::Rect position;
    if (has_saved_position_) {
        position =
            OverlayModel::ClampPosition(work_area_, saved_x_, saved_y_, window_w, window_h);
    } else {
        // Design pixels in, scaled once inside PillPosition (the old call
        // passed pre-scaled pixels and scaled them a second time).
        position = OverlayModel::PillPosition(work_area_, design_w, design_h,
                                              metrics::mini_margin, dpi_scale_);
    }
    glfwSetWindowPos(window_, position.x, position.y);
    glfwSetWindowSize(window_, window_w, window_h);
}

void Overlay::RenderFrame(float delta_seconds) {
    if (window_ == nullptr || imgui_context_ == nullptr) {
        return;
    }

    const OverlayModel::Visual visual = OverlayModel::VisualFor(state_, message_);
    if (!visual.visible) {
        if (glfwGetWindowAttrib(window_, GLFW_VISIBLE)) {
            glfwHideWindow(window_);
        }
        return;
    }
    if (!glfwGetWindowAttrib(window_, GLFW_VISIBLE)) {
        glfwShowWindow(window_);
    }

    if (visual.dwell_after_ms > 0) {
        dwell_remaining_ms_ -= delta_seconds * 1000.0f;
        if (dwell_remaining_ms_ <= 0.0f) {
            SetState(OverlayModel::PillState::Ready, {});
        }
    }

    fade_alpha_ = reduce_motion_
                      ? 1.0f
                      : std::clamp(fade_alpha_ + delta_seconds / metrics::fade_in, 0.0f, 1.0f);
    animation_time_ += delta_seconds;

    // Idle early-out (GUI-A5): Ready/Paused are non-animated once the fade
    // settles, so skip the GL/ImGui pass while dirty_ is clear. Always keep
    // the dwell/fade bookkeeping above (done), then poll hover cheaply at the
    // OS level so mouse entry still wakes the badge. A pending DPI scale or
    // an active drag forces a full frame. Other states keep rendering.
    //
    // The poll watches three verdicts, not one (R-W2-002): the inflated
    // pass-through zone (near_dot, as DrawBadge computes it), the EXACT dot
    // rect (hovered_/badge alpha), and the physical left-button state. The
    // old poll watched only the inflated zone, so moving from the ring onto
    // the dot changed nothing in the verdict: hovered_ stayed false (badge
    // stuck at idle alpha, IsMouseClicked never observed) and a click while
    // the cursor was parked over the dot never woke the frame. The exact-dot
    // poll uses the same center geometry as DrawBadge's dot_min (window
    // width centered, top-anchored); a fractional-DPI ±1px difference from
    // ImGui DisplaySize can cause at most one extra frame per crossing and
    // self-corrects on the next tick, since the verdict is re-polled every
    // frame while idle (cf. R-W2-005).
    const bool fade_settled = reduce_motion_ || fade_alpha_ >= 1.0f;
    // A pending theme (SetTheme) also forces a full frame. SetTheme already
    // sets dirty_, but naming it here keeps the early-out correct even if a
    // future caller queues a theme without setting dirty_.
    if (!dirty_ && fade_settled && !dragging_ && pending_dpi_scale_ <= 0.0f && !theme_pending_ &&
        (state_ == OverlayModel::PillState::Ready ||
         state_ == OverlayModel::PillState::Paused)) {
        int poll_win_x = 0;
        int poll_win_y = 0;
        int poll_win_w = 0;
        glfwGetWindowPos(window_, &poll_win_x, &poll_win_y);
        glfwGetWindowSize(window_, &poll_win_w, nullptr);
        const float poll_scale = static_cast<float>(dpi_scale_);
        const int poll_pad_px = static_cast<int>(metrics::mini_ring_pad * poll_scale);
        const int poll_dot_px = static_cast<int>(metrics::mini_dot * poll_scale);
        // Followups D6: the dot is anchored top-left at ring_pad (it was
        // horizontally centered when the window carried the message
        // reserve at all times); the poll must mirror DrawBadge's geometry.
        const int poll_dot_min_x = poll_pad_px;
        const int poll_dot_min_y = poll_pad_px;
        const int poll_inflate = static_cast<int>(metrics::mini_hover_inflate * poll_scale);
        const bool poll_near = WindowStyles::IsCursorInsideRect(
            poll_win_x + poll_dot_min_x, poll_win_y + poll_dot_min_y,
            poll_dot_px, poll_dot_px, poll_inflate);
        const bool poll_hover_dot = WindowStyles::IsCursorInsideRect(
            poll_win_x + poll_dot_min_x, poll_win_y + poll_dot_min_y,
            poll_dot_px, poll_dot_px, 0);
        const bool poll_button_down = WindowStyles::LeftMouseButtonDown();
        if (poll_near == poll_near_dot_ && poll_hover_dot == poll_hover_dot_ && poll_button_down == poll_button_down_) {
            return;  // Nothing changed; skip the full frame render.
        }
        poll_near_dot_ = poll_near;
        poll_hover_dot_ = poll_hover_dot;
        poll_button_down_ = poll_button_down;
        dirty_ = true;
    }

    ImGui::SetCurrentContext(imgui_context_);
    glfwMakeContextCurrent(window_);

    // A monitor scale change and a live theme swap land here, where this
    // window's ImGui context is current: re-apply the style and re-anchor
    // the geometry. The theme promotes only on an actual palette change
    // (pointer identity); the clear color below stays transparent so the
    // badge remains see-through (INVARIANT) — the Paper/Graphite chrome
    // draws inside DrawBadge convert in T15.
    if (pending_dpi_scale_ > 0.0f || theme_pending_) {
        bool dpi_changed = false;
        if (pending_dpi_scale_ > 0.0f) {
            dpi_scale_ = pending_dpi_scale_;
            pending_dpi_scale_ = 0.0f;
            dpi_changed = true;
        }
        if (theme_pending_) {
            current_palette_ = pending_theme_palette_;
            pending_theme_palette_ = nullptr;
            theme_pending_ = false;
        }
        lwf::ui::ApplyImGuiStyle(*current_palette_, static_cast<float>(dpi_scale_));
        if (dpi_changed) {
            cached_status_valid_ = false;  // font scale changed: remap cache key
            ApplyWindowGeometry();
        }
    }

    int fb_width = 0;
    int fb_height = 0;
    glfwGetFramebufferSize(window_, &fb_width, &fb_height);
    glViewport(0, 0, fb_width, fb_height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Re-assert the GL context before ImGui backend calls, since
    // MainWindow::RunOnce() may have left the main window's context current.
    glfwMakeContextCurrent(window_);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    DrawBadge();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
    dirty_ = false;
}

void Overlay::DrawBadge() {
    const float scale = static_cast<float>(dpi_scale_);
    const float dot = metrics::mini_dot * scale;

    // Full-window ImGui host: no visuals of its own, input only.
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::Begin("##mini", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // The dot sits at the top-left with ring_pad clearance (followups D6:
    // the window hugs the icons, so the dot is no longer horizontally
    // centered); the space below it carries the tiny status label, and the
    // attention expansion carries the hover message.
    const ImVec2 dot_min(metrics::mini_ring_pad * scale, metrics::mini_ring_pad * scale);
    const ImVec2 dot_max(dot_min.x + dot, dot_min.y + dot);
    const ImVec2 center((dot_min.x + dot_max.x) * 0.5f, (dot_min.y + dot_max.y) * 0.5f);
    const float radius = dot * 0.5f;

    // Hit test in window coordinates. The window is click-through unless the
    // cursor is near the dot or a drag is moving it; the OS-level switch
    // happens below.
    int win_x = 0;
    int win_y = 0;
    glfwGetWindowPos(window_, &win_x, &win_y);
    const bool near_dot = WindowStyles::IsCursorInsideRect(
        win_x + static_cast<int>(dot_min.x), win_y + static_cast<int>(dot_min.y),
        static_cast<int>(dot), static_cast<int>(dot), static_cast<int>(metrics::mini_hover_inflate * scale));
    if (dragging_) {
        if (pass_through_) {
            pass_through_ = false;
            WindowStyles::SetOverlayPassThrough(glfwGetWin32Window(window_), false);
        }
    } else if (near_dot != !pass_through_) {
        pass_through_ = !near_dot;
        WindowStyles::SetOverlayPassThrough(glfwGetWin32Window(window_), pass_through_);
    }

    // Input region over the dot. Events only arrive while pass-through is off.
    ImGui::SetCursorScreenPos(dot_min);
    ImGui::InvisibleButton("##dot", ImVec2(dot, dot));
    hovered_ = ImGui::IsItemHovered() || dragging_;
    UpdateDrag(near_dot);

    ImDrawList& draw = *ImGui::GetWindowDrawList();

    // Badge base: surface at the token opacity so it never blocks the
    // desktop; it rises while hovered, dragged or busy.
    float badge_alpha = fade_alpha_;
    if (state_ == OverlayModel::PillState::Ready && !hovered_) {
        badge_alpha *= metrics::mini_idle_alpha;
    } else {
        badge_alpha *= metrics::mini_active_alpha;
    }
    const float breathe = reduce_motion_ ? 0.75f : 0.6f + 0.4f * Wave01(animation_time_, metrics::ring_breathe);

    draw.AddCircleFilled(center, radius, Pack(current_palette_->surface, current_palette_->surface_alpha * badge_alpha), 0);
    draw.AddCircle(center, radius, Pack(current_palette_->border, badge_alpha), 0, 1.0f * scale);

    switch (state_) {
        case OverlayModel::PillState::Ready:
            DrawMic(draw, center, scale, Pack(current_palette_->text, badge_alpha), false);
            break;
        case OverlayModel::PillState::Listening: {
            // Filled brass mic, breathing ring, five level bars.
            // INVARIANT: levels_scratch_ is reused across frames so this
            // path performs zero heap allocations (WaveformRing::SnapshotInto).
            draw.AddCircle(center, radius + 6.0f * scale,
                           Pack(current_palette_->accent, breathe * fade_alpha_), 0, 2.0f * scale);
            DrawMic(draw, center, scale, Pack(current_palette_->accent, fade_alpha_), true);
            waveform_.SnapshotInto(levels_scratch_);
            const std::vector<float>& levels = levels_scratch_;
            float avg = 0.0f;
            for (float level : levels) {
                avg += level;
            }
            avg = levels.empty() ? 0.0f : avg / static_cast<float>(levels.size());
            const float bar_w = 2.0f * scale;
            const float bar_gap = 2.5f * scale;
            const float max_h = 10.0f * scale;
            const float base_y = center.y + 7.0f * scale;
            const float group_w = 5.0f * bar_w + 4.0f * bar_gap;
            float x = center.x + 10.0f * scale - group_w * 0.5f;
            for (int i = 0; i < 5; ++i) {
                float level = avg;
                if (!reduce_motion_ && waveform_.Size() >= static_cast<std::size_t>(5)) {
                    // Oldest-first indexed read (same order as levels[] above);
                    // WaveformRing::At is the bounds-checked hot accessor.
                    level = waveform_.At(waveform_.Size() - 5 + static_cast<std::size_t>(i));
                }
                const float h = std::clamp(2.0f + level * max_h, 2.0f * scale, max_h);
                draw.AddRectFilled(ImVec2(x, base_y - h), ImVec2(x + bar_w, base_y),
                                   Pack(current_palette_->accent, fade_alpha_), bar_w * 0.5f);
                x += bar_w + bar_gap;
            }
            break;
        }
        case OverlayModel::PillState::Thinking: {
            // Three dots fading in sequence, one every 300 ms.
            for (int i = 0; i < 3; ++i) {
                float alpha;
                if (reduce_motion_) {
                    alpha = 0.75f;
                } else {
                    const float phase =
                        std::fmod(animation_time_ - static_cast<float>(i) * metrics::think_dot_offset,
                                  metrics::think_dot_cycle);
                    alpha = phase < 0.0f ? phase + metrics::think_dot_cycle : phase;
                    alpha = 0.25f + 0.75f * (alpha / metrics::think_dot_cycle);
                }
                const ImVec2 p(center.x + (static_cast<float>(i) - 1.0f) * 7.0f * scale, center.y);
                draw.AddCircleFilled(p, 2.2f * scale,
                                     Pack(current_palette_->text, alpha * fade_alpha_), 0);
            }
            break;
        }
        case OverlayModel::PillState::Done: {
            // A check for 600 ms, then back to Ready (dwell handled above).
            const float s = scale;
            draw.AddLine(ImVec2(center.x - 6.0f * s, center.y), ImVec2(center.x - 1.5f * s, center.y + 5.0f * s),
                         Pack(current_palette_->ok, fade_alpha_), 2.0f * s);
            draw.AddLine(ImVec2(center.x - 1.5f * s, center.y + 5.0f * s), ImVec2(center.x + 6.5f * s, center.y - 6.0f * s),
                         Pack(current_palette_->ok, fade_alpha_), 2.0f * s);
            break;
        }
        case OverlayModel::PillState::Paused: {
            // Outline mic with a bar through it; click resumes.
            DrawMic(draw, center, scale, Pack(current_palette_->muted, fade_alpha_), false);
            draw.AddLine(ImVec2(center.x - 8.0f * scale, center.y + 8.0f * scale),
                         ImVec2(center.x + 8.0f * scale, center.y - 8.0f * scale),
                         Pack(current_palette_->muted, fade_alpha_), 1.5f * scale);
            break;
        }
        case OverlayModel::PillState::Attention: {
            draw.AddCircleFilled(center, 5.0f * scale,
                                  Pack(current_palette_->err_fill, fade_alpha_), 0);
            if (hovered_ && !message_.empty()) {
                DrawAttentionMessage(draw, dot_min, scale);
            }
            break;
        }
        case OverlayModel::PillState::Hidden:
            break;
    }

    // Tiny status word under the dot, hidden while the attention message is
    // showing (the message takes the space below the dot instead). Drawn at
    // the badge alpha (GUI-A6 contrast): no extra discount. CalcTextSize is
    // cached in a 1-entry cache keyed by word content (GUI-A5 idle cost);
    // the cache is invalidated whenever the DPI scale re-applies the style.
    if (state_ != OverlayModel::PillState::Attention) {
        const char* status_word = "";
        switch (state_) {
            case OverlayModel::PillState::Ready: status_word = "Ready"; break;
            case OverlayModel::PillState::Listening: status_word = "Listening"; break;
            case OverlayModel::PillState::Thinking: status_word = "Thinking"; break;
            case OverlayModel::PillState::Paused: status_word = "Paused"; break;
            default: break;
        }
        if (status_word[0] != '\0') {
            if (!cached_status_valid_ || cached_status_word_ != status_word) {
                cached_status_size_ = ImGui::CalcTextSize(status_word);
                cached_status_word_ = status_word;
                cached_status_valid_ = true;
            }
            const ImVec2 text_size = cached_status_size_;
            draw.AddText(ImVec2(center.x - text_size.x * 0.5f, dot_max.y + 2.0f * scale),
                         Pack(current_palette_->muted, badge_alpha), status_word);
        }
    }

    ImGui::End();
}

void Overlay::UpdateDrag(bool near_dot) {
    // Start: a press our window actually received while the cursor was on the
    // dot (pass-through is already off there). Any press that moves more than
    // a few pixels becomes a drag instead of a click.
    if (!dragging_) {
        if (near_dot && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            dragging_ = true;
            dirty_ = true;
            drag_travel_px_ = 0;
            WindowStyles::CursorScreenPoint(drag_cursor_origin_x_, drag_cursor_origin_y_);
            glfwGetWindowPos(window_, &drag_win_origin_x_, &drag_win_origin_y_);
        }
        return;
    }

    // Move: track the physical cursor in screen pixels so the badge keeps up
    // even when the cursor outruns the small window.
    int cursor_x = drag_cursor_origin_x_;
    int cursor_y = drag_cursor_origin_y_;
    WindowStyles::CursorScreenPoint(cursor_x, cursor_y);
    drag_travel_px_ = std::max(drag_travel_px_,
                               std::abs(cursor_x - drag_cursor_origin_x_) +
                                   std::abs(cursor_y - drag_cursor_origin_y_));
    if (drag_travel_px_ > kDragClickThresholdPx) {
        int design_w = 0;
        int design_h = 0;
        ComputeWindowExtent(&design_w, &design_h);
        const int window_w = static_cast<int>(static_cast<float>(design_w) * dpi_scale_);
        const int window_h = static_cast<int>(static_cast<float>(design_h) * dpi_scale_);
        const OverlayModel::Rect position = OverlayModel::ClampPosition(
            work_area_, drag_win_origin_x_ + cursor_x - drag_cursor_origin_x_,
            drag_win_origin_y_ + cursor_y - drag_cursor_origin_y_, window_w, window_h);
        glfwSetWindowPos(window_, position.x, position.y);
        dirty_ = true;  // badge window moved: keep rendering while dragging
    }

    // Release: persist a drag, or treat an in-place press on Paused as the
    // resume click. Polled from the OS so the release is never missed when
    // the cursor is outside the window.
    if (!WindowStyles::LeftMouseButtonDown()) {
        if (drag_travel_px_ > kDragClickThresholdPx) {
            if (on_position_persist_) {
                int design_w = 0;
                int design_h = 0;
                ComputeWindowExtent(&design_w, &design_h);
                const int window_w = static_cast<int>(static_cast<float>(design_w) * dpi_scale_);
                const int window_h = static_cast<int>(static_cast<float>(design_h) * dpi_scale_);
                const OverlayModel::Rect position = OverlayModel::ClampPosition(
                    work_area_, drag_win_origin_x_ + cursor_x - drag_cursor_origin_x_,
                    drag_win_origin_y_ + cursor_y - drag_cursor_origin_y_, window_w, window_h);
                on_position_persist_(position.x, position.y);
            }
        } else if (state_ == OverlayModel::PillState::Paused && on_activate_) {
            on_activate_();
        }
        dragging_ = false;
        dirty_ = true;
    }
}

void Overlay::DrawMic(ImDrawList& draw, const ImVec2& center, float scale, ImU32 color,
                      bool filled) const {
    // A microphone drawn from primitives: capsule, cradle arc, stem, base.
    const float w = 4.0f * scale;   // half width of the capsule
    const float h = 7.0f * scale;   // half height of the capsule
    const ImVec2 top_left(center.x - w, center.y - h - 1.0f * scale);
    const ImVec2 bottom_right(center.x + w, center.y + h - 1.0f * scale);
    if (filled) {
        draw.AddRectFilled(top_left, bottom_right, color, w, ImDrawFlags_RoundCornersAll);
    } else {
        draw.AddRect(top_left, bottom_right, color, w, ImDrawFlags_RoundCornersAll, 1.5f * scale);
    }
    const ImVec2 arc_center(center.x, center.y - 1.0f * scale);
    const float arc_r = w + 2.5f * scale;
    draw.PathArcTo(arc_center, arc_r, 0.0f, kPi, 0);
    draw.PathStroke(color, 0, 1.5f * scale);
    draw.AddLine(ImVec2(center.x, arc_center.y + arc_r), ImVec2(center.x, center.y + h + 3.0f * scale),
                 color, 1.5f * scale);
    draw.AddLine(ImVec2(center.x - w, center.y + h + 3.0f * scale),
                 ImVec2(center.x + w, center.y + h + 3.0f * scale), color, 1.5f * scale);
}

void Overlay::DrawAttentionMessage(ImDrawList& draw, const ImVec2& dot_min, float scale) {
    // One calm line under the dot, spanning the window width (the dot is
    // centered in it, so the box is window-wide, never clipped). Renders at
    // the 13px body design size via the second bitmap font (GUI-A6).
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float pad = metrics::space_s * scale;
    const float text_w = display.x - 2.0f * pad;
    const ImVec2 text_pos(pad, dot_min.y + metrics::mini_dot * scale + pad);
    ImGui::PushFont(message_font_, metrics::font_body_px);
    const ImVec2 text_size = ImGui::CalcTextSize(message_.c_str(), nullptr, false, text_w);
    const ImVec2 box_min(0.0f, dot_min.y + metrics::mini_dot * scale);
    const ImVec2 box_max(display.x, box_min.y + text_size.y + 2.0f * pad);
    draw.AddRectFilled(box_min, box_max, Pack(current_palette_->surface, 0.97f * fade_alpha_),
                       metrics::radius_control * scale);
    draw.AddRect(box_min, box_max, Pack(current_palette_->border, fade_alpha_),
                 metrics::radius_control * scale, 0, 1.0f * scale);
    draw.AddText(ImGui::GetFont(), ImGui::GetFontSize(), text_pos,
                 Pack(current_palette_->err, fade_alpha_), message_.c_str(), nullptr, text_w);
    ImGui::PopFont();
}
