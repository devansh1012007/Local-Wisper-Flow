#pragma once

#include <functional>

class IHotkeyListener {
public:
    virtual ~IHotkeyListener() = default;

    virtual void StartListening(std::function<void(bool is_pressed)> callback) = 0;
};
