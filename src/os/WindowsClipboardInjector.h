#pragma once

#include "core/ITextInjector.h"

#include <string>

class WindowsClipboardInjector final : public ITextInjector {
public:
    explicit WindowsClipboardInjector(int restore_delay_ms = 300);
    ~WindowsClipboardInjector() override;

    WindowsClipboardInjector(const WindowsClipboardInjector&) = delete;
    WindowsClipboardInjector& operator=(const WindowsClipboardInjector&) = delete;

    void InjectText(const std::string& text) override;

private:
    void* window_ = nullptr;
    int restore_delay_ms_;
};
