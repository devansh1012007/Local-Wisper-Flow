#pragma once

#include <string>

class SpokenCommandProcessor {
public:
    SpokenCommandProcessor() = default;

    void SetAggressive(bool aggressive);
    bool Aggressive() const;

    std::string ToSentinels(const std::string& raw) const;
    std::string ExpandSentinels(const std::string& text) const;

    static std::size_t CountSentinels(const std::string& text);
    static std::string SentinelPreservationHint();

private:
    bool aggressive_ = false;
};
