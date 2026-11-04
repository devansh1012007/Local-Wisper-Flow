#include <doctest/doctest.h>

#include "stt/cloud/LanguageMap.h"

#include <string>
#include <vector>

TEST_CASE("elevenlabs language code picks the first valid iso token") {
    CHECK(sttlang::ElevenLabsLanguageCode("kn,hi") == "kn");
    CHECK(sttlang::ElevenLabsLanguageCode("kn") == "kn");
    CHECK(sttlang::ElevenLabsLanguageCode("hin") == "hin");
    CHECK(sttlang::ElevenLabsLanguageCode(" bad-code,kn ") == "kn");
    CHECK(sttlang::ElevenLabsLanguageCode("EN") == "en");
}

TEST_CASE("elevenlabs language code falls back to auto detection") {
    CHECK(sttlang::ElevenLabsLanguageCode("").empty());
    CHECK(sttlang::ElevenLabsLanguageCode("auto").empty());
    CHECK(sttlang::ElevenLabsLanguageCode("indic-auto").empty());
    CHECK(sttlang::ElevenLabsLanguageCode("bad-code").empty());
    CHECK(sttlang::ElevenLabsLanguageCode("a").empty());
    CHECK(sttlang::ElevenLabsLanguageCode("auto,bad-code").empty());
}

TEST_CASE("muse language bias maps iso codes to vendor names") {
    CHECK(sttlang::MuseLanguageBias("en") == std::vector<std::string>{"English"});
    CHECK(sttlang::MuseLanguageBias("kn,hi") ==
          std::vector<std::string>{"Kannada", "Hindi"});
    CHECK(sttlang::MuseLanguageBias("zh") == std::vector<std::string>{"Mandarin Chinese"});
}

TEST_CASE("muse language bias maps indic-auto to the documented indic set") {
    const std::vector<std::string> bias = sttlang::MuseLanguageBias("indic-auto");
    const std::vector<std::string> expected = {"Bengali", "Hindi", "Kannada", "Marathi",
                                               "Tamil", "Telugu"};
    CHECK(bias == expected);
}

TEST_CASE("muse language bias deduplicates and skips unknown languages") {
    CHECK(sttlang::MuseLanguageBias("en,en,zz") == std::vector<std::string>{"English"});
    CHECK(sttlang::MuseLanguageBias("").empty());
    CHECK(sttlang::MuseLanguageBias("auto").empty());
    CHECK(sttlang::MuseLanguageBias("de,indic-auto") ==
          std::vector<std::string>{"German", "Bengali", "Hindi", "Kannada", "Marathi", "Tamil",
                                   "Telugu"});
}
