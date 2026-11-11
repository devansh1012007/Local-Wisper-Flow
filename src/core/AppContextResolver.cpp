#include "core/AppContextResolver.h"
#include "core/Log.h"

#include <nlohmann/json.hpp>
#include <regex>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <initializer_list>

namespace {

std::string ToLower(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

const AppProfile& FallbackProfile() {
    static const AppProfile fallback;
    return fallback;
}

bool ContainsAny(const std::string& haystack, std::initializer_list<const char*> needles) {
    for (const char* needle : needles) {
        if (haystack.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

}

bool AppContextResolver::Load(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::error_code rc_ec;
    if (!std::filesystem::is_regular_file(path, rc_ec) || rc_ec) {
        return false;
    }
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    nlohmann::json json;
    try {
        file >> json;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
    if (!json.is_object()) {
        return false;
    }

    std::vector<Rule> rules;
    std::unordered_map<std::string, AppProfile> profiles;
    warnings_.clear();

    if (json.contains("profiles") && json["profiles"].is_object()) {
        for (auto it = json["profiles"].begin(); it != json["profiles"].end(); ++it) {
            AppProfile profile;
            profile.name = it.key();
            const auto& value = it.value();
            if (value.contains("prompt_addendum") && value["prompt_addendum"].is_string()) {
                profile.prompt_addendum = value["prompt_addendum"].get<std::string>();
            }
            if (value.contains("casual") && value["casual"].is_boolean()) {
                profile.casual = value["casual"].get<bool>();
            }
            if (value.contains("trailing_period") && value["trailing_period"].is_boolean()) {
                profile.trailing_period = value["trailing_period"].get<bool>();
            }
            if (value.contains("use_llm") && value["use_llm"].is_boolean()) {
                profile.use_llm = value["use_llm"].get<bool>();
            }
            if (value.contains("paste_chord") && value["paste_chord"].is_string()) {
                profile.paste_chord = value["paste_chord"].get<std::string>();
            }
            if (value.contains("category") && value["category"].is_string()) {
                profile.category = value["category"].get<std::string>();
            }
            profiles[it.key()] = std::move(profile);
        }
    }

    if (json.contains("rules") && json["rules"].is_array()) {
        for (const auto& element : json["rules"]) {
            if (!element.is_object() || !element.contains("match") || !element.contains("profile")) {
                continue;
            }
            Rule rule;
            rule.profile = element["profile"].get<std::string>();
            if (element.contains("disabled") && element["disabled"].is_boolean()) {
                rule.disabled = element["disabled"].get<bool>();
            }
            const auto& match = element["match"];
            if (match.contains("process") && match["process"].is_string()) {
                rule.process_regex = match["process"].get<std::string>();
            }
            if (match.contains("title_contains") && match["title_contains"].is_string()) {
                rule.title_contains = match["title_contains"].get<std::string>();
            }
            if (match.contains("title_regex") && match["title_regex"].is_string()) {
                rule.title_regex = match["title_regex"].get<std::string>();
            }
            bool valid = rule.profile.empty() == false &&
                         (!rule.process_regex.empty() || !rule.title_contains.empty() ||
                          !rule.title_regex.empty());
            if (valid && !rule.process_regex.empty()) {
                try {
                    std::regex re(rule.process_regex, std::regex::icase);
                    (void)re;
                } catch (const std::regex_error&) {
                    warnings_.push_back("rule for profile '" + rule.profile +
                                        "' disabled: invalid process regex");
                    valid = false;
                }
            }
            if (valid && !rule.title_regex.empty()) {
                try {
                    std::regex re(rule.title_regex, std::regex::icase);
                    (void)re;
                } catch (const std::regex_error&) {
                    warnings_.push_back("rule for profile '" + rule.profile +
                                        "' disabled: invalid title regex");
                    valid = false;
                }
            }
            if (!valid) {
                rule.disabled = true;
                continue;
            }
            rules.push_back(std::move(rule));
        }
    }

    rules_ = std::move(rules);
    profiles_ = std::move(profiles);
    if (profiles_.find("default") == profiles_.end()) {
        profiles_["default"] = AppProfile{("default")};
    }
    loaded_ = true;
    path_ = path;
    return true;
}

void AppContextResolver::UseBuiltInProfiles() {
    std::lock_guard<std::mutex> lock(mutex_);
    rules_.clear();
    profiles_.clear();
    warnings_.clear();

    profiles_["email"] = AppProfile{
        "email",
        "This is dictated email text. Keep a friendly professional tone and a short greeting.",
        false, true, true, "ctrl+v"};
    profiles_["email"].category = "Emails";
    profiles_["chat"] = AppProfile{
        "chat",
        "This is a casual chat message. Keep it short and conversational, no formal sign-off.",
        true, false, true, "ctrl+v"};
    profiles_["chat"].category = "Work messages";
    profiles_["code"] = AppProfile{
        "code",
        "This is code or technical text. Do not capitalize identifiers, do not add a trailing "
        "period, do not rewrite code, only transcribe faithfully.",
        false, false, true, "ctrl+shift+v"};
    profiles_["code"].category = "Other tasks";
    profiles_["notes"] = AppProfile{
        "notes", "This is a personal note. Keep it concise.", false, true, true, "ctrl+v"};
    profiles_["notes"].category = "Documents";
    profiles_["default"] = AppProfile{"default"};
    profiles_["default"].category = "Other tasks";

    const auto add_rule = [this](const std::string& process, const std::string& title,
                                 const std::string& profile) {
        Rule rule;
        rule.process_regex = process;
        rule.title_contains = title;
        rule.profile = profile;
        rules_.push_back(std::move(rule));
    };
    const auto add_title_regex_rule = [this](const std::string& title, const std::string& profile) {
        Rule rule;
        rule.title_regex = title;
        rule.profile = profile;
        rules_.push_back(std::move(rule));
    };
    add_rule("outlook\\.exe", "", "email");
    add_title_regex_rule("gmail", "email");
    add_rule("slack|discord|teams|whatsapp", "", "chat");
    add_rule("code\\.exe|devenv\\.exe|cursor|windowsterminal|cmd\\.exe|powershell|conhost",
             "", "code");
    add_title_regex_rule("slack|discord", "chat");
    add_rule("notepad|onenote", "", "notes");
    loaded_ = true;
}

bool AppContextResolver::Save() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (path_.empty()) {
        return false;
    }
    nlohmann::json json;
    nlohmann::json profiles = nlohmann::json::object();
    for (const auto& kv : profiles_) {
        nlohmann::json p;
        p["prompt_addendum"] = kv.second.prompt_addendum;
        p["casual"] = kv.second.casual;
        p["trailing_period"] = kv.second.trailing_period;
        p["use_llm"] = kv.second.use_llm;
        p["paste_chord"] = kv.second.paste_chord;
        p["category"] = kv.second.category;
        profiles[kv.first] = std::move(p);
    }
    json["profiles"] = std::move(profiles);

    nlohmann::json rules = nlohmann::json::array();
    for (const auto& rule : rules_) {
        nlohmann::json r;
        nlohmann::json match = nlohmann::json::object();
        if (!rule.process_regex.empty()) {
            match["process"] = rule.process_regex;
        }
        if (!rule.title_contains.empty()) {
            match["title_contains"] = rule.title_contains;
        }
        if (!rule.title_regex.empty()) {
            match["title_regex"] = rule.title_regex;
        }
        r["match"] = std::move(match);
        r["profile"] = rule.profile;
        r["disabled"] = rule.disabled;
        rules.push_back(std::move(r));
    }
    json["rules"] = std::move(rules);

    std::string content;
    try {
        content = json.dump(2);
    } catch (const nlohmann::json::exception&) {
        return false;
    }
    return lwf::WriteAtomic(path_, content);
}

bool AppContextResolver::AssignCategory(const std::string& profile_name,
                                       const std::string& category) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = profiles_.find(profile_name);
    if (it == profiles_.end()) {
        return false;
    }
    const auto& names = CategoryNames();
    if (std::find(names.begin(), names.end(), category) == names.end()) {
        return false;
    }
    it->second.category = category;
    return true;
}

bool AppContextResolver::SetEnabled(const std::string& profile_name, bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = profiles_.find(profile_name);
    if (it == profiles_.end()) {
        return false;
    }
    bool matched = false;
    for (auto& rule : rules_) {
        if (rule.profile == profile_name) {
            rule.disabled = !enabled;
            matched = true;
        }
    }
    // Return true if profile exists, regardless of rules (per spec).
    (void)matched;
    return true;
}

bool AppContextResolver::ProfileEnabled(const std::string& profile_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = profiles_.find(profile_name);
    if (it == profiles_.end()) {
        return false;
    }
    bool has_rule = false;
    bool any_enabled = false;
    for (const auto& rule : rules_) {
        if (rule.profile == profile_name) {
            has_rule = true;
            if (!rule.disabled) {
                any_enabled = true;
            }
        }
    }
    if (!has_rule) {
        return true;
    }
    return any_enabled;
}

std::vector<std::string> AppContextResolver::ListProfiles() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> out;
    out.reserve(profiles_.size());
    for (const auto& kv : profiles_) {
        out.push_back(kv.first);
    }
    return out;
}

bool AppContextResolver::Reload() {
    std::string p;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (path_.empty()) {
            return false;
        }
        p = path_;
    }
    return Load(p);
}

const std::string& AppContextResolver::Path() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return path_;
}

const AppProfile& AppContextResolver::Resolve(const std::string& process_name,
                                              const std::string& window_title) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const Rule& rule : rules_) {
        if (rule.disabled) {
            continue;
        }
        bool matched = true;
        if (!rule.process_regex.empty()) {
            try {
                matched = matched && std::regex_search(process_name, std::regex(
                    rule.process_regex, std::regex::icase));
            } catch (const std::regex_error&) {
                matched = false;
            }
        }
        if (matched && !rule.title_contains.empty()) {
            matched = ToLower(window_title).find(ToLower(rule.title_contains)) !=
                      std::string::npos;
        }
        if (matched && !rule.title_regex.empty()) {
            try {
                matched = matched && std::regex_search(window_title, std::regex(
                    rule.title_regex, std::regex::icase));
            } catch (const std::regex_error&) {
                matched = false;
            }
        }
        if (matched) {
            const auto it = profiles_.find(rule.profile);
            if (it != profiles_.end()) {
                return it->second;
            }
        }
    }
    const auto it = profiles_.find("default");
    return it != profiles_.end() ? it->second : FallbackProfile();
}

const AppProfile& AppContextResolver::ProfileByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = profiles_.find(name);
    return it != profiles_.end() ? it->second : FallbackProfile();
}

const std::vector<std::string>& AppContextResolver::Warnings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return warnings_;
}

bool AppContextResolver::Loaded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loaded_;
}

const std::vector<std::string>& AppContextResolver::CategoryNames() {
    static const std::vector<std::string> names = {"AI prompts", "Emails", "Work messages",
                                                   "Personal", "Documents", "Other tasks"};
    return names;
}

std::string AppContextResolver::CategoryFor(const std::string& process_name,
                                            const std::string& window_title) {
    const std::string process = ToLower(process_name);
    const std::string title = ToLower(window_title);
    if (!process.empty() &&
        ContainsAny(process, {"chatgpt", "claude", "cursor", "gemini", "perplexity"})) {
        return "AI prompts";
    }
    if (!process.empty() && ContainsAny(process, {"outlook", "thunderbird", "gmail"})) {
        return "Emails";
    }
    if (!process.empty() && ContainsAny(process, {"slack", "teams"})) {
        return "Work messages";
    }
    if (!process.empty() && ContainsAny(process, {"whatsapp", "telegram", "messages"})) {
        return "Personal";
    }
    if (!process.empty() && ContainsAny(process, {"winword", "word.exe", "notion", "obsidian"})) {
        return "Documents";
    }
    if (!title.empty() &&
        ContainsAny(title, {"chatgpt", "claude", "cursor", "gemini", "perplexity"})) {
        return "AI prompts";
    }
    if (!title.empty() && ContainsAny(title, {"outlook", "thunderbird", "gmail"})) {
        return "Emails";
    }
    if (!title.empty() && ContainsAny(title, {"slack", "microsoft teams", "teams"})) {
        return "Work messages";
    }
    if (!title.empty() && ContainsAny(title, {"whatsapp", "telegram", "messages"})) {
        return "Personal";
    }
    if (!title.empty() &&
        ContainsAny(title, {"docs.google", "google docs", "notion", "obsidian"})) {
        return "Documents";
    }
    return "Other tasks";
}

std::string AppContextResolver::BuildCleanupPrompt(const std::string& base,
                                                   const AppProfile& profile) {
    if (profile.prompt_addendum.empty() && profile.name != "code") {
        return base;
    }
    std::string prompt = base;
    if (!profile.prompt_addendum.empty()) {
        prompt += "\n";
        prompt += profile.prompt_addendum;
    }
    if (profile.name == "code") {
        prompt += "\nNo auto-capitalization. No trailing period. Minimal rewriting.";
    } else if (profile.casual) {
        prompt += "\nKeep the tone casual.";
        if (!profile.trailing_period) {
            prompt += " Do not add a trailing period.";
        }
    }
    return prompt;
}
