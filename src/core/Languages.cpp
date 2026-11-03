#include "core/Languages.h"

namespace Languages {

const std::vector<Language>& WhisperLanguages() {
    static const std::vector<Language> languages = {
        {"en", "English"},    {"zh", "Chinese"},    {"de", "German"},
        {"es", "Spanish"},    {"ru", "Russian"},    {"ko", "Korean"},
        {"fr", "French"},     {"ja", "Japanese"},   {"pt", "Portuguese"},
        {"tr", "Turkish"},    {"pl", "Polish"},     {"ca", "Catalan"},
        {"nl", "Dutch"},      {"ar", "Arabic"},     {"sv", "Swedish"},
        {"it", "Italian"},    {"id", "Indonesian"}, {"hi", "Hindi"},
        {"fi", "Finnish"},    {"vi", "Vietnamese"}, {"he", "Hebrew"},
        {"uk", "Ukrainian"},  {"el", "Greek"},      {"ms", "Malay"},
        {"cs", "Czech"},      {"ro", "Romanian"},   {"da", "Danish"},
        {"hu", "Hungarian"},  {"ta", "Tamil"},      {"no", "Norwegian"},
    };
    return languages;
}

bool IsSupported(const std::string& code) {
    if (code == "auto") {
        return true;
    }
    for (const Language& language : WhisperLanguages()) {
        if (code == language.code) {
            return true;
        }
    }
    return false;
}

LanguageResolution ResolveLanguage(const std::string& model_path,
                                   const std::string& setting) {
    LanguageResolution resolution;
    resolution.effective = setting.empty() ? std::string("auto") : setting;

    if (model_path.size() >= 7 &&
        model_path.rfind(".en.bin") == model_path.size() - 7) {
        if (!resolution.effective.empty() && resolution.effective != "en" &&
            resolution.effective != "auto") {
            resolution.forced_to_english = true;
            resolution.effective = "en";
            resolution.warning =
                "English-only model cannot transcribe other languages; language forced to en";
        }
    }
    return resolution;
}

}
