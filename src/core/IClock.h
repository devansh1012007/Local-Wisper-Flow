#pragma once

#include <cstdint>

class IClock {
public:
    virtual ~IClock() = default;
    virtual std::int64_t NowMs() const = 0;
};

class RealClock : public IClock {
public:
    std::int64_t NowMs() const override;
};
