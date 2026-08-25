#include "ui/MainWindow.h"

#include "audio/MiniaudioCapture.h"

#include <GLFW/glfw3.h>
#include <cfloat>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>

static void GlfwErrorCallback(int code, const char* description) {
    std::cerr << "[glfw] error " << code << ": "
              << (description != nullptr ? description : "unknown") << '\n';
}

MainWindow::MainWindow(const DictationFSM& fsm, const MiniaudioCapture& audio)
    : fsm_(fsm), audio_(audio) {}

MainWindow::~MainWindow() {
    Shutdown();
}

bool MainWindow::Init() {
    glfwSetErrorCallback(GlfwErrorCallback);
    if (glfwInit() == GLFW_FALSE) {
        std::cerr << "[ui] glfwInit failed" << '\n';
        return false;
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    window_ = glfwCreateWindow(480, 200, "Local Wisper Flow", nullptr, nullptr);
    if (window_ == nullptr) {
        std::cerr << "[ui] window creation failed" << '\n';
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    if (!ImGui_ImplOpenGL3_Init("#version 130")) {
        std::cerr << "[ui] OpenGL3 backend init failed" << '\n';
        Shutdown();
        return false;
    }
    imgui_initialized_ = true;
    return true;
}

void MainWindow::Run() {
    while (window_ != nullptr && glfwWindowShouldClose(window_) == 0) {
        glfwPollEvents();

        const State state = fsm_.CurrentState();
        const float level = audio_.AudioLevel();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImVec2(420.0f, 140.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Local Wisper Flow");
        ImGui::SetWindowFontScale(1.8f);
        ImGui::TextUnformatted(ToString(state));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextUnformatted("Audio level");
        ImGui::ProgressBar(level, ImVec2(-FLT_MIN, 0.0f), "%.0f%%");
        ImGui::Text("%.1f FPS", static_cast<double>(ImGui::GetIO().Framerate));
        ImGui::End();

        ImGui::Render();
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window_, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }
    Shutdown();
}

void MainWindow::Shutdown() noexcept {
    if (!imgui_initialized_) {
        if (window_ != nullptr) {
            glfwDestroyWindow(window_);
            window_ = nullptr;
            glfwTerminate();
        }
        return;
    }
    imgui_initialized_ = false;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}
