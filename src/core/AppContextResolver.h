#pragma once

#include "core/AtomicFile.h"
#include "core/IActiveWindow.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct AppProfile {
    std::string name;
    std::string prompt_addendum;
    bool casual = false;
    bool trailing_period = true;
    bool use_llm = true;
    std::string paste_chord = "ctrl+v";
    std::string category = "Other tasks";
};

class AppContextResolver {
public:
    bool Load(const std::string& path);
    void UseBuiltInProfiles();

    bool Save() const;
    bool AssignCategory(const std::string& profile_name, const std::string& category);
    bool SetEnabled(const std::string& profile_name, bool enabled);
    bool ProfileEnabled(const std::string& profile_name) const;
    bool Reload();

    const std::string& Path() const;

    const AppProfile& Resolve(const std::string& process_name,
                              const std::string& window_title) const;
    const AppProfile& ProfileByName(const std::string& name) const;
    const std::vector<std::string>& Warnings() const;
    bool Loaded() const;
    std::vector<std::string> ListProfiles() const;

    static std::string BuildCleanupPrompt(const std::string& base, const AppProfile& profile);
    static std::string CategoryFor(const std::string& process_name,
                                  const std::string& window_title);
    static const std::vector<std::string>& CategoryNames();

private:
    struct Rule {
        std::string process_regex;
        std::string title_contains;
        std::string title_regex;
        std::string profile;
        bool disabled = false;
    };

    std::vector<Rule> rules_;
    std::unordered_map<std::string, AppProfile> profiles_;
    std::vector<std::string> warnings_;
    bool loaded_ = false;
    std::string path_;
    mutable std::mutex mutex_;
};
