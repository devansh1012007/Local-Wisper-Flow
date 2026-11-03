#pragma once

#include "core/OverlayModel.h"

#include <string>

#include <GLFW/glfw3.h>

struct ImFont;

class Overlay {
public:
    Overlay() = default;
    ~Overlay();

    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;

    bool Init();
    void Shutdown();
    void RenderFrame(float delta_seconds);

    void SetState(OverlayModel::PillState state, const std::string& message);
    void PushLevel(float level);
    void SetReduceMotion(bool reduce);
    void SetWorkArea(const OverlayModel::Rect& area, double dpi_scale);
    bool Visible() const;

private:
    void ApplyWindowFlags();
    void DrawPill(float delta_seconds);

    GLFWwindow* window_ = nullptr;
    OverlayModel::PillState state_ = OverlayModel::PillState::Hidden;
    OverlayModel::WaveformRing waveform_{64};
    OverlayModel::Rect work_area_;
    double dpi_scale_ = 1.0;
    bool reduce_motion_ = false;
    float auto_hide_remaining_ms_ = 0.0f;
    float fade_alpha_ = 0.0f;
    std::string message_;
};
