#pragma once

#include "core/ITextInjector.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class ScratchpadBuffer {
public:
    void Append(const std::string& text);
    std::string Text() const;
    void Clear();

private:
    mutable std::mutex mutex_;
    std::string text_;
};

class ScratchpadInjector final : public ITextInjector {
public:
    explicit ScratchpadInjector(ScratchpadBuffer& buffer);

    void InjectText(const std::string& text) override;

private:
    ScratchpadBuffer& buffer_;
};

class NotesStore {
public:
    struct Note {
        std::string id;
        std::string title;
        std::string body;
        std::int64_t created_at = 0;
        std::int64_t modified_at = 0;
        bool pinned = false;
    };

    explicit NotesStore(std::string data_dir);

    bool Reload();
    std::optional<Note> Create(const std::string& title, const std::string& body);
    bool Update(const std::string& id, const std::string& title, const std::string& body);
    std::optional<Note> Delete(const std::string& id);
    bool Restore(const Note& note);
    bool Pin(const std::string& id);
    bool Unpin(const std::string& id);
    std::vector<Note> List() const;
    std::vector<Note> Search(const std::string& query) const;
    std::optional<Note> Get(const std::string& id) const;
    std::size_t Size() const;

private:
    static std::int64_t NowSeconds();
    bool LoadLocked();
    bool WriteIndexLocked();
    bool WriteBodyLocked(const std::string& id, const std::string& body);
    bool RemoveBodyLocked(const std::string& id);
    std::string BodyPathLocked(const std::string& id) const;
    std::string GenerateIdLocked();

    mutable std::mutex mutex_;
    std::string notes_dir_;
    std::string index_path_;
    std::vector<Note> notes_;
};
