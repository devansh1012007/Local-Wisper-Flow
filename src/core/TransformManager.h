#pragma once

#include "core/ILlmProvider.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class TransformManager {
public:
    using CompletionCallback = std::function<void(std::string result)>;

    explicit TransformManager(ILlmProvider& provider, const std::string& transforms_path = "");
    ~TransformManager();

    TransformManager(const TransformManager&) = delete;
    TransformManager& operator=(const TransformManager&) = delete;

    void ExecuteTransform(const std::string& transform_name, const std::string& text,
                          CompletionCallback callback);

    std::future<std::string> ProcessAsync(const std::string& text, const std::string& system_prompt);

    void Post(std::function<void()> task);

    bool HasTransform(const std::string& transform_name) const;

private:
    struct Entry {
        std::string name;
        std::string prompt;
    };

    std::vector<Entry> LoadFile(const std::string& path) const;

    ILlmProvider& provider_;
    std::vector<Entry> transforms_;
    std::deque<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable work_signal_;
    bool stopping_ = false;
    std::thread worker_;
};
