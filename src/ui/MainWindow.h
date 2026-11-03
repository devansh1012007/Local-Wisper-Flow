#pragma once

#include "core/DictationFSM.h"

struct GLFWwindow;

class MiniaudioCapture;

class MainWindow {
public:
    MainWindow(const DictationFSM& fsm, const MiniaudioCapture& audio);
    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    bool Init();
    void Run();
    bool ShouldClose() const;
    void RunOnce();

private:
    void Shutdown() noexcept;

    const DictationFSM& fsm_;
    const MiniaudioCapture& audio_;
    GLFWwindow* window_ = nullptr;
    bool imgui_initialized_ = false;
    bool shutdown_done_ = false;
};
