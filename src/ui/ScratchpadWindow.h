#pragma once

#include "core/ITextInjector.h"
#include "core/NotesStore.h"
#include "core/SettingsStore.h"

#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

struct GLFWwindow;
struct ImGuiContext;

namespace lwf::ui {
struct Palette;
}

namespace lwf::ui::scratchpad {

inline constexpr const char* kEditorTitle = "Scratchpad";
inline constexpr const char* kSavedLabel = "Saved";
inline constexpr std::size_t kEditorDefaultWidth = 760;
inline constexpr std::size_t kEditorDefaultHeight = 620;
inline constexpr std::size_t kEditorMinWidth = 480;
inline constexpr std::size_t kEditorMinHeight = 360;

}

namespace lwf::ui::scratchpad {

inline std::size_t CountWords(const std::string& text) {
    std::size_t words = 0;
    bool in_word = false;
    for (const char c : text) {
        const bool blank = c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
        if (blank) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            ++words;
        }
    }
    return words;
}

inline std::string CountWordsLabel(const std::string& text) {
    return std::to_string(CountWords(text)) + " words";
}

inline void InsertAtCaret(std::string& body, std::size_t& caret, const std::string& text) {
    if (text.empty()) {
        return;
    }
    if (caret > body.size()) {
        caret = body.size();
    }
    body.insert(caret, text);
    caret += text.size();
}

inline void ClampSelection(std::size_t& start, std::size_t& end, std::size_t size) {
    if (start > size) {
        start = size;
    }
    if (end > size) {
        end = size;
    }
    if (start > end) {
        const std::size_t swap = start;
        start = end;
        end = swap;
    }
}

inline std::string ApplyBold(std::string body, std::size_t start, std::size_t end) {
    ClampSelection(start, end, body.size());
    if (start == end) {
        body.insert(start, "****");
        return body;
    }
    const bool wrapped = start >= 2 && end + 2 <= body.size() &&
                         body.compare(start - 2, 2, "**") == 0 &&
                         body.compare(end, 2, "**") == 0;
    if (wrapped) {
        body.erase(end, 2);
        body.erase(start - 2, 2);
        return body;
    }
    body.insert(end, "**");
    body.insert(start, "**");
    return body;
}

inline std::string ApplyItalic(std::string body, std::size_t start, std::size_t end) {
    ClampSelection(start, end, body.size());
    if (start == end) {
        body.insert(start, "**");
        return body;
    }
    const bool left_single = start >= 1 && body[start - 1] == '*' &&
                             (start < 2 || body[start - 2] != '*');
    const bool right_single = end < body.size() && body[end] == '*' &&
                              (end + 1 >= body.size() || body[end + 1] != '*');
    if (left_single && right_single) {
        body.erase(end, 1);
        body.erase(start - 1, 1);
        return body;
    }
    body.insert(end, "*");
    body.insert(start, "*");
    return body;
}

inline bool IsNumberedPrefix(const std::string& line, std::size_t& prefix_len) {
    std::size_t i = 0;
    while (i < line.size() && line[i] >= '0' && line[i] <= '9') {
        ++i;
    }
    if (i == 0) {
        return false;
    }
    if (i + 1 < line.size() && line[i] == '.' && line[i + 1] == ' ') {
        prefix_len = i + 2;
        return true;
    }
    return false;
}

struct LineSlice {
    std::size_t start = 0;
    std::size_t length = 0;
};

inline std::vector<LineSlice> SplitLines(const std::string& body) {
    std::vector<LineSlice> lines;
    std::size_t start = 0;
    while (start <= body.size()) {
        const std::size_t newline = body.find('\n', start);
        if (newline == std::string::npos) {
            if (start < body.size() || lines.empty()) {
                lines.push_back(LineSlice{start, body.size() - start});
            }
            break;
        }
        lines.push_back(LineSlice{start, newline - start});
        start = newline + 1;
        if (start == body.size()) {
            lines.push_back(LineSlice{start, 0});
            break;
        }
    }
    if (lines.empty()) {
        lines.push_back(LineSlice{0, 0});
    }
    return lines;
}

inline std::vector<LineSlice> LinesInRange(const std::string& body, std::size_t start,
                                           std::size_t end) {
    ClampSelection(start, end, body.size());
    const std::vector<LineSlice> all = SplitLines(body);
    std::vector<LineSlice> hit;
    for (const LineSlice& line : all) {
        const std::size_t line_end = line.start + line.length;
        if (start == end) {
            if (start >= line.start && start <= line_end) {
                hit.push_back(line);
                break;
            }
            continue;
        }
        if (line_end < start) {
            continue;
        }
        if (line.start > end) {
            break;
        }
        if (line.start == line_end && line.start != start) {
            continue;
        }
        hit.push_back(line);
    }
    if (hit.empty() && !all.empty()) {
        hit.push_back(all.back());
    }
    return hit;
}

inline std::string ApplyBullet(std::string body, std::size_t start, std::size_t end) {
    const std::vector<LineSlice> hit = LinesInRange(body, start, end);
    bool all_prefixed = !hit.empty();
    for (const LineSlice& line : hit) {
        const std::string text = body.substr(line.start, line.length);
        if (text.size() < 2 || text.compare(0, 2, "- ") != 0) {
            all_prefixed = false;
            break;
        }
    }
    for (std::size_t i = hit.size(); i-- > 0;) {
        const LineSlice& line = hit[i];
        const std::string text = body.substr(line.start, line.length);
        const bool prefixed = text.size() >= 2 && text.compare(0, 2, "- ") == 0;
        if (all_prefixed) {
            body.erase(line.start, 2);
        } else if (!prefixed) {
            body.insert(line.start, "- ");
        }
    }
    return body;
}

inline std::string ApplyNumbered(std::string body, std::size_t start, std::size_t end) {
    const std::vector<LineSlice> hit = LinesInRange(body, start, end);
    bool all_prefixed = !hit.empty();
    for (const LineSlice& line : hit) {
        const std::string text = body.substr(line.start, line.length);
        std::size_t prefix_len = 0;
        if (!IsNumberedPrefix(text, prefix_len)) {
            all_prefixed = false;
            break;
        }
    }
    if (all_prefixed) {
        for (std::size_t i = hit.size(); i-- > 0;) {
            const LineSlice& line = hit[i];
            const std::string text = body.substr(line.start, line.length);
            std::size_t prefix_len = 0;
            if (IsNumberedPrefix(text, prefix_len)) {
                body.erase(line.start, prefix_len);
            }
        }
        return body;
    }
    for (std::size_t i = hit.size(); i-- > 0;) {
        const LineSlice& line = hit[i];
        const std::string text = body.substr(line.start, line.length);
        std::size_t prefix_len = 0;
        if (IsNumberedPrefix(text, prefix_len)) {
            body.erase(line.start, prefix_len);
        }
    }
    for (std::size_t i = hit.size(); i-- > 0;) {
        const LineSlice& line = hit[i];
        const std::string prefix = std::to_string(i + 1) + ". ";
        body.insert(line.start, prefix);
    }
    return body;
}

inline std::string ApplyHeading(std::string body, std::size_t start, std::size_t end) {
    const std::vector<LineSlice> hit = LinesInRange(body, start, end);
    bool all_prefixed = !hit.empty();
    for (const LineSlice& line : hit) {
        const std::string text = body.substr(line.start, line.length);
        if (text.size() < 2 || text.compare(0, 2, "# ") != 0) {
            all_prefixed = false;
            break;
        }
    }
    for (std::size_t i = hit.size(); i-- > 0;) {
        const LineSlice& line = hit[i];
        const std::string text = body.substr(line.start, line.length);
        const bool prefixed = text.size() >= 2 && text.compare(0, 2, "# ") == 0;
        if (all_prefixed) {
            body.erase(line.start, 2);
        } else if (!prefixed) {
            body.insert(line.start, "# ");
        }
    }
    return body;
}

inline std::string EditorDisplayTitle(const std::string& title) {
    std::size_t begin = 0;
    while (begin < title.size() && (title[begin] == ' ' || title[begin] == '\t')) {
        ++begin;
    }
    std::size_t end = title.size();
    while (end > begin && (title[end - 1] == ' ' || title[end - 1] == '\t')) {
        --end;
    }
    if (begin >= end) {
        return "Untitled";
    }
    return title.substr(begin, end - begin);
}

inline bool IsValidCaret(const std::string& body, std::size_t caret) {
    return caret <= body.size();
}

}

namespace lwf::ui::scratchpad {

class ScratchpadRouteInjector final : public ITextInjector {
public:
    ScratchpadRouteInjector(ITextInjector& inner, ScratchpadBuffer& buffer,
                            std::atomic<bool>& active)
        : inner_(inner), buffer_(buffer), active_(active) {}

    void InjectText(const std::string& text) override {
        if (active_.load()) {
            buffer_.Append(text);
            return;
        }
        inner_.InjectText(text);
    }

private:
    ITextInjector& inner_;
    ScratchpadBuffer& buffer_;
    std::atomic<bool>& active_;
};

struct ScratchpadTab {
    std::string id;
    std::string title;
    std::string body;
    std::size_t caret = 0;
    std::size_t selection_start = 0;
    std::size_t selection_end = 0;
    bool dirty = false;
};

enum class ToolbarAction { Bold, Italic, Bullet, Numbered, Heading };

}

class ScratchpadWindow {
public:
    ScratchpadWindow(NotesStore& notes, SettingsStore& settings);
    ~ScratchpadWindow();

    ScratchpadWindow(const ScratchpadWindow&) = delete;
    ScratchpadWindow& operator=(const ScratchpadWindow&) = delete;

    bool Init();
    void Shutdown() noexcept;
    void SetVisible(bool visible);
    bool IsVisible() const;
    bool TakeCloseRequest();
    void RenderFrame(float delta_seconds);

    // Live theme swap (T2b, fed from main.cpp's ApplySettings drain). Does
    // NOT call ApplyImGuiStyle directly (the drain may run while another
    // window's context is current); defers to RenderFrame where this
    // window's context is current.
    void ApplyTheme(const lwf::ui::Palette& palette);

    void OpenNote(const std::string& id);
    void OpenNewNote();
    void SetDictationActive(bool active);
    bool IsDictationActive() const;
    std::atomic<bool>& DictationActiveFlag();
    ScratchpadBuffer& DictationBuffer();
    void DrainDictation();
    void InsertDictatedText(const std::string& text);
    void SetInputCaret(int cursor_pos, int selection_start, int selection_end);
    void ApplyToolbarAction(lwf::ui::scratchpad::ToolbarAction action);
    bool SaveActiveTab();
    std::size_t TabCount() const;
    int ActiveIndex() const;
    std::string ActiveBody() const;
    std::string ActiveTitle() const;
    std::size_t ActiveWordCount() const;

private:
    static void OnContentScale(GLFWwindow* window, float scale_x, float scale_y);
    void DrawTabs(float scale);
    void DrawToolbar(float scale);
    void DrawEditor(float scale);
    void DrawBottomBar(float scale);
    void SyncInputsFromActiveTab();
    void SyncActiveTabFromInputs();
    void RefreshDictationActive();
    void MarkDirtyAndSave();
    void ShowSaved(double now);

    NotesStore& notes_;
    SettingsStore& settings_;
    GLFWwindow* window_ = nullptr;
    ImGuiContext* imgui_context_ = nullptr;
    bool shutdown_done_ = false;
    bool close_requested_ = false;
    double dpi_scale_ = 1.0;
    float pending_dpi_scale_ = 0.0f;
    // T2b theme selection. INVARIANT: current_palette_ is the last-applied
    // palette (Init promotes the store selection into it); pending slots hold
    // a live-swap selection until RenderFrame applies it with this window's
    // context current. Pointer identity detects an actual change.
    const lwf::ui::Palette* current_palette_ = nullptr;
    const lwf::ui::Palette* pending_theme_palette_ = nullptr;
    bool theme_pending_ = false;
    bool focus_editor_pending_ = false;
    bool inputs_synced_ = false;
    double saved_until_ = -1.0;
    double last_edit_at_ = -1.0;
    std::string synced_tab_id_;
    char title_input_[256] = {};
    std::vector<char> body_input_;
    std::vector<lwf::ui::scratchpad::ScratchpadTab> tabs_;
    int active_index_ = -1;
    ScratchpadBuffer dictation_buffer_;
    std::atomic<bool> dictation_active_{false};
    std::size_t drained_len_ = 0;
};
