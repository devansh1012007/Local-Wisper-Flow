#pragma once

// ncurses frontend for the Linux TUI (plan 2026-11-21, change C07).
//
// Rendering and key mapping only: all navigation/editing logic lives in the
// headless-testable SettingsMenuModel. The app has four pages (groups/items,
// line editor, log viewer, status+help), a status line, and a key bar. F2
// persists the config atomically and SIGHUPs the daemon; F10 or q quits.

#include "ui/tui/SettingsMenuModel.h"

#include <string>

class ConfigManager;

class TuiApp {
public:
    // Returns the process exit code. config must outlive the app.
    TuiApp(ConfigManager& config, std::string log_path);
    int Run();

private:
    enum class OverlayPage { None, Log, Status, Help };

    void DrawFrame();
    void DrawGroupsItems();
    void DrawEditor();
    void DrawOverlay();
    void DrawKeyBar();
    bool HandleKey(int key);
    bool HandleEditorKey(int key);
    bool HandleOverlayKey(int key);
    bool SaveNow();
    void ShowOverlay(OverlayPage page);

    ConfigManager& config_;
    SettingsMenuModel model_;
    std::string log_path_;
    OverlayPage overlay_ = OverlayPage::None;
    std::vector<std::string> log_lines_;
    std::string status_flash_;
    bool quit_ = false;
};
