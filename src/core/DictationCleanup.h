#pragma once

#include "core/ILlmProvider.h"
#include "core/TransformManager.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <string>

struct CleanupSettings {
    bool enabled = true;
    std::size_t min_words = 4;
    std::chrono::milliseconds timeout{4000};
};

class DictationCleanup {
public:
    using Reporter = std::function<void(const std::string& note)>;

    DictationCleanup(ILlmProvider& llm, CleanupSettings settings, Reporter reporter = nullptr)
        : llm_(llm), settings_(settings), reporter_(std::move(reporter)), executor_(llm) {}

    std::string Clean(const std::string& raw);

    static const char* SystemPrompt();

    static bool AcceptOutput(const std::string& raw, const std::string& cleaned);

    static std::size_t WordCount(const std::string& text);

private:
    void Note(const std::string& text) const {
        if (reporter_) {
            reporter_(text);
        }
    }

    ILlmProvider& llm_;
    CleanupSettings settings_;
    Reporter reporter_;
    TransformManager executor_;
};
