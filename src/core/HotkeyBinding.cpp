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
    return bindings;
}
