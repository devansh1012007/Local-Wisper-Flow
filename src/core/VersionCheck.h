#pragma once

#include <chrono>
#include <string>

// Remote update check (followups-6, manual-check path). The "backend" is the
// release itself: docs/RELEASE.md step 8 attaches a latest.json to every
// GitHub Release, and the stable /releases/latest/download/latest.json URL
// always redirects to the newest one - no server of our own. The network
// seam is the injected IHttpTransport (same one the cloud STT client uses);
// this module is the portable half: URL splitting, version comparison and
// the latest.json evaluation, all unit-tested.
namespace lwf {

// Redirect-stable catalog URL (release workflow publishes the file).
inline constexpr const char* kLatestJsonUrl =
    "https://github.com/devansh1012007/Local-Wisper-Flow/releases/latest/download/latest.json";

// Where "Open download page" lands when latest.json carries no asset URLs.
inline constexpr const char* kReleasesPageUrl =
    "https://github.com/devansh1012007/Local-Wisper-Flow/releases/latest";

struct UrlParts {
    std::string host;
    std::string path;
};

// Splits https://host[/path] into host + path (path keeps its leading '/',
// defaults to "/"). Accepts https only - the update check must not be
// downgraded to http by a crafted config. False on anything else.
bool SplitHttpsUrl(const std::string& url, UrlParts& parts);

// Seconds since the Unix epoch (double). Persists the last update-check
// time (config key update_last_check_s); double outlives the 2038 int32
// rollover. Portable, deterministic enough for a 24 h cadence check.
inline double EpochSecondsNow() {
    return std::chrono::duration<double>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// Three-way semver compare of "x.y.z" strings (leading "v"/"V" ignored,
// numeric parts compared as integers, fewer parts = 0 padded). Negative
// when a < b, 0 when equal, positive when a > b. Unparseable parts compare
// as 0, so a garbage version never crashes a check.
int VersionCompare(const std::string& a, const std::string& b);

struct UpdateCheckResult {
    bool ok = false;  // false: the fetch produced an unusable body (error set)
    std::string error;
    bool update_available = false;  // latest_version > current_version
    std::string latest_version;
    // Installer asset from latest.json; falls back to the releases page.
    std::string download_url;
};

// Evaluates a latest.json body against the running version. Schema:
// {"version": "x.y.z", "installer_url": "...", "portable_url": "..."} -
// version is required, the asset URLs are optional (missing -> the
// releases page). A 4xx body that is not JSON fails with an error, never
// with update_available=true.
UpdateCheckResult EvaluateLatestJson(const std::string& body,
                                     const std::string& current_version);

}  // namespace lwf
