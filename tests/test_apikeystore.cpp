#include "core/ApiKeyStore.h"
#include "core/ConsentGate.h"
#include "core/Redact.h"

#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

std::string TestDir(const char* name) {
    const std::string dir =
        (std::filesystem::temp_directory_path() / (std::string("lwf_apikey_") + name)).string();
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

void SetEnvVar(const char* name, const char* value) {
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

void ClearEnv(const char* name) {
#if defined(_WIN32)
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

void ClearAllEnvs() {
    ClearEnv("GNANI_API_KEY");
    ClearEnv("ELEVENLABS_API_KEY");
    ClearEnv("META_MODEL_API_KEY");
}

std::string ReadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void WriteFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::trunc);
    file << content;
}

}

TEST_CASE("apikeystore store wins over env") {
    ClearAllEnvs();
    const std::string dir = TestDir("wins");
    const std::string path = (std::filesystem::path(dir) / "api_keys.json").string();
    WriteFile(path, "{\"gnani\": \"store-key-1\"}");
    ApiKeyStore store(path);
    SetEnvVar("GNANI_API_KEY", "env-key-1");
    const std::string resolved = ResolveApiKey("gnani", store);
    CHECK(resolved == "store-key-1");
    CHECK(store.Has("gnani"));
    ClearAllEnvs();
}

TEST_CASE("apikeystore empty store falls back to env") {
    ClearAllEnvs();
    const std::string dir = TestDir("fallback");
    const std::string path = (std::filesystem::path(dir) / "api_keys.json").string();
    {
        ApiKeyStore store(path);
        CHECK_FALSE(store.Has("gnani"));
        SetEnvVar("GNANI_API_KEY", "env-only-key");
        CHECK(ResolveApiKey("gnani", store) == "env-only-key");
        CHECK_FALSE(store.Has("gnani"));
    }
    ClearAllEnvs();
}

TEST_CASE("apikeystore missing file is env only") {
    ClearAllEnvs();
    const std::string dir = TestDir("missing");
    const std::string path = (std::filesystem::path(dir) / "api_keys.json").string();
    CHECK_FALSE(std::filesystem::exists(path));
    {
        ApiKeyStore store(path);
        CHECK(ResolveApiKey("gnani", store).empty());
        CHECK_FALSE(store.Has("gnani"));
        SetEnvVar("GNANI_API_KEY", "env-fallback");
        CHECK(ResolveApiKey("gnani", store) == "env-fallback");
    }
    ClearAllEnvs();
}

TEST_CASE("apikeystore set has remove round trip persists") {
    ClearAllEnvs();
    const std::string dir = TestDir("roundtrip");
    const std::string path = (std::filesystem::path(dir) / "api_keys.json").string();
    {
        ApiKeyStore store(path);
        CHECK(store.Set("gnani", "key-one"));
        CHECK(store.Has("gnani"));
        CHECK(store.Get("gnani") == "key-one");
        CHECK(store.Set("gnani", "key-two"));
        CHECK(store.Get("gnani") == "key-two");
        CHECK(std::filesystem::exists(path + ".bak"));
        CHECK(store.Remove("gnani"));
        CHECK_FALSE(store.Has("gnani"));
        CHECK(store.Get("gnani").empty());
    }
    {
        ApiKeyStore reloaded(path);
        CHECK_FALSE(reloaded.Has("gnani"));
    }
    {
        ApiKeyStore store(path);
        CHECK(store.Set("openrouter", "reserved-key"));
        CHECK(store.Has("openrouter"));
        CHECK(store.Get("openrouter") == "reserved-key");
        CHECK(ResolveApiKey("openrouter", store) == "reserved-key");
    }
    ClearAllEnvs();
}

TEST_CASE("apikeystore openrouter reserved has no env fallback") {
    ClearAllEnvs();
    const std::string dir = TestDir("openrouter");
    const std::string path = (std::filesystem::path(dir) / "api_keys.json").string();
    {
        ApiKeyStore store(path);
        SetEnvVar("OPENROUTER_API_KEY", "env-openrouter");
        CHECK(ResolveApiKey("openrouter", store).empty());
        CHECK_FALSE(store.Has("openrouter"));
        CHECK(store.Set("openrouter", "stored-openrouter"));
        CHECK(ResolveApiKey("openrouter", store) == "stored-openrouter");
    }
    ClearAllEnvs();
}

TEST_CASE("apikeystore corrupt file loads empty") {
    ClearAllEnvs();
    const std::string dir = TestDir("corrupt");
    const std::string path = (std::filesystem::path(dir) / "api_keys.json").string();
    WriteFile(path, "THIS IS NOT JSON");
    {
        ApiKeyStore store(path);
        CHECK_FALSE(store.Has("gnani"));
        CHECK(store.Get("gnani").empty());
    }
    CHECK(std::filesystem::exists(path + ".bad"));
    ClearAllEnvs();
}

TEST_CASE("apikeystore non object and non string values are ignored") {
    ClearAllEnvs();
    const std::string dir = TestDir("ignored");
    const std::string path = (std::filesystem::path(dir) / "api_keys.json").string();
    WriteFile(path, "{\"gnani\": 123, \"elevenlabs\": \"keep-me\", \"muse\": \"\"}");
    {
        ApiKeyStore store(path);
        CHECK_FALSE(store.Has("gnani"));
        CHECK(store.Has("elevenlabs"));
        CHECK(store.Get("elevenlabs") == "keep-me");
        CHECK_FALSE(store.Has("muse"));
    }
    WriteFile(path, "[1, 2, 3]");
    {
        ApiKeyStore store(path);
        CHECK_FALSE(store.Has("gnani"));
    }
    ClearAllEnvs();
}

TEST_CASE("apikeystore unknown keys survive save") {
    ClearAllEnvs();
    const std::string dir = TestDir("unknown");
    const std::string path = (std::filesystem::path(dir) / "api_keys.json").string();
    WriteFile(path, "{\"gnani\": \"first\", \"future\": \"keep-future\"}");
    {
        ApiKeyStore store(path);
        CHECK(store.Has("future"));
        CHECK(store.Get("future") == "keep-future");
        CHECK(store.Set("gnani", "second"));
    }
    const std::string saved = ReadFile(path);
    CHECK(saved.find("future") != std::string::npos);
    CHECK(saved.find("keep-future") != std::string::npos);
    CHECK(saved.find("second") != std::string::npos);
    ClearAllEnvs();
}

TEST_CASE("apikeystore redaction removes key bytes from log lines") {
    ClearAllEnvs();
    const std::string dir = TestDir("redact");
    const std::string path = (std::filesystem::path(dir) / "api_keys.json").string();
    {
        ApiKeyStore store(path);
        CHECK(store.Set("gnani", "sk-secret-xyz-123"));
        const std::string key = store.Get("gnani");
        REQUIRE_FALSE(key.empty());
        const std::string log_line = "cloud provider gnani enabled key=" + key;
        const std::string redacted = lwf::RedactSecret(log_line, key);
        CHECK(redacted.find(key) == std::string::npos);
        CHECK(redacted.find("[redacted]") != std::string::npos);
        const std::string header_line = "Authorization: Bearer " + key;
        const std::string masked = lwf::RedactAuthValues(header_line);
        CHECK(masked.find(key) == std::string::npos);
        CHECK(masked.find("[redacted]") != std::string::npos);
    }
    ClearAllEnvs();
}
