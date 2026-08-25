#pragma once

#include "core/ILlmProvider.h"

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>

class LocalLlamaProvider : public ILlmProvider {
public:
    explicit LocalLlamaProvider(const std::string& model_path,
                                std::size_t context_size = 2048,
                                std::size_t max_predict_tokens = 512);
    ~LocalLlamaProvider() override;

    LocalLlamaProvider(const LocalLlamaProvider&) = delete;
    LocalLlamaProvider& operator=(const LocalLlamaProvider&) = delete;

    std::string Process(const std::string& text, const std::string& system_prompt) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::mutex mutex_;
    std::size_t context_size_;
    std::size_t max_predict_tokens_;
};
