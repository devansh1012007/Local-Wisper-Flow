#include "os/LinuxKeysym.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace lwf::linuxkeys {
namespace {

struct KeysymName {
    std::uint32_t keysym;
    const char* name;
};

// Named keys shared between KeysymDisplayName and ParseKeysymName. Kept in a
// single table so the two directions cannot drift apart.
const KeysymName kNamedKeysyms[] = {
    {XK_Control_L, "Ctrl"},  {XK_Control_R, "Ctrl"},   {XK_Shift_L, "Shift"},
    {XK_Shift_R, "Shift"},   {XK_Alt_L, "LeftAlt"},    {XK_Alt_R, "RightAlt"},
    {XK_Super_L, "Win"},     {XK_Super_R, "Win"},      {XK_Escape, "Escape"},
    {XK_Tab, "Tab"},         {XK_Insert, "Insert"},    {XK_Caps_Lock, "CapsLock"},
    {XK_Scroll_Lock, "ScrollLock"}, {XK_Pause, "Pause"}, {XK_Return, "Enter"},
    {XK_BackSpace, "Backspace"}, {XK_Delete, "Delete"}, {XK_Home, "Home"},
    {XK_End, "End"},         {XK_F1, "F1"},
};

struct EvdevEntry {
    std::uint16_t code;
    std::uint32_t keysym;
};

// Evdev KEY_* -> X11 keysym for the modifiers, navigation cluster, function
// keys and the US printable layout. Values follow linux/input-event-codes.h.
const EvdevEntry kEvdevTable[] = {
    {KEY_ESC, XK_Escape},   {KEY_1, 0x31},         {KEY_TAB, XK_Tab},
    {KEY_Q, 0x51},          {KEY_A, 0x41},         {KEY_Z, 0x5A},
    {KEY_X, 0x58},          {KEY_C, 0x43},         {KEY_V, 0x56},
    {KEY_B, 0x42},          {KEY_N, 0x4E},         {KEY_M, 0x4D},
    {KEY_ENTER, XK_Return}, {KEY_LEFTCTRL, XK_Control_L},
    {KEY_SPACE, 0x20},      {KEY_CAPSLOCK, XK_Caps_Lock},
    {KEY_F1, XK_F1},        {KEY_LEFTSHIFT, XK_Shift_L},
    {KEY_LEFTALT, XK_Alt_L},{KEY_INSERT, XK_Insert},
    {KEY_SCROLLLOCK, XK_Scroll_Lock}, {KEY_PAUSE, XK_Pause},
    {KEY_RIGHTSHIFT, XK_Shift_R}, {KEY_RIGHTCTRL, XK_Control_R},
    {KEY_RIGHTALT, XK_Alt_R}, {KEY_LEFTMETA, XK_Super_L},
    {KEY_RIGHTMETA, XK_Super_R},
    {KEY_HOME, XK_Home},    {KEY_END, XK_End},     {KEY_UP, 0xff52},
    {KEY_DOWN, 0xff54},     {KEY_LEFT, 0xff51},    {KEY_RIGHT, 0xff53},
};

// Digit and letter rows of the US layout: contiguous evdev ranges mapping to
// contiguous keysym ranges, so they are expanded instead of listed.
struct EvdevRange {
    std::uint16_t first_code;
    std::uint16_t last_code;
    std::uint32_t first_keysym;
};

const EvdevRange kEvdevRanges[] = {
    {2, 11, 0x31},    // KEY_1..KEY_0 -> '1'..'0'
    {16, 25, 0x51},   // KEY_Q..KEY_P -> 'Q'..'P'
    {30, 38, 0x41},   // KEY_A..KEY_L -> 'A'..'L'
    {59, 68, XK_F1},  // KEY_F1..KEY_F10
};

// The bottom letter row (Z X C V B N M) is not ASCII-contiguous, so it is
// listed in the table above; F11/F12 sit outside the contiguous F-key range.
const EvdevEntry kEvdevExtra[] = {
    {87, XK_F1 + 10},  // KEY_F11
    {88, XK_F1 + 11},  // KEY_F12
    {111, XK_Delete},
};

std::string LowerAscii(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

}  // namespace

std::uint32_t KeysymToCoreVk(std::uint32_t keysym) {
    switch (keysym) {
        case XK_Control_L:
        case XK_Control_R:
            return kVkCtrl;
        case XK_Shift_L:
        case XK_Shift_R:
            return kVkShift;
        case XK_Alt_R:
            return kVkRightAlt;
        case XK_Super_L:
        case XK_Super_R:
            return kVkWin;
        case XK_Escape:
            return 0x1B;
        case XK_Tab:
            return 0x09;
        case XK_Insert:
            return 0x2D;
        case XK_Caps_Lock:
            return 0x14;
        case XK_Scroll_Lock:
            return 0x91;
        case XK_Pause:
            return 0x13;
        case XK_Return:
            return 0x0D;
        case XK_BackSpace:
            return 0x08;
        case XK_Delete:
            return 0x2E;
        case XK_Home:
            return 0x24;
        case XK_End:
            return 0x23;
        case 0xff52:  // XK_Up
            return 0x26;
        case 0xff54:  // XK_Down
            return 0x28;
        case 0xff51:  // XK_Left
            return 0x25;
        case 0xff53:  // XK_Right
            return 0x27;
        default:
            break;
    }
    // LeftAlt maps to the generic Alt VK so "Alt+X" chords match either
    // physical Alt key (the tracker sees 0xA4 for explicit LeftAlt chords).
    if (keysym == XK_Alt_L) {
        return kVkLeftAlt;
    }
    // Printable ASCII and F1..F24 coincide with the core VK encoding. X11
    // reports letters as lowercase keysyms (XK_a = 0x61); the core uses the
    // Windows VK convention of uppercase, so fold a-z up to A-Z.
    if (keysym >= 0x20 && keysym <= 0x5F) {
        return keysym;
    }
    if (keysym >= 0x61 && keysym <= 0x7A) {
        return keysym - 0x20;
    }
    if (keysym >= XK_F1 && keysym < XK_F1 + 24) {
        return kVkF1 + (keysym - XK_F1);
    }
    return 0;
}

bool IsModifierKeysym(std::uint32_t keysym) {
    switch (keysym) {
        case XK_Control_L:
        case XK_Control_R:
        case XK_Shift_L:
        case XK_Shift_R:
        case XK_Alt_L:
        case XK_Alt_R:
        case XK_Super_L:
        case XK_Super_R:
            return true;
        default:
            return false;
    }
}

std::uint32_t CoreVkToKeysym(std::uint32_t vk) {
    switch (vk) {
        case kVkCtrl:
            return XK_Control_L;
        case kVkShift:
            return XK_Shift_L;
        case kVkAlt:
        case kVkLeftAlt:
            return XK_Alt_L;
        case kVkRightAlt:
            return XK_Alt_R;
        case kVkWin:
            return XK_Super_L;
        case 0x1B:
            return XK_Escape;
        case 0x09:
            return XK_Tab;
        case 0x2D:
            return XK_Insert;
        case 0x14:
            return XK_Caps_Lock;
        case 0x91:
            return XK_Scroll_Lock;
        case 0x13:
            return XK_Pause;
        case 0x0D:
            return XK_Return;
        case 0x08:
            return XK_BackSpace;
        case 0x2E:
            return XK_Delete;
        case 0x24:
            return XK_Home;
        case 0x23:
            return XK_End;
        case 0x26:
            return 0xff52;  // Up
        case 0x28:
            return 0xff54;  // Down
        case 0x25:
            return 0xff51;  // Left
        case 0x27:
            return 0xff53;  // Right
        default:
            break;
    }
    if ((vk >= 0x20 && vk <= 0x5F) || (vk >= kVkF1 && vk < kVkF1 + 24)) {
        return vk;
    }
    return 0;
}

std::string KeysymDisplayName(std::uint32_t keysym) {
    for (const KeysymName& named : kNamedKeysyms) {
        if (named.keysym == keysym) {
            return named.name;
        }
    }
    if (keysym >= XK_F1 && keysym < XK_F1 + 24) {
        return "F" + std::to_string(keysym - XK_F1 + 1);
    }
    if (keysym >= 0x20 && keysym <= 0x7E) {
        const char c = static_cast<char>(keysym);
        return std::string(1, c);
    }
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "0x%x", keysym);
    return buffer;
}

std::optional<std::uint32_t> ParseKeysymName(const std::string& text_in) {
    const std::string text = LowerAscii(text_in);
    if (text.empty()) {
        return std::nullopt;
    }
    for (const KeysymName& named : kNamedKeysyms) {
        if (text == LowerAscii(named.name)) {
            return named.keysym;
        }
    }
    if (text == "control" || text == "lctrl") {
        return XK_Control_L;
    }
    if (text == "lshift") {
        return XK_Shift_L;
    }
    if (text == "rshift") {
        return XK_Shift_R;
    }
    if (text == "rctrl") {
        return XK_Control_R;
    }
    if (text == "lwin" || text == "super" || text == "meta") {
        return XK_Super_L;
    }
    if (text == "esc") {
        return XK_Escape;
    }
    if (text == "return") {
        return XK_Return;
    }
    if (text == "del") {
        return XK_Delete;
    }
    if (text.size() == 1) {
        const unsigned char c = static_cast<unsigned char>(text[0]);
        if (c >= 0x20 && c <= 0x7E) {
            // Uppercase letters and digits are their own keysyms; lowercase
            // letters map to the uppercase keysym like the Windows binding
            // parser does.
            if (c >= 'a' && c <= 'z') {
                return static_cast<std::uint32_t>(c - 'a' + 'A');
            }
            return static_cast<std::uint32_t>(c);
        }
        return std::nullopt;
    }
    if (text.size() >= 2 && text[0] == 'f' &&
        text.find_first_not_of("0123456789", 1) == std::string::npos) {
        const int index = std::stoi(text.substr(1));
        if (index >= 1 && index <= 24) {
            return XK_F1 + static_cast<std::uint32_t>(index - 1);
        }
        return std::nullopt;
    }
    if (text.rfind("0x", 0) == 0) {
        try {
            const unsigned long value = std::stoul(text, nullptr, 16);
            if (value <= 0xFFFFFFFFUL) {
                return static_cast<std::uint32_t>(value);
            }
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::uint32_t EvdevCodeToKeysym(std::uint16_t code) {
    for (const EvdevEntry& entry : kEvdevExtra) {
        if (entry.code == code) {
            return entry.keysym;
        }
    }
    for (const EvdevEntry& entry : kEvdevTable) {
        if (entry.code == code) {
            return entry.keysym;
        }
    }
    for (const EvdevRange& range : kEvdevRanges) {
        if (code >= range.first_code && code <= range.last_code) {
            const std::uint32_t offset = static_cast<std::uint32_t>(code - range.first_code);
            return range.first_keysym + offset;
        }
    }
    return 0;
}

std::vector<std::uint16_t> KnownEvdevCodes() {
    std::vector<std::uint16_t> codes;
    for (const EvdevEntry& entry : kEvdevTable) {
        codes.push_back(entry.code);
    }
    for (const EvdevEntry& entry : kEvdevExtra) {
        codes.push_back(entry.code);
    }
    for (const EvdevRange& range : kEvdevRanges) {
        for (int code = range.first_code; code <= range.last_code; ++code) {
            codes.push_back(static_cast<std::uint16_t>(code));
        }
    }
    std::sort(codes.begin(), codes.end());
    codes.erase(std::unique(codes.begin(), codes.end()), codes.end());
    return codes;
}

}  // namespace lwf::linuxkeys
