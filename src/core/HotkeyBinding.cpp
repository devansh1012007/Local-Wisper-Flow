#include "core/HotkeyBinding.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace {

struct NamedKey {
    const char* name;
    std::uint32_t vk;
};

const NamedKey kNamedKeys[] = {
    {"ctrl", HotkeyBinding::kCtrl},     {"control", HotkeyBinding::kCtrl},
    {"shift", HotkeyBinding::kShift},   {"alt", HotkeyBinding::kAlt},
    {"leftalt", HotkeyBinding::kLeftAlt}, {"rightalt", HotkeyBinding::kRightAlt},
    {"win", HotkeyBinding::kWin},       {"windows", HotkeyBinding::kWin},
    {"super", HotkeyBinding::kWin},     {"escape", HotkeyBinding::kEscape},
    {"esc", HotkeyBinding::kEscape},    {"space", HotkeyBinding::kSpace},
    {"tab", HotkeyBinding::kTab},       {"insert", HotkeyBinding::kInsert},
    {"ins", HotkeyBinding::kInsert},    {"capslock", HotkeyBinding::kCapsLock},
    {"scrolllock", HotkeyBinding::kScrollLock}, {"pause", HotkeyBinding::kPause},
};

std::string LowerAscii(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

bool ParseSingleKey(const std::string& token_in, std::uint32_t& out_vk,
                    std::string& out_name) {
    const std::string token = LowerAscii(token_in);
    if (token.empty()) {
        return false;
    }
    for (const NamedKey& named : kNamedKeys) {
        if (token == named.name) {
            out_vk = named.vk;
            out_name = named.name;
            return true;
        }
    }
    if (token.size() == 3 && token[0] == 'f' && std::isdigit(static_cast<unsigned char>(token[1])) &&
        std::isdigit(static_cast<unsigned char>(token[2]))) {
        const int index = std::stoi(token.substr(1));
        if (index >= 1 && index <= 24) {
            out_vk = HotkeyBinding::kF1 + static_cast<std::uint32_t>(index - 1);
            out_name = token;
            return true;
        }
        return false;
    }
    if (token.size() == 2 && token[0] == 'f' &&
        std::isdigit(static_cast<unsigned char>(token[1])) && token[1] != '0') {
        const int index = token[1] - '0';
        out_vk = HotkeyBinding::kF1 + static_cast<std::uint32_t>(index - 1);
        out_name = token;
        return true;
    }
    if (token.size() == 1) {
        const char c = token[0];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out_vk = static_cast<std::uint32_t>(
                std::toupper(static_cast<unsigned char>(c)));
            out_name = token;
            return true;
        }
    }
    if (token.rfind("0x", 0) == 0) {
        try {
            const unsigned long value = std::stoul(token, nullptr, 16);
            if (value <= 0xFF) {
                out_vk = static_cast<std::uint32_t>(value);
                out_name = token;
                return true;
            }
        } catch (const std::exception&) {
            return false;
        }
        return false;
    }
    return false;
}

std::string CanonicalName(std::uint32_t vk) {
    for (const NamedKey& named : kNamedKeys) {
        if (named.vk == vk) {
            std::string name = named.name;
            name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
            return name;
        }
    }
    if (vk >= HotkeyBinding::kF1 && vk < HotkeyBinding::kF1 + 24) {
        return "F" + std::to_string(vk - HotkeyBinding::kF1 + 1);
    }
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) {
        return std::string(1, static_cast<char>(vk));
    }
    std::ostringstream stream;
    stream << "0x" << std::hex << vk;
    return stream.str();
}

}

std::optional<Binding> HotkeyBinding::Parse(const std::string& text, BindingRole role) {
    const std::string trimmed_in = [](const std::string& input) {
        const std::size_t begin = input.find_first_not_of(" \t");
        if (begin == std::string::npos) {
            return std::string();
        }
        const std::size_t end = input.find_last_not_of(" \t");
        return input.substr(begin, end - begin + 1);
    }(text);
    if (trimmed_in.empty() || trimmed_in.front() == '+' || trimmed_in.back() == '+') {
        return std::nullopt;
    }

    std::vector<std::uint32_t> keys;
    std::vector<std::string> names;
    std::istringstream stream(trimmed_in);
    std::string token;
    while (std::getline(stream, token, '+')) {
        std::uint32_t vk = 0;
        std::string name;
        if (!ParseSingleKey(token, vk, name)) {
            return std::nullopt;
        }
        if (std::find(keys.begin(), keys.end(), vk) != keys.end()) {
            return std::nullopt;
        }
        keys.push_back(vk);
        names.push_back(name);
    }
    if (keys.empty()) {
        return std::nullopt;
    }

    std::sort(names.begin(), names.end());
    Binding binding;
    binding.role = role;
    binding.keys = std::move(keys);
    std::string label;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i > 0) {
            label += "+";
        }
        label += names[i];
    }
    binding.label = std::move(label);
    return binding;
}

std::string HotkeyBinding::Serialize(const Binding& binding) {
    std::vector<std::string> names;
    names.reserve(binding.keys.size());
    for (const std::uint32_t vk : binding.keys) {
        names.push_back(CanonicalName(vk));
    }
    std::sort(names.begin(), names.end());
    std::string label;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i > 0) {
            label += "+";
        }
        label += names[i];
    }
    return label;
}

bool HotkeyBinding::IsSatisfied(const Binding& binding,
                                const std::set<std::uint32_t>& pressed) {
    for (const std::uint32_t vk : binding.keys) {
        if (pressed.find(vk) == pressed.end()) {
            return false;
        }
    }
    return true;
}

bool HotkeyBinding::IsModifierVk(std::uint32_t vk) {
    return vk == kCtrl || vk == kShift || vk == kAlt || vk == kWin || vk == kLeftAlt ||
           vk == kRightAlt;
}

bool HotkeyBinding::ChordHasModifier(const Binding& binding) {
    for (const std::uint32_t vk : binding.keys) {
        if (IsModifierVk(vk)) {
            return true;
        }
    }
    return false;
}

namespace {

std::vector<std::uint32_t> SortedUniqueKeys(std::vector<std::uint32_t> keys) {
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    return keys;
}

bool KeySetIsSubset(const std::vector<std::uint32_t>& a,
                    const std::vector<std::uint32_t>& b) {
    return std::includes(b.begin(), b.end(), a.begin(), a.end());
}

}

bool HotkeyBinding::BindingsConflict(const Binding& a, const Binding& b) {
    if (a.keys.empty() || b.keys.empty()) {
        return false;
    }
    const std::vector<std::uint32_t> a_keys = SortedUniqueKeys(a.keys);
    const std::vector<std::uint32_t> b_keys = SortedUniqueKeys(b.keys);
    return KeySetIsSubset(a_keys, b_keys) || KeySetIsSubset(b_keys, a_keys);
}

bool HotkeyBinding::ChordsConflict(const std::string& a, const std::string& b) {
    const auto parsed_a = Parse(a, BindingRole::PushToTalk);
    if (!parsed_a.has_value()) {
        return false;
    }
    const auto parsed_b = Parse(b, BindingRole::PushToTalk);
    if (!parsed_b.has_value()) {
        return false;
    }
    return BindingsConflict(*parsed_a, *parsed_b);
}

std::optional<std::string> HotkeyBinding::FindConflict(
    const std::string& candidate, const std::vector<Binding>& registry,
    std::optional<BindingRole> exclude_role) {
    const auto parsed = Parse(candidate, BindingRole::PushToTalk);
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    for (const Binding& binding : registry) {
        if (exclude_role.has_value() && binding.role == *exclude_role) {
            continue;
        }
        if (BindingsConflict(*parsed, binding)) {
            if (!binding.label.empty()) {
                return binding.label;
            }
            return Serialize(binding);
        }
    }
    return std::nullopt;
}

std::string HotkeyBinding::ValidateRecorderChord(const std::string& candidate,
                                                 const std::vector<Binding>& registry,
                                                 BindingRole target_role) {
    const std::string trimmed_in = [](const std::string& input) {
        const std::size_t begin = input.find_first_not_of(" \t");
        if (begin == std::string::npos) {
            return std::string();
        }
        const std::size_t end = input.find_last_not_of(" \t");
        return input.substr(begin, end - begin + 1);
    }(candidate);
    if (trimmed_in.empty()) {
        return "A shortcut needs a key";
    }
    const auto parsed = Parse(trimmed_in, target_role);
    if (!parsed.has_value()) {
        return "A shortcut needs a key";
    }
    const bool escape_only = parsed->keys.size() == 1 && parsed->keys[0] == kEscape;
    if (!(escape_only && target_role == BindingRole::Cancel)) {
        if (!ChordHasModifier(*parsed)) {
            return "A shortcut needs a modifier key";
        }
    }
    if (FindConflict(trimmed_in, registry, target_role).has_value()) {
        return "That shortcut is already in use";
    }
    return {};
}

std::optional<BindingRole> HotkeyBinding::RoleForConfigKey(const std::string& key) {
    if (key == "hotkey_ptt") {
        return BindingRole::PushToTalk;
    }
    if (key == "hotkey_handsfree") {
        return BindingRole::HandsFreeToggle;
    }
    if (key == "hotkey_cancel") {
        return BindingRole::Cancel;
    }
    if (key == "hotkey_command") {
        return BindingRole::CommandMode;
    }
    if (key == "hotkey_paste_last") {
        return BindingRole::PasteLast;
    }
    return std::nullopt;
}

const char* HotkeyBinding::ConfigKeyForRole(BindingRole role) {
    switch (role) {
        case BindingRole::PushToTalk:
            return "hotkey_ptt";
        case BindingRole::HandsFreeToggle:
            return "hotkey_handsfree";
        case BindingRole::Cancel:
            return "hotkey_cancel";
        case BindingRole::CommandMode:
            return "hotkey_command";
        case BindingRole::PasteLast:
            return "hotkey_paste_last";
    }
    return "hotkey_ptt";
}

bool HotkeyBinding::IsConfigHotkeyKey(const std::string& key) {
    return RoleForConfigKey(key).has_value();
}

const std::vector<std::string>& HotkeyBinding::ConfigHotkeyKeys() {
    static const std::vector<std::string> keys = {"hotkey_ptt", "hotkey_handsfree",
                                                  "hotkey_cancel", "hotkey_command",
                                                  "hotkey_paste_last"};
    return keys;
}

const char* ToString(BindingRole role) {
    switch (role) {
        case BindingRole::PushToTalk: return "PushToTalk";
        case BindingRole::HandsFreeToggle: return "HandsFreeToggle";
        case BindingRole::Cancel: return "Cancel";
        case BindingRole::CommandMode: return "CommandMode";
        case BindingRole::PasteLast: return "PasteLast";
    }
    return "Unknown";
}

std::vector<Binding> LoadBindings(const SettingsStore& settings) {
    std::vector<Binding> bindings;
    std::string ptt = settings.GetString("hotkey_ptt");
    if (ptt.empty()) {
        const int legacy = settings.GetInt("hotkey_code");
        if (legacy == static_cast<int>(HotkeyBinding::kRightAlt)) {
            ptt = "RightAlt";
        } else if (legacy != 0) {
            ptt = "0x" + [] (unsigned int value) {
                std::ostringstream stream;
                stream << std::hex << value;
                return stream.str();
            }(static_cast<unsigned int>(legacy));
        } else {
            ptt = "Ctrl+Win";
        }
    }
    if (auto binding = HotkeyBinding::Parse(ptt, BindingRole::PushToTalk)) {
        bindings.push_back(*binding);
    }
    const std::string handsfree = settings.GetString("hotkey_handsfree");
    if (!handsfree.empty()) {
        if (auto binding = HotkeyBinding::Parse(handsfree, BindingRole::HandsFreeToggle)) {
            bindings.push_back(*binding);
        }
    }
    const std::string cancel = settings.GetString("hotkey_cancel");
    if (!cancel.empty()) {
        if (auto binding = HotkeyBinding::Parse(cancel, BindingRole::Cancel)) {
            bindings.push_back(*binding);
        }
    }
    const std::string command = settings.GetString("hotkey_command");
    if (!command.empty()) {
        if (auto binding = HotkeyBinding::Parse(command, BindingRole::CommandMode)) {
            bindings.push_back(*binding);
        }
    }
    const std::string paste_last = settings.GetString("hotkey_paste_last");
    if (!paste_last.empty()) {
        if (auto binding = HotkeyBinding::Parse(paste_last, BindingRole::PasteLast)) {
            bindings.push_back(*binding);
        }
    }
    std::vector<Binding> unique;
    unique.reserve(bindings.size());
    for (const Binding& binding : bindings) {
        bool conflicts = false;
        for (const Binding& kept : unique) {
            if (HotkeyBinding::BindingsConflict(binding, kept)) {
                conflicts = true;
                break;
            }
        }
        if (!conflicts) {
            unique.push_back(binding);
        }
    }
    return unique;
}
