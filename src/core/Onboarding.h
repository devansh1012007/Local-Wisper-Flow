#pragma once

#include "core/ErrorCode.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Onboarding {

struct ModelDownload {
    std::string name;
    std::string url;
    std::string sha256;
    std::uint64_t size_bytes = 0;
    // Optional display metadata (ModelsPage, followups-6). All fields are
    // optional in models.json: an old catalog without them parses with the
    // defaults below, and the first-run wizard keeps working unchanged.
    std::string display_name;  // falls back to name when empty
    std::string language;      // "en", "hi-hinglish", "multi", ... empty = "other"
    std::string license;       // shown as a chip, e.g. "MIT"
    std::string notes;         // one-line caption under the entry
};

// Display name of a catalog entry (display_name when present, else name).
std::string ModelDisplayName(const ModelDownload& model);

// Human label for a language tag. Unknown/empty tags map to "Other".
std::string LanguageDisplayName(const std::string& language);

// "148 MB" / "1.6 GB" style label (binary units), "unknown size" for 0.
std::string ModelSizeLabel(std::uint64_t size_bytes);

struct ModelCatalog {
    std::vector<ModelDownload> whisper;
    std::vector<ModelDownload> llm;
    std::vector<std::string> warnings;
};

ModelCatalog ParseModelsJson(const std::string& path);

struct ResumePlan {
    bool valid = false;
    std::uint64_t start_offset = 0;
    bool verify_required = true;
    std::string error;
};

ResumePlan PlanResume(std::uint64_t existing_bytes, std::uint64_t remote_size,
                      const std::string& expected_sha256, const std::string& actual_sha256);

// Portable download lifecycle in five states. The model is deliberately
// single-threaded: the transfer worker never touches it. It hands progress
// to the UI through polled atomics and the terminal result through a
// mutex latch; the UI thread then drives these pure transitions, so the
// latch consumer stays trivially thin. All input validation for
// thread-ownership lives with the caller, never inside this model.
enum class DownloadState {
    Idle,
    Downloading,
    Success,
    Cancelled,
    Failed,
};

struct DownloadError {
    ErrorCode code = ErrorCode::Unknown;
    std::string detail;
};

struct DownloadSnapshot {
    DownloadState state = DownloadState::Idle;
    std::uint64_t downloaded = 0;
    std::uint64_t total = 0;
    std::string destination;
    DownloadError error;
};

// Maps a transfer-layer error onto the latch-visible error. The two
// caller-facing download codes are preserved; anything else becomes
// Unknown with the original detail kept for diagnosis.
DownloadError MapDownloadError(ErrorCode code, const std::string& detail);

// The transfer layer (src/os/WindowsDownloader.cpp) throws
// DownloadFailed with the detail "download cancelled by user" when its
// progress callback returns false. This predicate pins that string
// contract in one place; the worker still prefers its cancel flag and
// consults this only as belt and braces.
bool IsCancellationDetail(const std::string& detail);

// A catalog name may be used as a file name only when it carries no path
// separator and cannot climb out of the models directory.
bool IsSafeFileName(const std::string& name);

struct DownloadStartPlan {
    bool skip = false;
    std::uint64_t resume_offset = 0;
};

// Decides whether a download can be skipped because the destination file
// is already complete, using the same PlanResume rule the transfer layer
// runs internally. skip = true means the caller should finish at once
// (success path, no thread, no transfer).
DownloadStartPlan PlanDownloadStart(std::uint64_t existing_bytes, std::uint64_t remote_size,
                                    const std::string& expected_sha256,
                                    const std::string& actual_sha256);

class DownloadModel {
public:
    DownloadModel() = default;

    // Begin a download into `destination`. Refused (false, no change) when
    // already Downloading or the destination is empty.
    bool Start(const std::string& destination, std::uint64_t total);

    // Progress update from the worker. Ignored unless Downloading.
    // Monotonic: a smaller value never lowers the counter; a total above
    // zero clamps the counter to the total.
    void OnProgress(std::uint64_t downloaded);

    // Terminal transitions; each applies only from Downloading.
    void OnSuccess();
    void OnCancelled();
    void OnFailed(const DownloadError& error);

    // Back to Idle from any state; clears counters, destination and error.
    void Reset();

    DownloadSnapshot Snapshot() const;
    bool IsDownloading() const;
    // Fraction of `total` received (0..1), or -1.0 while the total is
    // unknown (indeterminate progress).
    double ProgressFraction() const;

private:
    DownloadSnapshot snapshot_;
};

}
