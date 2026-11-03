#pragma once

#include <string>

class DictationCleanup;
class SpokenCommandProcessor;
class PersonalDictionary;
class SnippetStore;

class TextPipeline {
public:
    explicit TextPipeline(DictationCleanup& cleanup);

    void SetSpokenCommands(SpokenCommandProcessor* processor);
    void SetDictionary(PersonalDictionary* dictionary);
    void SetSnippets(SnippetStore* snippets);

    static std::string NormalizeWhitespace(const std::string& text);
    static std::string NormalizeLines(const std::string& text);

    std::string Process(const std::string& raw);

private:
    DictationCleanup& cleanup_;
    SpokenCommandProcessor* spoken_ = nullptr;
    PersonalDictionary* dictionary_ = nullptr;
    SnippetStore* snippets_ = nullptr;
};
