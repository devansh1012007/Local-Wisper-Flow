#include "core/AppContextResolver.h"
#include "core/Log.h"

#include <nlohmann/json.hpp>
#include <regex>

#include <algorithm>
#include <fstream>

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

}

bool AppContextResolver::Load(const std::string& path) {
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
            rule.disabled = !valid;
            if (!valid) {
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
    return true;
}

void AppContextResolver::UseBuiltInProfiles() {
    rules_.clear();
    profiles_.clear();
    warnings_.clear();

    profiles_["email"] = AppProfile{
        "email",
        "This is dictated email text. Keep a friendly professional tone and a short greeting.",
        false, true, true, "ctrl+v"};
    profiles_["chat"] = AppProfile{
        "chat",
        "This is a casual chat message. Keep it short and conversational, no formal sign-off.",
        true, false, true, "ctrl+v"};
    profiles_["code"] = AppProfile{
        "code",
        "This is code or technical text. Do not capitalize identifiers, do not add a trailing "
        "period, do not rewrite code, only transcribe faithfully.",
        false, false, true, "ctrl+shift+v"};
    profiles_["notes"] = AppProfile{
        "notes", "This is a personal note. Keep it concise.", false, true, true, "ctrl+v"};
    profiles_["default"] = AppProfile{"default"};

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

const AppProfile& AppContextResolver::Resolve(const std::string& process_name,
                                              const std::string& window_title) const {
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
    const auto it = profiles_.find(name);
    return it != profiles_.end() ? it->second : FallbackProfile();
}

const std::vector<std::string>& AppContextResolver::Warnings() const {
    return warnings_;
}

bool AppContextResolver::Loaded() const {
    return loaded_;
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
