#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>

struct DictationRecord {
    std::int64_t timestamp_local = 0;
    // Mic-open capture window: hotkey press -> release (the speaking time).
    std::int64_t duration_ms = 0;
    // End-to-end session: capture + transcription + text injection.
    // Insights divides by this for the "words written per minute" average;
    // 0 means "not tracked by this build" (legacy records).
    std::int64_t session_ms = 0;
    std::string final_text;
    std::string raw_text;
    std::string app_profile;
    std::string process_name;
    std::int64_t words_corrected = 0;
    std::int64_t dictionary_fixes = 0;
};

struct DayStats {
    std::int64_t words = 0;
    std::int64_t dictations = 0;
    // Accumulated mic-open capture time (press -> release).
    std::int64_t speaking_ms = 0;
    // Accumulated end-to-end session time (capture + transcription +
    // injection). Absent in stats.json files written before this field
    // existed; consumers fall back to speaking_ms there.
    std::int64_t session_ms = 0;
    std::int64_t words_corrected = 0;
    std::int64_t dictionary_fixes = 0;
    std::map<std::string, std::int64_t> by_category;
    std::map<std::string, std::int64_t> apps;
};

class StatsStore {
public:
    static std::string DateKey(std::int64_t timestamp_local);
    static std::int64_t CountWords(const std::string& text);

    explicit StatsStore(std::string path, bool enabled = true);
    void Record(const DictationRecord& record);
    void Clear();
    void SetUsageStatsEnabled(bool enabled);
    bool UsageStatsEnabled() const;
    std::map<std::string, DayStats> Snapshot() const;
    std::uint64_t Generation() const noexcept;

private:
    void LoadLocked();
    bool PersistLocked();
    bool EvictLocked();

    mutable std::mutex mutex_;
    std::string path_;
    bool enabled_ = true;
    std::map<std::string, DayStats> days_;
    std::atomic<std::uint64_t> generation_{0};
};
