#include "ui/tui/TuiApp.h"

#include "core/ConfigManager.h"
#include "core/Log.h"
#include "os/LinuxPlatform.h"

#include <ncurses.h>

#include <algorithm>
#include <csignal>
#include <new>
#include <string>
#include <vector>

namespace {

// Layout: rows 0..h-3 are the main pane, row h-2 the status line, row h-1
// the key bar. The right 60% of the main pane shows items once a group is
// open.
constexpr int kKeyBarHeight = 1;
constexpr int kStatusHeight = 1;

}  // namespace

TuiApp::TuiApp(ConfigManager& config, std::string log_path)
    : config_(config), model_(config.Store()), log_path_(std::move(log_path)) {}

void TuiApp::ShowOverlay(OverlayPage page) {
    overlay_ = page;
    if (page == OverlayPage::Log) {
        log_lines_ = SettingsMenuModel::TailFile(log_path_, 200);
    }
}

bool TuiApp::SaveNow() {
    config_.Save();
    const int pid = lwf::linuxplatform::ReadDaemonPid();
    const bool daemon_running = pid > 0 && lwf::linuxplatform::SendReloadToDaemon(pid);
    if (daemon_running) {
        lwf::Log::Info("config saved by TUI; SIGHUP sent to daemon " + std::to_string(pid));
    }
    model_.MarkSaved(daemon_running);
    return true;
}

int TuiApp::Run() {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_CYAN, -1);     // selection
        init_pair(2, COLOR_YELLOW, -1);   // status
        init_pair(3, COLOR_GREEN, -1);    // on/saved
        init_pair(4, COLOR_MAGENTA, -1);  // hot-apply marker
    }

    int exit_code = 0;
    while (!quit_) {
        DrawFrame();
        const int key = getch();
        if (overlay_ != OverlayPage::None) {
            if (!HandleOverlayKey(key)) {
                quit_ = true;
                exit_code = model_.dirty() ? 2 : 0;
            }
            continue;
        }
        if (model_.page() == SettingsMenuModel::Page::Editor) {
            if (!HandleEditorKey(key)) {
                quit_ = true;
                exit_code = model_.dirty() ? 2 : 0;
            }
            continue;
        }
        if (!HandleKey(key)) {
            quit_ = true;
            exit_code = model_.dirty() ? 2 : 0;
        }
    }

    endwin();
    return exit_code;
}

bool TuiApp::HandleKey(int key) {
    SettingsMenuModel::Action action;
    switch (key) {
        case KEY_UP:
        case 'k':
            action = SettingsMenuModel::Action::Up;
            break;
        case KEY_DOWN:
        case 'j':
            action = SettingsMenuModel::Action::Down;
            break;
        case KEY_PPAGE:
            action = SettingsMenuModel::Action::PageUp;
            break;
        case KEY_NPAGE:
            action = SettingsMenuModel::Action::PageDown;
            break;
        case KEY_ENTER:
        case '\n':
        case '\r':
        case KEY_RIGHT:
        case 'l':
            action = SettingsMenuModel::Action::Enter;
            break;
        case KEY_BACKSPACE:
        case 127:
        case KEY_LEFT:
        case 'h':
        case 27:  // Esc
            action = SettingsMenuModel::Action::Back;
            break;
        case KEY_F(2):
        case 's': {
            const std::string result = model_.Apply(SettingsMenuModel::Action::Save);
            if (result == "save") {
                SaveNow();
            }
            return true;
        }
        case 'r': {
            const std::string result = model_.Apply(SettingsMenuModel::Action::ReloadFromDisk);
            if (result == "reload") {
                config_.Reload();
                // Rebuild the model against the reloaded store.
                model_.~SettingsMenuModel();
                new (&model_) SettingsMenuModel(config_.Store());
                status_flash_ = "reloaded from disk";
            }
            return true;
        }
        case 'L':
            ShowOverlay(OverlayPage::Log);
            return true;
        case 'i':
            ShowOverlay(OverlayPage::Status);
            return true;
        case '?':
        case KEY_F(1):
            ShowOverlay(OverlayPage::Help);
            return true;
        case KEY_F(10):
        case 'q':
            return model_.Apply(SettingsMenuModel::Action::Quit) == "quit" ? false : true;
        default:
            return true;
    }
    model_.Apply(action);
    return true;
}

bool TuiApp::HandleEditorKey(int key) {
    std::string draft = model_.editor_draft();
    switch (key) {
        case KEY_ENTER:
        case '\n':
        case '\r': {
            const auto error = model_.CommitEditor();
            if (error.has_value()) {
                status_flash_ = *error;
            }
            curs_set(0);
            return true;
        }
        case KEY_BACKSPACE:
        case 127:
            if (!draft.empty()) {
                draft.pop_back();
                model_.SetEditorText(draft);
            }
            return true;
        case 27:  // Esc
            model_.Apply(SettingsMenuModel::Action::Back);
            curs_set(0);
            return true;
        default:
            if (key >= 32 && key < 127) {
                draft += static_cast<char>(key);
                model_.SetEditorText(draft);
            }
            return true;
    }
}

bool TuiApp::HandleOverlayKey(int key) {
    if (key == 'L') {
        ShowOverlay(OverlayPage::Log);  // refresh
        return true;
    }
    if (key == 'q' || key == 27 /* Esc */) {
        overlay_ = OverlayPage::None;
        return true;
    }
    if (key == KEY_F(10)) {
        return false;
    }
    return true;
}

void TuiApp::DrawFrame() {
    erase();
    if (overlay_ != OverlayPage::None) {
        DrawOverlay();
        DrawKeyBar();
        refresh();
        return;
    }
    if (model_.page() == SettingsMenuModel::Page::Editor) {
        DrawEditor();
    } else {
        DrawGroupsItems();
    }
    DrawKeyBar();
    refresh();
}

void TuiApp::DrawGroupsItems() {
    int width = 0;
    int height = 0;
    getmaxyx(stdscr, height, width);
    (void)width;
    const int main_height = height - kKeyBarHeight - kStatusHeight;

    attron(A_BOLD);
    mvprintw(0, 1, "Local Wisper Flow - Settings%s", model_.dirty() ? " *" : "");
    attroff(A_BOLD);

    const auto& groups = model_.group_names();
    const int visible_rows = std::max(1, main_height - 2);
    int first = std::max(0, model_.group_cursor() - visible_rows + 1);
    for (int i = first; i < static_cast<int>(groups.size()) && i < first + visible_rows; ++i) {
        const bool selected = i == model_.group_cursor() && model_.page() == SettingsMenuModel::Page::Groups;
        if (selected) {
            attron(COLOR_PAIR(1) | A_BOLD);
        }
        mvprintw(i - first + 2, 2, "%s %s", selected ? ">" : " ", groups[i].c_str());
        if (selected) {
            attroff(COLOR_PAIR(1) | A_BOLD);
        }
    }

    const int item_x = 26;
    const auto& items = model_.item_rows();
    int item_first = std::max(0, model_.item_cursor() - visible_rows + 1);
    for (int i = item_first; i < static_cast<int>(items.size()) && i < item_first + visible_rows;
         ++i) {
        const bool selected = i == model_.item_cursor();
        const int row = i - item_first + 2;
        if (selected) {
            attron(COLOR_PAIR(1) | A_BOLD);
        }
        std::string label = items[i].label.substr(0, 28);
        std::string value = items[i].value;
        if (value.empty()) {
            value = "(empty)";
        }
        mvprintw(row, item_x, "%c %-28s %-20s %s", selected ? '>' : ' ', label.c_str(),
                 value.substr(0, 20).c_str(), items[i].hot_apply ? "*" : " ");
        if (selected) {
            attroff(COLOR_PAIR(1) | A_BOLD);
        }
    }
    const std::string hint = model_.KeysymHint();
    if (!hint.empty() && model_.page() == SettingsMenuModel::Page::Items) {
        mvprintw(std::min(main_height, height - kKeyBarHeight - 2), item_x, "%s", hint.c_str());
    }
}

void TuiApp::DrawEditor() {
    int width = 0;
    int height = 0;
    getmaxyx(stdscr, height, width);
    attron(A_BOLD);
    mvprintw(2, 2, "Edit value");
    attroff(A_BOLD);
    const auto& items = model_.item_rows();
    std::string label;
    if (model_.item_cursor() >= 0 && model_.item_cursor() < static_cast<int>(items.size())) {
        label = items[model_.item_cursor()].label;
    }
    mvprintw(4, 2, "Key:    %s", label.c_str());
    mvprintw(5, 2, "Value:  %s_", model_.editor_draft().c_str());
    mvprintw(7, 2, "Enter commits, Esc cancels");
    curs_set(1);
    (void)width;
}

void TuiApp::DrawOverlay() {
    int width = 0;
    int height = 0;
    getmaxyx(stdscr, height, width);

    attron(A_BOLD);
    if (overlay_ == OverlayPage::Log) {
        mvprintw(0, 1, "Log viewer - %s", log_path_.c_str());
        attroff(A_BOLD);
        int row = 2;
        const std::size_t skip = log_lines_.size() > static_cast<std::size_t>(height - 4)
                                     ? log_lines_.size() - static_cast<std::size_t>(height - 4)
                                     : 0;
        for (std::size_t i = skip; i < log_lines_.size() && row < height - 2; ++i, ++row) {
            mvprintw(row, 1, "%.79s", log_lines_[i].c_str());
        }
    } else if (overlay_ == OverlayPage::Status) {
        mvprintw(0, 1, "Status");
        attroff(A_BOLD);
        const auto caps = lwf::linuxplatform::ProbeCapabilities();
        const int pid = lwf::linuxplatform::ReadDaemonPid();
        const std::string session = lwf::linuxplatform::ToString(lwf::linuxplatform::DetectSession());
        int row = 2;
        mvprintw(row++, 2, "Daemon:      %s",
                 pid > 0 ? ("running (pid " + std::to_string(pid) + ")").c_str() : "not running");
        mvprintw(row++, 2, "Session:     %s", session.c_str());
        mvprintw(row++, 2, "Hotkey X11:  %s", caps.x11_hotkey ? "available" : "no");
        mvprintw(row++, 2, "Hotkey evdev:%s", caps.evdev_hotkey ? "readable" : "no");
        mvprintw(row++, 2, "XTest inject:%s", caps.xtest_inject ? "built" : "no");
        mvprintw(row++, 2, "Toasts:      %s", caps.toasts ? "notify-send" : "missing");
        mvprintw(row++, 2, "Unsaved:     %s", model_.dirty() ? "yes (F2 saves)" : "no");
    } else {
        mvprintw(0, 1, "Help");
        attroff(A_BOLD);
        int row = 2;
        mvprintw(row++, 2, "j/k or arrows   move");
        mvprintw(row++, 2, "Enter           open group / edit value / toggle");
        mvprintw(row++, 2, "h/l or Esc      back / open");
        mvprintw(row++, 2, "F2 or s         save + reload daemon (SIGHUP)");
        mvprintw(row++, 2, "r               reload from disk (discard edits)");
        mvprintw(row++, 2, "L               log viewer");
        mvprintw(row++, 2, "i               status");
        mvprintw(row++, 2, "q / F10         quit");
        mvprintw(row++, 2, "*               hot-apply key (daemon applies on reload)");
    }
}

void TuiApp::DrawKeyBar() {
    int width = 0;
    int height = 0;
    getmaxyx(stdscr, height, width);
    (void)width;
    const int status_row = height - kKeyBarHeight - kStatusHeight;
    const std::string& model_status = model_.status();
    std::string status = !status_flash_.empty() ? status_flash_ : model_status;
    if (status.empty()) {
        status = model_.page() == SettingsMenuModel::Page::Groups ? "Pick a group" : "Pick a value";
    }
    if (has_colors()) {
        attron(COLOR_PAIR(2));
    }
    mvprintw(status_row, 1, "%.100s", status.c_str());
    if (has_colors()) {
        attroff(COLOR_PAIR(2));
    }
    status_flash_.clear();

    attron(A_REVERSE);
    const char* bar =
        overlay_ != OverlayPage::None
            ? " q: close   L: refresh   F10: quit"
            : " Enter: edit   F2: save   r: reload   L: log   i: status   ?: help   F10: quit";
    mvprintw(height - 1, 0, "%s", bar);
    clrtoeol();
    attroff(A_REVERSE);
}
