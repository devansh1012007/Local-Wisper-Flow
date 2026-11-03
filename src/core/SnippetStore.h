#pragma once

#include <string>
#include <vector>

class SnippetStore {
public:
    struct Entry {
        std::string trigger;
        std::string expansion;
    };

    bool Load(const std::string& path);
    void SetEntries(std::vector<Entry> entries);
    void SetMode(std::string mode);

    const std::vector<Entry>& Entries() const;
    const std::vector<std::string>& Warnings() const;
    bool Loaded() const;

    std::string Expand(const std::string& text) const;

    static std::string Normalize(const std::string& text);

private:
    std::vector<Entry> entries_;
    std::vector<std::string> warnings_;
    std::string mode_ = "whole";
    bool loaded_ = false;
};
