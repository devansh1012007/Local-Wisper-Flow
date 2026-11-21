#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Pure model for the Mini badge (formerly the overlay pill). No UI or window
// dependencies: everything here is unit tested from lwf_tests.
//
// State vocabulary (docs/design/mockup.html section 2):
//   Hidden    - badge not shown (reserved for "mini off"; not used by default)
//   Ready     - resident idle dot
//   Listening - capturing speech, level bars animate
//   Thinking  - transcribing / post-processing
//   Done      - transcript injected, brief check, then back to Ready
//   Paused    - dictation disabled
//   Attention - something needs the user; hover the badge to read the message
namespace OverlayModel {

enum class PillState { Hidden, Ready, Listening, Thinking, Done, Paused, Attention };

struct Visual {
    bool visible = false;
    PillState state = PillState::Hidden;
    std::string message;
    // Time in milliseconds the state stays after entering it before the badge
    // returns to Ready. 0 = stay until the state changes.
    int dwell_after_ms = 0;
};

Visual VisualFor(PillState state, const std::string& message);

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

// Anchors a badge of the given size bottom-center above the taskbar.
Rect PillPosition(const Rect& work_area, int pill_width, int pill_height, int margin,
                  double dpi_scale);

// Keeps a user-dragged window top-left inside the work area so the badge can
// never be dragged off-screen and lost. Windows larger than the work area
// are pinned to the work area origin.
Rect ClampPosition(const Rect& work_area, int x, int y, int width, int height);

}  // namespace OverlayModel
