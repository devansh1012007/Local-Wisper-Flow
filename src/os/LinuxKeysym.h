#pragma once

// Linux key-code translation tables (plan 2026-11-21, changes C03/C04/C05).
//
// The portable core (HotkeyBinding/HotkeyTracker) speaks Windows-style VK
// codes: modifiers use the VK constants (Ctrl 0x11, Shift 0x10, Alt 0x12,
// LWIN 0x5B) while letters, digits and punctuation use ASCII 0x20..0x5F and
// F-keys 0x70.. — which is exactly the X11 keysym encoding for those keys.
// Only modifiers and a few named keys need a real mapping, plus the evdev
// scan codes read from /dev/input on Wayland.
//
// Pure tables and functions: no X11 headers required, unit-testable on any
// platform. Evdev constants mirror the frozen <linux/input-event-codes.h>
// ABI values (KEY_*); X11 keysym constants mirror <X11/keysymdef.h>.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace lwf::linuxkeys {

// X11 modifier keysyms (keysymdef.h).
constexpr std::uint32_t XK_Control_L = 0xffe3;
constexpr std::uint32_t XK_Control_R = 0xffe4;
constexpr std::uint32_t XK_Shift_L = 0xffe1;
constexpr std::uint32_t XK_Shift_R = 0xffe2;
constexpr std::uint32_t XK_Alt_L = 0xffe9;
constexpr std::uint32_t XK_Alt_R = 0xffea;
constexpr std::uint32_t XK_Super_L = 0xffeb;
constexpr std::uint32_t XK_Super_R = 0xffec;
constexpr std::uint32_t XK_Escape = 0xff1b;
constexpr std::uint32_t XK_Tab = 0xff09;
constexpr std::uint32_t XK_Insert = 0xff63;
constexpr std::uint32_t XK_Caps_Lock = 0xffe5;
constexpr std::uint32_t XK_Scroll_Lock = 0xff14;
constexpr std::uint32_t XK_Pause = 0xff13;
constexpr std::uint32_t XK_Return = 0xff0d;
constexpr std::uint32_t XK_BackSpace = 0xff08;
constexpr std::uint32_t XK_Delete = 0xffff;
constexpr std::uint32_t XK_Home = 0xff50;
constexpr std::uint32_t XK_End = 0xff57;
constexpr std::uint32_t XK_F1 = 0xffbe;

// Core VK constants mirrored from core/HotkeyBinding.h (single source stays
// there; repeated here only so this header stays self-contained for tests).
constexpr std::uint32_t kVkCtrl = 0x11;
constexpr std::uint32_t kVkShift = 0x10;
constexpr std::uint32_t kVkAlt = 0x12;
constexpr std::uint32_t kVkLeftAlt = 0xA4;
constexpr std::uint32_t kVkRightAlt = 0xA5;
constexpr std::uint32_t kVkWin = 0x5B;
constexpr std::uint32_t kVkF1 = 0x70;

// Translates an X11 keysym to the core VK-ish code used by the hotkey
// tracker. Letters, digits and printable ASCII pass through unchanged
// (the two encodings coincide there). Returns 0 for unmapped keysyms.
std::uint32_t KeysymToCoreVk(std::uint32_t keysym);

// True when the keysym is one of the modifier keys the tracker reasons about.
bool IsModifierKeysym(std::uint32_t keysym);

// Inverse mapping for display purposes (0 when unknown).
std::uint32_t CoreVkToKeysym(std::uint32_t vk);

// Human label for a keysym: "Ctrl", "RightAlt", "F3", "A", "0xffbe".
std::string KeysymDisplayName(std::uint32_t keysym);

// Parses a keysym from a user-entered string: named keys ("ctrl", "rightalt",
// "f3", "escape"), single ASCII characters ("d", "D", "1") or 0x-prefixed
// hex keysyms. nullopt when not understood.
std::optional<std::uint32_t> ParseKeysymName(const std::string& text);

// Evdev KEY_* codes (frozen ABI values from linux/input-event-codes.h).
constexpr std::uint16_t KEY_ESC = 1;
constexpr std::uint16_t KEY_1 = 2;
constexpr std::uint16_t KEY_Q = 16;
constexpr std::uint16_t KEY_A = 30;
constexpr std::uint16_t KEY_Z = 44;
constexpr std::uint16_t KEY_X = 45;
constexpr std::uint16_t KEY_C = 46;
constexpr std::uint16_t KEY_V = 47;
constexpr std::uint16_t KEY_B = 48;
constexpr std::uint16_t KEY_N = 49;
constexpr std::uint16_t KEY_M = 50;
constexpr std::uint16_t KEY_SPACE = 57;
constexpr std::uint16_t KEY_F1 = 59;
constexpr std::uint16_t KEY_CAPSLOCK = 58;
constexpr std::uint16_t KEY_LEFTCTRL = 29;
constexpr std::uint16_t KEY_LEFTSHIFT = 42;
constexpr std::uint16_t KEY_LEFTALT = 56;
constexpr std::uint16_t KEY_TAB = 15;
constexpr std::uint16_t KEY_ENTER = 28;
constexpr std::uint16_t KEY_RIGHTCTRL = 97;
constexpr std::uint16_t KEY_RIGHTSHIFT = 54;
constexpr std::uint16_t KEY_RIGHTALT = 100;
constexpr std::uint16_t KEY_INSERT = 110;
constexpr std::uint16_t KEY_SCROLLLOCK = 70;
constexpr std::uint16_t KEY_PAUSE = 119;
constexpr std::uint16_t KEY_LEFTMETA = 125;
constexpr std::uint16_t KEY_RIGHTMETA = 126;
constexpr std::uint16_t KEY_HOME = 102;
constexpr std::uint16_t KEY_END = 107;
constexpr std::uint16_t KEY_UP = 103;
constexpr std::uint16_t KEY_DOWN = 108;
constexpr std::uint16_t KEY_LEFT = 105;
constexpr std::uint16_t KEY_RIGHT = 106;

// Maps an evdev KEY_* code to an X11 keysym (0 when unknown). Covers the
// modifiers, function keys, navigation block and the printable US layout.
std::uint32_t EvdevCodeToKeysym(std::uint16_t code);

// Lists the evdev codes the table knows about (used by tests to prove the
// mapping is total over the advertised set).
std::vector<std::uint16_t> KnownEvdevCodes();

}  // namespace lwf::linuxkeys
