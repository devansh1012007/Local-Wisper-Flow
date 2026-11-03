#include "core/Languages.h"
#include "core/OverlayModel.h"
#include "core/SettingsStore.h"
#include "core/SettingsUiModel.h"

#include <doctest/doctest.h>
#include <fstream>
#include <fstream>
#include <string>

TEST_CASE("waveform ring keeps the last N samples in order") {
    OverlayModel::WaveformRing ring(64);
    for (int i = 0; i < 70; ++i) {
        ring.Push(static_cast<float>(i));
    }
    CHECK(ring.Size() == 64);
    const std::vector<float> snapshot = ring.Snapshot();
    REQUIRE(snapshot.size() == 64);
    CHECK(snapshot.front() == 6.0f);
    CHECK(snapshot.back() == 69.0f);
}

TEST_CASE("visual mapping covers every state including auto-hide timers") {
    const auto hidden = OverlayModel::VisualFor(OverlayModel::PillState::Hidden, "");
    CHECK_FALSE(hidden.visible);
    CHECK(hidden.auto_hide_after_ms == 0);

    const auto listening = OverlayModel::VisualFor(OverlayModel::PillState::Listening, "");
    CHECK(listening.visible);
    CHECK(listening.auto_hide_after_ms == 0);

    const auto processing = OverlayModel::VisualFor(OverlayModel::PillState::Processing, "");
    CHECK(processing.visible);

    const auto error = OverlayModel::VisualFor(OverlayModel::PillState::Error, "mic blocked");
    CHECK(error.visible);
    CHECK(error.message == "mic blocked");
    CHECK(error.auto_hide_after_ms == 3000);

    const auto cancelled = OverlayModel::VisualFor(OverlayModel::PillState::Cancelled, "");
    CHECK(cancelled.visible);
    CHECK(cancelled.auto_hide_after_ms == 600);
}

TEST_CASE("pill position is bottom center above the taskbar with dpi scaling") {
    const OverlayModel::Rect work_area{0, 0, 1920, 1040};
    const OverlayModel::Rect position =
        OverlayModel::PillPosition(work_area, 200, 40, 16, 1.0);
    CHECK(position.x == 1920 / 2 - 100);
    CHECK(position.y == 1040 - 40 - 16);
    CHECK(position.width == 200);

    const OverlayModel::Rect scaled =
        OverlayModel::PillPosition(work_area, 200, 40, 16, 1.5);
    CHECK(scaled.width == 300);
    CHECK(scaled.height == 60);
    CHECK(scaled.x == 1920 / 2 - 150);
    CHECK(scaled.y == 1040 - 60 - 24);

    const OverlayModel::Rect second_monitor{1920, 0, 2560, 1400};
    const OverlayModel::Rect moved =
        OverlayModel::PillPosition(second_monitor, 200, 40, 16, 1.0);
    CHECK(moved.x == 1920 + 2560 / 2 - 100);
    CHECK(moved.y == 1400 - 40 - 16);
}

TEST_CASE("settings diff classifies hot apply versus restart required") {
    const std::string path = "/tmp/lwf_diff_settings.json";
    {
        std::ofstream file(path, std::ios::trunc);
        file << "{}";
    }
    SettingsStore before(path);
    SettingsStore after_store(path);
    after_store.SetInt("min_hold_ms", 400);
    after_store.SetString("whisper_model_path", "ggml-new.bin");
    after_store.SetBool("sound_enabled", false);

    const SettingsUiModel::SettingDiff diff = SettingsUiModel::ClassifyDiff(before, after_store);
    REQUIRE(diff.hot_keys.size() == 2);
    CHECK(diff.hot_keys[0] == "min_hold_ms");
    CHECK(diff.hot_keys[1] == "sound_enabled");
    REQUIRE(diff.restart_keys.size() == 1);
    CHECK(diff.restart_keys[0] == "whisper_model_path");
}

TEST_CASE("validators reject out of range and unsupported values") {
    CHECK(SettingsUiModel::ValidateInt("min_hold_ms", 400, 50, 5000).empty());
    CHECK_FALSE(SettingsUiModel::ValidateInt("min_hold_ms", 10, 50, 5000).empty());
    CHECK(SettingsUiModel::ValidateModelPath("models/ggml.bin").empty());
    CHECK_FALSE(SettingsUiModel::ValidateModelPath(std::string(600, 'x')).empty());
    CHECK(SettingsUiModel::ValidateLanguage("en").empty());
    CHECK(SettingsUiModel::ValidateLanguage("auto").empty());
    CHECK_FALSE(SettingsUiModel::ValidateLanguage("xx").empty());
}

TEST_CASE("dictionary editor adds removes and dedupes") {
    SettingsUiModel::DictionaryEditorModel editor;
    editor.AddWord("Kubernetes");
    editor.AddWord("Docker");
    editor.AddWord("Kubernetes");
    editor.AddReplacement("gihub", "GitHub");
    editor.AddReplacement("gihub", "GitHub");
    editor.Dedupe();
    CHECK(editor.words.size() == 2);
    CHECK(editor.replacements.size() == 1);
    editor.RemoveWord(0);
    CHECK(editor.words.size() == 1);
    CHECK(editor.words[0].value == "Docker");
    editor.AddWord("");
    CHECK(editor.words.size() == 1);
}

TEST_CASE("snippet editor validates and dedupes") {
    SettingsUiModel::SnippetEditorModel editor;
    editor.Add("sig", "John Doe");
    editor.Add("sig", "Other");
    editor.Add("", "bad");
    editor.Dedupe();
    CHECK(editor.rows.size() == 1);

    std::string error;
    CHECK(editor.Valid(error));
    editor.Add("big", std::string(4001, 'x'));
    CHECK_FALSE(editor.Valid(error));
    CHECK(error.find("4000") != std::string::npos);
}
