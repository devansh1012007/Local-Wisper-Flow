#pragma once

#include <string>

class ITextInjector {
public:
    virtual ~ITextInjector() = default;

    virtual void InjectText(const std::string& text) = 0;
};
