#pragma once

#include "core/ILlmProvider.h"
#include "core/WorkerQueue.h"

#include <functional>
#include <string>
#include <vector>

class TransformManager {
public:
    using CompletionCallback = std::function<void(std::string result)>;

    TransformManager(ILlmProvider& provider, const std::string& transforms_path);
    ~TransformManager() = default;

    TransformManager(const TransformManager&) = delete;
    TransformManager& operator=(const TransformManager&) = delete;

    void ExecuteTransform(const std::string& transform_name, const std::string& text,
                          CompletionCallback callback);

private:
    struct Entry {
        std::string name;
        std::string prompt;
    };

    std::vector<Entry> LoadFile(const std::string& path) const;

    ILlmProvider& provider_;
    std::vector<Entry> transforms_;
    WorkerQueue worker_;
};
