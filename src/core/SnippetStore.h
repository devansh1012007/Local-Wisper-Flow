#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class SnippetStore {
public:
    struct Entry {
        std::string trigger;
        std::string expansion;
        std::string id;
        std::int64_t created_at = 0;
        std::int64_t updated_at = 0;
    };

    static constexpr std::size_t kMaxExpansionChars = 4000;

    bool Load(const std::string& path);
    bool Save(const std::string& path) const;
    bool Save() const;
    void SetEntries(std::vector<Entry> entries);
    void SetMode(std::string mode);

    const std::vector<Entry>& Entries() const;
    const std::vector<std::string>& Warnings() const;
    bool Loaded() const;

    std::string Expand(const std::string& text) const;

    static std::string Normalize(const std::string& text);

    std::string Validate(const std::string& trigger, const std::string& expansion) const;
    std::string Validate(const std::string& trigger, const std::string& expansion,
                         const std::string& exclude_id) const;
    bool Upsert(const std::string& trigger, const std::string& expansion, std::string& error);
    bool Upsert(const std::string& trigger, const std::string& expansion,
                const std::string& exclude_id, std::string& error);
    void RemoveByTrigger(const std::string& trigger);
    void RemoveById(const std::string& id);

private:
    std::string ValidateLocked(const std::string& trigger, const std::string& expansion,
                               const std::string& exclude_id) const;
    std::string GenerateIdLocked() const;
    static std::int64_t NowSeconds();

    mutable std::mutex mutex_;
    std::vector<Entry> entries_;
    std::vector<std::string> warnings_;
    std::string mode_ = "whole";
    std::string path_;
    bool loaded_ = false;
};
