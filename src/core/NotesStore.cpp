#include "core/NotesStore.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace {

std::string TrimCopy(const std::string& text) {
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::string ToLowerCopy(const std::string& text) {
    std::string lowered;
    lowered.reserve(text.size());
    for (const char c : text) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x80) {
            lowered.push_back(static_cast<char>(std::tolower(u)));
        } else {
            lowered.push_back(c);
        }
    }
    return lowered;
}

std::string DeriveTitleFromBody(const std::string& body) {
    std::size_t start = 0;
    while (start <= body.size()) {
        const std::size_t end = body.find('\n', start);
        const std::string raw = body.substr(start, end - start);
        const std::string trimmed = TrimCopy(raw);
        if (!trimmed.empty()) {
            return trimmed;
        }
        if (end == std::string::npos) {
            break;
        }
        ++start;
        if (start > body.size()) {
            break;
        }
    }
    return {};
}

std::string ResolveTitle(const std::string& title, const std::string& body) {
    const std::string trimmed = TrimCopy(title);
    if (!trimmed.empty()) {
        return trimmed;
    }
    const std::string derived = DeriveTitleFromBody(body);
    if (!derived.empty()) {
        return derived;
    }
    return "Untitled";
}

bool IsValidId(const std::string& id) {
    if (id.empty() || id.size() > 128) {
        return false;
    }
    if (id == "." || id == "..") {
        return false;
    }
    if (id.find_first_of("/\\") != std::string::npos) {
        return false;
    }
    for (const char c : id) {
        if (static_cast<unsigned char>(c) < 0x20) {
            return false;
        }
    }
    return true;
}

std::size_t FindIndex(const std::vector<NotesStore::Note>& notes, const std::string& id) {
    for (std::size_t i = 0; i < notes.size(); ++i) {
        if (notes[i].id == id) {
            return i;
        }
    }
    return notes.size();
}

std::vector<NotesStore::Note> SortedCopy(const std::vector<NotesStore::Note>& notes) {
    std::vector<NotesStore::Note> sorted = notes;
    std::sort(sorted.begin(), sorted.end(),
              [](const NotesStore::Note& left, const NotesStore::Note& right) {
                  if (left.pinned != right.pinned) {
                      return left.pinned > right.pinned;
                  }
                  if (left.modified_at != right.modified_at) {
                      return left.modified_at > right.modified_at;
                  }
                  if (left.created_at != right.created_at) {
                      return left.created_at > right.created_at;
                  }
                  return left.id < right.id;
              });
    return sorted;
}

bool ReadBodyFile(const std::string& path, std::string& body) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    body = stream.str();
    return true;
}

}

NotesStore::NotesStore(std::string data_dir) {
    notes_dir_ = (std::filesystem::path(data_dir) / "notes").string();
    index_path_ = (std::filesystem::path(notes_dir_) / "index.json").string();
    std::lock_guard<std::mutex> lock(mutex_);
    LoadLocked();
}

std::int64_t NotesStore::NowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

bool NotesStore::Reload() {
    std::lock_guard<std::mutex> lock(mutex_);
    return LoadLocked();
}

bool NotesStore::LoadLocked() {
    notes_.clear();
    std::error_code ec;
    if (!std::filesystem::exists(notes_dir_, ec)) {
        return true;
    }
    bool corrupt = false;
    bool dirty = false;
    std::ifstream file(index_path_);
    if (file.is_open()) {
        nlohmann::json json;
        try {
            file >> json;
        } catch (const nlohmann::json::exception&) {
            corrupt = true;
        }
        if (!corrupt && !json.is_array()) {
            corrupt = true;
        }
        if (corrupt) {
            file.close();
            std::error_code rename_ec;
            std::filesystem::rename(index_path_, index_path_ + ".bad", rename_ec);
        } else {
            for (const nlohmann::json& element : json) {
                if (!element.is_object()) {
                    continue;
                }
                if (!element.contains("id") || !element["id"].is_string()) {
                    continue;
                }
                const std::string id = element["id"].get<std::string>();
                if (!IsValidId(id)) {
                    continue;
                }
                if (FindIndex(notes_, id) != notes_.size()) {
                    continue;
                }
                std::string body;
                if (!ReadBodyFile(BodyPathLocked(id), body)) {
                    dirty = true;
                    continue;
                }
                Note note;
                note.id = id;
                note.body = std::move(body);
                note.created_at = 0;
                if (element.contains("created_at") && element["created_at"].is_number()) {
                    note.created_at = element["created_at"].get<std::int64_t>();
                }
                note.modified_at = note.created_at;
                if (element.contains("modified_at") && element["modified_at"].is_number()) {
                    note.modified_at = element["modified_at"].get<std::int64_t>();
                }
                note.pinned = false;
                if (element.contains("pinned") && element["pinned"].is_boolean()) {
                    note.pinned = element["pinned"].get<bool>();
                }
                std::string title;
                if (element.contains("title") && element["title"].is_string()) {
                    title = element["title"].get<std::string>();
                }
                const std::string resolved = ResolveTitle(title, note.body);
                if (resolved != title) {
                    dirty = true;
                }
                note.title = std::move(resolved);
                notes_.push_back(std::move(note));
            }
        }
    }
    file.close();
    const std::int64_t adopted_at = NowSeconds();
    std::error_code iter_ec;
    std::filesystem::directory_iterator iter(notes_dir_, iter_ec);
    if (!iter_ec) {
        for (const std::filesystem::directory_entry& entry : iter) {
            std::error_code type_ec;
            if (!entry.is_regular_file(type_ec) || type_ec) {
                continue;
            }
            const std::filesystem::path path = entry.path();
            if (path.extension() != ".md") {
                continue;
            }
            const std::string stem = path.stem().string();
            if (!IsValidId(stem)) {
                continue;
            }
            if (FindIndex(notes_, stem) != notes_.size()) {
                continue;
            }
            std::string body;
            if (!ReadBodyFile(path.string(), body)) {
                continue;
            }
            Note note;
            note.id = stem;
            note.body = std::move(body);
            note.title = ResolveTitle("", note.body);
            note.created_at = adopted_at;
            note.modified_at = adopted_at;
            note.pinned = false;
            notes_.push_back(std::move(note));
            dirty = true;
        }
    }
    if (dirty || corrupt) {
        WriteIndexLocked();
    }
    return !corrupt;
}

bool NotesStore::WriteIndexLocked() {
    const std::vector<Note> sorted = SortedCopy(notes_);
    nlohmann::json root = nlohmann::json::array();
    for (const Note& note : sorted) {
        nlohmann::json item = nlohmann::json::object();
        item["id"] = note.id;
        item["title"] = note.title;
        item["created_at"] = note.created_at;
        item["modified_at"] = note.modified_at;
        item["pinned"] = note.pinned;
        root.push_back(std::move(item));
    }
    const std::string tmp_path = index_path_ + ".tmp";
    {
        std::ofstream tmp(tmp_path, std::ios::trunc);
        if (!tmp.is_open()) {
            return false;
        }
        tmp << root.dump(4) << '\n';
        tmp.flush();
        if (!tmp.good()) {
            tmp.close();
            std::error_code cleanup_ec;
            std::filesystem::remove(tmp_path, cleanup_ec);
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp_path, index_path_, ec);
    if (ec) {
        std::error_code cleanup_ec;
        std::filesystem::remove(tmp_path, cleanup_ec);
        return false;
    }
    return true;
}

bool NotesStore::WriteBodyLocked(const std::string& id, const std::string& body) {
    std::error_code dir_ec;
    std::filesystem::create_directories(notes_dir_, dir_ec);
    if (dir_ec) {
        return false;
    }
    const std::string path = BodyPathLocked(id);
    const std::string tmp_path = path + ".tmp";
    {
        std::ofstream tmp(tmp_path, std::ios::trunc);
        if (!tmp.is_open()) {
            return false;
        }
        tmp << body;
        tmp.flush();
        if (!tmp.good()) {
            tmp.close();
            std::error_code cleanup_ec;
            std::filesystem::remove(tmp_path, cleanup_ec);
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp_path, path, ec);
    if (ec) {
        std::error_code cleanup_ec;
        std::filesystem::remove(tmp_path, cleanup_ec);
        return false;
    }
    return true;
}

bool NotesStore::RemoveBodyLocked(const std::string& id) {
    std::error_code ec;
    (void)std::filesystem::remove(BodyPathLocked(id), ec);
    return !ec;
}

std::string NotesStore::BodyPathLocked(const std::string& id) const {
    return (std::filesystem::path(notes_dir_) / (id + ".md")).string();
}

std::string NotesStore::GenerateIdLocked() {
    const std::int64_t now = NowSeconds();
    for (std::size_t suffix = 1;; ++suffix) {
        const std::string candidate = "note-" + std::to_string(now) + "-" + std::to_string(suffix);
        if (FindIndex(notes_, candidate) != notes_.size()) {
            continue;
        }
        std::error_code ec;
        if (std::filesystem::exists(BodyPathLocked(candidate), ec)) {
            continue;
        }
        return candidate;
    }
}

std::optional<NotesStore::Note> NotesStore::Create(const std::string& title,
                                                   const std::string& body) {
    std::lock_guard<std::mutex> lock(mutex_);
    Note note;
    note.id = GenerateIdLocked();
    note.title = ResolveTitle(title, body);
    note.body = body;
    note.created_at = NowSeconds();
    note.modified_at = note.created_at;
    note.pinned = false;
    if (!WriteBodyLocked(note.id, note.body)) {
        return std::nullopt;
    }
    notes_.push_back(note);
    if (!WriteIndexLocked()) {
        notes_.erase(notes_.begin() + static_cast<std::ptrdiff_t>(notes_.size() - 1));
        return std::nullopt;
    }
    return note;
}

bool NotesStore::Update(const std::string& id, const std::string& title,
                        const std::string& body) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::size_t pos = FindIndex(notes_, id);
    if (pos == notes_.size()) {
        return false;
    }
    const Note backup = notes_[pos];
    notes_[pos].title = ResolveTitle(title, body);
    notes_[pos].body = body;
    notes_[pos].modified_at = NowSeconds();
    if (!WriteBodyLocked(id, body)) {
        notes_[pos] = backup;
        return false;
    }
    if (!WriteIndexLocked()) {
        WriteBodyLocked(id, backup.body);
        notes_[pos] = backup;
        return false;
    }
    return true;
}

std::optional<NotesStore::Note> NotesStore::Delete(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::size_t pos = FindIndex(notes_, id);
    if (pos == notes_.size()) {
        return std::nullopt;
    }
    const Note backup = notes_[pos];
    if (!RemoveBodyLocked(id)) {
        return std::nullopt;
    }
    notes_.erase(notes_.begin() + static_cast<std::ptrdiff_t>(pos));
    if (!WriteIndexLocked()) {
        WriteBodyLocked(id, backup.body);
        notes_.insert(notes_.begin() + static_cast<std::ptrdiff_t>(pos), backup);
        return std::nullopt;
    }
    return backup;
}

bool NotesStore::Restore(const Note& note) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!IsValidId(note.id)) {
        return false;
    }
    if (FindIndex(notes_, note.id) != notes_.size()) {
        return false;
    }
    Note restored = note;
    restored.title = ResolveTitle(note.title, note.body);
    if (restored.created_at <= 0) {
        restored.created_at = restored.modified_at > 0 ? restored.modified_at : NowSeconds();
    }
    if (restored.modified_at <= 0) {
        restored.modified_at = restored.created_at;
    }
    if (!WriteBodyLocked(restored.id, restored.body)) {
        return false;
    }
    notes_.push_back(restored);
    if (!WriteIndexLocked()) {
        const std::size_t pos = FindIndex(notes_, restored.id);
        if (pos != notes_.size()) {
            notes_.erase(notes_.begin() + static_cast<std::ptrdiff_t>(pos));
        }
        return false;
    }
    return true;
}

bool NotesStore::Pin(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::size_t pos = FindIndex(notes_, id);
    if (pos == notes_.size()) {
        return false;
    }
    if (notes_[pos].pinned) {
        return true;
    }
    notes_[pos].pinned = true;
    if (!WriteIndexLocked()) {
        notes_[pos].pinned = false;
        return false;
    }
    return true;
}

bool NotesStore::Unpin(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::size_t pos = FindIndex(notes_, id);
    if (pos == notes_.size()) {
        return false;
    }
    if (!notes_[pos].pinned) {
        return true;
    }
    notes_[pos].pinned = false;
    if (!WriteIndexLocked()) {
        notes_[pos].pinned = true;
        return false;
    }
    return true;
}

std::vector<NotesStore::Note> NotesStore::List() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return SortedCopy(notes_);
}

std::vector<NotesStore::Note> NotesStore::Search(const std::string& query) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string needle = ToLowerCopy(TrimCopy(query));
    const std::vector<Note> sorted = SortedCopy(notes_);
    if (needle.empty()) {
        return sorted;
    }
    std::vector<Note> hits;
    for (const Note& note : sorted) {
        if (ToLowerCopy(note.title).find(needle) != std::string::npos ||
            ToLowerCopy(note.body).find(needle) != std::string::npos) {
            hits.push_back(note);
        }
    }
    return hits;
}

std::optional<NotesStore::Note> NotesStore::Get(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::size_t pos = FindIndex(notes_, id);
    if (pos == notes_.size()) {
        return std::nullopt;
    }
    return notes_[pos];
}

std::size_t NotesStore::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return notes_.size();
}
