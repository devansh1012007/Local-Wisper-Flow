#pragma once

#include "core/ITextInjector.h"

class WindowsTextInjector : public ITextInjector {
public:
    void InjectText(const std::string& text) override;
};
