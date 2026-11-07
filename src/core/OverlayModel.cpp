#include "core/OverlayModel.h"

#include <algorithm>
#include <stdexcept>

namespace OverlayModel {

Visual VisualFor(PillState state, const std::string& message) {
    Visual visual;
    visual.state = state;
    switch (state) {
        case PillState::Hidden:
            visual.visible = false;
            break;
        case PillState::Ready:
        case PillState::Listening:
        case PillState::Thinking:
        case PillState::Paused:
            visual.visible = true;
            break;
        case PillState::Done:
            visual.visible = true;
            visual.dwell_after_ms = 600;
            break;
        case PillState::Attention:
            visual.visible = true;
            visual.message = message;
            visual.dwell_after_ms = 6000;
            break;
    }
    return visual;
}

WaveformRing::WaveformRing(std::size_t capacity) : buffer_(capacity, 0.0f) {}

void WaveformRing::Push(float level) {
    buffer_[head_] = level;
    head_ = (head_ + 1) % buffer_.size();
    if (filled_ < buffer_.size()) {
        ++filled_;
    }
}

void WaveformRing::SnapshotInto(std::vector<float>& out) const {
    out.clear();
    out.reserve(filled_);
    const std::size_t start = filled_ < buffer_.size() ? 0 : head_;
    for (std::size_t i = 0; i < filled_; ++i) {
        out.push_back(buffer_[(start + i) % buffer_.size()]);
    }
}

float WaveformRing::At(std::size_t index) const {
    if (index >= filled_) {
        throw std::out_of_range("WaveformRing::At past newest sample");
    }
    const std::size_t start = filled_ < buffer_.size() ? 0 : head_;
    return buffer_[(start + index) % buffer_.size()];
}

std::vector<float> WaveformRing::Snapshot() const {
    std::vector<float> snapshot;
    SnapshotInto(snapshot);
    return snapshot;
}

std::size_t WaveformRing::Size() const {
    return filled_;
}

Rect PillPosition(const Rect& work_area, int pill_width, int pill_height, int margin,
                  double dpi_scale) {
    const int scaled_width = static_cast<int>(static_cast<double>(pill_width) * dpi_scale);
    const int scaled_height = static_cast<int>(static_cast<double>(pill_height) * dpi_scale);
    const int scaled_margin = static_cast<int>(static_cast<double>(margin) * dpi_scale);
    Rect position;
    position.width = scaled_width;
    position.height = scaled_height;
    position.x = work_area.x + (work_area.width - scaled_width) / 2;
    position.x = std::max(position.x, work_area.x);
    position.y = work_area.y + work_area.height - scaled_height - scaled_margin;
    position.y = std::max(position.y, work_area.y);
    return position;
}

Rect ClampPosition(const Rect& work_area, int x, int y, int width, int height) {
    Rect position;
    position.width = width;
    position.height = height;
    const int max_x = std::max(work_area.x, work_area.x + work_area.width - width);
    const int max_y = std::max(work_area.y, work_area.y + work_area.height - height);
    position.x = std::clamp(x, work_area.x, max_x);
    position.y = std::clamp(y, work_area.y, max_y);
    return position;
}

}  // namespace OverlayModel
