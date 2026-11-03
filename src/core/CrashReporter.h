#pragma once

#include <string>

namespace lwf {

class CrashReporter {
public:
    static std::string WriteCrashReport(const std::string& logs_dir,
                                        const std::string& exception_code,
                                        const std::string& fsm_state,
                                        const std::string& detail);
    static bool HasCrashReports(const std::string& logs_dir);
    static std::string FindUnseenCrash(const std::string& logs_dir);
    static bool MarkSeen(const std::string& crash_path);
};

}
