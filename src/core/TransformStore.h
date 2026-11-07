#pragma once

#include "core/HotkeyBinding.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class TransformStore {
public:
    struct Entry {
        int slot = 0;
        std::string name;
        std::string prompt;
        std::string shortcut;
        bool built_in = false;
        std::int64_t created_at = 0;
        std::int64_t updated_at = 0;
    };

    static constexpr int kSlotCount = 10;
    static constexpr std::size_t kMaxPromptChars = 2000;

    TransformStore();

    bool Load(const std::string& path);
    bool Save(const std::string& path) const;
    bool Save() const;

    std::vector<Entry> Entries() const;
    std::vector<std::string> Warnings() const;
    bool Loaded() const;

    bool Enabled() const;
    void SetEnabled(bool enabled);
    std::string DiffShortcut() const;
    void SetDiffShortcut(std::string shortcut);

    void SetHotkeyBindings(std::vector<Binding> bindings);
    bool Conflicted(int slot) const;
    bool IsSlotEnabled(int slot) const;

    std::string Validate(int slot, const std::string& name, const std::string& prompt) const;
    bool SetSlot(int slot, const std::string& name, const std::string& prompt,
                 std::string& error);
    bool SetSlot(const Entry& entry, std::string& error);

    static std::string DefaultShortcutForSlot(int slot);
    static std::string DefaultPolishPrompt();
    static std::string DefaultPromptEngineerPrompt();
    static std::string EnsureTextPlaceholder(std::string prompt);
    static bool ShortcutsConflict(const std::string& shortcut,
                                  const std::vector<Binding>& bindings);

private:
    std::string ValidateLocked(int slot, const std::string& name,
                               const std::string& prompt) const;
    void SeedDefaults();
    void RecomputeConflicts();
    void EnsureAllSlotsLocked();
    static std::int64_t NowSeconds();

    mutable std::mutex mutex_;
    std::vector<Entry> entries_;
    std::vector<std::string> warnings_;
    std::vector<std::string> load_warnings_;
    std::vector<bool> conflicted_;
    std::vector<Binding> bindings_;
    std::string diff_shortcut_ = "Win+Alt+O";
    std::string path_;
    bool enabled_ = false;
    bool loaded_ = false;
};
