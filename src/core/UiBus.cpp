#include "core/UiBus.h"

#include <cstring>
#include <utility>

void StatusSnapshot::Publish(const StatusSnapshotValue& value) {
    const std::uint32_t sequence = seq_.load(std::memory_order_relaxed);
    seq_.store(sequence + 1, std::memory_order_release);

    state_.store(value.state, std::memory_order_relaxed);
    audio_level_.store(value.audio_level, std::memory_order_relaxed);
    error_code_.store(value.error_code, std::memory_order_relaxed);
    error_counter_.store(value.error_counter, std::memory_order_relaxed);
    error_timestamp_.store(value.error_timestamp, std::memory_order_relaxed);
    mode_.store(value.mode, std::memory_order_relaxed);
    enabled_.store(value.enabled, std::memory_order_relaxed);
    std::memcpy(error_message_.data(), value.error_message, error_message_.size());
    message_valid_.store(true, std::memory_order_relaxed);

    seq_.store(sequence + 2, std::memory_order_release);
}

StatusSnapshotValue StatusSnapshot::Read() const {
    StatusSnapshotValue value;
    for (;;) {
        const std::uint32_t before = seq_.load(std::memory_order_seq_cst);
        if (before % 2 != 0) {
            continue;
        }
        std::atomic_thread_fence(std::memory_order_seq_cst);
        value.state = state_.load(std::memory_order_relaxed);
        value.audio_level = audio_level_.load(std::memory_order_relaxed);
        value.error_code = error_code_.load(std::memory_order_relaxed);
        value.error_counter = error_counter_.load(std::memory_order_relaxed);
        value.error_timestamp = error_timestamp_.load(std::memory_order_relaxed);
        value.mode = mode_.load(std::memory_order_relaxed);
        value.enabled = enabled_.load(std::memory_order_relaxed);
        value.error_message[0] = '\0';
        if (message_valid_.load(std::memory_order_relaxed)) {
            std::memcpy(value.error_message, error_message_.data(),
                        sizeof(value.error_message) - 1);
            value.error_message[sizeof(value.error_message) - 1] = '\0';
        }
        std::atomic_thread_fence(std::memory_order_seq_cst);
        const std::uint32_t after = seq_.load(std::memory_order_seq_cst);
        if (before == after) {
            return value;
        }
    }
}

void UiBus::PostCommand(UiCommandType type, std::string payload) {
    UiCommand command;
    command.type = type;
    command.payload = std::move(payload);
    commands_.Push(std::move(command));
}

bool UiBus::PollCommand(UiCommand& out) {
    std::optional<UiCommand> command = commands_.PopFor(0);
    if (!command) {
        return false;
    }
    out = std::move(*command);
    return true;
}

void UiBus::CloseCommands() {
    commands_.Close();
}

void UiBus::PublishStatus(const StatusSnapshotValue& value) {
    snapshot_.Publish(value);
}

StatusSnapshotValue UiBus::ReadStatus() const {
    return snapshot_.Read();
}
