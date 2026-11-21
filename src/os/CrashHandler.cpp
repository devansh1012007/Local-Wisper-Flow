#include "os/CrashHandler.h"

#include "core/AppVersion.h"
#include "core/CrashReporter.h"

#include <atomic>
#include <cstdio>
#include <string>

#ifdef _WIN32

#include <windows.h>

namespace {

std::string* g_logs_dir = nullptr;
const std::atomic<int>* g_fsm_state = nullptr;

std::string DescribeExceptionCode(std::uint32_t code) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "0x%08X", code);
    return buffer;
}

std::string FsmStateString() {
    if (g_fsm_state == nullptr) {
        return "unknown";
    }
    return std::to_string(g_fsm_state->load(std::memory_order_relaxed));
}

LONG WINAPI UnhandledFilter(EXCEPTION_POINTERS* info) {
    const std::uint32_t code = info != nullptr && info->ExceptionRecord != nullptr
                                   ? info->ExceptionRecord->ExceptionCode
                                   : 0u;
    lwf::CrashReporter::WriteCrashReport(*g_logs_dir, DescribeExceptionCode(code),
                                         FsmStateString(), "");
    return EXCEPTION_EXECUTE_HANDLER;
}

}

namespace lwf {

void CrashHandler::Install(const std::string& logs_dir, const std::atomic<int>* fsm_state) {
    static std::string logs_dir_storage;
    logs_dir_storage = logs_dir;
    g_logs_dir = &logs_dir_storage;
    g_fsm_state = fsm_state;
    SetUnhandledExceptionFilter(&UnhandledFilter);
}

}

#else

// POSIX daemon crash handling: a sigaction handler for the fatal signals
// that writes the same crash-<ms>.txt marker the Windows filter produces,
// using only async-signal-safe calls (open/write/close/clock_gettime).
// The next daemon start surfaces it through CrashReporter::FindUnseenCrash.

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <cstring>

namespace {

char g_logs_dir[512] = {0};
const std::atomic<int>* g_fsm_state = nullptr;

void AppendDigits(char* buffer, std::size_t& length, long long value) {
    char digits[24];
    int count = 0;
    if (value <= 0) {
        digits[count++] = '0';
        value = 0;
    }
    while (value > 0 && count < 24) {
        digits[count++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }
    for (int i = count - 1; i >= 0; --i) {
        buffer[length++] = digits[i];
    }
}

void AppendText(char* buffer, std::size_t& length, const char* text) {
    while (*text != '\0' && length < sizeof(buffer) - 1) {
        buffer[length++] = *text++;
    }
}

extern "C" void FatalSignalHandler(int signal_number, siginfo_t* info, void*) {
    char path[600];
    std::size_t path_length = 0;
    timespec now{};
    clock_gettime(CLOCK_REALTIME, &now);
    const long long stamp_ms =
        static_cast<long long>(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
    AppendText(path, path_length, g_logs_dir);
    AppendText(path, path_length, "/crash-");
    AppendDigits(path, path_length, stamp_ms);
    AppendText(path, path_length, ".txt");

    char body[512];
    std::size_t body_length = 0;
    AppendText(body, body_length, "app: Local Wisper Flow\nexception: signal ");
    AppendDigits(body, body_length, signal_number);
    AppendText(body, body_length, "\nfsm_state: ");
    AppendDigits(body, body_length,
                 g_fsm_state != nullptr ? g_fsm_state->load(std::memory_order_relaxed) : -1);
    AppendText(body, body_length, "\naddress: ");
    AppendDigits(body, body_length,
                  reinterpret_cast<long long>(info != nullptr ? info->si_addr : nullptr));
    AppendText(body, body_length, "\n");

    const int fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        ssize_t ignored = ::write(fd, body, body_length);
        (void)ignored;
        ::close(fd);
    }
    ::_exit(128 + signal_number);
}

}  // namespace

namespace lwf {

void CrashHandler::Install(const std::string& logs_dir, const std::atomic<int>* fsm_state) {
    std::strncpy(g_logs_dir, logs_dir.c_str(), sizeof(g_logs_dir) - 1);
    g_fsm_state = fsm_state;

    struct sigaction action{};
    action.sa_sigaction = &FatalSignalHandler;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    ::sigemptyset(&action.sa_mask);
    const int signals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGILL};
    for (const int signal_number : signals) {
        ::sigaction(signal_number, &action, nullptr);
    }
}

}  // namespace

#endif
