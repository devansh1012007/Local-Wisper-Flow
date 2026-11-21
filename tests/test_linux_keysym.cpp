#include <doctest/doctest.h>

#include "core/HotkeyBinding.h"
#include "core/SettingsStore.h"
#include "os/LinuxKeysym.h"

#include <cstdio>
#include <filesystem>

using namespace lwf::linuxkeys;

TEST_CASE("keysym to core vk: modifiers map to the Windows VK constants") {
    CHECK(KeysymToCoreVk(XK_Control_L) == kVkCtrl);
    CHECK(KeysymToCoreVk(XK_Control_R) == kVkCtrl);
    CHECK(KeysymToCoreVk(XK_Shift_L) == kVkShift);
    CHECK(KeysymToCoreVk(XK_Shift_R) == kVkShift);
    CHECK(KeysymToCoreVk(XK_Alt_R) == kVkRightAlt);
    CHECK(KeysymToCoreVk(XK_Alt_L) == kVkLeftAlt);
    CHECK(KeysymToCoreVk(XK_Super_L) == kVkWin);
    CHECK(KeysymToCoreVk(XK_Super_R) == kVkWin);
    CHECK(IsModifierKeysym(XK_Control_L));
    CHECK(IsModifierKeysym(XK_Alt_R));
    CHECK_FALSE(IsModifierKeysym(0x41));
}

TEST_CASE("keysym to core vk: X11 lowercase letters fold to the VK convention") {
    // XLookupKeysym reports XK_a = 0x61; the core chords use 0x41.
    CHECK(KeysymToCoreVk(0x61) == 0x41);
    CHECK(KeysymToCoreVk(0x7A) == 0x5A);
    CHECK(KeysymToCoreVk(0x41) == 0x41);
    // Digits and punctuation pass through, unknown keysyms are dropped.
    CHECK(KeysymToCoreVk(0x30) == 0x30);
    CHECK(KeysymToCoreVk(0x20) == 0x20);
    CHECK(KeysymToCoreVk(0x2C) == 0x2C);
    CHECK(KeysymToCoreVk(0) == 0);
    CHECK(KeysymToCoreVk(0xdeadbeef) == 0);
}

TEST_CASE("keysym to core vk: named keys and function keys") {
    CHECK(KeysymToCoreVk(XK_Escape) == 0x1B);
    CHECK(KeysymToCoreVk(XK_Tab) == 0x09);
    CHECK(KeysymToCoreVk(XK_Insert) == 0x2D);
    CHECK(KeysymToCoreVk(XK_Caps_Lock) == 0x14);
    CHECK(KeysymToCoreVk(XK_Scroll_Lock) == 0x91);
    CHECK(KeysymToCoreVk(XK_Pause) == 0x13);
    CHECK(KeysymToCoreVk(XK_F1) == kVkF1);
    CHECK(KeysymToCoreVk(XK_F1 + 5) == kVkF1 + 5);
    CHECK(KeysymToCoreVk(XK_F1 + 23) == kVkF1 + 23);
}

TEST_CASE("core vk to keysym round trips for the tracker-relevant set") {
    const std::uint32_t vks[] = {kVkCtrl, kVkShift, kVkAlt, kVkWin, 0x1B, 0x09, 0x2D};
    for (const std::uint32_t vk : vks) {
        const std::uint32_t keysym = CoreVkToKeysym(vk);
        REQUIRE(keysym != 0);
        if (vk == kVkAlt) {
            // Generic Alt maps to a concrete side; both sides must decode
            // back to a code the tracker treats as Alt.
            CHECK(KeysymToCoreVk(keysym) == kVkLeftAlt);
        } else {
            CHECK(KeysymToCoreVk(keysym) == vk);
        }
    }
}

TEST_CASE("keysym display and parse are inverse for named keys") {
    for (const std::uint32_t keysym :
         {XK_Control_L, XK_Shift_L, XK_Alt_R, XK_Super_L, XK_Escape, XK_Tab, XK_Insert,
          XK_Caps_Lock, XK_Scroll_Lock, XK_Pause, XK_Return, XK_Delete, XK_F1,
          XK_F1 + 11}) {
        const std::string name = KeysymDisplayName(keysym);
        CAPTURE(name);
        const auto parsed = ParseKeysymName(name);
        REQUIRE(parsed.has_value());
        CHECK(*parsed == keysym);
    }
    CHECK(*ParseKeysymName("f") == 0x46);  // single letters are key names
    CHECK(ParseKeysymName("D") == 0x44);
    CHECK(ParseKeysymName("f10") == XK_F1 + 9);
    CHECK(ParseKeysymName("f24") == XK_F1 + 23);
    CHECK_FALSE(ParseKeysymName("f25").has_value());
    CHECK_FALSE(ParseKeysymName("nonsense").has_value());
    CHECK(ParseKeysymName("0xffe9") == XK_Alt_L);
    CHECK_FALSE(ParseKeysymName("").has_value());
}

TEST_CASE("evdev mapping covers the advertised codes and hits core VKs") {
    const std::vector<std::uint16_t> codes = KnownEvdevCodes();
    REQUIRE_FALSE(codes.empty());
    for (const std::uint16_t code : codes) {
        CAPTURE(code);
        const std::uint32_t keysym = EvdevCodeToKeysym(code);
        CHECK(keysym != 0);
        CHECK(KeysymToCoreVk(keysym) != 0);
    }
    // Spot checks against the frozen input-event-codes.h ABI.
    CHECK(EvdevCodeToKeysym(KEY_LEFTCTRL) == XK_Control_L);
    CHECK(EvdevCodeToKeysym(KEY_RIGHTALT) == XK_Alt_R);
    CHECK(EvdevCodeToKeysym(KEY_A) == 0x41);
    CHECK(EvdevCodeToKeysym(KEY_SPACE) == 0x20);
    CHECK(EvdevCodeToKeysym(KEY_F1) == XK_F1);
    CHECK(EvdevCodeToKeysym(88) == XK_F1 + 11);  // KEY_F12
    CHECK(EvdevCodeToKeysym(0) == 0);
    CHECK(EvdevCodeToKeysym(999) == 0);
}

namespace {
SettingsStore StoreForFile(const std::string& path) {
    return SettingsStore(path);
}
}  // namespace

TEST_CASE("the additive hotkey_keysym key loads with its default") {
    const std::string path = "/tmp/lwf-test-keysym-schema.json";
    std::remove(path.c_str());
    SettingsStore store = StoreForFile(path);
    CHECK(store.HasKey("hotkey_keysym"));
    CHECK(store.GetInt("hotkey_keysym") == 0);
    CHECK(store.GetBool("hotkey_evdev_enabled") == false);
    CHECK(store.GetBool("daemon_toasts") == true);
    // Additive: the Windows legacy key keeps its VK semantics untouched.
    CHECK(store.GetInt("hotkey_code") == 165);
    std::remove(path.c_str());
}
