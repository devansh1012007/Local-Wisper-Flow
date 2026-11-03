#pragma once

#include <string>

class SettingsStore;
class UiBus;

class SettingsWindow {
public:
    SettingsWindow(SettingsStore& settings, UiBus& bus);

    bool Init();
    void Shutdown();
    void RenderFrame();
    bool IsOpen() const;
    void Open();
    void Close();

private:
    SettingsStore& settings_;
    UiBus& bus_;
    bool open_ = false;
    bool restart_needed_ = false;
    std::string validation_error_;
    void* window_ = nullptr;
};
