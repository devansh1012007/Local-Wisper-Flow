#pragma once

#include "core/EventQueue.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

enum class UiCommandType {
    SetEnabled,
    OpenSettings,
    OpenHistory,
    OpenLogsFolder,
    ToggleAutostart,
    StartLearnHotkey,
    ApplySettings,
    Quit,
};

struct UiCommand {
    UiCommandType type;
    std::string payload;
};

struct StatusSnapshotValue {
    int state = 0;
    float audio_level = 0.0f;
    int error_code = 0;
    int error_counter = 0;
    std::int64_t error_timestamp = 0;
    int mode = 0;
    bool enabled = false;
    char error_message[128] = {};
};

class StatusSnapshot {
public:
    void Publish(const StatusSnapshotValue& value);
    StatusSnapshotValue Read() const;

private:
    std::atomic<std::uint32_t> seq_{0};
    std::atomic<int> state_{0};
    std::atomic<float> audio_level_{0.0f};
    std::atomic<int> error_code_{0};
    std::atomic<int> error_counter_{0};
    std::atomic<std::int64_t> error_timestamp_{0};
    std::atomic<int> mode_{0};
    std::atomic<bool> enabled_{false};
    std::atomic<bool> message_valid_{false};
    std::array<char, 128> error_message_{};
};

class UiBus {
public:
    void PostCommand(UiCommandType type, std::string payload = {});
    bool PollCommand(UiCommand& out);
    void CloseCommands();

    void PublishStatus(const StatusSnapshotValue& value);
    StatusSnapshotValue ReadStatus() const;

private:
    EventQueue<UiCommand> commands_;
    StatusSnapshot snapshot_;
};
