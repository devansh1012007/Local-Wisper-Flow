#pragma once

#include <cstddef>

namespace OpenRouterContract {

constexpr const char* kHost = "openrouter.ai";
constexpr const char* kPath = "/api/v1/chat/completions";
constexpr const char* kApiKeyHeader = "Authorization";
constexpr const char* kApiKeyEnv = "OPENROUTER_API_KEY";
constexpr const char* kApiKeyProvider = "openrouter";
constexpr const char* kDefaultModel = "";
constexpr const char* kContentType = "application/json";
constexpr const char* kModelField = "model";
constexpr const char* kMessagesField = "messages";
constexpr const char* kRoleField = "role";
constexpr const char* kContentField = "content";
constexpr const char* kSystemRole = "system";
constexpr const char* kUserRole = "user";
constexpr const char* kChoicesField = "choices";
constexpr const char* kMessageField = "message";
constexpr const char* kTextField = "text";
constexpr const char* kFinishReasonField = "finish_reason";
constexpr const char* kIdField = "id";
constexpr const char* kErrorField = "error";
constexpr const char* kCodeField = "code";
constexpr const char* kErrorMessageField = "message";
constexpr const char* kMetadataField = "metadata";
constexpr const char* kErrorTypeField = "error_type";
constexpr const char* kDetailField = "detail";
constexpr const char* kDocsChecked = "2026-11-07";
constexpr const char* kDocsApiRef = "openrouter.ai/docs/api-reference/overview";
constexpr const char* kDocsErrors = "openrouter.ai/docs/api_reference/errors-and-debugging";
constexpr const char* kDocsQuickstart = "openrouter.ai/docs/quickstart";
constexpr int kDefaultTimeoutMs = 8000;
constexpr std::size_t kMaxDetailChars = 500;

}
