#pragma once

#include "core/ILlmProvider.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>

class LocalLlamaProvider : public ILlmProvider {
public:
    struct Options {
        std::size_t context_size = 2048;
        std::size_t max_predict_tokens = 1024;
        int gpu_layers = 0;
        float temperature = 0.2f;
        float top_p = 0.9f;
    };

    explicit LocalLlamaProvider(const std::string& model_path, Options options);
    ~LocalLlamaProvider() override;

    LocalLlamaProvider(const LocalLlamaProvider&) = delete;
    LocalLlamaProvider& operator=(const LocalLlamaProvider&) = delete;

    std::string Process(const std::string& text, const std::string& system_prompt) override;
    void RequestCancel() override;
    bool LastGenerationTruncated() const override;

    bool WarmUp();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::mutex mutex_;
    Options options_;
    std::atomic<bool> cancel_requested_{false};
    std::atomic<bool> last_truncated_{false};
};
