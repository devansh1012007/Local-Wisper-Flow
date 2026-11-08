#pragma once

#include "ui/theme/ThemePalette.h"

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
    bool visible_ = false;
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
    bool visible_ = false;
};

struct Toast {
    void Show(const std::string& message, const std::string& action_label);
    void Dismiss();
    bool IsVisible() const;
    // DECISION: default is Ivory primary (kLightPalette via Widgets.h
    // companion); callers that carry a Palette pass it explicitly so the dark
    // variant is a runtime choice, not a second hard-coded palette.
    void Draw(float scale,
              const lwf::ui::Palette& palette = lwf::ui::kLightPalette);
    bool TakeAction();

private:
    std::string message_;
    std::string action_label_;
    float shown_at_ = -1.0f;
    bool action_pending_ = false;
};

}
