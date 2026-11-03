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

namespace lwf {

void CrashHandler::Install(const std::string&, const std::atomic<int>*) {}

}

#endif
