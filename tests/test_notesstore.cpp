#include "core/NotesStore.h"

#include <doctest/doctest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

std::filesystem::path TestDir(const char* name) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / (std::string("lwf_notesstore_") + name);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

void WriteFile(const std::filesystem::path& path, const std::string& content) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream file(path, std::ios::trunc);
    file << content;
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::filesystem::path IndexPath(const std::filesystem::path& dir) {
    return dir / "notes" / "index.json";
}

std::filesystem::path BodyPath(const std::filesystem::path& dir, const std::string& id) {
    return dir / "notes" / (id + ".md");
}

void CheckNoteEqual(const NotesStore::Note& left, const NotesStore::Note& right) {
    CHECK(left.id == right.id);
    CHECK(left.title == right.title);
    CHECK(left.body == right.body);
    CHECK(left.created_at == right.created_at);
    CHECK(left.modified_at == right.modified_at);
    CHECK(left.pinned == right.pinned);
}

}

TEST_CASE("notes store starts empty and create update delete round trip persists") {
    const std::filesystem::path dir = TestDir("crud");
    NotesStore store(dir.string());
    CHECK(store.Size() == 0);
    CHECK(store.List().empty());
    CHECK(store.Get("missing") == std::nullopt);

    const auto created = store.Create("Shopping", "# Groceries\n- Milk");
    REQUIRE(created.has_value());
    CHECK_FALSE(created->id.empty());
    CHECK(created->title == "Shopping");
    CHECK(created->body == "# Groceries\n- Milk");
    CHECK(created->created_at > 0);
    CHECK(created->modified_at >= created->created_at);
    CHECK_FALSE(created->pinned);
    REQUIRE(std::filesystem::exists(IndexPath(dir)));
    REQUIRE(std::filesystem::exists(BodyPath(dir, created->id)));
    CHECK(ReadFile(BodyPath(dir, created->id)) == "# Groceries\n- Milk");

    const auto got = store.Get(created->id);
    REQUIRE(got.has_value());
    CheckNoteEqual(*got, *created);

    REQUIRE(store.Update(created->id, "Errands", "# Errands\n- Bread"));
    const auto after = store.Get(created->id);
    REQUIRE(after.has_value());
    CHECK(after->title == "Errands");
    CHECK(after->body == "# Errands\n- Bread");
    CHECK(after->modified_at >= created->modified_at);
    CHECK(ReadFile(BodyPath(dir, created->id)) == "# Errands\n- Bread");
    CHECK(ReadFile(IndexPath(dir)).find("Errands") != std::string::npos);

    NotesStore reloaded(dir.string());
    const auto persisted = reloaded.Get(created->id);
    REQUIRE(persisted.has_value());
    CheckNoteEqual(*persisted, *after);

    const auto deleted = reloaded.Delete(created->id);
    REQUIRE(deleted.has_value());
    CHECK(deleted->body == "# Errands\n- Bread");
    CHECK(reloaded.Get(created->id) == std::nullopt);
    CHECK(reloaded.Size() == 0);
    CHECK_FALSE(std::filesystem::exists(BodyPath(dir, created->id)));
    CHECK(ReadFile(IndexPath(dir)).find(created->id) == std::string::npos);

    NotesStore again(dir.string());
    CHECK(again.Size() == 0);
}

TEST_CASE("list puts pinned first and pin does not touch modified_at") {
    const std::filesystem::path dir = TestDir("pin");
    NotesStore store(dir.string());
    const auto alpha = store.Create("Alpha", "alpha body");
    const auto beta = store.Create("Beta", "beta body");
    const auto gamma = store.Create("Gamma", "gamma body");
    REQUIRE(alpha.has_value());
    REQUIRE(beta.has_value());
    REQUIRE(gamma.has_value());

    const auto beta_before = store.Get(beta->id);
    REQUIRE(beta_before.has_value());
    CHECK(store.Pin(beta->id));
    const std::vector<NotesStore::Note> pinned = store.List();
    REQUIRE(pinned.size() == 3);
    CHECK(pinned[0].id == beta->id);
    CHECK(pinned[0].pinned);
    CHECK_FALSE(pinned[1].pinned);
    CHECK_FALSE(pinned[2].pinned);

    CHECK(store.Pin(alpha->id));
    const std::vector<NotesStore::Note> both = store.List();
    REQUIRE(both.size() == 3);
    CHECK(both[0].pinned);
    CHECK(both[1].pinned);
    CHECK_FALSE(both[2].pinned);
    CHECK(both[2].id == gamma->id);

    CHECK(store.Unpin(beta->id));
    const auto beta_after = store.Get(beta->id);
    REQUIRE(beta_after.has_value());
    CHECK(beta_after->modified_at == beta_before->modified_at);
    CHECK_FALSE(beta_after->pinned);
    const std::vector<NotesStore::Note> one = store.List();
    REQUIRE(one.size() == 3);
    CHECK(one[0].id == alpha->id);
    CHECK(one[0].pinned);
    CHECK_FALSE(one[1].pinned);
    CHECK_FALSE(one[2].pinned);

    CHECK(store.Unpin(beta->id));
    CHECK(store.Pin(alpha->id));
}

TEST_CASE("search matches title and body case insensitively") {
    const std::filesystem::path dir = TestDir("search");
    NotesStore store(dir.string());
    const auto first = store.Create("Project Alpha", "hello World");
    const auto second = store.Create("Beta", "nothing here");
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());

    const std::vector<NotesStore::Note> by_body = store.Search("HELLO");
    REQUIRE(by_body.size() == 1);
    CHECK(by_body[0].id == first->id);

    const std::vector<NotesStore::Note> by_title = store.Search("alpha");
    REQUIRE(by_title.size() == 1);
    CHECK(by_title[0].id == first->id);

    const std::vector<NotesStore::Note> beta = store.Search("BETA");
    REQUIRE(beta.size() == 1);
    CHECK(beta[0].id == second->id);

    CHECK(store.Search("absent term").empty());
    CHECK(store.Search("").size() == 2);
    CHECK(store.Search("   ").size() == 2);
    CHECK(store.Search("o").size() == 2);
}

TEST_CASE("blank title falls back to first body line then untitled") {
    const std::filesystem::path dir = TestDir("titles");
    NotesStore store(dir.string());

    const auto plain = store.Create("", "First line here\nSecond line");
    REQUIRE(plain.has_value());
    CHECK(plain->title == "First line here");

    const auto padded = store.Create("   ", "\n\n  Padded first  \nrest");
    REQUIRE(padded.has_value());
    CHECK(padded->title == "Padded first");

    const auto blank = store.Create("", "");
    REQUIRE(blank.has_value());
    CHECK(blank->title == "Untitled");

    const auto spaces = store.Create("", "   \n\t\n");
    REQUIRE(spaces.has_value());
    CHECK(spaces->title == "Untitled");

    const auto explicit_title = store.Create("Explicit", "Body first line");
    REQUIRE(explicit_title.has_value());
    CHECK(explicit_title->title == "Explicit");

    REQUIRE(store.Update(plain->id, "", "New first\nrest"));
    CHECK(store.Get(plain->id)->title == "New first");
    REQUIRE(store.Update(plain->id, "Set", "New first\nrest"));
    CHECK(store.Get(plain->id)->title == "Set");

    NotesStore reloaded(dir.string());
    CHECK(reloaded.Get(plain->id)->title == "Set");
    CHECK(reloaded.Get(padded->id)->title == "Padded first");
    CHECK(reloaded.Get(blank->id)->title == "Untitled");
}

TEST_CASE("orphan markdown files are adopted on load and persisted") {
    const std::filesystem::path dir = TestDir("orphan");
    WriteFile(dir / "notes" / "orphan1.md", "Orphan title line\nbody text");
    WriteFile(dir / "notes" / "orphan2.md", "");
    WriteFile(dir / "notes" / "readme.txt", "not a note");
    WriteFile(dir / "notes" / "stale.md.tmp", "partial write");

    NotesStore store(dir.string());
    REQUIRE(store.Size() == 2);
    const auto first = store.Get("orphan1");
    REQUIRE(first.has_value());
    CHECK(first->title == "Orphan title line");
    CHECK(first->body == "Orphan title line\nbody text");
    CHECK_FALSE(first->pinned);
    CHECK(first->created_at > 0);
    CHECK(first->modified_at == first->created_at);
    const auto second = store.Get("orphan2");
    REQUIRE(second.has_value());
    CHECK(second->title == "Untitled");
    CHECK(second->body.empty());

    NotesStore reloaded(dir.string());
    CHECK(reloaded.Size() == 2);
    CHECK(reloaded.Get("orphan1").has_value());
    CHECK(reloaded.Get("orphan2").has_value());
    CHECK(ReadFile(IndexPath(dir)).find("orphan1") != std::string::npos);
}

TEST_CASE("index entries with missing body files are dropped and persisted") {
    const std::filesystem::path dir = TestDir("dangling");
    WriteFile(dir / "notes" / "good.md", "good body");
    WriteFile(IndexPath(dir),
              R"([{"id":"good","title":"Good","created_at":10,"modified_at":11,"pinned":false},{"id":"ghost","title":"Ghost","created_at":20,"modified_at":21,"pinned":true}])");

    NotesStore store(dir.string());
    REQUIRE(store.Size() == 1);
    REQUIRE(store.Get("good").has_value());
    CHECK(store.Get("good")->body == "good body");
    CHECK(store.Get("ghost") == std::nullopt);
    CHECK_FALSE(std::filesystem::exists(dir / "notes" / "ghost.md"));
    CHECK(ReadFile(IndexPath(dir)).find("ghost") == std::string::npos);

    NotesStore reloaded(dir.string());
    CHECK(reloaded.Size() == 1);
}

TEST_CASE("invalid ids in the index never escape the notes directory") {
    const std::filesystem::path dir = TestDir("hostile");
    WriteFile(dir / "evil.md", "outside body");
    WriteFile(dir / "notes" / "good.md", "good body");
    WriteFile(IndexPath(dir),
              R"([{"id":"../evil","title":"Evil","created_at":30,"modified_at":31,"pinned":false},{"id":"good","title":"Good","created_at":10,"modified_at":11,"pinned":false}])");

    NotesStore store(dir.string());
    REQUIRE(store.Size() == 1);
    const std::vector<NotesStore::Note> listed = store.List();
    REQUIRE(listed.size() == 1);
    CHECK(listed[0].id == "good");
    CHECK(listed[0].body == "good body");
    CHECK(store.Get("../evil") == std::nullopt);
}

TEST_CASE("corrupt index moves to bad file and orphan sweep keeps bodies") {
    const std::filesystem::path dir = TestDir("corrupt");
    WriteFile(dir / "notes" / "keep.md", "Recovered line\nrest");
    WriteFile(IndexPath(dir), "not json");

    NotesStore store(dir.string());
    REQUIRE(store.Size() == 1);
    const auto kept = store.Get("keep");
    REQUIRE(kept.has_value());
    CHECK(kept->title == "Recovered line");
    CHECK(kept->body == "Recovered line\nrest");
    CHECK(std::filesystem::exists(dir / "notes" / "index.json.bad"));
    CHECK(ReadFile(dir / "notes" / "index.json.bad") == "not json");

    NotesStore reloaded(dir.string());
    CHECK(reloaded.Size() == 1);
    CHECK(reloaded.Get("keep").has_value());
}

TEST_CASE("delete returns the entry so undo can restore it") {
    const std::filesystem::path dir = TestDir("undo");
    NotesStore store(dir.string());
    const auto created = store.Create("Undo me", "undo body line\nmore");
    REQUIRE(created.has_value());
    REQUIRE(store.Pin(created->id));
    const auto before = store.Get(created->id);
    REQUIRE(before.has_value());

    const auto deleted = store.Delete(created->id);
    REQUIRE(deleted.has_value());
    CheckNoteEqual(*deleted, *before);
    CHECK(deleted->pinned);
    CHECK(store.Get(created->id) == std::nullopt);
    CHECK_FALSE(std::filesystem::exists(BodyPath(dir, created->id)));

    REQUIRE(store.Restore(*deleted));
    const auto restored = store.Get(created->id);
    REQUIRE(restored.has_value());
    CheckNoteEqual(*restored, *before);
    CHECK(std::filesystem::exists(BodyPath(dir, created->id)));

    NotesStore reloaded(dir.string());
    CHECK(reloaded.Get(created->id).has_value());
    CHECK(reloaded.Get(created->id)->body == "undo body line\nmore");

    CHECK_FALSE(store.Restore(*deleted));
    NotesStore::Note invasive;
    invasive.id = "../evil";
    invasive.title = "Evil";
    invasive.body = "evil body";
    CHECK_FALSE(store.Restore(invasive));

    CHECK(store.Delete("missing") == std::nullopt);
    CHECK_FALSE(store.Update("missing", "t", "b"));
    CHECK_FALSE(store.Pin("missing"));
    CHECK_FALSE(store.Unpin("missing"));
}

TEST_CASE("reload picks up a note file added outside the store") {
    const std::filesystem::path dir = TestDir("reload");
    NotesStore store(dir.string());
    const auto first = store.Create("First", "first body");
    REQUIRE(first.has_value());
    WriteFile(dir / "notes" / "late.md", "Late arrival\nbody");
    REQUIRE(store.Reload());
    CHECK(store.Size() == 2);
    CHECK(store.Get("late").has_value());
    CHECK(store.Get("late")->title == "Late arrival");
}

TEST_CASE("concurrent creates stay thread safe and persist every note") {
    const std::filesystem::path dir = TestDir("threads");
    NotesStore store(dir.string());
    std::atomic<int> failures{0};
    const int thread_count = 4;
    const int creates_per_thread = 25;
    const int expected = thread_count * creates_per_thread;
    std::vector<std::thread> workers;
    for (int t = 0; t < thread_count; ++t) {
        workers.emplace_back([&store, &failures, t, creates_per_thread]() {
            for (int i = 0; i < creates_per_thread; ++i) {
                const std::string body = "body-marker-" + std::to_string(t) + "-" + std::to_string(i);
                if (!store.Create("Thread note", body).has_value()) {
                    ++failures;
                }
                if (i % 5 == 0) {
                    (void)store.List();
                    (void)store.Search("marker-");
                    (void)store.Size();
                }
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    CHECK(failures.load() == 0);
    REQUIRE(store.Size() == static_cast<std::size_t>(expected));
    const std::vector<NotesStore::Note> listed = store.List();
    REQUIRE(listed.size() == static_cast<std::size_t>(expected));
    std::set<std::string> ids;
    for (const NotesStore::Note& note : listed) {
        ids.insert(note.id);
        CHECK(std::filesystem::exists(BodyPath(dir, note.id)));
    }
    CHECK(ids.size() == static_cast<std::size_t>(expected));
    CHECK(store.Search("MARKER-").size() == static_cast<std::size_t>(expected));

    NotesStore reloaded(dir.string());
    CHECK(reloaded.Size() == static_cast<std::size_t>(expected));
}
