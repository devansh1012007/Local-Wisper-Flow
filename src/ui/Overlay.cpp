#include "ui/Overlay.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/gl.h>

#include <os/WindowStyles.h>

#include <algorithm>

namespace {

constexpr int kPillWidth = 260;
constexpr int kPillHeight = 44;
constexpr int kTaskbarMargin = 16;

OverlayModel::Rect PrimaryWorkArea() {
    OverlayModel::Rect area;
#ifdef _WIN32
    RECT work{};
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
        area.x = work.left;
        area.y = work.top;
        area.width = work.right - work.left;
        area.height = work.bottom - work.top;
    }
#endif
    if (area.width == 0) {
        area.width = 1920;
        area.height = 1040;
    }
    return area;
}

}

Overlay::~Overlay() {
    Shutdown();
}

bool Overlay::Init() {
    if (!glfwInit()) {
        return false;
    }
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_POSITION_X, 0);
    glfwWindowHint(GLFW_POSITION_Y, 0);

    window_ = glfwCreateWindow(kPillWidth, kPillHeight, "lwf-overlay", nullptr, nullptr);
    if (window_ == nullptr) {
        return false;
    }
    WindowStyles::ApplyOverlayStyles(glfwGetWin32Window(window_));

    work_area_ = PrimaryWorkArea();
    const OverlayModel::Rect position =
        OverlayModel::PillPosition(work_area_, kPillWidth, kPillHeight, kTaskbarMargin, dpi_scale_);
    glfwSetWindowPos(window_, position.x, position.y);
    glfwSetWindowSize(window_, position.width, position.height);

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    ImGui_ImplGlfw_InitForOpenGL(window_, false);
    ImGui_ImplOpenGL3_Init("#version 130");
    return true;
}

void Overlay::Shutdown() {
    if (window_ == nullptr) {
        return;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    glfwDestroyWindow(window_);
    window_ = nullptr;
}

void Overlay::SetState(OverlayModel::PillState state, const std::string& message) {
    state_ = state;
    message_ = message;
    const OverlayModel::Visual visual = OverlayModel::VisualFor(state, message);
    auto_hide_remaining_ms_ = static_cast<float>(visual.auto_hide_after_ms);
    if (window_ != nullptr && visual.visible) {
        glfwShowWindow(window_);
    }
}

void Overlay::PushLevel(float level) {
    waveform_.Push(level);
}

void Overlay::SetReduceMotion(bool reduce) {
    reduce_motion_ = reduce;
}

void Overlay::SetWorkArea(const OverlayModel::Rect& area, double dpi_scale) {
    work_area_ = area;
    dpi_scale_ = dpi_scale;
}

bool Overlay::Visible() const {
    return state_ != OverlayModel::PillState::Hidden;
}

void Overlay::RenderFrame(float delta_seconds) {
    if (window_ == nullptr) {
        return;
    }
    glfwMakeContextCurrent(window_);
    const OverlayModel::Visual visual = OverlayModel::VisualFor(state_, message_);
    if (!visual.visible) {
        if (glfwGetWindowAttrib(window_, GLFW_VISIBLE)) {
            glfwHideWindow(window_);
        }
        return;
    }
    if (visual.auto_hide_after_ms > 0) {
        auto_hide_remaining_ms_ -= delta_seconds * 1000.0f;
        if (auto_hide_remaining_ms_ <= 0.0f) {
            SetState(OverlayModel::PillState::Hidden, {});
            glfwHideWindow(window_);
            return;
        }
    }

    const float target_alpha = reduce_motion_ ? 1.0f
                                              : std::clamp(fade_alpha_ + delta_seconds / 0.12f,
                                                           0.0f, 1.0f);
    fade_alpha_ = target_alpha;

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    DrawPill(delta_seconds);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
}

void Overlay::DrawPill(float delta_seconds) {
    (void)delta_seconds;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 22.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 0.92f * fade_alpha_));
    ImGui::Begin("##pill", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);
    switch (state_) {
        case OverlayModel::PillState::Listening: {
            const std::vector<float> waveform = waveform_.Snapshot();
            if (!waveform.empty()) {
                ImGui::PlotLines("##wave", waveform.data(), static_cast<int>(waveform.size()), 0,
                                 nullptr, 0.0f, 1.0f, ImVec2(-1.0f, 24.0f));
            }
            break;
        }
        case OverlayModel::PillState::Processing:
            ImGui::Text("transcribing...");
            break;
        case OverlayModel::PillState::Error:
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", message_.c_str());
            break;
        case OverlayModel::PillState::Cancelled:
            ImGui::Text("cancelled");
            break;
        case OverlayModel::PillState::Hidden:
            break;
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}
