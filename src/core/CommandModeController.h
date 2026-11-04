#pragma once

#include "core/ErrorCode.h"
#include "core/ILlmProvider.h"

#include <optional>
#include <string>

class ISelectionCapture {
public:
    virtual ~ISelectionCapture() = default;
    virtual std::optional<std::string> CaptureSelection() = 0;
    virtual void RestoreClipboard() = 0;
};

class CommandModeController {
public:
    struct Result {
        bool ok = false;
        ErrorCode code = ErrorCode::Unknown;
        std::string error;
        std::string edited_text;
    };

    CommandModeController(ISelectionCapture& capture, ILlmProvider* llm,
                          class ITextInjector& injector);

    bool BeginSelection();
    Result Run(const std::string& instruction, int timeout_ms);
    void CancelPending();

private:
    ISelectionCapture& capture_;
    ILlmProvider* llm_;
    ITextInjector& injector_;
    std::optional<std::string> pending_selection_;
};
