#include "core/VersionCheck.h"

#include <doctest/doctest.h>
#include <string>

TEST_CASE("SplitHttpsUrl accepts https only") {
    lwf::UrlParts parts;
    CHECK(lwf::SplitHttpsUrl("https://example.com/releases/latest", parts));
    CHECK(parts.host == "example.com");
    CHECK(parts.path == "/releases/latest");

    CHECK(lwf::SplitHttpsUrl("https://github.com/a/b/releases/latest/download/latest.json",
                             parts));
    CHECK(parts.host == "github.com");
    CHECK(parts.path == "/a/b/releases/latest/download/latest.json");

    CHECK(lwf::SplitHttpsUrl("https://bare-host.example", parts));
    CHECK(parts.host == "bare-host.example");
    CHECK(parts.path == "/");

    CHECK_FALSE(lwf::SplitHttpsUrl("http://example.com/x", parts));
    CHECK_FALSE(lwf::SplitHttpsUrl("ftp://example.com/x", parts));
    CHECK_FALSE(lwf::SplitHttpsUrl("https://", parts));
    CHECK_FALSE(lwf::SplitHttpsUrl("", parts));
    CHECK_FALSE(lwf::SplitHttpsUrl("https://user@example.com/x", parts));
}

TEST_CASE("VersionCompare orders semver numerically") {
    CHECK(lwf::VersionCompare("1.0.0", "1.0.0") == 0);
    CHECK(lwf::VersionCompare("0.1.0", "0.2.0") < 0);
    CHECK(lwf::VersionCompare("0.2.0", "0.1.9") > 0);
    CHECK(lwf::VersionCompare("0.10.0", "0.9.0") > 0);  // numeric, not lexical
    CHECK(lwf::VersionCompare("1.0", "1.0.0") == 0);    // missing parts pad 0
    CHECK(lwf::VersionCompare("v1.2.3", "1.2.3") == 0);  // leading v ignored
    CHECK(lwf::VersionCompare("V0.1.0", "v0.1.1") < 0);
    CHECK(lwf::VersionCompare("1.2.3-beta", "1.2.3") == 0);  // suffix ignored
    CHECK(lwf::VersionCompare("garbage", "0.0.1") < 0);      // garbage = 0
    CHECK(lwf::VersionCompare("vX", "0.0.0") == 0);
}

TEST_CASE("EvaluateLatestJson evaluates the release catalog") {
    const lwf::UpdateCheckResult newer = lwf::EvaluateLatestJson(
        R"({"version": "0.2.0", "installer_url": "https://github.com/a/b/releases/download/v0.2.0/EchoFlowSetup-0.2.0.exe"})",
        "0.1.0");
    CHECK(newer.ok);
    CHECK(newer.update_available);
    CHECK(newer.latest_version == "0.2.0");
    CHECK(newer.download_url ==
          "https://github.com/a/b/releases/download/v0.2.0/EchoFlowSetup-0.2.0.exe");

    const lwf::UpdateCheckResult same = lwf::EvaluateLatestJson(
        R"({"version": "v0.1.0", "portable_url": "https://github.com/a/b/x.zip"})", "0.1.0");
    CHECK(same.ok);
    CHECK_FALSE(same.update_available);
    // No installer asset -> the portable asset; both missing -> releases page.
    CHECK(same.download_url == "https://github.com/a/b/x.zip");

    const lwf::UpdateCheckResult no_assets =
        lwf::EvaluateLatestJson(R"({"version": "0.1.0"})", "0.1.0");
    CHECK(no_assets.ok);
    CHECK_FALSE(no_assets.update_available);
    CHECK(no_assets.download_url == lwf::kReleasesPageUrl);

    const lwf::UpdateCheckResult older =
        lwf::EvaluateLatestJson(R"({"version": "0.0.9"})", "0.1.0");
    CHECK(older.ok);
    CHECK_FALSE(older.update_available);

    // Non-https asset URLs fall back to the releases page.
    const lwf::UpdateCheckResult evil = lwf::EvaluateLatestJson(
        R"({"version": "9.9.9", "installer_url": "http://evil.example/x.exe"})", "0.1.0");
    CHECK(evil.ok);
    CHECK(evil.update_available);
    CHECK(evil.download_url == lwf::kReleasesPageUrl);

    // Unusable bodies fail with an error, never with update_available.
    const lwf::UpdateCheckResult bad_json = lwf::EvaluateLatestJson("{ broken", "0.1.0");
    CHECK_FALSE(bad_json.ok);
    CHECK_FALSE(bad_json.update_available);
    CHECK_FALSE(bad_json.error.empty());

    const lwf::UpdateCheckResult no_version =
        lwf::EvaluateLatestJson(R"({"installer_url": "https://x/y"})", "0.1.0");
    CHECK_FALSE(no_version.ok);
    CHECK_FALSE(no_version.update_available);

    const lwf::UpdateCheckResult html_body =
        lwf::EvaluateLatestJson("<html>404</html>", "0.1.0");
    CHECK_FALSE(html_body.ok);
    CHECK_FALSE(html_body.update_available);
}
