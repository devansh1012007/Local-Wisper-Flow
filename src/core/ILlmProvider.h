#pragma once

#include <string>

class ILlmProvider {
public:
    virtual ~ILlmProvider() = default;

    virtual std::string Process(const std::string& text, const std::string& system_prompt) = 0;
};
