#pragma once

#include <cstddef>

namespace MuseContract {

constexpr const char* kHost = "api.meta.ai";
constexpr const char* kPath = "/v1/asr/transcribe";
constexpr const char* kApiKeyHeader = "Authorization";
constexpr const char* kApiKeyEnv = "META_MODEL_API_KEY";
constexpr const char* kDefaultModel = "muse-voice-transcribe-1.0";
constexpr const char* kMode = "PUSH_TO_TALK";
constexpr const char* kAudioEncoding = "WAV";
constexpr const char* kBoundary = "lwf-muse-7c41aa9d52";
constexpr const char* kRequestField = "request";
constexpr const char* kAudioField = "audio";
constexpr const char* kAudioFilename = "audio.wav";
constexpr const char* kAudioContentType = "audio/wav";
constexpr std::size_t kMaxClipMs = 595000;
constexpr std::size_t kKeywordsCap = 200;
constexpr std::size_t kKeywordMaxChars = 50;

}
