#pragma once

#include "core/DictationEngine.h"
#include "core/DictationFSM.h"

#include <atomic>

struct GLFWwindow;

class MiniaudioCapture;

class MainWindow {
public:
    MainWindow(const DictationFSM& fsm, const MiniaudioCapture& audio,
               const EngineStatusSink& status, const std::atomic<bool>& engine_alive,
               std::atomic<bool>& quit);
    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    bool Init();
    void Run();

private:
    void Shutdown() noexcept;

    const DictationFSM& fsm_;
    const MiniaudioCapture& audio_;
    const EngineStatusSink& status_;
    const std::atomic<bool>& engine_alive_;
    std::atomic<bool>& quit_;
    GLFWwindow* window_ = nullptr;
    bool imgui_initialized_ = false;
};
