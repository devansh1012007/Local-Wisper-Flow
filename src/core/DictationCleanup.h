#pragma once

#include "core/ILlmProvider.h"
#include "core/WorkerQueue.h"

#include <atomic>
#include <memory>
#include <string>

class DictationCleanup {
public:
    explicit DictationCleanup(ILlmProvider* llm);

    void SetEnabled(bool enabled);
    bool Enabled() const;
    void SetMinWords(int min_words);
    void SetTimeoutMs(int timeout_ms);
    void SetPromptAddendum(std::string addendum);
    void SetPreserveSentinels(bool preserve);

    std::string Process(const std::string& raw);

    std::size_t GuardRejections() const;

private:
    std::string ComposePrompt(const std::string& raw) const;
    std::string ApplyGuards(const std::string& raw, std::string cleaned) const;

    ILlmProvider* llm_;
    bool enabled_ = true;
    int min_words_ = 4;
    int timeout_ms_ = 2000;
    std::string prompt_addendum_;
    bool preserve_sentinels_ = false;
    WorkerQueue worker_;
    std::shared_ptr<std::atomic<std::size_t>> guard_rejections_;
};
