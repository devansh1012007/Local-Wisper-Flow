#include "core/TransformManager.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
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
    : provider_(provider), transforms_(LoadFile(transforms_path)) {}

TransformManager::~TransformManager() {
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

std::vector<TransformManager::Entry> TransformManager::LoadFile(
    const std::string& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("TransformManager: cannot open '" + path + "'");
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("TransformManager: failed to parse '" + path + "': " +
                                 e.what());
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

    workers_.erase(std::remove_if(workers_.begin(), workers_.end(),
                                  [](std::thread& worker) { return !worker.joinable(); }),
                   workers_.end());

    const std::string prompt_template = found->prompt;
    ILlmProvider* provider = &provider_;

    workers_.emplace_back([provider, prompt_template, text,
                           callback = std::move(callback)]() {
        try {
            const std::string filled_prompt = FillPlaceholder(prompt_template, "{text}", text);
            std::cout << "[transform] running on thread " << std::this_thread::get_id()
                      << std::endl;
            std::string result = provider->Process(text, filled_prompt);
            callback(std::move(result));
        } catch (const std::exception& e) {
            std::cerr << "[transform] error: " << e.what() << '\n';
            callback({});
        } catch (...) {
            std::cerr << "[transform] unknown error" << '\n';
            callback({});
        }
    });
}
