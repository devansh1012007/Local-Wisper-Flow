#include "core/VersionCheck.h"

#include <nlohmann/json.hpp>

namespace lwf {

namespace {

// One dot-separated numeric component. Non-numeric characters stop the
// component and the remaining text is ignored ("1.2.3-beta1" == "1.2.3").
int VersionComponent(const std::string& version, std::size_t index) {
    std::size_t part = 0;
    std::size_t begin = 0;
    std::size_t pos = 0;
    while (pos <= version.size()) {
        if (pos == version.size() || version[pos] == '.') {
            if (part == index) {
                int value = 0;
                for (std::size_t i = begin; i < pos; ++i) {
                    const char c = version[i];
                    if (c < '0' || c > '9') {
                        break;
                    }
                    value = value * 10 + (c - '0');
                }
                return value;
            }
            ++part;
            begin = pos + 1;
        }
        ++pos;
    }
    return 0;
}

std::string StripVPrefix(std::string version) {
    if (version.size() >= 2 && (version[0] == 'v' || version[0] == 'V')) {
        // Only strip when the second character continues a version number;
        // "vX" from a hostile file is not a version.
        if (version[1] >= '0' && version[1] <= '9') {
            return version.substr(1);
        }
    }
    return version;
}

}  // namespace

bool SplitHttpsUrl(const std::string& url, UrlParts& parts) {
    const std::string scheme = "https://";
    if (url.rfind(scheme, 0) != 0) {
        return false;
    }
    const std::string rest = url.substr(scheme.size());
    const std::size_t slash = rest.find('/');
    if (slash == std::string::npos) {
        parts.host = rest;
        parts.path = "/";
    } else {
        parts.host = rest.substr(0, slash);
        parts.path = rest.substr(slash);
    }
    if (parts.host.empty()) {
        return false;
    }
    // No userinfo trickery ("user@host") and no explicit port games beyond
    // a plain :port suffix.
    return parts.host.find('@') == std::string::npos;
}

int VersionCompare(const std::string& raw_a, const std::string& raw_b) {
    const std::string a = StripVPrefix(raw_a);
    const std::string b = StripVPrefix(raw_b);
    for (std::size_t i = 0; i < 4; ++i) {
        const int part_a = VersionComponent(a, i);
        const int part_b = VersionComponent(b, i);
        if (part_a != part_b) {
            return part_a < part_b ? -1 : 1;
        }
    }
    return 0;
}

UpdateCheckResult EvaluateLatestJson(const std::string& body,
                                     const std::string& current_version) {
    UpdateCheckResult result;
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(body);
    } catch (const nlohmann::json::exception& e) {
        result.error = std::string("update check: latest.json is not valid JSON (") + e.what() + ")";
        return result;
    }
    if (!json.is_object() || !json.contains("version") || !json["version"].is_string()) {
        result.error = "update check: latest.json has no version field";
        return result;
    }
    result.latest_version = json["version"].get<std::string>();
    if (result.latest_version.empty()) {
        result.error = "update check: latest.json version is empty";
        return result;
    }
    const auto read_url = [&json](const char* key) -> std::string {
        if (json.contains(key) && json[key].is_string()) {
            const std::string value = json[key].get<std::string>();
            // Only https asset URLs are accepted; anything else falls back
            // to the releases page.
            if (value.rfind("https://", 0) == 0) {
                return value;
            }
        }
        return std::string();
    };
    result.download_url = read_url("installer_url");
    if (result.download_url.empty()) {
        result.download_url = read_url("portable_url");
    }
    if (result.download_url.empty()) {
        result.download_url = kReleasesPageUrl;
    }
    result.update_available = VersionCompare(result.latest_version, current_version) > 0;
    result.ok = true;
    return result;
}

}  // namespace lwf
