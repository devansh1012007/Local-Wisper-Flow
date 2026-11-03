#include "core/Languages.h"

#include <doctest/doctest.h>
#include <set>
#include <string>

TEST_CASE("language list has at least thirty unique whisper codes from the real table") {
    const auto& languages = Languages::WhisperLanguages();
    CHECK(languages.size() >= 30);

    std::set<std::string> codes;
    for (const auto& language : languages) {
        codes.insert(language.code);
    }
    CHECK(codes.size() == languages.size());

    const char* expected[] = {"en", "zh", "de", "es", "ru", "ko", "fr", "ja",
                              "pt", "tr", "pl", "ca", "nl", "ar", "sv", "it",
                              "id", "hi", "fi", "vi", "he", "uk", "el", "ms",
                              "cs", "ro", "da", "hu", "ta", "no"};
    for (const char* code : expected) {
        CHECK(codes.count(code) == 1);
        INFO("code=", code);
    }
    CHECK(Languages::IsSupported("en"));
    CHECK(Languages::IsSupported("auto"));
    CHECK(Languages::IsSupported("de"));
    CHECK_FALSE(Languages::IsSupported("xx"));
    CHECK_FALSE(Languages::IsSupported("engl"));
}

TEST_CASE("english-only model forces en with a warning") {
    const auto forced = Languages::ResolveLanguage("models/ggml-base.en.bin", "de");
    CHECK(forced.effective == "en");
    CHECK(forced.forced_to_english);
    CHECK_FALSE(forced.warning.empty());

    const auto english_ok = Languages::ResolveLanguage("models/ggml-base.en.bin", "en");
    CHECK(english_ok.effective == "en");
    CHECK_FALSE(english_ok.forced_to_english);

    const auto auto_ok = Languages::ResolveLanguage("models/ggml-base.en.bin", "auto");
    CHECK(auto_ok.effective == "auto");
    CHECK_FALSE(auto_ok.forced_to_english);
}

TEST_CASE("multilingual model keeps the configured language") {
    const auto kept = Languages::ResolveLanguage("models/ggml-base.bin", "de");
    CHECK(kept.effective == "de");
    CHECK_FALSE(kept.forced_to_english);
    CHECK(kept.warning.empty());

    const auto default_auto = Languages::ResolveLanguage("models/ggml-base.bin", "");
    CHECK(default_auto.effective == "auto");
}
