#pragma once

#include <string>

struct ActiveWindowInfo {
    std::string process_name;
    std::string window_title;
};

class IActiveWindow {
public:
    virtual ~IActiveWindow() = default;
    virtual ActiveWindowInfo Get() = 0;
};
