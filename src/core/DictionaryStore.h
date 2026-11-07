#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class DictionaryStore {
public:
    enum class Source { Manual, Imported };

    struct Entry {
        std::int64_t id = 0;
        std::string phrase;
        std::string replacement;
        bool has_replacement = false;
        bool starred = false;
        Source source = Source::Manual;
        std::int64_t usage_count = 0;
        std::int64_t created_at = 0;
        std::int64_t updated_at = 0;
    };

    static constexpr std::size_t kMaxEntries = 2000;

    explicit DictionaryStore(std::string path);

    bool Load();
    bool Reload();
    bool Save();

    std::vector<Entry> Entries() const;
    std::vector<Entry> Search(const std::string& query) const;

    bool Add(const std::string& phrase, const std::string& replacement, std::string* error);
    bool Update(std::int64_t id, const std::string& phrase, const std::string& replacement,
                std::string* error);
    bool Remove(std::int64_t id);
    bool SetStarred(std::int64_t id, bool starred);
    bool RecordUsage(std::int64_t id);

    std::string ExportJson() const;
    bool ImportJson(const std::string& content, std::string* error);
    bool ExportFile(const std::string& path) const;
    bool ImportFile(const std::string& path, std::string* error);

    bool Loaded() const;
    std::size_t Size() const;
    const std::string& Path() const;

private:
    std::int64_t NowSeconds() const;
    void SeedLocked();

    mutable std::mutex mutex_;
    std::string path_;
    std::vector<Entry> entries_;
    std::int64_t next_id_ = 1;
    bool loaded_ = false;
    bool migrated_ = false;
};
