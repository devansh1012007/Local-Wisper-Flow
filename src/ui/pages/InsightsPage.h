#pragma once

#include "core/InsightsModel.h"
#include "core/StatsStore.h"
#include "ui/theme/ThemePalette.h"

#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace lwf::ui::insights {

inline constexpr const char* kPageTitle = "Insights";
inline constexpr const char* kSpokenCaption = "WORDS SPOKEN PER MINUTE";
inline constexpr const char* kSpokenTooltip = "Your average speaking speed while dictating.";
inline constexpr const char* kWrittenCaption = "WORDS WRITTEN PER MINUTE";
inline constexpr const char* kWrittenTooltip =
    "How fast your dictated words reach the page, transcription time included.";
inline constexpr const char* kEmptyWpm = "—";
inline constexpr const char* kEmptyHint = "No dictations yet. Hold Ctrl + Win to start.";
inline constexpr const char* kFloorHint = "Averages appear after your first minute of dictation.";

inline std::string WpmLabel(const std::optional<double>& wpm) {
    if (!wpm.has_value()) {
        return kEmptyWpm;
    }
    return std::to_string(static_cast<long long>(std::llround(*wpm)));
}

inline std::int64_t Clamped(std::int64_t value) {
    return value < 0 ? 0 : value;
}

struct InsightsSummary {
    std::int64_t total_words = 0;
    std::int64_t total_speaking_ms = 0;
    std::int64_t total_session_ms = 0;
    std::optional<double> spoken_wpm;
    std::optional<double> written_wpm;
    bool has_data = false;
};

inline InsightsSummary SummarizeSnapshot(const std::map<std::string, DayStats>& snapshot) {
    InsightsSummary summary;
    for (const auto& entry : snapshot) {
        summary.total_words += Clamped(entry.second.words);
        summary.total_speaking_ms += Clamped(entry.second.speaking_ms);
        summary.total_session_ms += Clamped(entry.second.session_ms);
    }
    // The written rate divides by the end-to-end session time. Days recorded
    // before session_ms was tracked carry 0; those fall back to the speaking
    // time so the metric stays defined instead of going empty.
    const std::int64_t written_ms =
        summary.total_session_ms > 0 ? summary.total_session_ms : summary.total_speaking_ms;
    summary.spoken_wpm = lwf::Wpm(summary.total_words, summary.total_speaking_ms);
    summary.written_wpm = lwf::Wpm(summary.total_words, written_ms);
    summary.has_data = summary.total_words > 0;
    return summary;
}

class InsightsPage {
public:
    explicit InsightsPage(StatsStore& store) : store_(store) {}

    InsightsPage(const InsightsPage&) = delete;
    InsightsPage& operator=(const InsightsPage&) = delete;

    // The live palette is explicit (T10a/b/c/d pattern). The default keeps
    // any old call site compiling; MainWindow passes *current_palette_.
    void Draw(float scale, const lwf::ui::Palette& palette = lwf::ui::kDarkPalette);

    std::uint64_t LastGeneration() const {
        return last_generation_;
    }

    const InsightsSummary& Summary() const {
        return summary_;
    }

    const std::map<std::string, DayStats>& CachedSnapshot() const {
        return snapshot_;
    }

    bool SyncIfStale() {
        const std::uint64_t generation = store_.Generation();
        if (snapshot_valid_ && generation == last_generation_) {
            return false;
        }
        snapshot_ = store_.Snapshot();
        last_generation_ = generation;
        snapshot_valid_ = true;
        Recompute();
        return true;
    }

    bool SyncFromSnapshot(const std::map<std::string, DayStats>& snapshot) {
        snapshot_ = snapshot;
        last_generation_ = store_.Generation();
        snapshot_valid_ = true;
        Recompute();
        return true;
    }

private:
    void Recompute() {
        summary_ = SummarizeSnapshot(snapshot_);
    }

    void DrawHeader(float scale, const lwf::ui::Palette& palette);
    void DrawRow(float scale, const lwf::ui::Palette& palette);
    void DrawStatCard(const char* id, const std::optional<double>& wpm, const char* caption,
                      const char* tooltip, float width, float scale,
                      const lwf::ui::Palette& palette);

    StatsStore& store_;
    bool snapshot_valid_ = false;
    std::uint64_t last_generation_ = 0;
    std::map<std::string, DayStats> snapshot_;
    InsightsSummary summary_;
};

}
