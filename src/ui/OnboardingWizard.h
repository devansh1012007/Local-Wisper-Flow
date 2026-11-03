#pragma once

#include <string>

class SettingsStore;

class OnboardingWizard {
public:
    explicit OnboardingWizard(SettingsStore& settings);

    bool IsNeeded() const;
    bool IsOpen() const;
    void Open();
    void RenderFrame();
    void SetMicLevel(float level);
    void SetDetectedHotkey(const std::string& chord);

private:
    SettingsStore& settings_;
    int step_ = 0;
    float mic_level_ = 0.0f;
    std::string detected_hotkey_;
};
