#pragma once

#include "core/OverlayModel.h"

#include "imgui.h"

#include <functional>
#include <string>

#include <GLFW/glfw3.h>

struct ImGuiContext;

// The Mini badge: a quarter-size, highly transparent resident dot with a
// tiny status label. It starts anchored bottom-center above the taskbar and
// can be dragged anywhere with the mouse; the chosen position is persisted
// through the handler set with SetPositionPersistHandler. Owns its GLFW
// window AND its Dear ImGui context (one context per OS window,
// docs/UI_AUDIT.md section 1). All state changes arrive on the main loop
// thread; the engine thread never calls into this class.
//
// Visual spec: docs/design/mockup.html section 2 (states, motion, hover),
// resized per the 2026-11 mini-window feedback: dot 22 px (a quarter of the
// old area), tiny text, strong transparency. The badge is click-through
// except while the cursor is over it, when it can be hovered or dragged
// (Ready rises from idle transparency, Attention shows its message, Paused
// can be clicked to re-enable dictation).
class Overlay {
public:
    Overlay() = default;
    ~Overlay();

    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;

    bool Init();
    void Shutdown();

    // Enters a state. Done and Attention dwell for their model-defined time
    // and then fall back to Ready. Re-entering the current state is a no-op.
    void SetState(OverlayModel::PillState state, const std::string& message);
    void PushLevel(float level);
    void SetReduceMotion(bool reduce);
    void SetWorkArea(const OverlayModel::Rect& area, double dpi_scale);

    // Restores a previously persisted top-left position (call before Init).
    // A negative coordinate means "no saved position": the default anchor
    // bottom-center above the taskbar is used instead.
    void SetSavedPosition(int x, int y);
    // Called after a drag ends, with the new top-left in screen pixels.
    void SetPositionPersistHandler(std::function<void(int, int)> handler);

    // Called when the user clicks the Paused badge (resume dictation).
    void SetActivateHandler(std::function<void()> handler);

    bool Visible() const;
    void RenderFrame(float delta_seconds);

private:
    void ApplyWindowGeometry();
    void DrawBadge();
    void DrawMic(ImDrawList& draw, const ImVec2& center, float scale, ImU32 color,
                 bool filled) const;
    void DrawAttentionMessage(ImDrawList& draw, const ImVec2& dot_min, float scale);
    void UpdateDrag(bool near_dot);
    // GLFW trampoline: stores the new scale; it is applied on the next
    // RenderFrame with this window's ImGui context current.
    static void OnContentScale(GLFWwindow* window, float scale_x, float scale_y);

    GLFWwindow* window_ = nullptr;
    ImGuiContext* imgui_context_ = nullptr;
    OverlayModel::PillState state_ = OverlayModel::PillState::Ready;
    std::string message_;
    OverlayModel::WaveformRing waveform_{64};
    OverlayModel::Rect work_area_;
    double dpi_scale_ = 1.0;
    float pending_dpi_scale_ = 0.0f;  // deferred content-scale callback value
    bool reduce_motion_ = false;
    float dwell_remaining_ms_ = 0.0f;
    float fade_alpha_ = 0.0f;
    float animation_time_ = 0.0f;
    bool pass_through_ = true;
    bool hovered_ = false;

    // Drag-to-move. The press starts on the dot (the only interactive zone);
    // movement above a few pixels turns the press into a drag instead of a
    // click, and the release persists the position.
    bool dragging_ = false;
    int drag_cursor_origin_x_ = 0;
    int drag_cursor_origin_y_ = 0;
    int drag_win_origin_x_ = 0;
    int drag_win_origin_y_ = 0;
    int drag_travel_px_ = 0;

    bool has_saved_position_ = false;
    int saved_x_ = 0;
    int saved_y_ = 0;
    std::function<void(int, int)> on_position_persist_;
    std::function<void()> on_activate_;
};
