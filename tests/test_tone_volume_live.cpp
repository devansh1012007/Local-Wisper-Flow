// Followups-2 D1 pin: the tone volume must follow SetSettings live. The
// engine half of the chain is verified behaviourally here (no restart, no
// cached snapshot); the UI half (live commit + PreviewTone sample) is
// source-pinned in test_settings.cpp.

#include "EngineHarness.h"

#include "core/DictationEngine.h"
#include "core/IToneFeedback.h"
#include "core/ToneGenerator.h"

#include <doctest/doctest.h>

#include <memory>
#include <vector>

namespace {

class RecordingTones final : public IToneFeedback {
public:
    void Play(const ToneGenerator::ToneSpec& spec, double volume) override {
        specs.push_back(spec);
        volumes.push_back(volume);
    }

    std::vector<ToneGenerator::ToneSpec> specs;
    std::vector<double> volumes;
};

}  // namespace

TEST_CASE("tone volume follows SetSettings live (Followups-2 D1)") {
    Harness h;
    h.settings.sound_enabled = true;
    h.settings.sound_volume = 0.3;
    auto engine = MakeEngine(h);
    RecordingTones tones;
    engine->SetToneFeedback(&tones);

    // First dictation: start + stop tones both at the startup volume.
    PressAndSpeak(h, *engine);
    REQUIRE(tones.volumes.size() == 2);
    CHECK(tones.volumes[0] == doctest::Approx(0.3));
    CHECK(tones.volumes[1] == doctest::Approx(0.3));

    // User drags the volume slider mid-session: the ApplySettings drain
    // reloads EngineSettings and pushes them into the live engine.
    EngineSettings updated = h.settings;
    updated.sound_volume = 0.95;
    engine->SetSettings(updated);

    // Second dictation without restart: every tone must use the new volume.
    tones.volumes.clear();
    PressAndSpeak(h, *engine);
    REQUIRE(tones.volumes.size() == 2);
    CHECK(tones.volumes[0] == doctest::Approx(0.95));
    CHECK(tones.volumes[1] == doctest::Approx(0.95));

    // Zero must silence the tone path as well (slider dragged to the left end).
    EngineSettings muted = h.settings;
    muted.sound_volume = 0.0;
    engine->SetSettings(muted);
    tones.volumes.clear();
    PressAndSpeak(h, *engine);
    REQUIRE(tones.volumes.size() == 2);
    CHECK(tones.volumes[0] == doctest::Approx(0.0));
    CHECK(tones.volumes[1] == doctest::Approx(0.0));
}
