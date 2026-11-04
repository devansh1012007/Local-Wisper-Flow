#pragma once

#include "core/OverlayModel.h"

#include "imgui.h"

#include <functional>
#include <string>

#include <GLFW/glfw3.h>

struct ImGuiContext;

// The Mini badge: a resident dot anchored bottom-center above the taskbar.
// Owns its GLFW window AND its Dear ImGui context (one context per OS window,
// docs/UI_AUDIT.md section 1). All state changes arrive on the main loop
// thread; the engine thread never calls into this class.
//
// Visual spec: docs/design/mockup.html section 2 (states, motion, hover).
// The dot is 44 px and always uses the dark palette in this step; the badge
// is click-through except while the cursor is over it, when it can be hovered
// (Ready rises from 55 percent opacity, Attention shows its message, Paused
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

    GLFWwindow* window_ = nullptr;
    ImGuiContext* imgui_context_ = nullptr;
    OverlayModel::PillState state_ = OverlayModel::PillState::Ready;
    std::string message_;
    OverlayModel::WaveformRing waveform_{64};
    OverlayModel::Rect work_area_;
    double dpi_scale_ = 1.0;
    bool reduce_motion_ = false;
    float dwell_remaining_ms_ = 0.0f;
    float fade_alpha_ = 0.0f;
    float animation_time_ = 0.0f;
    bool pass_through_ = true;
    bool hovered_ = false;
    std::function<void()> on_activate_;
};
