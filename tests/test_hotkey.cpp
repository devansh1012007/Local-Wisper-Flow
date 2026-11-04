#include "core/HotkeyTracker.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using Role = BindingRole;

struct Step {
    std::uint32_t vk;
    bool down;
};

struct Expectation {
    std::string event_types;
    bool swallow;
};

std::vector<EngineEventType> FeedSequence(HotkeyTracker& tracker,
                                          const std::vector<Step>& steps,
                                          std::int64_t start_ms = 1'000'000) {
    std::vector<EngineEventType> emitted;
    std::int64_t now = start_ms;
    for (const Step& step : steps) {
        RawKeyEvent event;
        event.vk = step.vk;
        event.is_down = step.down;
        event.timestamp_ms = now;
        now += 10;
        const TrackerResult result = tracker.Feed(event);
        for (const EngineEvent& engine_event : result.events) {
            emitted.push_back(engine_event.type);
        }
    }
    return emitted;
}

std::string JoinTypes(const std::vector<EngineEventType>& types) {
    std::string joined;
    for (const EngineEventType type : types) {
        if (!joined.empty()) {
            joined += ",";
        }
        switch (type) {
            case EngineEventType::PttPress: joined += "PttPress"; break;
            case EngineEventType::PttRelease: joined += "PttRelease"; break;
            case EngineEventType::Toggle: joined += "Toggle"; break;
            case EngineEventType::Cancel: joined += "Cancel"; break;
            case EngineEventType::CommandMode: joined += "CommandMode"; break;
            case EngineEventType::PasteLast: joined += "PasteLast"; break;
            case EngineEventType::Quit: joined += "Quit"; break;
        }
    }
    return joined;
}

bool SwallowOf(HotkeyTracker& tracker, const RawKeyEvent& event) {
    return tracker.Feed(event).swallow;
}

HotkeyTracker MakeTracker() {
    HotkeyTracker tracker;
    std::vector<Binding> bindings;
    auto ptt = HotkeyBinding::Parse("Ctrl+Win", Role::PushToTalk);
    REQUIRE(ptt.has_value());
    bindings.push_back(*ptt);
    auto cancel = HotkeyBinding::Parse("Escape", Role::Cancel);
    REQUIRE(cancel.has_value());
    bindings.push_back(*cancel);
    tracker.SetBindings(bindings);
    return tracker;
}

}

TEST_CASE("parser accepts known chord names and rejects unknown") {
    const auto chord = HotkeyBinding::Parse("Ctrl+Win", Role::PushToTalk);
    REQUIRE(chord.has_value());
    CHECK(chord->keys.size() == 2);
    CHECK(HotkeyBinding::Serialize(*chord) == "Ctrl+Win");

    const auto single = HotkeyBinding::Parse("RightAlt", Role::PushToTalk);
    REQUIRE(single.has_value());
    CHECK(single->keys[0] == HotkeyBinding::kRightAlt);

    const auto f13 = HotkeyBinding::Parse("F13", Role::CommandMode);
    REQUIRE(f13.has_value());
    CHECK(f13->keys[0] == HotkeyBinding::kF1 + 12);

    const auto triple = HotkeyBinding::Parse("Ctrl+Shift+Space", Role::CommandMode);
    REQUIRE(triple.has_value());
    CHECK(triple->keys.size() == 3);

    CHECK_FALSE(HotkeyBinding::Parse("NotAKey", Role::PushToTalk).has_value());
    CHECK_FALSE(HotkeyBinding::Parse("Ctrl+", Role::PushToTalk).has_value());
    CHECK_FALSE(HotkeyBinding::Parse("", Role::PushToTalk).has_value());
    CHECK_FALSE(HotkeyBinding::Parse("Ctrl+Ctrl", Role::PushToTalk).has_value());
}

TEST_CASE("chord fires regardless of press order and releases on any key up") {
    SUBCASE("ctrl first") {
        HotkeyTracker tracker = MakeTracker();
        const auto events = FeedSequence(tracker, {{HotkeyBinding::kCtrl, true},
                                                   {HotkeyBinding::kWin, true},
                                                   {HotkeyBinding::kWin, false},
                                                   {HotkeyBinding::kCtrl, false}});
        CHECK(JoinTypes(events) == "PttPress,PttRelease");
    }
    SUBCASE("win first") {
        HotkeyTracker tracker = MakeTracker();
        const auto events = FeedSequence(tracker, {{HotkeyBinding::kWin, true},
                                                   {HotkeyBinding::kCtrl, true},
                                                   {HotkeyBinding::kCtrl, false},
                                                   {HotkeyBinding::kWin, false}});
        CHECK(JoinTypes(events) == "PttPress,PttRelease");
    }
}

TEST_CASE("modifier released first still ends the chord") {
    HotkeyTracker tracker = MakeTracker();
    const auto events = FeedSequence(tracker, {{HotkeyBinding::kCtrl, true},
                                               {HotkeyBinding::kWin, true},
                                               {HotkeyBinding::kCtrl, false},
                                               {HotkeyBinding::kWin, false}});
    CHECK(JoinTypes(events) == "PttPress,PttRelease");
}

TEST_CASE("first chord key passes through so normal typing keeps working") {
    HotkeyTracker tracker = MakeTracker();
    CHECK_FALSE(SwallowOf(tracker, {HotkeyBinding::kCtrl, true, false, 0}));
    CHECK_FALSE(SwallowOf(tracker, {HotkeyBinding::kCtrl, false, false, 5}));
    CHECK_FALSE(SwallowOf(tracker, {HotkeyBinding::kWin, true, false, 10}));
}

TEST_CASE("win events are swallowed while the chord is in progress") {
    HotkeyTracker tracker = MakeTracker();
    (void)tracker.Feed({HotkeyBinding::kCtrl, true, false, 0});
    CHECK(SwallowOf(tracker, {HotkeyBinding::kWin, true, false, 10}));
    CHECK(SwallowOf(tracker, {HotkeyBinding::kWin, false, false, 20}));
}

TEST_CASE("key repeats are ignored") {
    HotkeyTracker tracker = MakeTracker();
    (void)tracker.Feed({HotkeyBinding::kCtrl, true, false, 0});
    (void)tracker.Feed({HotkeyBinding::kWin, true, false, 10});
    const auto repeat = tracker.Feed({HotkeyBinding::kWin, true, false, 20});
    CHECK(repeat.swallow);
    CHECK(repeat.events.empty());
}

TEST_CASE("two bindings sharing ctrl do not cross fire") {
    HotkeyTracker tracker;
    std::vector<Binding> bindings;
    auto ptt = HotkeyBinding::Parse("Ctrl+Win", Role::PushToTalk);
    auto command = HotkeyBinding::Parse("Ctrl+Shift+Space", Role::CommandMode);
    REQUIRE(ptt.has_value());
    REQUIRE(command.has_value());
    bindings.push_back(*ptt);
    bindings.push_back(*command);
    tracker.SetBindings(bindings);

    const auto events = FeedSequence(tracker, {{HotkeyBinding::kCtrl, true},
                                               {HotkeyBinding::kWin, true},
                                               {HotkeyBinding::kWin, false}});
    CHECK(JoinTypes(events) == "PttPress,PttRelease");

    const auto command_events = FeedSequence(tracker, {{HotkeyBinding::kShift, true},
                                                       {HotkeyBinding::kSpace, true},
                                                       {HotkeyBinding::kSpace, false},
                                                       {HotkeyBinding::kShift, false}});
    CHECK(JoinTypes(command_events) == "CommandMode");
}

TEST_CASE("double tap fires toggle inside the window and not outside") {
    SUBCASE("399 ms") {
        HotkeyTracker tracker = MakeTracker();
        (void)tracker.Feed({HotkeyBinding::kCtrl, true, false, 0});
        const auto first = tracker.Feed({HotkeyBinding::kWin, true, false, 10});
        REQUIRE(first.events.size() == 1);
        CHECK(first.events[0].type == EngineEventType::PttPress);
        (void)tracker.Feed({HotkeyBinding::kWin, false, false, 60});
        (void)tracker.Feed({HotkeyBinding::kCtrl, false, false, 70});

        (void)tracker.Feed({HotkeyBinding::kCtrl, true, false, 400});
        const auto second = tracker.Feed({HotkeyBinding::kWin, true, false, 410});
        REQUIRE(second.events.size() == 1);
        CHECK(second.events[0].type == EngineEventType::Toggle);
    }
    SUBCASE("401 ms") {
        HotkeyTracker tracker = MakeTracker();
        (void)tracker.Feed({HotkeyBinding::kCtrl, true, false, 0});
        (void)tracker.Feed({HotkeyBinding::kWin, true, false, 10});
        (void)tracker.Feed({HotkeyBinding::kWin, false, false, 60});
        (void)tracker.Feed({HotkeyBinding::kCtrl, false, false, 70});

        (void)tracker.Feed({HotkeyBinding::kCtrl, true, false, 500});
        const auto second = tracker.Feed({HotkeyBinding::kWin, true, false, 901});
        REQUIRE(second.events.size() == 1);
        CHECK(second.events[0].type == EngineEventType::PttPress);
    }
}

TEST_CASE("stuck chord is force released by tick") {
    HotkeyTracker tracker = MakeTracker();
    HotkeyTracker::Options options;
    options.stuck_key_guard_ms = 1000;
    tracker.SetOptions(options);

    (void)tracker.Feed({HotkeyBinding::kCtrl, true, false, 0});
    (void)tracker.Feed({HotkeyBinding::kWin, true, false, 10});

    const auto early = tracker.Tick(500);
    CHECK(early.events.empty());

    const auto late = tracker.Tick(1500);
    REQUIRE(late.events.size() == 1);
    CHECK(late.events[0].type == EngineEventType::PttRelease);

    const auto after = tracker.Feed({HotkeyBinding::kWin, false, false, 1600});
    CHECK_FALSE(after.swallow);
}

TEST_CASE("injected events are never swallowed nor tracked") {
    HotkeyTracker tracker = MakeTracker();
    (void)tracker.Feed({HotkeyBinding::kCtrl, true, false, 0});
    const TrackerResult injected = tracker.Feed({HotkeyBinding::kWin, true, true, 10});
    CHECK_FALSE(injected.swallow);
    CHECK(injected.events.empty());
}

TEST_CASE("altgr right alt with ctrl is not ptt when avoidance is on") {
    SUBCASE("avoidance on") {
        HotkeyTracker tracker;
        auto right_alt = HotkeyBinding::Parse("RightAlt", Role::PushToTalk);
        REQUIRE(right_alt.has_value());
        tracker.SetBindings({*right_alt});
        HotkeyTracker::Options options;
        options.avoid_altgr_conflict = true;
        tracker.SetOptions(options);

        (void)tracker.Feed({HotkeyBinding::kCtrl, true, false, 0});
        const auto result = tracker.Feed({HotkeyBinding::kRightAlt, true, false, 10});
        CHECK_FALSE(result.swallow);
        CHECK(result.events.empty());
        CHECK_FALSE(tracker.Feed({HotkeyBinding::kRightAlt, false, false, 20}).swallow);
        CHECK_FALSE(tracker.Feed({HotkeyBinding::kCtrl, false, false, 30}).swallow);

        const auto alone = tracker.Feed({HotkeyBinding::kRightAlt, true, false, 40});
        CHECK(alone.swallow);
        REQUIRE(alone.events.size() == 1);
        CHECK(alone.events[0].type == EngineEventType::PttPress);
    }
    SUBCASE("avoidance off") {
        HotkeyTracker tracker;
        auto right_alt = HotkeyBinding::Parse("RightAlt", Role::PushToTalk);
        REQUIRE(right_alt.has_value());
        tracker.SetBindings({*right_alt});
        HotkeyTracker::Options options;
        options.avoid_altgr_conflict = false;
        tracker.SetOptions(options);

        (void)tracker.Feed({HotkeyBinding::kCtrl, true, false, 0});
        const auto result = tracker.Feed({HotkeyBinding::kRightAlt, true, false, 10});
        CHECK(result.swallow);
        REQUIRE(result.events.size() == 1);
        CHECK(result.events[0].type == EngineEventType::PttPress);
    }
}

TEST_CASE("escape is swallowed only while cancel is armed") {
    HotkeyTracker tracker = MakeTracker();

    const auto unarmed = tracker.Feed({HotkeyBinding::kEscape, true, false, 0});
    CHECK_FALSE(unarmed.swallow);
    CHECK(unarmed.events.empty());

    tracker.SetCancelArmed(true);
    const auto armed = tracker.Feed({HotkeyBinding::kEscape, true, false, 10});
    CHECK(armed.swallow);
    REQUIRE(armed.events.size() == 1);
    CHECK(armed.events[0].type == EngineEventType::Cancel);
}

TEST_CASE("load bindings derives ptt from legacy hotkey code") {
    SettingsStore store((std::filesystem::path("/tmp") / "lwf_bind_test.json").string());
    store.SetInt("hotkey_code", 165);
    const std::vector<Binding> bindings = LoadBindings(store);
    REQUIRE_FALSE(bindings.empty());
    CHECK(bindings[0].role == Role::PushToTalk);
    CHECK(bindings[0].keys.size() == 1);
    CHECK(bindings[0].keys[0] == HotkeyBinding::kRightAlt);

    store.SetString("hotkey_ptt", "Ctrl+Win");
    store.SetString("hotkey_command", "F13");
    const std::vector<Binding> expanded = LoadBindings(store);
    REQUIRE(expanded.size() == 3);
    CHECK(expanded[0].role == Role::PushToTalk);
    CHECK(expanded[0].keys.size() == 2);
    CHECK(expanded[1].role == Role::Cancel);
    CHECK(expanded[2].role == Role::CommandMode);
}
