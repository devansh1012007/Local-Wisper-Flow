#pragma once

#include <string>
#include <vector>

namespace Languages {

struct Language {
    const char* code;
    const char* name;
};

const std::vector<Language>& WhisperLanguages();
bool IsSupported(const std::string& code);

struct LanguageResolution {
    std::string effective;
    bool forced_to_english = false;
    std::string warning;
};

LanguageResolution ResolveLanguage(const std::string& model_path,
                                   const std::string& setting);

}
