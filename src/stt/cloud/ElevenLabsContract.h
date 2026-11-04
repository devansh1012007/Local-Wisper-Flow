#pragma once

#include <cstddef>

namespace ElevenLabsContract {

constexpr const char* kHost = "api.elevenlabs.io";
constexpr const char* kPath = "/v1/speech-to-text";
constexpr const char* kApiKeyHeader = "xi-api-key";
constexpr const char* kApiKeyEnv = "ELEVENLABS_API_KEY";
constexpr const char* kDefaultModel = "scribe_v2";
constexpr const char* kBoundary = "lwf-elevenlabs-3d81aa6f2c";
constexpr const char* kModelField = "model_id";
constexpr const char* kLanguageField = "language_code";
constexpr const char* kKeytermsField = "keyterms";
constexpr std::size_t kKeytermsCap = 200;
constexpr std::size_t kKeytermMaxChars = 50;
constexpr std::size_t kKeytermMaxWords = 5;
constexpr const char* kAudioField = "file";
constexpr const char* kAudioFilename = "audio.wav";
constexpr const char* kAudioContentType = "audio/wav";
constexpr std::size_t kMaxClipMs = 29500;
constexpr const char* kRequestIdHeaders[] = {"X-Request-Id", "Request-Id"};
constexpr std::size_t kRequestIdHeaderCount = 2;

}
