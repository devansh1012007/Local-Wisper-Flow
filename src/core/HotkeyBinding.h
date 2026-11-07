#pragma once

#include "core/EngineTypes.h"
#include "core/SettingsStore.h"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

enum class BindingRole { PushToTalk, HandsFreeToggle, Cancel, CommandMode, PasteLast };

struct Binding {
    BindingRole role = BindingRole::PushToTalk;
    std::vector<std::uint32_t> keys;
    std::string label;
};

class HotkeyBinding {
public:
    static std::optional<Binding> Parse(const std::string& text, BindingRole role);
    static std::string Serialize(const Binding& binding);
    static bool IsSatisfied(const Binding& binding, const std::set<std::uint32_t>& pressed);
    static bool IsModifierVk(std::uint32_t vk);
    static bool ChordHasModifier(const Binding& binding);
    static bool BindingsConflict(const Binding& a, const Binding& b);
    static bool ChordsConflict(const std::string& a, const std::string& b);
    static std::optional<std::string> FindConflict(const std::string& candidate,
                                                   const std::vector<Binding>& registry,
                                                   std::optional<BindingRole> exclude_role = std::nullopt);
    static std::string ValidateRecorderChord(const std::string& candidate,
                                             const std::vector<Binding>& registry,
                                             BindingRole target_role);
    static std::optional<BindingRole> RoleForConfigKey(const std::string& key);
    static const char* ConfigKeyForRole(BindingRole role);
    static bool IsConfigHotkeyKey(const std::string& key);
    static const std::vector<std::string>& ConfigHotkeyKeys();

    static constexpr std::uint32_t kCtrl = 0x11;
    static constexpr std::uint32_t kShift = 0x10;
    static constexpr std::uint32_t kAlt = 0x12;
    static constexpr std::uint32_t kWin = 0x5B;
    static constexpr std::uint32_t kEscape = 0x1B;
    static constexpr std::uint32_t kSpace = 0x20;
    static constexpr std::uint32_t kTab = 0x09;
    static constexpr std::uint32_t kInsert = 0x2D;
    static constexpr std::uint32_t kLeftAlt = 0xA4;
    static constexpr std::uint32_t kRightAlt = 0xA5;
    static constexpr std::uint32_t kCapsLock = 0x14;
    static constexpr std::uint32_t kScrollLock = 0x91;
    static constexpr std::uint32_t kPause = 0x13;
    static constexpr std::uint32_t kF1 = 0x70;
};

std::vector<Binding> LoadBindings(const SettingsStore& settings);
const char* ToString(BindingRole role);
