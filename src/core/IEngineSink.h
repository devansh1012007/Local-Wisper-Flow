#pragma once

#include "core/ErrorCode.h"

#include <string>

class IEngineSink {
public:
    virtual ~IEngineSink() = default;
    virtual void OnStatus(const std::string& message) = 0;
    virtual void OnError(ErrorCode code, const std::string& detail) = 0;
};
