#pragma once

#include "core/ITextInjector.h"

#include <cstddef>
#include <string>
#include <vector>

class ChunkedInjector : public ITextInjector {
public:
    explicit ChunkedInjector(ITextInjector& inner, std::size_t max_chunk_chars = 4000);

    void InjectText(const std::string& text) override;

    static std::vector<std::string> Split(const std::string& text, std::size_t max_chunk_chars);

private:
    ITextInjector& inner_;
    std::size_t max_chunk_chars_;
};
