#pragma once

#include <string>

namespace lwf::ui::widgets {

struct Modal {
    void Open();
    void Close();
    bool IsOpen() const;
    bool Begin(const char* title, float scale);
    void End();

private:
    bool open_ = false;
    bool armed_ = false;
};

struct Sheet {
    void Open();
    void Close();
    bool IsOpen() const;
    bool Begin(const char* title, float width, float scale);
    void End();

private:
    bool open_ = false;
    bool armed_ = false;
};

struct Toast {
    void Show(const std::string& message, const std::string& action_label);
    void Dismiss();
    bool IsVisible() const;
    void Draw(float scale);
    bool TakeAction();

private:
    std::string message_;
    std::string action_label_;
    float shown_at_ = -1.0f;
    bool action_pending_ = false;
};

}
