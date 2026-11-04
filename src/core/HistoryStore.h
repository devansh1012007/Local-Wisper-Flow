#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct HistoryEntry {
    std::int64_t timestamp = 0;
    std::string raw;
    std::string text;
    std::string app_profile;
    std::int64_t duration_ms = 0;
    std::string language;
};

class HistoryStore {
public:
    HistoryStore(std::string path, bool enabled, bool keep_raw, int max_entries,
                 int max_age_days);

    void SetEnabled(bool enabled);
    bool Enabled() const;

    void Append(const HistoryEntry& entry);
    std::vector<HistoryEntry> Search(const std::string& substring, std::size_t limit) const;
    std::optional<HistoryEntry> Last() const;
    void Clear();
    bool Export(const std::string& path) const;
    std::size_t Size() const;

private:
    void LoadAllLocked();
    void CompactLocked(std::int64_t now_seconds);
    std::int64_t NowSeconds() const;

    mutable std::mutex mutex_;
    std::string path_;
    bool enabled_;
    bool keep_raw_;
    int max_entries_;
    int max_age_days_;
    std::vector<HistoryEntry> entries_;
    std::size_t appends_since_compaction_ = 0;
};
