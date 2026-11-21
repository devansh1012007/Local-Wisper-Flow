#pragma once

// Subprocess plumbing for the Linux port (plan 2026-11-21, changes C05/C06).
//
// fork/execvp with argv vectors — never a shell — so untrusted text (model
// transcripts, notification bodies) can never turn into command syntax. The
// runner is injectable so unit tests can exercise notifier/injector logic
// without spawning real processes.

#include <functional>
#include <string>
#include <vector>

namespace lwf::process {

struct RunResult {
    bool launched = false;   // false: fork/exec failed (see error)
    bool timed_out = false;
    int exit_code = -1;
    std::string output;      // merged stdout+stderr
    std::string error;       // human-readable failure reason
};

// The command to run: executable resolved through PATH plus argv (argv[0] is
// prepended automatically from the executable name).
struct Command {
    std::string executable;
    std::vector<std::string> args;
};

// Runs a command, feeds stdin_data, captures merged stdout/stderr and enforces
// a wall-clock timeout (SIGKILL on expiry). Empty timeout_ms means no timeout.
RunResult Run(const Command& command, const std::string& stdin_data, int timeout_ms);

// Convenience overload without stdin.
RunResult Run(const Command& command, int timeout_ms);

// True when the executable exists and is runnable (PATH lookup like which(1)).
bool Exists(const std::string& executable);

// Injectable runner used by LinuxNotifier and LinuxTextInjector so their
// logic stays unit-testable without real subprocesses.
using Runner = std::function<RunResult(const Command&, const std::string&, int)>;

// Returns the real runner as a std::function.
Runner RealRunner();

}  // namespace lwf::process
