#pragma once

#include "core/ToneGenerator.h"

class IToneFeedback {
public:
    virtual ~IToneFeedback() = default;
    virtual void Play(const ToneGenerator::ToneSpec& spec, double volume) = 0;
};
