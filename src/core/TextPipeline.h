#pragma once

#include <cstdint>
#include <string>

class DictationCleanup;
class SpokenCommandProcessor;
class PersonalDictionary;
class SnippetStore;

struct ProcessResult {
    std::string text;
    std::int64_t words_corrected = 0;
    std::int64_t dictionary_fixes = 0;
};

class TextPipeline {
public:
    explicit TextPipeline(DictationCleanup& cleanup);

    void SetSpokenCommands(SpokenCommandProcessor* processor);
    void SetDictionary(PersonalDictionary* dictionary);
    void SetSnippets(SnippetStore* snippets);

    static std::string NormalizeWhitespace(const std::string& text);
    static std::string NormalizeLines(const std::string& text);

    ProcessResult ProcessWithStats(const std::string& raw);
    std::string Process(const std::string& raw);

private:
    DictationCleanup& cleanup_;
    SpokenCommandProcessor* spoken_ = nullptr;
    PersonalDictionary* dictionary_ = nullptr;
    SnippetStore* snippets_ = nullptr;
};
