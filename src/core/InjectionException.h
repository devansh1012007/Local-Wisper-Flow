#pragma once

#include "core/ErrorCode.h"

#include <stdexcept>
#include <string>

class InjectionException : public std::runtime_error {
public:
    InjectionException(ErrorCode code, const std::string& detail)
        : std::runtime_error(detail), code_(code) {}

    ErrorCode Code() const { return code_; }

private:
    ErrorCode code_;
};

