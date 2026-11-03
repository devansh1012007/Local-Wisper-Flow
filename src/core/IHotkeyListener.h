#pragma once

#include "core/EngineTypes.h"

class IHotkeyListener {
public:
    virtual ~IHotkeyListener() = default;

    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual bool PollEvent(EngineEvent& out) = 0;
    virtual void SetCancelArmed(bool armed) = 0;
};
