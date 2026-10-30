#pragma once

#include <cstddef>

namespace GnaniContract {

constexpr const char* kHost = "api.vachana.ai";
constexpr const char* kPath = "/stt/v3";
constexpr const char* kApiKeyHeader = "X-API-Key-ID";
constexpr const char* kApiKeyEnv = "GNANI_API_KEY";
constexpr const char* kDefaultModel = "prisma-v3.0";
constexpr const char* kDefaultLanguage = "indic-auto";
constexpr const char* kBoundary = "lwf-gnani-9f2c7a4b1d";
constexpr const char* kModelField = "model";
constexpr const char* kLanguageField = "language";
constexpr const char* kKeytermsField = "keyterms";
constexpr std::size_t kKeytermsCap = 200;
constexpr const char* kAudioField = "audio";
constexpr const char* kAudioFilename = "audio.wav";
constexpr const char* kAudioContentType = "audio/wav";
constexpr std::size_t kMaxClipMs = 29500;
constexpr const char* kRequestIdHeaders[] = {"X-Request-Id", "Request-Id"};
constexpr std::size_t kRequestIdHeaderCount = 2;

}
