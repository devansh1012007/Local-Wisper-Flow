#include "core/TextPipeline.h"
#include "core/DictationCleanup.h"
#include "core/PersonalDictionary.h"
#include "core/SnippetStore.h"
#include "core/SpokenCommandProcessor.h"

#include <cctype>

TextPipeline::TextPipeline(DictationCleanup& cleanup) : cleanup_(cleanup) {}

void TextPipeline::SetSpokenCommands(SpokenCommandProcessor* processor) {
    spoken_ = processor;
}

void TextPipeline::SetDictionary(PersonalDictionary* dictionary) {
    dictionary_ = dictionary;
}

void TextPipeline::SetSnippets(SnippetStore* snippets) {
    snippets_ = snippets;
}

std::string TextPipeline::NormalizeWhitespace(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    bool pending_space = false;
    for (const char c : text) {
        if (std::isspace(static_cast<unsigned char>(c)) != 0) {
            if (!out.empty()) {
                pending_space = true;
            }
            continue;
        }
        if (pending_space) {
            out.push_back(' ');
            pending_space = false;
        }
        out.push_back(c);
    }
    return out;
}

std::string TextPipeline::NormalizeLines(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    bool pending_space = false;
    for (const char c : text) {
        if (c == '\n') {
            while (!out.empty() && out.back() == ' ') {
                out.pop_back();
            }
            out.push_back('\n');
            pending_space = false;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c)) != 0) {
            if (!out.empty() && out.back() != '\n') {
                pending_space = true;
            }
            continue;
        }
        if (pending_space) {
            out.push_back(' ');
            pending_space = false;
        }
        out.push_back(c);
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

std::string TextPipeline::Process(const std::string& raw) {
    const std::string normalized = NormalizeWhitespace(raw);

    std::string sentineled = normalized;
    if (spoken_ != nullptr) {
        sentineled = spoken_->ToSentinels(normalized);
        if (!sentineled.empty()) {
            cleanup_.SetPreserveSentinels(
                SpokenCommandProcessor::CountSentinels(sentineled) > 0);
        }
    }

    std::string cleaned = cleanup_.Process(sentineled);

    if (spoken_ != nullptr) {
        const std::size_t expected = SpokenCommandProcessor::CountSentinels(sentineled);
        if (expected > 0 && SpokenCommandProcessor::CountSentinels(cleaned) != expected) {
            cleaned = sentineled;
        }
        cleaned = spoken_->ExpandSentinels(cleaned);
    }

    if (dictionary_ != nullptr && dictionary_->Loaded()) {
        cleaned = dictionary_->Enforce(cleaned, true);
    }

    if (snippets_ != nullptr && snippets_->Loaded()) {
        cleaned = snippets_->Expand(cleaned);
    }

    return NormalizeLines(cleaned);
}
