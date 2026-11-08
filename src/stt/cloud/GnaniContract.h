#pragma once

#include <cstddef>

namespace GnaniContract {

// Docs checked 2026-11-08 against https://docs.gnani.ai/api/STT/speech-to-text.md (verified from docs, NOT yet against live API)
// keyterms is sent as a single JSON-array string field matching the docs-example form (checked 2026-11-08).
constexpr const char* kHost = "api.vachana.ai";
constexpr const char* kPath = "/stt/v3";
constexpr const char* kApiKeyHeader = "X-API-Key-ID";
constexpr const char* kApiKeyEnv = "GNANI_API_KEY";
constexpr const char* kDefaultModel = "prisma-v3.0";
constexpr const char* kDefaultLanguage = "indic-auto";
constexpr const char* kBoundary = "lwf-gnani-9b21af4c17";
constexpr const char* kModelField = "model";
constexpr const char* kLanguageField = "language_code";
constexpr const char* kKeytermsField = "keyterms";
constexpr std::size_t kKeytermsCap = 200;
constexpr std::size_t kKeytermMaxChars = 50;
constexpr std::size_t kKeytermMaxWords = 5;
constexpr const char* kAudioField = "audio_file";
constexpr const char* kAudioFilename = "audio.wav";
constexpr const char* kAudioContentType = "audio/wav";
constexpr std::size_t kMaxClipMs = 29500;
constexpr const char* kRequestIdHeaders[] = {"X-Request-Id", "Request-Id"};
constexpr std::size_t kRequestIdHeaderCount = 2;

}
