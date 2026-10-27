#include "core/TransformManager.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {

std::string FillPlaceholder(std::string prompt, const std::string& placeholder,
                            const std::string& value) {
    for (std::size_t pos = prompt.find(placeholder); pos != std::string::npos;
         pos = prompt.find(placeholder)) {
        prompt.replace(pos, placeholder.size(), value);
    }
    return prompt;
}

std::string TrimCopy(const std::string& text) {
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

}  // namespace

TransformManager::TransformManager(ILlmProvider& provider, const std::string& transforms_path)
    : provider_(provider) {
    if (!transforms_path.empty()) {
        try {
            transforms_ = LoadFile(transforms_path);
        } catch (const std::exception& e) {
            std::cerr << "[transform] no transforms loaded (" << e.what() << ")\n";
        }
    }
    worker_ = std::thread([this] {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                work_signal_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
                if (stopping_ && tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
            try {
                task();
            } catch (const std::exception& e) {
                std::cerr << "[transform] task error: " << e.what() << '\n';
            } catch (...) {
                std::cerr << "[transform] unknown task error" << '\n';
            }
        }
    });
}

TransformManager::~TransformManager() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        tasks_.clear();
    }
    work_signal_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void TransformManager::Post(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push_back(std::move(task));
    }
    work_signal_.notify_one();
}

bool TransformManager::HasTransform(const std::string& transform_name) const {
    return std::any_of(transforms_.begin(), transforms_.end(),
                       [&](const Entry& entry) { return entry.name == transform_name; });
}

std::future<std::string> TransformManager::ProcessAsync(const std::string& text,
                                                        const std::string& system_prompt) {
    auto promise = std::make_shared<std::promise<std::string>>();
    std::future<std::string> future = promise->get_future();
    ILlmProvider* provider = &provider_;
    Post([provider, promise, text, system_prompt] {
        try {
            promise->set_value(provider->Process(text, system_prompt));
        } catch (...) {
            promise->set_value(std::string());
        }
    });
    return future;
}

void TransformManager::ExecuteTransform(const std::string& transform_name,
                                        const std::string& text, CompletionCallback callback) {
    const Entry* found = nullptr;
    for (const Entry& entry : transforms_) {
        if (entry.name == transform_name) {
            found = &entry;
            break;
        }
    }

    if (found == nullptr) {
        std::cerr << "[transform] unknown transform '" << transform_name << "'\n";
        callback({});
        return;
    }
    if (TrimCopy(text).empty()) {
        callback({});
        return;
    }

    const std::string prompt_template = found->prompt;
    ILlmProvider* provider = &provider_;
    Post([provider, prompt_template, text, callback = std::move(callback)] {
        std::string result;
        try {
            const std::string filled_prompt = FillPlaceholder(prompt_template, "{text}", text);
            result = provider->Process(text, filled_prompt);
        } catch (const std::exception& e) {
            std::cerr << "[transform] error: " << e.what() << '\n';
            result.clear();
        } catch (...) {
            std::cerr << "[transform] unknown error" << '\n';
            result.clear();
        }
        callback(std::move(result));
    });
}

std::vector<TransformManager::Entry> TransformManager::LoadFile(const std::string& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("TransformManager: cannot open '" + path + "'");
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("TransformManager: failed to parse '" + path + "': " + e.what());
    }

    if (!json.is_array()) {
        throw std::runtime_error("TransformManager: '" + path + "' must contain an array");
    }

    std::vector<Entry> entries;
    entries.reserve(json.size());
    for (const auto& element : json) {
        Entry entry;
        entry.name = TrimCopy(element.at("name").get<std::string>());
        entry.prompt = element.at("prompt").get<std::string>();
        if (entry.name.empty()) {
            throw std::runtime_error("TransformManager: transform with empty name in '" + path +
                                     "'");
        }
        if (entry.prompt.find("{text}") == std::string::npos) {
            throw std::runtime_error("TransformManager: transform '" + entry.name +
                                     "' prompt must contain '{text}'");
        }
        for (const Entry& existing : entries) {
            if (existing.name == entry.name) {
                throw std::runtime_error("TransformManager: duplicate transform name '" +
                                         entry.name + "'");
            }
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}
