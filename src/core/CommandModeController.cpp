#include "core/CommandModeController.h"
#include "core/ITextInjector.h"
#include "core/WorkerQueue.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <future>
#include <memory>

namespace {

bool StartsPreamble(const std::string& text) {
    static const char* kPreambles[] = {"sure", "here is", "here's", "certainly", "as an ai"};
    std::string lower;
    lower.reserve(text.size());
    std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return false;
    }
    for (std::size_t i = first; i < text.size(); ++i) {
        const unsigned char u = static_cast<unsigned char>(text[i]);
        lower.push_back(u < 0x80 ? static_cast<char>(std::tolower(u)) : text[i]);
    }
    for (const char* preamble : kPreambles) {
        const std::string candidate = preamble;
        if (lower.rfind(candidate, 0) == 0) {
            const std::size_t after = candidate.size();
            if (lower.size() == after || lower[after] == ' ' || lower[after] == ':' ||
                lower[after] == ',' || lower[after] == '!') {
                return true;
            }
        }
    }
    return false;
}

std::string TrimCopy(const std::string& text) {
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

}

CommandModeController::CommandModeController(ISelectionCapture& capture, ILlmProvider* llm,
                                             ITextInjector& injector)
    : capture_(capture), llm_(llm), injector_(injector) {}

void CommandModeController::CancelPending() {
    if (pending_selection_.has_value()) {
        capture_.RestoreClipboard();
        pending_selection_.reset();
    }
}

bool CommandModeController::BeginSelection() {
    if (pending_selection_.has_value()) {
        return true;
    }
    pending_selection_ = capture_.CaptureSelection();
    if (!pending_selection_.has_value() || TrimCopy(*pending_selection_).empty()) {
        capture_.RestoreClipboard();
        pending_selection_.reset();
        return false;
    }
    return true;
}

CommandModeController::Result CommandModeController::Run(const std::string& instruction,
                                                         int timeout_ms) {
    Result result;

    if (llm_ == nullptr) {
        result.code = ErrorCode::ModelMissing;
        result.error = "Command mode needs the language model";
        return result;
    }

    if (!pending_selection_.has_value()) {
        if (!BeginSelection()) {
            result.code = ErrorCode::Unknown;
            result.error = "Select some text first";
            return result;
        }
    }

    const std::string selection = *pending_selection_;
    const std::string system_prompt =
        std::string("You edit text. Apply the instruction to the text. Output only the edited "
                    "text. Keep the input language, never translate.\nInstruction: ") +
        instruction;

    WorkerQueue worker;
    auto task = std::make_shared<std::packaged_task<std::string()>>(
        [this, selection, system_prompt]() { return llm_->Process(selection, system_prompt); });
    std::future<std::string> future = task->get_future();
    worker.Submit([task] { (*task)(); });

    if (future.wait_for(std::chrono::milliseconds(timeout_ms)) != std::future_status::ready) {
        llm_->RequestCancel();
        future.wait_for(std::chrono::seconds(3));
        capture_.RestoreClipboard();
        pending_selection_.reset();
        result.code = ErrorCode::LlmTimeout;
        result.error = "Command mode timed out";
        return result;
    }

    std::string edited;
    try {
        edited = future.get();
    } catch (const std::exception& e) {
        capture_.RestoreClipboard();
        pending_selection_.reset();
        result.code = ErrorCode::LlmFailed;
        result.error = e.what();
        return result;
    }

    edited = TrimCopy(std::move(edited));
    if (edited.empty() || StartsPreamble(edited)) {
        capture_.RestoreClipboard();
        pending_selection_.reset();
        result.code = ErrorCode::LlmFailed;
        result.error = "The model did not return usable edited text";
        return result;
    }

    try {
        injector_.InjectText(edited);
    } catch (const std::exception& e) {
        capture_.RestoreClipboard();
        pending_selection_.reset();
        result.code = ErrorCode::InjectFailed;
        result.error = e.what();
        return result;
    }

    capture_.RestoreClipboard();
    pending_selection_.reset();
    result.ok = true;
    result.edited_text = std::move(edited);
    return result;
}
