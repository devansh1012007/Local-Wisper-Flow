#include "os/LinuxNotifier.h"

#include "core/ErrorCode.h"
#include "core/Log.h"

namespace {

// notification spec icon names; a missing icon degrades to the default one.
const char* kIconName = "audio-input-microphone";

const char* UrgencyFlag(LinuxNotifier::Urgency urgency) {
    switch (urgency) {
        case LinuxNotifier::Urgency::Low:
            return "low";
        case LinuxNotifier::Urgency::Critical:
            return "critical";
        case LinuxNotifier::Urgency::Normal:
        default:
            return "normal";
    }
}

}  // namespace

LinuxNotifier::LinuxNotifier(lwf::process::Runner runner) : runner_(std::move(runner)) {}

bool LinuxNotifier::Available() {
    if (available_ < 0) {
        available_ = lwf::process::Exists("notify-send") ? 1 : 0;
    }
    return available_ == 1;
}

bool LinuxNotifier::Notify(const std::string& title, const std::string& body, int timeout_ms,
                           Urgency urgency) {
    if (!enabled_) {
        return true;  // deliberately silenced: not an error
    }
    if (!Available()) {
        if (!missing_logged_) {
            lwf::Log::Warning("notify-send not found; desktop toasts disabled "
                              "(install libnotify-bin)");
            missing_logged_ = true;
        }
        return false;
    }
    lwf::process::Command command;
    command.executable = "notify-send";
    command.args = {
        "--app-name", "Local Wisper Flow",
        "--icon", kIconName,
        "--urgency", UrgencyFlag(urgency),
        "--expire-time", std::to_string(timeout_ms > 0 ? timeout_ms : 4000),
        title,
        body,
    };
    const lwf::process::RunResult result = runner_(command, "", 5000);
    if (!result.launched || result.exit_code != 0) {
        lwf::Log::Warning("notify-send failed: " +
                          (result.error.empty() ? ("exit " + std::to_string(result.exit_code))
                                                : result.error));
        return false;
    }
    return true;
}

void LinuxNotifier::NotifyError(ErrorCode code, const std::string& detail) {
    std::string body = UserMessage(code);
    if (!detail.empty() && detail != body) {
        body += ": " + detail;
    }
    Notify("Dictation error", body, 6000, Urgency::Normal);
}
