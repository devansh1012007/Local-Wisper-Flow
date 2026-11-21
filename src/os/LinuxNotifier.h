#pragma once

// Desktop notifications for the Linux port (plan 2026-11-21, change C06).
//
// Wraps the `notify-send` utility from libnotify-bin through the shell-free
// subprocess runner: no compile-time libnotify dependency, no extra link
// surface, and the daemon still builds on minimal servers (where toasts just
// degrade to log lines). Bodies carry status only — never transcripts.

#include "core/ErrorCode.h"
#include "os/LinuxProcess.h"

#include <string>

class LinuxNotifier {
public:
    explicit LinuxNotifier(lwf::process::Runner runner = lwf::process::RealRunner());

    enum class Urgency { Low, Normal, Critical };

    // Shows a desktop toast. Returns false when notify-send is unavailable
    // (the reason is logged once; subsequent calls are cheap no-ops).
    bool Notify(const std::string& title, const std::string& body, int timeout_ms,
                Urgency urgency = Urgency::Normal);

    // Engine-surface error reporting: user message + redacted detail line.
    void NotifyError(ErrorCode code, const std::string& detail);

    // Silences every toast (config key daemon_toasts); logs still happen.
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool Enabled() const { return enabled_; }

    // True when notify-send exists on PATH (probed once, cached).
    bool Available();

private:
    lwf::process::Runner runner_;
    bool enabled_ = true;
    int available_ = -1;  // -1 unknown, 0 no, 1 yes
    bool missing_logged_ = false;
};
