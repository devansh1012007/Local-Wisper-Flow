#include <doctest/doctest.h>

#include "core/WhisperText.h"

#include <string>

TEST_CASE("hallucination only transcripts are filtered to empty") {
    CHECK(WhisperText::FilterHallucinations("[BLANK_AUDIO]").empty());
    CHECK(WhisperText::FilterHallucinations("  (music)  ").empty());
    CHECK(WhisperText::FilterHallucinations("[silence]").empty());
    CHECK(WhisperText::FilterHallucinations("...").empty());
    CHECK(WhisperText::FilterHallucinations("").empty());
    CHECK(WhisperText::FilterHallucinations("   ").empty());
    CHECK(WhisperText::FilterHallucinations("(no speech)").empty());
}

TEST_CASE("real speech survives the hallucination filter") {
    CHECK(WhisperText::FilterHallucinations("hello world this is a test") ==
          "hello world this is a test");
    CHECK(WhisperText::FilterHallucinations("  trimmed speech  ") == "trimmed speech");
    CHECK(WhisperText::FilterHallucinations("turn on the music please") ==
          "turn on the music please");
    CHECK(WhisperText::FilterHallucinations("the song had (music) in the title") ==
          "the song had (music) in the title");
}

TEST_CASE("hallucination detection is case insensitive and bracket tolerant") {
    CHECK(WhisperText::IsOnlyHallucination("[blank_audio]"));
    CHECK(WhisperText::IsOnlyHallucination("[Blank_Audio]"));
    CHECK_FALSE(WhisperText::IsOnlyHallucination("music"));
    CHECK_FALSE(WhisperText::IsOnlyHallucination("music was playing"));
    CHECK(WhisperText::IsOnlyHallucination("[Music]"));
}
