#include "core/TransformManager.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
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

}

TransformManager::TransformManager(ILlmProvider& provider, const std::string& transforms_path)
    : provider_(provider), transforms_(LoadFile(transforms_path)) {}

std::vector<TransformManager::Entry> TransformManager::LoadFile(
    const std::string& path) const {
    std::error_code rc_ec;
    if (!std::filesystem::is_regular_file(path, rc_ec) || rc_ec) {
        throw std::runtime_error("TransformManager: cannot open '" + path + "'");
    }
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

    nlohmann::json items;
    if (json.is_array()) {
        items = json;
    } else if (json.is_object()) {
        if (json.contains("slots") && json["slots"].is_array()) {
            items = json["slots"];
        } else if (json.contains("transforms") && json["transforms"].is_array()) {
            items = json["transforms"];
        } else {
            throw std::runtime_error("TransformManager: '" + path +
                                     "' must contain an array or slots object");
        }
    } else {
        throw std::runtime_error("TransformManager: '" + path + "' must contain an array");
    }

    std::vector<Entry> entries;
    entries.reserve(items.size());
    const bool from_slots = json.is_object();
    for (const auto& element : items) {
        if (!element.is_object() || !element.contains("name") ||
            !element.contains("prompt")) {
            if (from_slots) {
                continue;
            }
        }
        Entry entry;
        entry.name = TrimCopy(element.value("name", std::string()));
        entry.prompt = element.value("prompt", std::string());
        if (entry.name.empty()) {
            if (from_slots) {
                continue;
            }
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
        callback({});
        return;
    }
    if (TrimCopy(text).empty()) {
        callback({});
        return;
    }

    const std::string prompt_template = found->prompt;
    ILlmProvider* provider = &provider_;

    worker_.Submit([provider, prompt_template, text, callback = std::move(callback)]() {
        try {
            const std::string filled_prompt = FillPlaceholder(prompt_template, "{text}", text);
            std::string result = provider->Process(text, filled_prompt);
            callback(std::move(result));
        } catch (const std::exception&) {
            callback({});
        } catch (...) {
            callback({});
        }
    });
}
