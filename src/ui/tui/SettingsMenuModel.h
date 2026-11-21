#pragma once

// Settings menu model for the Linux TUI (plan 2026-11-21, change C07).
//
// Pure presentation/navigation state machine over SettingsStore: no ncurses
// types anywhere, so the whole control plane (group/item navigation, value
// editing, validation, clamping, save state) is unit-testable headless. The
// ncurses frontend (TuiApp) only maps keys to Action values and renders the
// model's snapshots.
//
// Editing contract: values are written into the in-memory store immediately
// and marked dirty; the Save action persists atomically (SettingsStore tmp +
// rename) and sends the daemon a SIGHUP so it reloads without a restart.

#include "core/SettingsStore.h"

#include <optional>
#include <string>
#include <vector>

class SettingsMenuModel {
public:
    // Frontend intents; TuiApp maps ncurses keys onto these.
    enum class Action {
        Up, Down, PageUp, PageDown,
        Enter,          // open group / begin editing / toggle bool
        Back,           // leave group / cancel editor
        Save,           // persist + reload daemon
        ReloadFromDisk, // discard unsaved edits, re-read config.json
        Quit,
    };

    enum class Page { Groups, Items, Editor, Log, Status, Help };

    struct ItemRow {
        std::string key;
        std::string label;
        std::string value;
        SettingsStore::Type type = SettingsStore::Type::String;
        bool hot_apply = false;
    };

    explicit SettingsMenuModel(SettingsStore& store);

    // Drives the state machine; returns a short status-line message for the
    // frontend to display ("" when nothing to say).
    std::string Apply(Action action);

    // --- snapshots for rendering ---
    Page page() const { return page_; }
    const std::vector<std::string>& group_names() const { return group_names_; }
    int group_cursor() const { return group_cursor_; }
    const std::vector<ItemRow>& item_rows() const { return item_rows_; }
    int item_cursor() const { return item_cursor_; }
    const std::string& editor_draft() const { return editor_draft_; }
    bool dirty() const { return dirty_; }
    const std::string& status() const { return status_; }

    // Hint for keysym-valued int keys ("0xffe9 = RightAlt"), "" otherwise.
    std::string KeysymHint() const;

    // Editing entry points for frontends that already hold text.
    void SetEditorText(const std::string& text);
    std::optional<std::string> CommitEditor();

    // Log viewer tail: reads the last lines of a log file (cross-process,
    // the daemon writes it). Empty when unreadable.
    static std::vector<std::string> TailFile(const std::string& path, std::size_t max_lines);

    // Called by the frontend after persisting; keeps dirty in sync.
    void MarkSaved(bool persisted);

private:
    struct GroupDef {
        std::string name;
        std::vector<std::string> keys;
    };

    void BuildGroups();
    void EnterGroup(int index);
    void RefreshItemRows();
    void BeginEdit();
    bool ToggleBool(const ItemRow& row);
    void MoveCursor(int delta, int page_delta);

    SettingsStore& store_;
    std::vector<GroupDef> groups_;
    std::vector<std::string> group_names_;
    std::vector<ItemRow> item_rows_;
    int group_cursor_ = 0;
    int item_cursor_ = 0;
    Page page_ = Page::Groups;
    std::string editor_key_;
    std::string editor_draft_;
    bool dirty_ = false;
    std::string status_;
};
