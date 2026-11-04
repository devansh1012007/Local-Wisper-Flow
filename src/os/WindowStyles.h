#pragma once

namespace WindowStyles {

// Base styles for the Mini badge: topmost tool window that never activates.
// Note: this also enables click-through; use SetOverlayPassThrough to toggle
// it when the badge needs hover or click interaction.
void ApplyOverlayStyles(void* hwnd);

// Enables or disables mouse click-through (WS_EX_TRANSPARENT) on the Mini
// badge window. When disabled the window receives normal mouse events.
void SetOverlayPassThrough(void* hwnd, bool pass_through);

// True when the screen-space cursor is inside the rect (screen pixels,
// work-area coordinates) inflated by inflate pixels on every side.
bool IsCursorInsideRect(int x, int y, int width, int height, int inflate);

}  // namespace WindowStyles
