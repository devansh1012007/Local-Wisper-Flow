#pragma once

#include "core/CommandModeController.h"
#include "core/ITextInjector.h"

#include <functional>
#include <string>

class WindowsTextInjector : public ITextInjector {
public:
    WindowsTextInjector() = default;

    void InjectText(const std::string& text) override;

    void SetPasteChordProvider(std::function<std::string()> provider);
    void SetInjectDelayMs(int delay_ms);

private:
    void InjectViaClipboard(const std::string& text, const std::string& paste_chord);
    bool InjectViaTyping(const std::string& text);
    void PasteChord(const std::string& paste_chord);
    bool ForegroundIsElevated() const;

    std::function<std::string()> paste_chord_provider_;
    int inject_delay_ms_ = 0;
};

class WindowsSelectionCapture : public ISelectionCapture {
public:
    std::optional<std::string> CaptureSelection() override;
    void RestoreClipboard() override;

private:
    std::string saved_clipboard_;
    bool clipboard_saved_ = false;
};
