#pragma once

#include <atomic>
#include <string>

namespace lwf {

class CrashHandler {
public:
    static void Install(const std::string& logs_dir, const std::atomic<int>* fsm_state);
};

}
