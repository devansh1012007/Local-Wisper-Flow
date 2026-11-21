#include "os/LinuxProcess.h"

#include <fcntl.h>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <thread>

namespace lwf::process {
namespace {

struct Pipes {
    int in[2] = {-1, -1};    // parent -> child stdin
    int out[2] = {-1, -1};   // child stdout+stderr -> parent
};

void CloseEnd(int& fd) {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

void ClosePipes(Pipes& pipes) {
    CloseEnd(pipes.in[0]);
    CloseEnd(pipes.in[1]);
    CloseEnd(pipes.out[0]);
    CloseEnd(pipes.out[1]);
}

void SetNonBlocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

// Reads whatever is available right now; the parent read end is
// non-blocking so this never stalls the supervision loop.
void DrainPipe(int fd, std::string& out) {
    char buffer[4096];
    for (;;) {
        const ssize_t got = ::read(fd, buffer, sizeof(buffer));
        if (got > 0) {
            out.append(buffer, static_cast<std::size_t>(got));
            continue;
        }
        return;  // EOF, EAGAIN or EINTR: nothing more available now
    }
}

void WriteAll(int fd, const std::string& data) {
    // SIGPIPE must not kill the daemon when a short-lived child exits before
    // consuming all of stdin; block it around the write, thread-local.
    sigset_t pipemask;
    sigset_t oldmask;
    ::sigemptyset(&pipemask);
    ::sigaddset(&pipemask, SIGPIPE);
    ::pthread_sigmask(SIG_BLOCK, &pipemask, &oldmask);

    const char* cursor = data.data();
    std::size_t remaining = data.size();
    while (remaining > 0) {
        const ssize_t wrote = ::write(fd, cursor, remaining);
        if (wrote > 0) {
            cursor += wrote;
            remaining -= static_cast<std::size_t>(wrote);
            continue;
        }
        if (wrote < 0 && errno == EINTR) {
            continue;
        }
        break;  // EAGAIN (pipe full, child not draining) or child gone
    }
    ::pthread_sigmask(SIG_SETMASK, &oldmask, nullptr);
}

}  // namespace

RunResult Run(const Command& command, const std::string& stdin_data, int timeout_ms) {
    RunResult result;
    if (command.executable.empty()) {
        result.error = "empty executable";
        return result;
    }

    Pipes pipes;
    if (::pipe(pipes.in) != 0) {
        result.error = "pipe failed";
        return result;
    }
    if (::pipe(pipes.out) != 0) {
        ClosePipes(pipes);
        result.error = "pipe failed";
        return result;
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        ClosePipes(pipes);
        result.error = "fork failed";
        return result;
    }
    if (pid == 0) {
        // Child: wire stdio onto the pipes, then execvp. Never returns.
        ::close(pipes.in[1]);
        ::close(pipes.out[0]);
        ::dup2(pipes.in[0], STDIN_FILENO);
        ::dup2(pipes.out[1], STDOUT_FILENO);
        ::dup2(pipes.out[1], STDERR_FILENO);
        ::close(pipes.in[0]);
        ::close(pipes.out[1]);
        std::vector<char*> argv;
        argv.reserve(command.args.size() + 2);
        argv.push_back(const_cast<char*>(command.executable.c_str()));
        for (const std::string& arg : command.args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        ::execvp(argv[0], argv.data());
        ::_exit(127);
    }

    // Parent: the read end is non-blocking so the supervision loop below can
    // interleave draining with waitpid and the timeout check.
    ::close(pipes.in[0]);
    pipes.in[0] = -1;
    ::close(pipes.out[1]);
    pipes.out[1] = -1;
    SetNonBlocking(pipes.out[0]);

    if (!stdin_data.empty()) {
        WriteAll(pipes.in[1], stdin_data);
    }
    CloseEnd(pipes.in[1]);

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : 0);
    for (;;) {
        DrainPipe(pipes.out[0], result.output);
        int status = 0;
        const pid_t reaped = ::waitpid(pid, &status, WNOHANG);
        if (reaped == pid) {
            if (WIFEXITED(status)) {
                result.exit_code = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                result.exit_code = 128 + WTERMSIG(status);
            }
            DrainPipe(pipes.out[0], result.output);  // final drain at EOF
            break;
        }
        if (reaped < 0 && errno != EINTR) {
            result.error = "waitpid failed";
            break;
        }
        if (timeout_ms > 0 && std::chrono::steady_clock::now() >= deadline) {
            ::kill(pid, SIGKILL);
            int kill_status = 0;
            ::waitpid(pid, &kill_status, 0);
            result.timed_out = true;
            result.error = "timed out";
            DrainPipe(pipes.out[0], result.output);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ClosePipes(pipes);
    result.launched = result.error.empty();
    if (result.launched && result.exit_code == 127) {
        // execvp failure convention; surface it as not-launched.
        result.launched = false;
        result.error = "exec failed: " + command.executable;
    }
    return result;
}

RunResult Run(const Command& command, int timeout_ms) {
    return Run(command, std::string(), timeout_ms);
}

bool Exists(const std::string& executable) {
    if (executable.empty()) {
        return false;
    }
    if (executable.find('/') != std::string::npos) {
        return ::access(executable.c_str(), X_OK) == 0;
    }
    const char* path_env = std::getenv("PATH");
    if (path_env == nullptr) {
        return false;
    }
    const std::string path(path_env);
    std::size_t begin = 0;
    while (begin <= path.size()) {
        std::size_t end = path.find(':', begin);
        if (end == std::string::npos) {
            end = path.size();
        }
        const std::string dir = path.substr(begin, end - begin);
        if (!dir.empty()) {
            const std::filesystem::path candidate = std::filesystem::path(dir) / executable;
            if (::access(candidate.c_str(), X_OK) == 0) {
                return true;
            }
        }
        if (end == path.size()) {
            break;
        }
        begin = end + 1;
    }
    return false;
}

Runner RealRunner() {
    return [](const Command& command, const std::string& stdin_data, int timeout_ms) {
        return Run(command, stdin_data, timeout_ms);
    };
}

}  // namespace lwf::process
