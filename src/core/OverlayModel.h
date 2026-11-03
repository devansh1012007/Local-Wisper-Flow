#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace OverlayModel {

enum class PillState { Hidden, Listening, Processing, Error, Cancelled };

struct Visual {
    bool visible = false;
    PillState state = PillState::Hidden;
    std::string message;
    int auto_hide_after_ms = 0;
};

Visual VisualFor(PillState state, const std::string& error_message);

class WaveformRing {
public:
    explicit WaveformRing(std::size_t capacity = 64);

    void Push(float level);
    std::vector<float> Snapshot() const;
    std::size_t Size() const;

private:
    std::vector<float> buffer_;
    std::size_t head_ = 0;
    std::size_t filled_ = 0;
};

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

Rect PillPosition(const Rect& work_area, int pill_width, int pill_height, int margin,
                  double dpi_scale);

}
