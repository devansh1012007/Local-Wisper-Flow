#include "stt/cloud/LanguageMap.h"

#include <cstddef>
#include <set>

namespace {

std::vector<std::string> SplitLanguageList(const std::string& value) {
    std::vector<std::string> tokens;
    std::string current;
    for (const char c : value) {
        if (c == ',') {
            tokens.push_back(current);
            current.clear();
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            continue;
        }
        current.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
    }
    tokens.push_back(current);
    return tokens;
}

bool IsoLanguageCode(const std::string& token) {
    if (token.size() < 2 || token.size() > 3) {
        return false;
    }
    for (const char c : token) {
        if (c < 'a' || c > 'z') {
            return false;
        }
    }
    return true;
}

struct LanguageName {
    const char* code;
    const char* name;
};

const LanguageName kMuseLanguages[] = {
    {"ar", "Arabic"},       {"bn", "Bengali"},   {"nl", "Dutch"},
    {"en", "English"},      {"fr", "French"},    {"de", "German"},
    {"he", "Hebrew"},       {"hi", "Hindi"},     {"id", "Indonesian"},
    {"it", "Italian"},      {"ja", "Japanese"},  {"kn", "Kannada"},
    {"ko", "Korean"},       {"ms", "Malay"},     {"zh", "Mandarin Chinese"},
    {"mr", "Marathi"},      {"pl", "Polish"},    {"pt", "Portuguese"},
    {"es", "Spanish"},      {"tl", "Tagalog"},   {"ta", "Tamil"},
    {"te", "Telugu"},       {"th", "Thai"},      {"tr", "Turkish"},
    {"vi", "Vietnamese"},
};
constexpr std::size_t kMuseLanguageCount = sizeof(kMuseLanguages) / sizeof(kMuseLanguages[0]);

const char* MuseLanguageName(const std::string& code) {
    for (std::size_t i = 0; i < kMuseLanguageCount; ++i) {
        if (code == kMuseLanguages[i].code) {
            return kMuseLanguages[i].name;
        }
    }
    return nullptr;
}

bool AutoDetectToken(const std::string& token) {
    return token.empty() || token == "auto" || token == "indic-auto";
}

}  // namespace

namespace sttlang {

std::string ElevenLabsLanguageCode(const std::string& stt_language) {
    for (const std::string& token : SplitLanguageList(stt_language)) {
        if (AutoDetectToken(token)) {
            continue;
        }
        if (IsoLanguageCode(token)) {
            return token;
        }
    }
    return std::string();
}

std::vector<std::string> MuseLanguageBias(const std::string& stt_language) {
    std::vector<std::string> bias;
    std::set<std::string> seen;
    for (const std::string& token : SplitLanguageList(stt_language)) {
        if (token == "indic-auto") {
            for (const char* indic : {"bn", "hi", "kn", "mr", "ta", "te"}) {
                const std::string name = MuseLanguageName(indic);
                if (seen.insert(name).second) {
                    bias.push_back(name);
                }
            }
            continue;
        }
        if (AutoDetectToken(token)) {
            continue;
        }
        const char* name = MuseLanguageName(token);
        if (name != nullptr && seen.insert(name).second) {
            bias.emplace_back(name);
        }
    }
    return bias;
}

}
